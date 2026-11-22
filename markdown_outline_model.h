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

#ifndef CYBERIADA_MARKDOWN_OUTLINE_MODEL
#define CYBERIADA_MARKDOWN_OUTLINE_MODEL

#include <QAbstractItemModel>
#include <QIcon>
#include <QList>

#include "markdown_outline.h"
#include "markdown_tools.h"

// The outline of a Markdown tab (docs/MARKDOWN.md, Outline; EDIT-MD-13..16): the row of the
// document, its headings below it by level, its images as leaves. The text is its source: a
// reading reconciles the rows with the text, nothing here changes the text
class MarkdownOutlineModel: public QAbstractItemModel {
Q_OBJECT
public:
    explicit MarkdownOutlineModel(QObject* parent = nullptr);
    ~MarkdownOutlineModel();

    // read the outline of the text: a row of the same or a similar heading or image under the
    // same parent is kept, the others are removed and added (EDIT-MD-14)
    void             refresh(const QString& documentName, const QString& text, const QString& baseDir);
    // a reading runs: a current row the removed rows move is no choice of the user
    bool             isReading() const { return m_reading; }
    QModelIndex      documentIndex() const;
    // null for an invalid index
    const OutlineEntry* entry(const QModelIndex& index) const;
    // the row of the source place (EDIT-MD-15): the last image of the line at or before the
    // column (the first one before it), else the last heading at or before the line, else the
    // document row
    QModelIndex      indexAt(int line, int column) const;
    // the only row of the kind and the name of the entry that the last reading added; invalid
    // without one or with more (EDIT-MD-16)
    QModelIndex      addedNamesake(const OutlineEntry& named) const;
    // the expanded state of a row, kept while the tab is in the background; a new row is expanded
    bool             isExpanded(const QModelIndex& index) const;
    void             setExpanded(const QModelIndex& index, bool on);
    void             setAllExpanded(bool on);

    QModelIndex      index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex      parent(const QModelIndex& index) const override;
    int              rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int              columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant         data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags    flags(const QModelIndex& index) const override;

private:
    struct Node;
    Node*            nodeOf(const QModelIndex& index) const;
    QModelIndex      indexOf(Node* node) const;
    // the kept rows of kept take the entries and the order of the read rows of read
    void             reconcile(Node* kept, Node* read);
    void             flatten(Node* node);
    QString          toolTip(const OutlineEntry& entry) const;
    QIcon            icon(const OutlineEntry& entry) const;

    Node*            m_document;
    bool             m_reading = false;
    QString          m_baseDir;
    // the rows the last reading added, their subtrees with them
    QList<Node*>     m_added;
    // the rows in the order of the text
    QList<Node*>     m_flat;
    DiagramFileCache m_diagrams;
};

#endif
