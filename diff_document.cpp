/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The diff tab: two revisions of a diagram side by side, their differences marked
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

#include <QCoreApplication>
#include <QLabel>
#include <QSplitter>
#include <QUndoStack>
#include <QVBoxLayout>

#include "diff_document.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_editor_view.h"
#include "cyberiadasm_editor_items.h"

// the geometry metainformation is shown with the geometry only: the editor's view
// state, as hsm-git does, and the stored regions mark the editor writes on a save
static bool geometryMeta(const Cyberiada::String& name)
{
    return name == "cyberiadaEditorView" || name == CYBERIADA_META_REGION_GEOMETRY;
}

// a side: its title over its read-only canvas
static QWidget* sidePane(QLabel* label, CyberiadaSMGraphicsView* view)
{
    QWidget* pane = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(pane);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(label);
    layout->addWidget(view, 1);
    return pane;
}

DiffDocument::DiffDocument(QObject* parent):
    EditorDocument(parent)
{
    m_undo = new QUndoStack(this);
    // both sides are read as stored and never edited (EDIT-MODE-10)
    m_baseModel = new CyberiadaSMModel(this);
    m_targetModel = new CyberiadaSMModel(this);
    m_baseModel->setInspected(true);
    m_targetModel->setInspected(true);
    m_baseScene = new CyberiadaSMEditorScene(m_baseModel, this);
    m_targetScene = new CyberiadaSMEditorScene(m_targetModel, this);
    m_baseView = new CyberiadaSMGraphicsView();
    m_targetView = new CyberiadaSMGraphicsView();
    m_baseView->setScene(m_baseScene);
    m_targetView->setScene(m_targetScene);
    m_baseLabel = new QLabel();
    m_targetLabel = new QLabel();
    m_page = new QSplitter(Qt::Horizontal);
    m_page->addWidget(sidePane(m_baseLabel, m_baseView));
    m_page->addWidget(sidePane(m_targetLabel, m_targetView));
}

// the views go first (they paint the scenes), the scenes before the models
DiffDocument::~DiffDocument()
{
    delete m_page;
    delete m_baseScene;
    delete m_targetScene;
}

QWidget* DiffDocument::page() const
{
    return m_page;
}

QString DiffDocument::title() const
{
    return m_target.label + QString::fromUtf8(" ↔ ") + m_base.label;
}

void DiffDocument::setSides(const std::string& base, const std::string& target)
{
    m_baseBuffer = base;
    m_targetBuffer = target;
    // the reset reloads each scene from its document as it is stored
    m_baseModel->restoreSnapshot(base);
    m_targetModel->restoreSnapshot(target);
    m_baseLabel->setText(tr("Base: %1").arg(m_base.label));
    m_targetLabel->setText(tr("Target: %1").arg(m_target.label));
    compare();
    emit titleChanged();
}

void DiffDocument::setGeometry(bool on)
{
    if (m_geometry == on) return;
    m_geometry = on;
    compare();
}

// the word of an element in a row, as hsm-git names it
static QString elementWord(Cyberiada::ElementType type)
{
    switch (type) {
    case Cyberiada::elementSM:              return QObject::tr("state machine");
    case Cyberiada::elementSimpleState:
    case Cyberiada::elementCompositeState:  return QObject::tr("state");
    case Cyberiada::elementSubmachineState: return QObject::tr("submachine");
    case Cyberiada::elementComment:         return QObject::tr("comment");
    case Cyberiada::elementFormalComment:   return QObject::tr("formal comment");
    case Cyberiada::elementInitial:         return QObject::tr("initial");
    case Cyberiada::elementFinal:           return QObject::tr("final");
    case Cyberiada::elementChoice:          return QObject::tr("choice");
    case Cyberiada::elementTerminate:       return QObject::tr("terminate");
    case Cyberiada::elementShallowHistory:  return QObject::tr("shallow history");
    case Cyberiada::elementDeepHistory:     return QObject::tr("deep history");
    case Cyberiada::elementEntryPoint:      return QObject::tr("entry point");
    case Cyberiada::elementExitPoint:       return QObject::tr("exit point");
    case Cyberiada::elementTransition:      return QObject::tr("transition");
    default:                                return QObject::tr("element");
    }
}

