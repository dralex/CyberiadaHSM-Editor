/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The outline model of a Markdown tab: read from its text, read-only
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

#include "markdown_outline_model.h"

#include <QApplication>
#include <QDir>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QVector>

#include "cyberiada_constants.h"
#include "text_diff.h"

struct MarkdownOutlineModel::Node {
    OutlineEntry entry;
    Node*        parent = nullptr;
    // a child moved into the kept tree leaves a null behind in a read one
    QList<Node*> children;
    bool         expanded = true;
    ~Node() { qDeleteAll(children); }
    int row() const { return parent ? parent->children.indexOf(const_cast<Node*>(this)) : 0; }
};

MarkdownOutlineModel::MarkdownOutlineModel(QObject* parent):
    QAbstractItemModel(parent)
{
    m_document = new Node;
    m_document->entry.line = 1;
    m_document->entry.column = 1;
}

MarkdownOutlineModel::~MarkdownOutlineModel()
{
    delete m_document;
}

void MarkdownOutlineModel::refresh(const QString& documentName, const QString& text, const QString& baseDir)
{
    m_reading = true;
    m_added.clear();
    m_baseDir = baseDir;
    Node* read = new Node;
    read->entry = m_document->entry;
    read->entry.title = documentName;
    // the read rows as a tree: a heading under the nearest earlier heading of a lower level, an
    // image under the heading before it
    QList<Node*> headings;
    headings << read;
    for (const OutlineEntry& entry : markdownOutline(text, baseDir, &m_diagrams)) {
        if (entry.kind == OutlineEntry::outlineHeading) {
            while (headings.size() > 1 && headings.last()->entry.level >= entry.level) headings.removeLast();
        }
        Node* node = new Node;
        node->entry = entry;
        node->parent = headings.last();
        headings.last()->children << node;
        if (entry.kind == OutlineEntry::outlineHeading) headings << node;
    }
    reconcile(m_document, read);
    delete read;
    m_flat.clear();
    flatten(m_document);
    m_reading = false;
}

void MarkdownOutlineModel::reconcile(Node* kept, Node* read)
{
    if (kept->entry != read->entry) {
        kept->entry = read->entry;
        QModelIndex at = indexOf(kept);
        emit dataChanged(at, at);
    }
    QStringList keptKeys, readKeys;
    for (Node* child : kept->children) keptKeys << child->entry.key();
    for (Node* child : read->children) readKeys << child->entry.key();
    // a pair of the comparison is kept when the names are similar, not the keys alone
    TextDiff::Result diff = TextDiff::diffLines(keptKeys, readKeys);
    QVector<int> match(read->children.size(), -1);
    QVector<bool> matched(kept->children.size(), false);
    for (const TextDiff::Row& row : diff.rows) {
        bool same = row.kind == TextDiff::rowEqual;
        if (row.kind == TextDiff::rowChanged) {
            const OutlineEntry& a = kept->children.at(row.base)->entry;
            const OutlineEntry& b = read->children.at(row.target)->entry;
            same = a.isImage() == b.isImage() &&
                TextDiff::similarity(a.name(), b.name()) >= MARKDOWN_OUTLINE_SIMILARITY;
        }
        if (same) {
            match[row.target] = row.base;
            matched[row.base] = true;
        }
    }
    QModelIndex parent = indexOf(kept);
    for (int i = kept->children.size() - 1; i >= 0; i--) {
        if (matched[i]) continue;
        beginRemoveRows(parent, i, i);
        delete kept->children.takeAt(i);
        endRemoveRows();
    }
    // the matches keep the order: the kept row of a read row j stands at j once the rows before
    // it are added
    QList<QPair<Node*, Node*>> pairs;
    for (int j = 0; j < read->children.size(); j++) {
        Node* node = read->children.at(j);
        if (match[j] >= 0) {
            pairs << qMakePair(kept->children.at(j), node);
            continue;
        }
        read->children[j] = nullptr;
        beginInsertRows(parent, j, j);
        node->parent = kept;
        kept->children.insert(j, node);
        endInsertRows();
        QList<Node*> added;
        added << node;
        while (!added.isEmpty()) {
            Node* next = added.takeFirst();
            m_added << next;
            added << next->children;
        }
    }
    for (const QPair<Node*, Node*>& pair : pairs) reconcile(pair.first, pair.second);
}

void MarkdownOutlineModel::flatten(Node* node)
{
    for (Node* child : node->children) {
        m_flat << child;
        flatten(child);
    }
}

QModelIndex MarkdownOutlineModel::documentIndex() const
{
    return indexOf(m_document);
}

const OutlineEntry* MarkdownOutlineModel::entry(const QModelIndex& index) const
{
    Node* node = nodeOf(index);
    return node ? &node->entry : nullptr;
}

QModelIndex MarkdownOutlineModel::indexAt(int line, int column) const
{
    Node* heading = nullptr;
    Node* image = nullptr;
    Node* first = nullptr;
    for (Node* node : m_flat) {
        const OutlineEntry& e = node->entry;
        if (e.line > line) break;
        if (!e.isImage()) {
            heading = node;
        } else if (e.line == line) {
            if (!first) first = node;
            if (e.column <= column) image = node;
        }
    }
    if (image) return indexOf(image);
    // before the images of a heading's line the heading is the place
    if (first && !(heading && heading->entry.line == line)) return indexOf(first);
    return indexOf(heading ? heading : m_document);
}

