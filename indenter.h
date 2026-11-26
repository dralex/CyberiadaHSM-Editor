/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The keys that indent a source editor: Return, Tab, Shift+Tab, the electric characters
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

#ifndef CYBERIADA_INDENTER
#define CYBERIADA_INDENTER

#include <QObject>
#include <functional>
#include "code_highlighter.h"
#include "text_indent.h"

class QKeyEvent;
class QPlainTextEdit;

// The indenting keys of one text (EDIT-INDENT-1..4, docs/INDENT.md): the owner calls it around
// the keys it handles, with the width, the form and the switch of the settings
class Indenter {
public:
    void setRules(const TextIndentRules& rules) { m_rules = rules; }
    const TextIndentRules& rules() const { return m_rules; }
    // a text with a header before its code (the code dialogs): its start found at each key
    void setHeaderRole(CodeRole role) { m_role = role; }
    // before the editor's own handling: Return, Tab, Shift+Tab, a character that moves its line
    // typed with the line re-indented in one step; true when done
    bool key(QKeyEvent* event, QPlainTextEdit* editor);
    // after a Return done by key(): the position after its line feed, -1 after another key
    int  returnEnd() const { return m_returnEnd; }
    // the name of the undo step of an edit, empty after it
    std::function<void(const QString&)> nameStep;

private:
    // the rules of a line: of a Markdown code block holding it, after the header of a dialog
    TextIndentRules rulesFor(const QString& text, int line) const;
    void returnKey(QPlainTextEdit* editor);
    void tabKey(QPlainTextEdit* editor);
    void backtabKey(QPlainTextEdit* editor);
    bool electricKey(QChar typed, QPlainTextEdit* editor);
    // the edit in one block begun at the editor's cursor, the anchor and the cursor of the edit set
    void apply(QPlainTextEdit* editor, const TextFillEdit& edit, const QString& step);

    TextIndentRules m_rules;
    CodeRole        m_role = codeRoleNone;
    int             m_returnEnd = -1;
    // the place after the last Tab: a Tab there again goes on through the cycle
    int             m_tabBlock = -1;
    int             m_tabRevision = -1;
    int             m_tabPosition = -1;
};

// The keys of a code dialog's text (EDIT-INDENT-8): it lives with its editor
class IndentFilter: public QObject {
public:
    IndentFilter(QPlainTextEdit* editor, const TextIndentRules& rules, CodeRole role);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QPlainTextEdit* m_editor;
    Indenter        m_indenter;
};

#endif