QString DiffDocument::label(const CyberiadaSMModel* model, const QString& id) const
{
    const Cyberiada::Element* element = model->idToElement(id);
    // a region is no element of the model: its id ends with the separator
    if (!element) return id.endsWith(':') ? tr("region %1").arg(id) : id;
    QString name = element->has_name() ? QString::fromStdString(element->get_name()) : QString();
    auto shortName = [model](const Cyberiada::ID& end) {
        const Cyberiada::Element* e = model->idToElement(QString::fromStdString(end));
        if (!e) return QString::fromStdString(end);
        return e->has_name() && !e->get_name().empty() ? QString::fromStdString(e->get_name())
                                                       : elementWord(e->get_type());
    };
    QString text = elementWord(element->get_type());
    if (element->get_type() == Cyberiada::elementTransition) {
        const Cyberiada::Transition* t = static_cast<const Cyberiada::Transition*>(element);
        text += " " + shortName(t->source_element_id()) + " -> " + shortName(t->target_element_id());
        QString trigger = QString::fromStdString(t->get_action().get_trigger());
        if (!trigger.isEmpty()) text += " [" + trigger + "]";
    } else if (!name.isEmpty()) {
        text += " " + name;
    }
    if (m_geometry) text += " (" + id + ")";
    return text;
}

// the words of what differs in a pair; added and removed have their own rows
static QString changeWords(Cyberiada::DiffFlags flags)
{
    static const struct { Cyberiada::DiffFlags flag; const char* word; } words[] = {
        { Cyberiada::diffType,        QT_TRANSLATE_NOOP("DiffDocument", "type") },
        { Cyberiada::diffTitle,       QT_TRANSLATE_NOOP("DiffDocument", "title") },
        { Cyberiada::diffActions,     QT_TRANSLATE_NOOP("DiffDocument", "actions") },
        { Cyberiada::diffParent,      QT_TRANSLATE_NOOP("DiffDocument", "parent") },
        { Cyberiada::diffLink,        QT_TRANSLATE_NOOP("DiffDocument", "link") },
        { Cyberiada::diffComment,     QT_TRANSLATE_NOOP("DiffDocument", "comment") },
        { Cyberiada::diffColor,       QT_TRANSLATE_NOOP("DiffDocument", "color") },
        { Cyberiada::diffGeometry,    QT_TRANSLATE_NOOP("DiffDocument", "geometry") },
        { Cyberiada::diffCollapsed,   QT_TRANSLATE_NOOP("DiffDocument", "collapsed") },
        { Cyberiada::diffSource,      QT_TRANSLATE_NOOP("DiffDocument", "source") },
        { Cyberiada::diffTarget,      QT_TRANSLATE_NOOP("DiffDocument", "target") },
        { Cyberiada::diffTrigger,     QT_TRANSLATE_NOOP("DiffDocument", "trigger") },
        { Cyberiada::diffGuard,       QT_TRANSLATE_NOOP("DiffDocument", "guard") },
        { Cyberiada::diffBehavior,    QT_TRANSLATE_NOOP("DiffDocument", "behaviour") },
        { Cyberiada::diffPropagation, QT_TRANSLATE_NOOP("DiffDocument", "propagation") },
        { Cyberiada::diffSubject,     QT_TRANSLATE_NOOP("DiffDocument", "subject") },
        { Cyberiada::diffElements,    QT_TRANSLATE_NOOP("DiffDocument", "elements") },
    };
    QStringList result;
    for (const auto& w : words) {
        if (flags & w.flag) result << QCoreApplication::translate("DiffDocument", w.word);
    }
    return result.join(", ");
}

