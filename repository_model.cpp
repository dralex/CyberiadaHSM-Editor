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

#include <QApplication>
#include <QSet>
#include <QStyle>
#include <algorithm>

#include "repository_model.h"

// the mark of a file whose tab has unsaved edits (EDIT-GIT-4)
static const QChar UNSAVED_MARK(0x25CF);
// the mark of a folder holding a change (EDIT-GIT-5)
static const QChar FOLDER_CHANGES('*');
static const QChar PATH_SEPARATOR('/');

// the groups in their order (EDIT-GIT-5)
enum GroupId { groupConflicts = 0, groupStaged, groupChanges, groupUntracked, groupIgnored, groupCount };

static bool inGroup(const StatusEntry& e, int group)
{
    switch (group) {
    case groupConflicts: return e.conflicted();
    case groupStaged:    return e.staged();
    case groupChanges:   return e.modified();
    case groupUntracked: return e.untracked();
    case groupIgnored:   return e.ignored();
    default:             return false;
    }
}

static QString groupTitle(int group, int count)
{
    switch (group) {
    case groupConflicts: return RepositoryModel::tr("Conflicts (%1)").arg(count);
    case groupStaged:    return RepositoryModel::tr("Staged (%1)").arg(count);
    case groupChanges:   return RepositoryModel::tr("Changes (%1)").arg(count);
    case groupUntracked: return RepositoryModel::tr("Untracked (%1)").arg(count);
    default:             return RepositoryModel::tr("Ignored (%1)").arg(count);
    }
}

// a change the dock shows: an ignored file is none
static bool isChange(const StatusEntry& e)
{
    return !e.unchanged() && !e.ignored();
}

static QString letter(QChar c)
{
    return c == STATUS_NONE ? QString() : QString(c);
}

RepositoryModel::RepositoryModel(QObject* parent):
    QAbstractItemModel(parent), m_root(new Node())
{
    m_root->kind = kindFolder;
}

RepositoryModel::~RepositoryModel()
{
    clearNodes();
    delete m_root;
}

void RepositoryModel::clearNodes()
{
    QVector<Node*> stack = m_root->children;
    while (!stack.isEmpty()) {
        Node* node = stack.takeLast();
        stack += node->children;
        delete node;
    }
    m_root->children.clear();
}

void RepositoryModel::setSnapshot(const RepositorySnapshot& snapshot)
{
    m_snapshot = snapshot;
    rebuild();
}

void RepositoryModel::setOpenFiles(const QHash<QString, bool>& open)
{
    if (open == m_open) return;
    m_open = open;
    // the marks are in the names: the rows stay
    if (rowCount() > 0) emit dataChanged(index(0, columnName), index(rowCount() - 1, columnName));
    QModelIndexList stack;
    for (int i = 0; i < rowCount(); i++) stack << index(i, columnName);
    while (!stack.isEmpty()) {
        QModelIndex at = stack.takeLast();
        int rows = rowCount(at);
        if (rows == 0) continue;
        emit dataChanged(index(0, columnName, at), index(rows - 1, columnName, at));
        for (int i = 0; i < rows; i++) stack << index(i, columnName, at);
    }
}

void RepositoryModel::setLayout(Layout layout)
{
    if (layout == m_layout) return;
    m_layout = layout;
    rebuild();
}

void RepositoryModel::setChangedOnly(bool on)
{
    if (on == m_changedOnly) return;
    m_changedOnly = on;
    rebuild();
}

void RepositoryModel::rebuild()
{
    beginResetModel();
    // the old rows go with their nodes
    clearNodes();
    if (m_layout == layoutTree) buildTree();
    else buildGroups();
    endResetModel();
}

RepositoryModel::Node* RepositoryModel::folderNode(Node* root, const QString& path)
{
    Node* at = root;
    QString prefix;
    for (const QString& part : path.split(PATH_SEPARATOR, QString::SkipEmptyParts)) {
        prefix += part + PATH_SEPARATOR;
        Node* found = nullptr;
        for (Node* child : at->children) {
            if (child->kind == kindFolder && child->path == prefix) {
                found = child;
                break;
            }
        }
        if (!found) {
            found = new Node();
            found->kind = kindFolder;
            found->name = part;
            found->path = prefix;
            found->parent = at;
            at->children.append(found);
        }
        at = found;
    }
    return at;
}

void RepositoryModel::buildTree()
{
    for (int i = 0; i < m_snapshot.entries.size(); i++) {
        const StatusEntry& e = m_snapshot.entries.at(i);
        bool change = isChange(e);
        if (m_changedOnly && !change) continue;
        // an ignored folder is one row of its own, its name ending with '/'
        bool folderEntry = e.path.endsWith(PATH_SEPARATOR);
        QString path = folderEntry ? e.path.left(e.path.size() - 1) : e.path;
        int slash = path.lastIndexOf(PATH_SEPARATOR);
        Node* parent = slash < 0 ? m_root : folderNode(m_root, path.left(slash));
        Node* node = new Node();
        node->kind = kindFile;
        node->name = path.mid(slash + 1) + (folderEntry ? QString(PATH_SEPARATOR) : QString());
        node->path = e.path;
        node->entry = i;
        node->parent = parent;
        parent->children.append(node);
        if (change) {
            for (Node* p = parent; p && p != m_root; p = p->parent) p->changes = true;
        }
    }
    sortTree(m_root);
}

