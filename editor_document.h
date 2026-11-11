/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The document of a tab: its mode, its read-only flag and its title
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

#ifndef CYBERIADA_EDITOR_DOCUMENT
#define CYBERIADA_EDITOR_DOCUMENT

#include <QObject>
#include <QString>

#include "document_mode.h"

class QWidget;
class QUndoStack;

// One tab of the window (docs/MODES.md): a document in a mode, with the
// read-only flag of the tab (EDIT-MODE-1/4); the kinds derive from it
class EditorDocument: public QObject {
Q_OBJECT
public:
    EditorDocument(QObject* parent = nullptr): QObject(parent) {}

    // the mode id (EDIT-MODE-1)
    virtual QString          mode() const = 0;
    const ModeTraits&        traits() const { return modeTraits(mode()); }
    // the flag of Document > Read-only, off in a new tab
    bool                     readOnlyFlag() const { return m_readOnly; }
    void                     setReadOnlyFlag(bool on);
    // the flag, or a mode that is always read-only
    bool                     isReadOnly() const { return m_readOnly || traits().readOnly; }
    // nothing keeps the edits off: neither the flag, nor the mode, nor a plugin lock
    virtual bool             isEditable() const { return !isReadOnly(); }

    // the title without the marker (EDIT-DOC-10)
    virtual QString          title() const = 0;
    // " [inspection]", " [FA, ro]" ... after the title (EDIT-MODE-2)
    QString                  modeMarker(bool window = false) const;
    virtual QString          filePath() const = 0;
    bool                     hasFile() const { return !filePath().isEmpty(); }
    virtual bool             isClean() const = 0;

    // the page of the document stack and the stack of the undo group
    virtual QWidget*         page() const = 0;
    virtual QUndoStack*      undoStack() const = 0;

signals:
    void                     titleChanged();
    void                     modeChanged();
    void                     readOnlyChanged(bool on);

protected:
    // the kind applies the flag to its parts
    virtual void             applyReadOnlyFlag(bool on) { Q_UNUSED(on); }

private:
    bool                     m_readOnly = false;
};

#endif
