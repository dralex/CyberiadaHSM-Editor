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

#ifndef CYBERIADA_HISTORY_VIEW
#define CYBERIADA_HISTORY_VIEW

#include <QAbstractTableModel>
#include <QWidget>

#include "repository_status.h"

class QAction;
class QLabel;
class QToolBar;
class QTreeView;

// the commits of one query and the More... row while there may be more (EDIT-GIT-15)
class HistoryModel: public QAbstractTableModel {
Q_OBJECT
public:
    enum Column { columnHash = 0, columnDate, columnAuthor, columnSubject, columnsCount };
    enum Role { HashRole = Qt::UserRole + 1 };

    explicit HistoryModel(QObject* parent = nullptr);

    // the commits shown, more: a More... row after them
    void              setEntries(const QVector<LogEntry>& entries, bool more);
    void              appendEntries(const QVector<LogEntry>& entries, bool more);
    const QVector<LogEntry>& entries() const { return m_entries; }
    bool              hasMore() const { return m_more; }
    bool              isMoreRow(const QModelIndex& index) const;
    // the commit of the current version, bold (EDIT-GIT-15)
    void              setCurrentHash(const QString& hash);
    QString           currentHash() const { return m_current; }

    int               rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int               columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant          data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant          headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    QVector<LogEntry> m_entries;
    bool              m_more = false;
    QString           m_current;
};

// The contents of the History dock (EDIT-GIT-15, docs/GIT.md, The history): the scope, the
// title of the query, the commits; the window reads them and acts on the requests
class HistoryView: public QWidget {
Q_OBJECT
public:
    explicit HistoryView(QWidget* parent = nullptr);

    HistoryModel*     model() const { return m_model; }
    QTreeView*        list() const { return m_list; }
    QAction*          fileAction() const { return m_fileAction; }
    QAction*          repositoryAction() const { return m_repositoryAction; }
    // the commits of the work tree, not of a file
    bool              repositoryScope() const;
    void              setRepositoryScope(bool on);
    // the line above the commits: the file, the work tree or why there are none
    void              setTitle(const QString& title);
    QString           title() const;
    // the hash of the selected commit, empty without one
    QString           selectedHash() const;

signals:
    void              compareRequested(const QString& hash);
    void              moreRequested();
    void              scopeChanged(bool repository);

private:
    void              rowActivated(const QModelIndex& index);
    void              showMenu(const QPoint& pos);

    HistoryModel*     m_model;
    QToolBar*         m_toolBar;
    QLabel*           m_title;
    QTreeView*        m_list;
    QAction*          m_fileAction;
    QAction*          m_repositoryAction;
};

#endif
