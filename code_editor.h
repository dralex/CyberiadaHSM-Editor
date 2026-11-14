/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The page of a text tab: the plain text editor with the line numbers
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

#ifndef CYBERIADA_CODE_EDITOR
#define CYBERIADA_CODE_EDITOR

#include <QPlainTextEdit>
#include "autoreplace.h"

struct TextScheme;

// The text of a text tab (docs/MODES.md, Text tabs): the line numbers in a margin;
// Undo and Redo belong to the window, so the undo stack of the tab is the one
// the edits go through (EDIT-MODE-13)
class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    CodeEditor(QWidget* parent = nullptr);

    int  lineNumberAreaWidth() const;
    void paintLineNumbers(QPaintEvent* event);
    // the number shown at a block from 0: its own number; a diff side has its lines
    virtual QString lineLabel(int block) const { return QString::number(block + 1); }
    // what the text is for the typed replacements (EDIT-AUTO-2/3): none unless set
    void setAutoreplaceKind(AutoreplaceKind kind) { m_kind = kind; }
    AutoreplaceKind autoreplaceKind() const { return m_kind; }
    // the page, the selection and the margin in the colors of the scheme (EDIT-COLOR-1)
    void setScheme(const TextScheme& scheme);

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void updateMargin();
    void updateLineNumbers(const QRect& rect, int dy);

private:
    QWidget* lineNumbers;
    AutoreplaceKind m_kind = autoreplaceNone;
    Autoreplacer m_replacer;
};

#endif