void DiffDocument::compare()
{
    m_rows.clear();
    m_added = m_removed = m_changed = 0;
    m_error.clear();
    const Cyberiada::LocalDocument* base = m_baseModel->rootDocument();
    const Cyberiada::LocalDocument* target = m_targetModel->rootDocument();
    Cyberiada::DocumentDiff result;
    Cyberiada::DiffOptions options;
    options.geometry = m_geometry;
    try {
        Cyberiada::LocalDocument empty;
        result = (base ? *base : empty).diff(target ? *target : empty, options);
    } catch (const Cyberiada::Exception& e) {
        m_error = QString::fromStdString(e.str());
    }
    auto count = [this](const QString& change) {
        if (change == "added") m_added++;
        else if (change == "removed") m_removed++;
        else m_changed++;
    };
    // the metainformation, the view state with the geometry only
    for (const Cyberiada::MetaDiff& meta : result.meta) {
        if (!m_geometry && geometryMeta(meta.name)) continue;
        DiffRow row;
        row.element = tr("meta %1").arg(QString::fromStdString(meta.name));
        row.change = !meta.in_first ? "added" : !meta.in_second ? "removed" :
            QString::fromStdString(meta.first) + " -> " + QString::fromStdString(meta.second);
        if (meta.in_first && meta.in_second) {
            m_changed++;
        } else {
            count(row.change);
        }
        m_rows.append(row);
    }
    for (const Cyberiada::StateMachineDiff& sm : result.machines) {
        QString machine = sm.second.empty() ? label(m_baseModel, QString::fromStdString(sm.first))
                                            : label(m_targetModel, QString::fromStdString(sm.second));
        // the machine itself: added, removed, or its own fields changed
        Cyberiada::DiffFlags own = sm.flags & ~Cyberiada::DiffFlags(Cyberiada::diffElements);
        if (sm.first.empty() || sm.second.empty() || own) {
            DiffRow row;
            row.machine = machine;
            row.element = machine;
            row.first = QString::fromStdString(sm.first);
            row.second = QString::fromStdString(sm.second);
            row.change = sm.first.empty() ? "added" : sm.second.empty() ? "removed" : changeWords(own);
            count(row.change);
            m_rows.append(row);
        }
        for (const Cyberiada::ElementDiff& e : sm.elements) {
            if (!e.flags) continue;
            DiffRow row;
            row.machine = machine;
            row.first = QString::fromStdString(e.first);
            row.second = QString::fromStdString(e.second);
            if (e.kind == Cyberiada::diffKindCommentLink) {
                QString comment = e.added() ? label(m_targetModel, QString::fromStdString(e.second_comment))
                                            : label(m_baseModel, QString::fromStdString(e.first_comment));
                row.element = tr("link of %1").arg(comment);
            } else {
                // a pair reads in its present names, a removed element in its former ones
                row.element = e.removed() ? label(m_baseModel, row.first) : label(m_targetModel, row.second);
            }
            row.change = e.added() ? "added" : e.removed() ? "removed" : changeWords(e.flags);
            count(row.change);
            m_rows.append(row);
        }
    }
    mark();
    emit compared();
}

void DiffDocument::mark()
{
    QMap<QString, CyberiadaSMEditorAbstractItem::RunMark> base, target;
    for (const DiffRow& row : m_rows) {
        if (row.machine.isEmpty()) continue;
        if (row.change == "added") {
            target.insert(row.second, CyberiadaSMEditorAbstractItem::runMarkAdded);
        } else if (row.change == "removed") {
            base.insert(row.first, CyberiadaSMEditorAbstractItem::runMarkRemoved);
        } else {
            base.insert(row.first, CyberiadaSMEditorAbstractItem::runMarkChanged);
            target.insert(row.second, CyberiadaSMEditorAbstractItem::runMarkChanged);
        }
    }
    m_baseScene->setDiffMarks(base);
    m_targetScene->setDiffMarks(target);
}

void DiffDocument::showElement(const QString& first, const QString& second)
{
    auto show = [](CyberiadaSMEditorScene* scene, CyberiadaSMGraphicsView* view, const QString& id) {
        scene->clearSelection();
        QGraphicsItem* item = id.isEmpty() ? nullptr : scene->getMap().value(id.toStdString());
        if (!item) return;
        item->setSelected(true);
        view->centerOn(item);
    };
    show(m_baseScene, m_baseView, first);
    show(m_targetScene, m_targetView, second);
}

void DiffDocument::zoomIn()
{
    m_baseView->zoomIn();
    m_targetView->zoomIn();
}

void DiffDocument::zoomOut()
{
    m_baseView->zoomOut();
    m_targetView->zoomOut();
}

void DiffDocument::fit()
{
    for (CyberiadaSMEditorScene* scene : {m_baseScene, m_targetScene}) {
        CyberiadaSMGraphicsView* view = scene == m_baseScene ? m_baseView : m_targetView;
        QRectF bounds = scene->visibleItemsBoundingRect();
        if (bounds.isNull()) continue;
        scene->updateSceneRect();
        view->fitInView(bounds, Qt::KeepAspectRatio);
    }
}
