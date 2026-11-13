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

#ifndef CYBERIADA_DIAGRAM_FIND
#define CYBERIADA_DIAGRAM_FIND

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QVector>

#include "find_query.h"

namespace Cyberiada { class Element; }
class CyberiadaSMModel;
class CyberiadaSMEditorScene;
class EditableTextItem;

// The find of a diagram tab (EDIT-FIND-6..8, docs/FIND.md): the texts the canvas
// draws in the --dump-text order, the marks on their items, the current match
// selected and scrolled to; the matches are data, the items are rebuilt often
class DiagramFind: public QObject {
Q_OBJECT
public:
    struct Match {
        QPointer<EditableTextItem> text;
        QString element;
        // the place: the element in the walk of the tree, the text in textItemsOf
        int     elementNumber;
        int     textNumber;
        int     from;
        int     to;
    };

    DiagramFind(CyberiadaSMModel* model, CyberiadaSMEditorScene* scene, QObject* parent = nullptr);

    void setQuery(const FindQuery& query, bool navigate);
    void move(int step);
    // keep: the bar closed, the view takes the focus
    void clear(bool keep);
    const QVector<Match>& matches() const { return m_matches; }
    int  current() const { return m_current; }

signals:
    void resultChanged(int current, int total, bool more);

private slots:
    // a drawn text changed: one search after the change, not navigated
    void scheduleSearch();

private:
    // the first match at or after the current one, else the first selected element
    void search(bool navigate);
    void walk(Cyberiada::Element* element, const QRegularExpression& expression, int& number);
    void mark();
    void navigate();

    CyberiadaSMModel*        m_model;
    CyberiadaSMEditorScene*  m_scene;
    FindQuery                m_query;
    QVector<Match>           m_matches;
    int                      m_current = -1;
    bool                     m_more = false;
    bool                     m_pending = false;
    QList<QPointer<EditableTextItem>> m_marked;
    // the element ids by their number in the last walk
    QHash<QString, int>      m_numbers;
};

#endif