void RepositoryModel::sortTree(Node* node)
{
    // the folders first, the changed before the unchanged, then by name (EDIT-GIT-5)
    auto changed = [this](const Node* n) {
        return n->kind == kindFolder ? n->changes : isChange(m_snapshot.entries.at(n->entry));
    };
    std::stable_sort(node->children.begin(), node->children.end(), [&changed](const Node* a, const Node* b) {
        if ((a->kind == kindFolder) != (b->kind == kindFolder)) return a->kind == kindFolder;
        if (changed(a) != changed(b)) return changed(a);
        return a->name < b->name;
    });
    for (Node* child : node->children) {
        if (child->kind == kindFolder) sortTree(child);
    }
}

void RepositoryModel::buildGroups()
{
    for (int group = 0; group < groupCount; group++) {
        QVector<int> members;
        for (int i = 0; i < m_snapshot.entries.size(); i++) {
            if (inGroup(m_snapshot.entries.at(i), group)) members.append(i);
        }
        if (members.isEmpty()) continue;
        Node* node = new Node();
        node->kind = kindGroup;
        node->name = groupTitle(group, members.size());
        node->path = QString::number(group);
        node->parent = m_root;
        m_root->children.append(node);
        for (int i : members) {
            Node* file = new Node();
            file->kind = kindFile;
            file->name = m_snapshot.entries.at(i).path;
            file->path = m_snapshot.entries.at(i).path;
            file->entry = i;
            file->parent = node;
            node->children.append(file);
        }
    }
}

RepositoryModel::Node* RepositoryModel::nodeOf(const QModelIndex& index) const
{
    return index.isValid() ? static_cast<Node*>(index.internalPointer()) : m_root;
}

QModelIndex RepositoryModel::indexOfNode(Node* node) const
{
    if (!node || node == m_root || !node->parent) return QModelIndex();
    return createIndex(node->parent->children.indexOf(node), columnName, node);
}

QModelIndex RepositoryModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column < 0 || column >= columnsCount || row < 0) return QModelIndex();
    Node* node = nodeOf(parent);
    if (row >= node->children.size()) return QModelIndex();
    return createIndex(row, column, node->children.at(row));
}

QModelIndex RepositoryModel::parent(const QModelIndex& child) const
{
    if (!child.isValid()) return QModelIndex();
    return indexOfNode(nodeOf(child)->parent);
}

int RepositoryModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() && parent.column() != columnName) return 0;
    return nodeOf(parent)->children.size();
}

int RepositoryModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return columnsCount;
}

QVariant RepositoryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) return QVariant();
    const Node* node = nodeOf(index);
    const StatusEntry* e = node->kind == kindFile ? &m_snapshot.entries.at(node->entry) : nullptr;
    switch (role) {
    case Qt::DisplayRole:
        if (index.column() == columnName) {
            if (e && m_open.value(e->path, false)) return node->name + ' ' + UNSAVED_MARK;
            return node->name;
        }
        if (!e) return index.column() == columnTree && node->changes ? QString(FOLDER_CHANGES) : QString();
        return letter(index.column() == columnIndex ? e->index : e->tree);
    case Qt::ToolTipRole:
        if (!e) return QVariant();
        if (!e->oldPath.isEmpty()) return tr("%1, renamed from %2").arg(e->path, e->oldPath);
        return e->path;
    case Qt::TextAlignmentRole:
        return index.column() == columnName ? QVariant() : QVariant(Qt::AlignCenter);
    case Qt::DecorationRole:
        if (index.column() != columnName) return QVariant();
        return QApplication::style()->standardIcon(node->kind == kindFile && !node->path.endsWith(PATH_SEPARATOR) ?
                                                   QStyle::SP_FileIcon : QStyle::SP_DirIcon);
    case PathRole:
        return node->kind == kindGroup ? QString() : node->path;
    case KindRole:
        return int(node->kind);
    default:
        return QVariant();
    }
}

QVariant RepositoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return QVariant();
    switch (section) {
    case columnName:  return tr("Name");
    case columnIndex: return tr("Index");
    case columnTree:  return tr("Tree");
    default:          return QVariant();
    }
}

const StatusEntry* RepositoryModel::entry(const QModelIndex& index) const
{
    if (!index.isValid()) return nullptr;
    const Node* node = nodeOf(index);
    return node->kind == kindFile ? &m_snapshot.entries.at(node->entry) : nullptr;
}

void RepositoryModel::collect(const Node* node, QStringList* files)
{
    if (node->kind == kindFile) {
        files->append(node->path);
        return;
    }
    for (const Node* child : node->children) collect(child, files);
}

QStringList RepositoryModel::files(const QModelIndexList& indexes) const
{
    QStringList all;
    for (const QModelIndex& index : indexes) {
        if (index.isValid() && index.column() == columnName) collect(nodeOf(index), &all);
    }
    QStringList files;
    QSet<QString> seen;
    for (const QString& path : all) {
        if (seen.contains(path)) continue;
        seen.insert(path);
        files.append(path);
    }
    return files;
}

QModelIndex RepositoryModel::indexOf(const QString& path) const
{
    QVector<Node*> stack;
    for (int i = m_root->children.size() - 1; i >= 0; i--) stack.append(m_root->children.at(i));
    while (!stack.isEmpty()) {
        Node* node = stack.takeLast();
        if (node->kind == kindFile && node->path == path) return indexOfNode(node);
        for (int i = node->children.size() - 1; i >= 0; i--) stack.append(node->children.at(i));
    }
    return QModelIndex();
}

QString RepositoryModel::folderPath(const QModelIndex& index) const
{
    const Node* node = nodeOf(index);
    return node->kind == kindFile ? QString() : node->path;
}

QModelIndex RepositoryModel::folderIndex(const QString& path) const
{
    QVector<Node*> stack = m_root->children;
    while (!stack.isEmpty()) {
        Node* node = stack.takeLast();
        if (node->kind != kindFile && node->path == path) return indexOfNode(node);
        if (node->kind != kindFile) stack += node->children;
    }
    return QModelIndex();
}
