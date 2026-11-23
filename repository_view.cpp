/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Repository dock: the files of the work tree of the active tab
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

#include <QActionGroup>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyle>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

#include "repository_view.h"

RepositoryView::RepositoryView(QWidget* parent):
    QWidget(parent), m_model(new RepositoryModel(this))
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_toolBar = new QToolBar(this);
    m_toolBar->setObjectName("repositoryToolBar");
    QActionGroup* layouts = new QActionGroup(this);
    m_treeAction = m_toolBar->addAction(tr("Tree"));
    m_treeAction->setObjectName("actionRepositoryTree");
    m_treeAction->setToolTip(tr("The folders and the files of the work tree"));
    m_groupsAction = m_toolBar->addAction(tr("Groups"));
    m_groupsAction->setObjectName("actionRepositoryGroups");
    m_groupsAction->setToolTip(tr("The changed files by their status"));
    for (QAction* action : {m_treeAction, m_groupsAction}) {
        action->setCheckable(true);
        layouts->addAction(action);
    }
    m_treeAction->setChecked(true);
    m_toolBar->addSeparator();
    m_changedOnlyAction = m_toolBar->addAction(tr("Changed only"));
    m_changedOnlyAction->setObjectName("actionRepositoryChangedOnly");
    m_changedOnlyAction->setCheckable(true);
    m_changedOnlyAction->setToolTip(tr("Only the changed files and the folders holding them"));
    m_ignoredAction = m_toolBar->addAction(tr("Ignored"));
    m_ignoredAction->setObjectName("actionRepositoryIgnored");
    m_ignoredAction->setCheckable(true);
    m_ignoredAction->setToolTip(tr("The files git ignores too"));
    m_toolBar->addSeparator();
    m_refreshAction = m_toolBar->addAction(style()->standardIcon(QStyle::SP_BrowserReload), tr("Refresh"));
    m_refreshAction->setObjectName("actionRepositoryRefresh");
    m_refreshAction->setToolTip(tr("Read the status of the work tree again"));
    layout->addWidget(m_toolBar);

    m_pages = new QStackedWidget(this);
    m_tree = new QTreeView(m_pages);
    m_tree->setObjectName("repositoryTree");
    m_tree->setModel(m_model);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // a folder opens and closes by the activation, the double click and Return alike
    m_tree->setExpandsOnDoubleClick(false);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(RepositoryModel::columnName, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(RepositoryModel::columnIndex, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(RepositoryModel::columnTree, QHeaderView::ResizeToContents);
    m_pages->addWidget(m_tree);
    m_line = new QLabel(m_pages);
    m_line->setObjectName("repositoryLine");
    m_line->setWordWrap(true);
    m_line->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_line->setMargin(m_tree->fontMetrics().averageCharWidth());
    m_pages->addWidget(m_line);
    m_pages->setCurrentWidget(m_line);
    layout->addWidget(m_pages);

    connect(m_tree, &QTreeView::activated, this, &RepositoryView::rowActivated);
    connect(m_treeAction, &QAction::triggered, this, [this]() {
        setLayoutMode(RepositoryModel::layoutTree);
        emit layoutModeChanged(RepositoryModel::layoutTree);
    });
    connect(m_groupsAction, &QAction::triggered, this, [this]() {
        setLayoutMode(RepositoryModel::layoutGroups);
        emit layoutModeChanged(RepositoryModel::layoutGroups);
    });
    connect(m_changedOnlyAction, &QAction::triggered, this, [this](bool on) {
        setChangedOnly(on);
        emit changedOnlyChanged(on);
    });
    connect(m_ignoredAction, &QAction::triggered, this, &RepositoryView::ignoredChanged);
    connect(m_refreshAction, &QAction::triggered, this, &RepositoryView::refreshRequested);
}

void RepositoryView::setRepository(const QString& root, const QString& line)
{
    if (root != m_root) {
        saveViewState();
        m_root = root;
        m_currentFile.clear();
    }
    if (root.isEmpty()) {
        m_line->setText(line);
        m_pages->setCurrentWidget(m_line);
        m_model->setSnapshot(RepositorySnapshot());
        return;
    }
    m_pages->setCurrentWidget(m_tree);
}

bool RepositoryView::showsLine() const
{
    return m_pages->currentWidget() == m_line;
}

QString RepositoryView::lineText() const
{
    return showsLine() ? m_line->text() : QString();
}

void RepositoryView::setSnapshot(const RepositorySnapshot& snapshot)
{
    if (snapshot.root != m_root) return;
    QStringList selected = m_model->files(m_tree->selectionModel()->selectedRows(RepositoryModel::columnName));
    saveViewState();
    m_model->setSnapshot(snapshot);
    restoreViewState();
    // the rows the user chose stay chosen; else the file of the active tab
    QItemSelectionModel* selection = m_tree->selectionModel();
    for (const QString& path : selected) {
        QModelIndex at = m_model->indexOf(path);
        if (at.isValid()) selection->select(at, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
    if (selected.isEmpty()) setCurrentFile(m_currentFile);
}

void RepositoryView::setCurrentFile(const QString& path)
{
    m_currentFile = path;
    QModelIndex at = m_model->indexOf(path);
    if (!at.isValid()) return;
    m_tree->selectionModel()->setCurrentIndex(at, QItemSelectionModel::ClearAndSelect |
                                                  QItemSelectionModel::Rows);
    m_tree->scrollTo(at);
}

void RepositoryView::setLayoutMode(RepositoryModel::Layout layout)
{
    (layout == RepositoryModel::layoutTree ? m_treeAction : m_groupsAction)->setChecked(true);
    if (layout == m_model->layout()) return;
    saveViewState();
    m_model->setLayout(layout);
    restoreViewState();
    setCurrentFile(m_currentFile);
}

void RepositoryView::setChangedOnly(bool on)
{
    m_changedOnlyAction->setChecked(on);
    if (on == m_model->changedOnly()) return;
    saveViewState();
    m_model->setChangedOnly(on);
    restoreViewState();
    setCurrentFile(m_currentFile);
}

void RepositoryView::setIgnored(bool on)
{
    m_ignoredAction->setChecked(on);
}

bool RepositoryView::ignored() const
{
    return m_ignoredAction->isChecked();
}

void RepositoryView::rowActivated(const QModelIndex& index)
{
    QModelIndex row = index.sibling(index.row(), RepositoryModel::columnName);
    if (m_model->entry(row)) {
        emit openRequested(m_model->data(row, RepositoryModel::PathRole).toString());
        return;
    }
    m_tree->setExpanded(row, !m_tree->isExpanded(row));
}

void RepositoryView::saveViewState()
{
    if (m_root.isEmpty() || m_model->snapshot().root != m_root) return;
    ViewState state = m_states.value(m_root);
    // the folders of the other layout keep their state
    QModelIndexList stack;
    for (int i = 0; i < m_model->rowCount(); i++) stack << m_model->index(i, RepositoryModel::columnName);
    while (!stack.isEmpty()) {
        QModelIndex at = stack.takeLast();
        if (m_model->rowCount(at) == 0) continue;
        QString path = m_model->folderPath(at);
        state.seen.insert(path);
        if (m_tree->isExpanded(at)) state.expanded.insert(path);
        else state.expanded.remove(path);
        for (int i = 0; i < m_model->rowCount(at); i++) stack << m_model->index(i, RepositoryModel::columnName, at);
    }
    state.scroll = m_tree->verticalScrollBar()->value();
    state.known = true;
    m_states.insert(m_root, state);
}

void RepositoryView::restoreViewState()
{
    ViewState state = m_states.value(m_root);
    QModelIndexList stack;
    for (int i = 0; i < m_model->rowCount(); i++) stack << m_model->index(i, RepositoryModel::columnName);
    while (!stack.isEmpty()) {
        QModelIndex at = stack.takeLast();
        if (m_model->rowCount(at) == 0) continue;
        QString path = m_model->folderPath(at);
        // a row shown first: the groups and the folders holding a change open
        bool open = state.seen.contains(path) ? state.expanded.contains(path) :
            (m_model->data(at, RepositoryModel::KindRole).toInt() == RepositoryModel::kindGroup ||
             !m_model->data(at.sibling(at.row(), RepositoryModel::columnTree)).toString().isEmpty());
        m_tree->setExpanded(at, open);
        for (int i = 0; i < m_model->rowCount(at); i++) stack << m_model->index(i, RepositoryModel::columnName, at);
    }
    if (state.known) m_tree->verticalScrollBar()->setValue(state.scroll);
}
