/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The files of a git work tree as a tree of folders or as status groups
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

#ifndef CYBERIADA_REPOSITORY_MODEL
#define CYBERIADA_REPOSITORY_MODEL

#include <QAbstractItemModel>
#include <QHash>
#include <QIcon>
#include <QStringList>

#include "repository_icons.h"
#include "repository_status.h"

// The rows of the Repository dock (EDIT-GIT-4/5, docs/GIT.md, The model): the snapshot of a
// work tree laid out as a tree of folders or as status groups, one column; the status is a
// marker on the icon, the open files are bold
class RepositoryModel: public QAbstractItemModel {
Q_OBJECT
public:
    enum Layout { layoutTree = 0, layoutGroups };
    enum Column { columnName = 0, columnsCount };
    enum Kind { kindFile = 0, kindFolder, kindGroup };
    enum Role {
        PathRole = Qt::UserRole + 1,   // a file: its path relative to the root
        KindRole,
        MarkerRole                     // StatusMarker::key() of the row, 0 without a marker
    };

    explicit RepositoryModel(QObject* parent = nullptr);
    ~RepositoryModel() override;

    void                  setSnapshot(const RepositorySnapshot& snapshot);
    const RepositorySnapshot& snapshot() const { return m_snapshot; }
    // the open files of the work tree, bold: the path and whether its tab has unsaved edits
    void                  setOpenFiles(const QHash<QString, bool>& open);
    void                  setLayout(Layout layout);
    Layout                layout() const { return m_layout; }
    // only the changed files and the folders holding them (the tree)
    void                  setChangedOnly(bool on);
    bool                  changedOnly() const { return m_changedOnly; }
    // the untracked files shown: off leaves them out of the rows and the folder markers
    void                  setUntracked(bool on);
    bool                  untracked() const { return m_untracked; }

    // the files of the rows: a folder or a group stands for its files, each file once
    QStringList           files(const QModelIndexList& indexes) const;
    // the entry of a file row, nullptr for a folder or a group
    const StatusEntry*    entry(const QModelIndex& index) const;
    // the row of a file (the first one in the groups), invalid without one
    QModelIndex           indexOf(const QString& path) const;
    // the path of a folder row, for its expansion kept over a rebuild
    QString               folderPath(const QModelIndex& index) const;
    QModelIndex           folderIndex(const QString& path) const;

    QModelIndex           index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex           parent(const QModelIndex& child) const override;
    int                   rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int                   columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant              data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant              headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    struct Node {
        Kind           kind = kindFile;
        QString        name;            // shown: a file or folder name, a path in a group
        QString        path;            // a file: its path; a folder: its path with '/'; a group: its id
        int            entry = -1;      // the entry of a file in the snapshot
        int            places = 0;      // a folder: the places of the changes under it
        Node*          parent = nullptr;
        QVector<Node*> children;
    };

    void                  rebuild();
    void                  clearNodes();
    void                  buildTree();
    void                  buildGroups();
    Node*                 folderNode(Node* root, const QString& path);
    void                  sortTree(Node* node);
    QModelIndex           indexOfNode(Node* node) const;
    Node*                 nodeOf(const QModelIndex& index) const;
    static void           collect(const Node* node, QStringList* files);
    StatusMarker          markerOf(const Node* node) const;
    QIcon                 iconOf(const Node* node) const;
    QString               toolTipOf(const Node* node) const;

    RepositorySnapshot    m_snapshot;
    QHash<QString, bool>  m_open;
    Layout                m_layout = layoutTree;
    bool                  m_changedOnly = false;
    bool                  m_untracked = true;
    Node*                 m_root;
    // the icons built, by the kind, the marker and the greying
    mutable QHash<int, QIcon> m_icons;
};

#endif