QModelIndex MarkdownOutlineModel::addedNamesake(const OutlineEntry& named) const
{
    Node* found = nullptr;
    for (Node* node : m_added) {
        if (node->entry.isImage() != named.isImage() || node->entry.name() != named.name()) continue;
        if (found) return QModelIndex();
        found = node;
    }
    return found ? indexOf(found) : QModelIndex();
}

bool MarkdownOutlineModel::isExpanded(const QModelIndex& index) const
{
    Node* node = nodeOf(index);
    return node && node->expanded;
}

void MarkdownOutlineModel::setExpanded(const QModelIndex& index, bool on)
{
    if (Node* node = nodeOf(index)) node->expanded = on;
}

void MarkdownOutlineModel::setAllExpanded(bool on)
{
    m_document->expanded = on;
    for (Node* node : m_flat) node->expanded = on;
}

MarkdownOutlineModel::Node* MarkdownOutlineModel::nodeOf(const QModelIndex& index) const
{
    if (!index.isValid() || index.model() != this) return nullptr;
    return static_cast<Node*>(index.internalPointer());
}

QModelIndex MarkdownOutlineModel::indexOf(Node* node) const
{
    if (!node) return QModelIndex();
    return createIndex(node->row(), 0, node);
}

QModelIndex MarkdownOutlineModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0 || row < 0) return QModelIndex();
    if (!parent.isValid()) return row == 0 ? indexOf(m_document) : QModelIndex();
    Node* node = nodeOf(parent);
    if (!node || row >= node->children.size()) return QModelIndex();
    return createIndex(row, 0, node->children.at(row));
}

QModelIndex MarkdownOutlineModel::parent(const QModelIndex& index) const
{
    Node* node = nodeOf(index);
    return node ? indexOf(node->parent) : QModelIndex();
}

int MarkdownOutlineModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid()) return 1;
    if (parent.column() != 0) return 0;
    Node* node = nodeOf(parent);
    return node ? node->children.size() : 0;
}

int MarkdownOutlineModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return 1;
}

Qt::ItemFlags MarkdownOutlineModel::flags(const QModelIndex& index) const
{
    // read-only: nothing is edited or dragged here (EDIT-MD-13)
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
}

QVariant MarkdownOutlineModel::data(const QModelIndex& index, int role) const
{
    Node* node = nodeOf(index);
    if (!node) return QVariant();
    const OutlineEntry& e = node->entry;
    switch (role) {
    case Qt::DisplayRole:
        // a heading without a title shows its level
        if (e.kind == OutlineEntry::outlineHeading && e.title.isEmpty()) return QString(e.level, '#');
        return e.label();
    case Qt::DecorationRole: {
        QIcon i = icon(e);
        return i.isNull() ? QVariant() : QVariant(i);
    }
    case Qt::ToolTipRole:
        return toolTip(e);
    default:
        return QVariant();
    }
}

QString MarkdownOutlineModel::toolTip(const OutlineEntry& e) const
{
    QDir base(m_baseDir);
    auto shown = [&](const QString& path) { return m_baseDir.isEmpty() ? path : base.relativeFilePath(path); };
    switch (e.kind) {
    case OutlineEntry::outlineHeading:
        return tr("Line %1\n#%2").arg(e.line).arg(e.anchor);
    case OutlineEntry::outlineImage:
        return tr("Line %1, column %2\n%3").arg(e.line).arg(e.column).arg(e.target);
    case OutlineEntry::outlineDiagram:
        return tr("Line %1, column %2\n%3\n%4\n%5").arg(e.line).arg(e.column).arg(shown(e.svg))
            .arg(shown(e.graphml)).arg(e.upToDate ? tr("The image is up to date") : tr("The image is out of date"));
    default:
        return QString();
    }
}

// the icon with the style's warning over its lower right corner
static QIcon markedIcon(const QIcon& icon)
{
    QIcon warning = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
    QIcon marked;
    for (const QSize& size : icon.availableSizes()) {
        QPixmap pixmap = icon.pixmap(size);
        QSize part = size / MARKDOWN_OUTLINE_MARK_PART;
        QPainter painter(&pixmap);
        painter.drawPixmap(QRect(QPoint(size.width() - part.width(), size.height() - part.height()), part),
                           warning.pixmap(part));
        painter.end();
        marked.addPixmap(pixmap);
    }
    return marked;
}

QIcon MarkdownOutlineModel::icon(const OutlineEntry& e) const
{
    static const QIcon image(":/Icons/images/insert-image.png");
    static const QIcon diagram(":/Icons/images/insert-diagram.png");
    static const QIcon outdated = markedIcon(diagram);
    switch (e.kind) {
    case OutlineEntry::outlineDocument:
        return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
    case OutlineEntry::outlineImage:
        return image;
    case OutlineEntry::outlineDiagram:
        return e.upToDate ? diagram : outdated;
    default:
        return QIcon();
    }
}
