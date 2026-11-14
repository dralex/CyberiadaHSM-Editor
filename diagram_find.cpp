/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The find in the drawn texts of a diagram
 *
 * Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see https://www.gnu.org/licenses/
 *
 * ----------------------------------------------------------------------------- */

#include <QGraphicsView>
#include <QTimer>

#include "diagram_find.h"
#include "cyberiada_constants.h"
#include "cyberiadasm_dump.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_model.h"
#include "editable_text_item.h"

DiagramFind::DiagramFind(CyberiadaSMModel* model, CyberiadaSMEditorScene* scene, QObject* parent):
    QObject(parent), m_model(model), m_scene(scene)
{
    connect(m_scene, &CyberiadaSMEditorScene::textsChanged, this, &DiagramFind::scheduleSearch);
}

void DiagramFind::setQuery(const FindQuery& query, bool navigate)
{
    m_query = query;
    search(navigate);
}

void DiagramFind::walk(Cyberiada::Element* element, const QRegularExpression& expression, int& number)
{
    if (m_more) return;
    QString id = QString::fromStdString(element->get_id());
    int own = number++;
    m_numbers.insert(id, own);
    QGraphicsItem* item = nullptr;
    if (element->get_type() != Cyberiada::elementRoot) item = m_scene->getMap().value(element->get_id(), nullptr);
    if (item) {
        std::vector<EditableTextItem*> texts = textItemsOf(item);
        for (size_t t = 0; t < texts.size() && !m_more; t++) {
            FindRanges ranges = findRanges(texts[t]->toPlainText(), expression,
                                           FIND_MATCH_LIMIT - m_matches.size(), &m_more);
            for (const FindRange& range : ranges) {
                Match match = { texts[t], id, own, int(t), range.from, range.to };
                m_matches.append(match);
            }
        }
    }
    if (Cyberiada::ElementCollection* collection = dynamic_cast<Cyberiada::ElementCollection*>(element)) {
        const Cyberiada::ElementList& children = collection->get_children();
        for (Cyberiada::ElementList::const_iterator i = children.begin(); i != children.end(); i++) {
            walk(*i, expression, number);
        }
    }
}

void DiagramFind::search(bool navigate)
{
    bool had = m_current >= 0;
    Match last = had ? m_matches[m_current] : Match();
    m_matches.clear();
    m_numbers.clear();
    m_more = false;
    Cyberiada::LocalDocument* root = m_model->rootDocument();
    QRegularExpression expression = m_query.expression();
    int number = 0;
    if (!m_query.isEmpty() && root && expression.isValid()) walk(root, expression, number);

    // the place: the current match in the new walk, an element gone keeping its number
    int element = 0, text = 0, offset = 0;
    if (had) {
        element = m_numbers.value(last.element, last.elementNumber);
        text = last.textNumber;
        offset = last.from;
    } else {
        QStringList selected = m_scene->selectedIds();
        if (!selected.isEmpty()) element = m_numbers.value(selected.first(), 0);
    }
    m_current = m_matches.isEmpty() ? -1 : 0;
    for (int i = 0; i < m_matches.size(); i++) {
        const Match& m = m_matches[i];
        if (m.elementNumber > element ||
            (m.elementNumber == element && (m.textNumber > text || (m.textNumber == text && m.from >= offset)))) {
            m_current = i;
            break;
        }
    }
    mark();
    if (navigate) this->navigate();
    emit resultChanged(m_current, m_matches.size(), m_more);
}

void DiagramFind::scheduleSearch()
{
    if (m_query.isEmpty() || m_pending) return;
    m_pending = true;
    QTimer::singleShot(0, this, [this]() {
        m_pending = false;
        if (!m_query.isEmpty()) search(false);
    });
}

void DiagramFind::mark()
{
    QHash<EditableTextItem*, FindRanges> ranges;
    QHash<EditableTextItem*, int> currents;
    for (int i = 0; i < m_matches.size(); i++) {
        EditableTextItem* text = m_matches[i].text;
        if (!text) continue;
        if (i == m_current) currents.insert(text, ranges[text].size());
        FindRange range = { m_matches[i].from, m_matches[i].to };
        ranges[text].append(range);
    }
    for (const QPointer<EditableTextItem>& text : m_marked) {
        if (text && !ranges.contains(text)) text->setFindRanges(FindRanges(), -1);
    }
    m_marked.clear();
    for (QHash<EditableTextItem*, FindRanges>::const_iterator i = ranges.begin(); i != ranges.end(); i++) {
        i.key()->setFindRanges(i.value(), currents.value(i.key(), -1));
        m_marked.append(i.key());
    }
}

void DiagramFind::navigate()
{
    if (m_current < 0) return;
    const Match& m = m_matches[m_current];
    QGraphicsItem* owner = m_scene->getMap().value(m.element.toStdString(), nullptr);
    if (owner) {
        m_scene->clearSelection();
        owner->setSelected(true);
    }
    // the selection may have rebuilt the item
    if (!m.text) return;
    QRectF box = m.text->mapRectToScene(m.text->findRangeBounds(m.from, m.to));
    for (QGraphicsView* view : m_scene->views()) view->ensureVisible(box);
}

void DiagramFind::move(int step)
{
    int count = m_matches.size();
    if (count > 0) {
        m_current = ((m_current + step) % count + count) % count;
        mark();
        navigate();
    }
    emit resultChanged(m_current, count, m_more);
}

void DiagramFind::clear(bool keep)
{
    m_query = FindQuery();
    m_matches.clear();
    m_current = -1;
    m_more = false;
    mark();
    if (keep && !m_scene->views().isEmpty()) m_scene->views().first()->setFocus();
}
