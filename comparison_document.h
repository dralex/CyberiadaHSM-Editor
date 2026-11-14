/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * A comparison tab: two sides from their origins and the list of their changes
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

#ifndef CYBERIADA_COMPARISON_DOCUMENT
#define CYBERIADA_COMPARISON_DOCUMENT

#include <QList>
#include <QPointer>
#include <string>

#include "editor_document.h"
#include "plugin_values.h"

class QUndoStack;

// where a side of a comparison comes from, so Refresh takes it again (EDIT-MODE-12)
struct DiffOrigin {
    enum Kind {
        originTab = 0,      // the snapshot of an open tab
        originFile,         // a file
        originSaved,        // the saved file of a tab
        originPlugin        // the output of a plugin run on a tab (EDIT-PLUG-58)
    };
    Kind                     kind = originTab;
    QPointer<EditorDocument> tab;
    QString                  path;
    // the name of the side in the title: a tab title, a file name, "saved", the
    // values of a plugin run
    QString                  label;
    // a plugin base: the action Refresh runs again with the same values
    QString                  pluginId;
    QString                  actionId;
    PluginValues             values;
};

// a row of the change list in the tree dock (EDIT-MODE-11/22)
struct ChangeEntry {
    enum Kind { changeAdded = 0, changeRemoved, changeChanged };
    QString group;          // the parent row: a state machine; empty for a top row
    QString element;        // the first column
    QString change;         // the second: "added", "removed" or what differs
    Kind    kind = changeChanged;
};

// A comparison tab (docs/MODES.md): the base and the target, read-only always;
// the kinds compare diagrams (DiffDocument) or texts (TextDiffDocument)
class ComparisonDocument: public EditorDocument {
Q_OBJECT
public:
    ComparisonDocument(QObject* parent = nullptr);

    // "<target> ↔ <base>"
    QString          title() const override;
    QString          filePath() const override { return QString(); }
    bool             isClean() const override { return true; }
    QUndoStack*      undoStack() const override { return m_undo; }

    DiffOrigin&      base() { return m_base; }
    DiffOrigin&      target() { return m_target; }
    // the sides as buffers - a diagram in any format the library reads, a text in
    // UTF-8, an empty one empty - compared again
    void             setSides(const std::string& base, const std::string& target);
    const std::string& baseBuffer() const { return m_baseBuffer; }
    const std::string& targetBuffer() const { return m_targetBuffer; }

    // the rows of the change list
    virtual QList<ChangeEntry> changes() const = 0;
    // the current row, -1 without one; a row made current is shown in both sides
    int              currentChange() const { return m_current; }
    bool             setCurrentChange(int index);
    // Next Change (+1), Previous Change (-1); false at an end (EDIT-MODE-22)
    bool             moveChange(int step);
    // the whole comparison in view
    virtual void     fit() {}

signals:
    void             compared();
    void             currentChangeChanged(int index);

protected:
    // read the buffers into the sides and compare them, emitting compared
    virtual void     loadSides() = 0;
    virtual void     showChange(int index) = 0;

    DiffOrigin       m_base;
    DiffOrigin       m_target;
    std::string      m_baseBuffer;
    std::string      m_targetBuffer;
    QUndoStack*      m_undo;
    int              m_current = -1;
};

#endif
