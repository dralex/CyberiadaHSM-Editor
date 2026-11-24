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

#include <QFont>
#include <QSet>
#include <algorithm>

#include "repository_model.h"

// the mark of a file whose tab has unsaved edits (EDIT-GIT-4)
static const QChar UNSAVED_MARK(0x25CF);
static const QChar PATH_SEPARATOR('/');
static const QString LINE_BREAK("\n");
// the icon cache keys: a group apart from the files and folders
static const int ICON_KEY_GREYED = 1;
static const int ICON_KEY_MARKER_SHIFT = 1;
static const int ICON_KEY_KIND_SHIFT = 10;
static const int ICON_KEY_GROUP = 1 << 16;

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

// the marker of a group, the legend of the markers of its files (EDIT-GIT-5)
static StatusMarker groupMarker(int group)
{
    StatusMarker m;
    switch (group) {
    case groupConflicts:
        m.places = StatusMarker::placeConflict;
        m.glyph = StatusMarker::glyphConflict;
        break;
    case groupStaged:
        m.places = StatusMarker::placeIndex;
        break;
    case groupChanges:
        m.places = StatusMarker::placeTree;
        break;
    case groupUntracked:
        m.places = StatusMarker::placeTree;
        m.glyph = StatusMarker::glyphUntracked;
        break;
    default:
        m.places = StatusMarker::placeIgnored;
        break;
    }
    return m;
}

// a status letter in words (EDIT-GIT-4)
static QString changeWord(QChar c)
{
    switch (c.unicode()) {
    case 'M': return RepositoryModel::tr("modified");
    case 'T': return RepositoryModel::tr("type changed");
    case 'A': return RepositoryModel::tr("added");
    case 'D': return RepositoryModel::tr("deleted");
    case 'R': return RepositoryModel::tr("renamed");
    case 'C': return RepositoryModel::tr("copied");
    default:  return QString(c);
    }
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
    // the marks are the names and their fonts: the rows stay
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

void RepositoryModel::setUntracked(bool on)
{
    if (on == m_untracked) return;
    m_untracked = on;
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
        if (!m_untracked && e.untracked()) continue;
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
            int places = StatusMarker::of(e).places;
            for (Node* p = parent; p && p != m_root; p = p->parent) p->places |= places;
        }
    }
    sortTree(m_root);
}

void RepositoryModel::sortTree(Node* node)
{
    // the folders first, the changed before the unchanged, then by name (EDIT-GIT-5)
    auto changed = [this](const Node* n) {
        return n->kind == kindFolder ? n->places != 0 : isChange(m_snapshot.entries.at(n->entry));
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
        if (group == groupUntracked && !m_untracked) continue;
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
        if (e && m_open.value(e->path, false)) return node->name + ' ' + UNSAVED_MARK;
        return node->name;
    case Qt::FontRole: {
        // a file open in a tab (EDIT-GIT-4)
        if (!e || !m_open.contains(e->path)) return QVariant();
        QFont bold;
        bold.setBold(true);
        return bold;
    }
    case Qt::ToolTipRole:
        return toolTipOf(node);
    case Qt::DecorationRole:
        return iconOf(node);
    case PathRole:
        return node->kind == kindGroup ? QString() : node->path;
    case KindRole:
        return int(node->kind);
    case MarkerRole:
        return markerOf(node).key();
    default:
        return QVariant();
    }
}

StatusMarker RepositoryModel::markerOf(const Node* node) const
{
    if (node->kind == kindGroup) return groupMarker(node->path.toInt());
    if (node->kind == kindFile) return StatusMarker::of(m_snapshot.entries.at(node->entry));
    StatusMarker m;
    m.places = node->places;
    return m;
}

QIcon RepositoryModel::iconOf(const Node* node) const
{
    StatusMarker marker = markerOf(node);
    if (node->kind == kindGroup) {
        int key = ICON_KEY_GROUP | marker.key();
        if (!m_icons.contains(key)) m_icons.insert(key, markerIcon(marker));
        return m_icons.value(key);
    }
    FileKind kind = node->kind == kindFolder ? FileKind::folder : fileKind(node->path);
    bool greyed = node->kind == kindFile && m_snapshot.entries.at(node->entry).ignored();
    int key = (int(kind) << ICON_KEY_KIND_SHIFT) | (marker.key() << ICON_KEY_MARKER_SHIFT) |
              (greyed ? ICON_KEY_GREYED : 0);
    if (!m_icons.contains(key)) m_icons.insert(key, markedIcon(kind, marker, greyed));
    return m_icons.value(key);
}

QString RepositoryModel::toolTipOf(const Node* node) const
{
    if (node->kind == kindGroup) return QString();
    QStringList lines;
    lines << node->path;
    if (node->kind == kindFolder) {
        if (node->places & StatusMarker::placeConflict) lines << tr("Conflicts inside");
        bool staged = node->places & StatusMarker::placeIndex;
        bool changed = node->places & StatusMarker::placeTree;
        if (staged && changed) lines << tr("Staged and not staged changes inside");
        else if (staged) lines << tr("Staged changes inside");
        else if (changed) lines << tr("Changes not staged inside");
        return lines.join(LINE_BREAK);
    }
    // the status in words (EDIT-GIT-4)
    const StatusEntry& e = m_snapshot.entries.at(node->entry);
    if (!e.oldPath.isEmpty()) lines << tr("Renamed from %1").arg(e.oldPath);
    if (e.conflicted()) {
        lines << tr("Conflicted");
    } else if (e.untracked()) {
        lines << tr("Untracked");
    } else if (e.ignored()) {
        lines << tr("Ignored");
    } else {
        if (e.index != STATUS_NONE) lines << tr("Staged: %1").arg(changeWord(e.index));
        if (e.tree != STATUS_NONE) lines << tr("Not staged: %1").arg(changeWord(e.tree));
    }
    if (m_open.contains(e.path)) lines << (m_open.value(e.path) ? tr("Open, unsaved edits") : tr("Open"));
    return lines.join(LINE_BREAK);
}

QVariant RepositoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return QVariant();
    return section == columnName ? QVariant(tr("Name")) : QVariant();
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
