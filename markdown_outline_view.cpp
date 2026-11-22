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

#include "markdown_outline_view.h"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>

#include "markdown_outline_model.h"

MarkdownOutlineView::MarkdownOutlineView(QWidget* parent):
    QTreeView(parent)
{
    setHeaderHidden(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setDragEnabled(false);
    setAcceptDrops(false);
    setDragDropMode(QAbstractItemView::NoDragDrop);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setUniformRowHeights(true);
    connect(this, &QTreeView::expanded, this, [this](const QModelIndex& index) {
        if (m_outline) m_outline->setExpanded(index, true);
    });
    connect(this, &QTreeView::collapsed, this, [this](const QModelIndex& index) {
        if (m_outline) m_outline->setExpanded(index, false);
    });
}

void MarkdownOutlineView::setOutlineModel(MarkdownOutlineModel* model)
{
    if (m_outline == model && QTreeView::model() == model) return;
    disconnect(m_inserted);
    m_following = true;
    m_outline = model;
    setModel(model);
    m_following = false;
    if (!model) return;
    // the rows a reading adds open as they were kept, a new one expanded
    m_inserted = connect(model, &QAbstractItemModel::rowsInserted, this,
                         [this](const QModelIndex& parent, int first, int last) { restoreExpanded(parent, first, last); });
    restoreExpanded(QModelIndex(), 0, model->rowCount() - 1);
}

void MarkdownOutlineView::restoreExpanded(const QModelIndex& parent, int first, int last)
{
    if (!m_outline) return;
    for (int row = first; row <= last; row++) {
        QModelIndex index = m_outline->index(row, 0, parent);
        if (!index.isValid()) continue;
        setExpanded(index, m_outline->isExpanded(index));
        int count = m_outline->rowCount(index);
        if (count > 0) restoreExpanded(index, 0, count - 1);
    }
}

void MarkdownOutlineView::setAllExpanded(bool on)
{
    if (!m_outline) return;
    // the view's own expand all signals no row
    if (on) expandAll();
    else collapseAll();
    m_outline->setAllExpanded(on);
}

void MarkdownOutlineView::follow(const QModelIndex& index)
{
    if (!index.isValid() || index.model() != model()) return;
    m_following = true;
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    scrollTo(index);
    m_following = false;
}

void MarkdownOutlineView::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    QTreeView::currentChanged(current, previous);
    // the rows a reading removes or the text's place move the current row by themselves
    if (m_following || !current.isValid() || !m_outline || m_outline->isReading()) return;
    emit rowShown(current);
}

void MarkdownOutlineView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (currentIndex().isValid()) emit rowEntered(currentIndex());
        event->accept();
        return;
    }
    QTreeView::keyPressEvent(event);
}

void MarkdownOutlineView::mousePressEvent(QMouseEvent* event)
{
    QModelIndex before = currentIndex();
    QTreeView::mousePressEvent(event);
    // a click on the current row shows its place again; the branch sign only opens it
    QModelIndex at = indexAt(event->pos());
    if (event->button() == Qt::LeftButton && at.isValid() && at == before && visualRect(at).contains(event->pos())) {
        emit rowShown(at);
    }
}

void MarkdownOutlineView::mouseDoubleClickEvent(QMouseEvent* event)
{
    QModelIndex at = indexAt(event->pos());
    const OutlineEntry* entry = m_outline ? m_outline->entry(at) : nullptr;
    if (entry && entry->kind == OutlineEntry::outlineDiagram && visualRect(at).contains(event->pos())) {
        emit diagramRequested(at);
        event->accept();
        return;
    }
    QTreeView::mouseDoubleClickEvent(event);
}

void MarkdownOutlineView::contextMenuEvent(QContextMenuEvent* event)
{
    QModelIndex at = indexAt(event->pos());
    const OutlineEntry* entry = m_outline ? m_outline->entry(at) : nullptr;
    QMenu menu(this);
    QAction* go = menu.addAction(tr("Go to"));
    go->setShortcut(QKeySequence(Qt::Key_Return));
    go->setEnabled(entry != nullptr);
    QAction* diagram = menu.addAction(tr("Open Diagram"));
    diagram->setEnabled(entry && entry->kind == OutlineEntry::outlineDiagram);
    QAction* image = menu.addAction(tr("Open Image"));
    image->setEnabled(entry && entry->isImage());
    menu.addSeparator();
    QAction* expand = menu.addAction(tr("Expand All"));
    QAction* collapse = menu.addAction(tr("Collapse All"));
    QAction* chosen = menu.exec(event->globalPos());
    if (!chosen) return;
    if (chosen == go) emit rowEntered(at);
    else if (chosen == diagram) emit diagramRequested(at);
    else if (chosen == image) emit imageRequested(at);
    else if (chosen == expand) setAllExpanded(true);
    else if (chosen == collapse) setAllExpanded(false);
}
