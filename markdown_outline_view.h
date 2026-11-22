/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The outline tree of a Markdown tab: it navigates, it edits nothing
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

#ifndef CYBERIADA_MARKDOWN_OUTLINE_VIEW
#define CYBERIADA_MARKDOWN_OUTLINE_VIEW

#include <QPointer>
#include <QTreeView>

class MarkdownOutlineModel;

// The Outline dock's tree (EDIT-MD-13/15): no drag and no edit; the rows the user makes current
// are signalled, the rows the text makes current are not
class MarkdownOutlineView: public QTreeView {
Q_OBJECT
public:
    explicit MarkdownOutlineView(QWidget* parent = nullptr);

    // the model of the active Markdown tab, null for another tab; its rows expanded as it keeps them
    void                  setOutlineModel(MarkdownOutlineModel* model);
    MarkdownOutlineModel* outlineModel() const { return m_outline; }
    // the row of the source's place made current and scrolled into view, without rowShown
    void                  follow(const QModelIndex& index);

signals:
    // a row made current by a click or the arrow keys
    void                  rowShown(const QModelIndex& index);
    // Enter or Go to: the place shown and the source focused
    void                  rowEntered(const QModelIndex& index);
    // a double click on a diagram leaf, or Open Diagram
    void                  diagramRequested(const QModelIndex& index);
    // Open Image: the system viewer
    void                  imageRequested(const QModelIndex& index);

protected:
    void                  currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
    void                  keyPressEvent(QKeyEvent* event) override;
    void                  mousePressEvent(QMouseEvent* event) override;
    void                  mouseDoubleClickEvent(QMouseEvent* event) override;
    void                  contextMenuEvent(QContextMenuEvent* event) override;

private:
    // the rows of the parent and below expanded as the model keeps them
    void                  restoreExpanded(const QModelIndex& parent, int first, int last);
    void                  setAllExpanded(bool on);

    QPointer<MarkdownOutlineModel> m_outline;
    QMetaObject::Connection m_inserted;
    bool                  m_following = false;
};

#endif
