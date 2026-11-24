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
#include <QEvent>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include "history_view.h"
#include "repository_view.h"

// the shares of the first line: the branch its text first, then the folder
static const int HEADING_STRETCH = 1;
static const int BRANCH_STRETCH = 100;

RepositoryView::RepositoryView(QWidget* parent):
    QWidget(parent), m_model(new RepositoryModel(this))
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // the first line: the work tree and its branch; it never widens the dock, the branch takes
    // its text first, the folder the rest (EDIT-GIT-3)
    m_headingRow = new QWidget(this);
    QHBoxLayout* row = new QHBoxLayout(m_headingRow);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    m_heading = new QLabel(m_headingRow);
    m_heading->setObjectName("repositoryHeading");
    m_heading->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_heading->setMargin(m_heading->fontMetrics().averageCharWidth() / 2);
    m_heading->installEventFilter(this);
    row->addWidget(m_heading, HEADING_STRETCH);
    m_branch = new QToolButton(m_headingRow);
    m_branch->setObjectName("repositoryBranch");
    m_branch->setPopupMode(QToolButton::InstantPopup);
    m_branch->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_branch->setAutoRaise(true);
    m_branch->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_branch->hide();
    row->addWidget(m_branch, BRANCH_STRETCH);
    m_headingRow->hide();
    layout->addWidget(m_headingRow);

    // the tabs under the first line, which serves both; Files starts with the tools, icons in
    // one row (EDIT-GIT-3/15)
    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName("repositoryTabs");
    m_tabs->setDocumentMode(true);
    QWidget* files = new QWidget(m_tabs);
    files->setObjectName("repositoryFiles");
    QVBoxLayout* filesLayout = new QVBoxLayout(files);
    filesLayout->setContentsMargins(0, 0, 0, 0);
    filesLayout->setSpacing(0);
    m_toolBar = new QToolBar(files);
    m_toolBar->setObjectName("repositoryToolBar");
    m_toolBar->setMovable(false);
    m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    int icon = style()->pixelMetric(QStyle::PM_SmallIconSize);
    m_toolBar->setIconSize(QSize(icon, icon));
    QActionGroup* layouts = new QActionGroup(this);
    m_treeAction = m_toolBar->addAction(QIcon(":/Icons/images/repository-tree.png"), tr("Tree"));
    m_treeAction->setObjectName("actionRepositoryTree");
    m_treeAction->setToolTip(tr("The folders and the files of the work tree"));
    m_groupsAction = m_toolBar->addAction(QIcon(":/Icons/images/repository-groups.png"), tr("Groups"));
    m_groupsAction->setObjectName("actionRepositoryGroups");
    m_groupsAction->setToolTip(tr("The changed files by their status"));
    for (QAction* action : {m_treeAction, m_groupsAction}) {
        action->setCheckable(true);
        layouts->addAction(action);
    }
    m_treeAction->setChecked(true);
    m_toolBar->addSeparator();
    m_changedOnlyAction = m_toolBar->addAction(QIcon(":/Icons/images/repository-changed.png"),
                                               tr("Changed only"));
    m_changedOnlyAction->setObjectName("actionRepositoryChangedOnly");
    m_changedOnlyAction->setCheckable(true);
    m_changedOnlyAction->setToolTip(tr("Only the changed files and the folders holding them"));
    m_untrackedAction = m_toolBar->addAction(QIcon(":/Icons/images/repository-untracked.png"),
                                             tr("Untracked"));
    m_untrackedAction->setObjectName("actionRepositoryUntracked");
    m_untrackedAction->setCheckable(true);
    m_untrackedAction->setChecked(true);
    m_untrackedAction->setToolTip(tr("The files git does not track yet"));
    m_ignoredAction = m_toolBar->addAction(QIcon(":/Icons/images/repository-ignored.png"), tr("Ignored"));
    m_ignoredAction->setObjectName("actionRepositoryIgnored");
    m_ignoredAction->setCheckable(true);
    m_ignoredAction->setToolTip(tr("The files git ignores too"));
    m_toolBar->addSeparator();
    m_refreshAction = m_toolBar->addAction(QIcon(":/Icons/images/refresh.png"), tr("Refresh"));
    m_refreshAction->setObjectName("actionRepositoryRefresh");
    m_refreshAction->setToolTip(tr("Read the status of the work tree again"));
    filesLayout->addWidget(m_toolBar);

    m_pages = new QStackedWidget(files);
    m_tree = new QTreeView(m_pages);
    m_tree->setObjectName("repositoryTree");
    m_tree->setModel(m_model);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // a folder opens and closes by the activation, the double click and Return alike
    m_tree->setExpandsOnDoubleClick(false);
    // one column: the status is the marker on the icon (EDIT-GIT-4)
    m_tree->setHeaderHidden(true);
    m_pages->addWidget(m_tree);
    m_line = new QLabel(m_pages);
    m_line->setObjectName("repositoryLine");
    m_line->setWordWrap(true);
    m_line->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_line->setMargin(m_tree->fontMetrics().averageCharWidth());
    m_pages->addWidget(m_line);
    m_pages->setCurrentWidget(m_line);
    filesLayout->addWidget(m_pages);
    m_tabs->addTab(files, tr("Files"));
    m_history = new HistoryView(m_tabs);
    m_history->setObjectName("historyView");
    m_tabs->addTab(m_history, tr("History"));
    layout->addWidget(m_tabs);
    connect(m_tabs, &QTabWidget::currentChanged, this,
            [this](int index) { emit historyShownChanged(m_tabs->widget(index) == m_history); });

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
    connect(m_untrackedAction, &QAction::triggered, this, [this](bool on) {
        setUntracked(on);
        emit untrackedChanged(on);
    });
    connect(m_ignoredAction, &QAction::triggered, this, &RepositoryView::ignoredChanged);
    connect(m_refreshAction, &QAction::triggered, this, &RepositoryView::refreshRequested);
}

