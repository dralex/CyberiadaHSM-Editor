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

#ifndef CYBERIADA_REPOSITORY_VIEW
#define CYBERIADA_REPOSITORY_VIEW

#include <QHash>
#include <QSet>
#include <QWidget>

#include "repository_model.h"

class QAction;
class QLabel;
class QStackedWidget;
class QToolBar;
class QTreeView;

// The contents of the Repository dock (EDIT-GIT-3..8, docs/GIT.md): the toolbar, the
// files of the work tree, or the line saying why there are none; the window gives it the
// work tree of the active tab and acts on its requests
class RepositoryView: public QWidget {
Q_OBJECT
public:
    explicit RepositoryView(QWidget* parent = nullptr);

    RepositoryModel*  model() const { return m_model; }
    QTreeView*        tree() const { return m_tree; }
    QToolBar*         toolBar() const { return m_toolBar; }
    QAction*          treeAction() const { return m_treeAction; }
    QAction*          groupsAction() const { return m_groupsAction; }
    QAction*          changedOnlyAction() const { return m_changedOnlyAction; }
    QAction*          ignoredAction() const { return m_ignoredAction; }
    QAction*          refreshAction() const { return m_refreshAction; }

    // the work tree shown, its folders and its scroll as they were last shown; an empty
    // root shows the line instead
    void              setRepository(const QString& root, const QString& line);
    QString           root() const { return m_root; }
    bool              showsLine() const;
    QString           lineText() const;
    // a new status of the shown work tree: the expanded folders and the current row stay
    void              setSnapshot(const RepositorySnapshot& snapshot);
    // the first line: the folder of the work tree and its branch, empty without one
    void              setHeading(const QString& text);
    QString           heading() const;
    // the row of the file of the active tab made current and shown
    void              setCurrentFile(const QString& path);
    // the files of the selected rows, a folder or a group standing for its files
    QStringList       selectedFiles() const;

    // the state of the toolbar (EDIT-GIT-5); the signals tell the user's changes only
    void              setLayoutMode(RepositoryModel::Layout layout);
    void              setChangedOnly(bool on);
    void              setIgnored(bool on);
    bool              ignored() const;

signals:
    // a file row activated: its path relative to the root (EDIT-GIT-7)
    void              openRequested(const QString& path);
    void              layoutModeChanged(RepositoryModel::Layout layout);
    void              changedOnlyChanged(bool on);
    void              ignoredChanged(bool on);
    void              refreshRequested();

private:
    struct ViewState {
        QSet<QString> expanded;   // the folder and group paths open
        QSet<QString> seen;       // the paths whose state is known
        int           scroll = 0;
        bool          known = false;
    };
    void              saveViewState();
    void              restoreViewState();
    void              rowActivated(const QModelIndex& index);

    RepositoryModel*  m_model;
    QToolBar*         m_toolBar;
    QLabel*           m_heading;
    QStackedWidget*   m_pages;
    QTreeView*        m_tree;
    QLabel*           m_line;
    QAction*          m_treeAction;
    QAction*          m_groupsAction;
    QAction*          m_changedOnlyAction;
    QAction*          m_ignoredAction;
    QAction*          m_refreshAction;
    QString           m_root;
    QString           m_currentFile;
    QHash<QString, ViewState> m_states;
};

#endif
