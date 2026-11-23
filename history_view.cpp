/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The History dock: the commits of a file or of the work tree
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
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

#include "history_view.h"

HistoryModel::HistoryModel(QObject* parent):
    QAbstractTableModel(parent)
{
}

void HistoryModel::setEntries(const QVector<LogEntry>& entries, bool more)
{
    beginResetModel();
    m_entries = entries;
    m_more = more;
    endResetModel();
}

void HistoryModel::appendEntries(const QVector<LogEntry>& entries, bool more)
{
    beginResetModel();
    m_entries += entries;
    m_more = more;
    endResetModel();
}

bool HistoryModel::isMoreRow(const QModelIndex& index) const
{
    return index.isValid() && m_more && index.row() == m_entries.size();
}

int HistoryModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_entries.size() + (m_more ? 1 : 0);
}

int HistoryModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : columnsCount;
}

QVariant HistoryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) return QVariant();
    if (isMoreRow(index)) {
        if (role == Qt::DisplayRole && index.column() == columnHash) return tr("More...");
        return QVariant();
    }
    const LogEntry& e = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case columnHash:    return e.shortHash;
        case columnDate:    return e.date;
        case columnAuthor:  return e.author;
        case columnSubject: return e.subject;
        default:            return QVariant();
        }
    case Qt::ToolTipRole:
        return e.hash + "\n" + e.subject;
    case HashRole:
        return e.hash;
    default:
        return QVariant();
    }
}

QVariant HistoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return QVariant();
    switch (section) {
    case columnHash:    return tr("Commit");
    case columnDate:    return tr("Date");
    case columnAuthor:  return tr("Author");
    case columnSubject: return tr("Subject");
    default:            return QVariant();
    }
}

HistoryView::HistoryView(QWidget* parent):
    QWidget(parent), m_model(new HistoryModel(this))
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_toolBar = new QToolBar(this);
    m_toolBar->setObjectName("historyToolBar");
    QActionGroup* scopes = new QActionGroup(this);
    m_fileAction = m_toolBar->addAction(tr("File"));
    m_fileAction->setObjectName("actionHistoryFile");
    m_fileAction->setToolTip(tr("The commits of the file, its renames followed"));
    m_repositoryAction = m_toolBar->addAction(tr("Repository"));
    m_repositoryAction->setObjectName("actionHistoryRepository");
    m_repositoryAction->setToolTip(tr("The commits of the whole repository"));
    for (QAction* action : {m_fileAction, m_repositoryAction}) {
        action->setCheckable(true);
        scopes->addAction(action);
    }
    m_fileAction->setChecked(true);
    layout->addWidget(m_toolBar);
    m_title = new QLabel(this);
    m_title->setObjectName("historyTitle");
    m_title->setMargin(fontMetrics().averageCharWidth() / 2);
    // a long file name never widens the dock
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_title);
    m_list = new QTreeView(this);
    m_list->setObjectName("historyList");
    m_list->setModel(m_model);
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->header()->setStretchLastSection(true);
    for (int column = HistoryModel::columnHash; column < HistoryModel::columnSubject; column++) {
        m_list->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    layout->addWidget(m_list);

    connect(m_list, &QTreeView::activated, this, &HistoryView::rowActivated);
    connect(m_list, &QWidget::customContextMenuRequested, this, &HistoryView::showMenu);
    connect(m_fileAction, &QAction::triggered, this, [this]() { emit scopeChanged(false); });
    connect(m_repositoryAction, &QAction::triggered, this, [this]() { emit scopeChanged(true); });
}

bool HistoryView::repositoryScope() const
{
    return m_repositoryAction->isChecked();
}

void HistoryView::setRepositoryScope(bool on)
{
    (on ? m_repositoryAction : m_fileAction)->setChecked(true);
}

void HistoryView::setTitle(const QString& title)
{
    m_title->setText(title);
}

QString HistoryView::title() const
{
    return m_title->text();
}

QString HistoryView::selectedHash() const
{
    QModelIndex at = m_list->currentIndex();
    return at.isValid() ? m_model->data(at, HistoryModel::HashRole).toString() : QString();
}

void HistoryView::rowActivated(const QModelIndex& index)
{
    if (m_model->isMoreRow(index)) {
        emit moreRequested();
        return;
    }
    QString hash = m_model->data(index, HistoryModel::HashRole).toString();
    if (!hash.isEmpty()) emit compareRequested(hash);
}

void HistoryView::showMenu(const QPoint& pos)
{
    QModelIndex at = m_list->indexAt(pos);
    QString hash = at.isValid() ? m_model->data(at, HistoryModel::HashRole).toString() : QString();
    if (hash.isEmpty()) return;
    QMenu menu(this);
    QAction* compare = menu.addAction(tr("Compare with This Revision"));
    QAction* copy = menu.addAction(tr("Copy Hash"));
    QAction* chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
    if (chosen == compare) emit compareRequested(hash);
    if (chosen == copy) QApplication::clipboard()->setText(hash);
}