void RepositoryView::setHeading(const QString& text, const QString& toolTip)
{
    m_headingText = text;
    m_heading->setToolTip(toolTip);
    m_headingRow->setVisible(!text.isEmpty());
    elideHeading();
}

void RepositoryView::elideHeading()
{
    int room = m_heading->contentsRect().width() - 2 * m_heading->margin();
    m_heading->setText(m_heading->fontMetrics().elidedText(m_headingText, Qt::ElideRight, qMax(0, room)));
}

bool RepositoryView::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_heading && event->type() == QEvent::Resize) elideHeading();
    return QWidget::eventFilter(watched, event);
}

void RepositoryView::setBranch(const QString& text)
{
    m_branch->setText(text);
    // the branch takes no more than its text: the rest goes to the folder
    m_branch->setMaximumWidth(text.isEmpty() ? 0 : m_branch->sizeHint().width());
    m_branch->setVisible(!text.isEmpty());
}

bool RepositoryView::historyShown() const
{
    return m_tabs->currentWidget() == m_history;
}

void RepositoryView::showHistory()
{
    m_tabs->setCurrentWidget(m_history);
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

QStringList RepositoryView::selectedFiles() const
{
    return m_model->files(m_tree->selectionModel()->selectedRows(RepositoryModel::columnName));
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

void RepositoryView::setUntracked(bool on)
{
    m_untrackedAction->setChecked(on);
    if (on == m_model->untracked()) return;
    saveViewState();
    m_model->setUntracked(on);
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
             m_model->data(at, RepositoryModel::MarkerRole).toInt() != 0);
        m_tree->setExpanded(at, open);
        for (int i = 0; i < m_model->rowCount(at); i++) stack << m_model->index(i, RepositoryModel::columnName, at);
    }
    if (state.known) m_tree->verticalScrollBar()->setValue(state.scroll);
}
