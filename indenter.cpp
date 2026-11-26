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

#include <QCoreApplication>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextDocument>

#include "indenter.h"
#include "settings_manager.h"

namespace {

// the lines the selection touches, from 0: a selection ending at a line start leaves that line
void selectedLines(const QTextCursor& cursor, int* first, int* last)
{
    int from = cursor.selectionStart();
    int to = cursor.selectionEnd();
    QTextDocument* document = cursor.document();
    *first = document->findBlock(from).blockNumber();
    QTextBlock end = document->findBlock(to);
    *last = end.blockNumber();
    if (to > from && to == end.position() && *last > *first) (*last)--;
}

QString stepName(const char* name)
{
    return QCoreApplication::translate("Indenter", name);
}

} // namespace

TextIndentRules Indenter::rulesFor(const QString& text) const
{
    TextIndentRules rules = m_rules;
    if (m_role == codeRoleNone) return rules;
    int start = codeBehaviourStart(text, m_role);
    // no code yet, the trigger typed: as a text without rules
    if (start < 0) return textIndentRules(static_cast<const CodeLanguage*>(nullptr));
    rules.codeStart = start;
    return rules;
}

bool Indenter::key(QKeyEvent* event, QPlainTextEdit* editor)
{
    m_returnEnd = -1;
    bool tab = event->key() == Qt::Key_Tab && !(event->modifiers() & ~Qt::KeypadModifier);
    if (!tab) m_tabBlock = -1;
    if (editor->isReadOnly() || m_rules.style == textIndentNone) return false;
    Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && modifiers == Qt::NoModifier) {
        if (!SettingsManager::instance().getAutoIndent()) return false;
        returnKey(editor);
        return true;
    }
    if (tab) {
        tabKey(editor);
        return true;
    }
    if (event->key() == Qt::Key_Backtab || (event->key() == Qt::Key_Tab && modifiers == Qt::ShiftModifier)) {
        backtabKey(editor);
        return true;
    }
    QString typed = event->text();
    if (typed.length() == 1 && !(modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        return electricKey(typed[0], editor);
    }
    return false;
}

bool Indenter::electricKey(QChar typed, QPlainTextEdit* editor)
{
    static const QString ELECTRIC = "}])#{:";
    SettingsManager& sm = SettingsManager::instance();
    if (!ELECTRIC.contains(typed) || !sm.getAutoIndent()) return false;
    QTextCursor cursor = editor->textCursor();
    QString text = editor->document()->toPlainText();
    // the text as the character makes it
    int at = cursor.selectionStart() + 1;
    QString after = text.left(cursor.selectionStart()) + typed + text.mid(cursor.selectionEnd());
    TextIndentRules rules = rulesFor(after);
    if (!textIndentElectric(after, at, rules)) return false;
    int line = after.left(at).count('\n');
    int column = textIndentColumn(after, line, rules, sm.getIndentWidth());
    if (column < 0) return false;
    TextFillEdit edit = textSetIndent(after, line, column, sm.getIndentTabs(), at, at);
    // the character and the indent of its line, one step: Undo takes both
    cursor.beginEditBlock();
    cursor.insertText(QString(typed));
    if (edit.changed) {
        cursor.setPosition(edit.from);
        cursor.setPosition(edit.to, QTextCursor::KeepAnchor);
        cursor.insertText(edit.text);
    }
    cursor.endEditBlock();
    cursor.setPosition(edit.changed ? edit.cursor : at);
    editor->setTextCursor(cursor);
    editor->ensureCursorVisible();
    return true;
}

void Indenter::returnKey(QPlainTextEdit* editor)
{
    SettingsManager& sm = SettingsManager::instance();
    // the block begun at the cursor: an undo puts the cursor back there
    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();
    if (cursor.hasSelection()) cursor.removeSelectedText();
    QString text = editor->document()->toPlainText();
    TextFillEdit edit = textReturnEdit(text, cursor.position(), rulesFor(text), sm.getIndentWidth(),
                                       sm.getIndentTabs());
    cursor.setPosition(edit.from);
    cursor.setPosition(edit.to, QTextCursor::KeepAnchor);
    cursor.insertText(edit.text);
    cursor.endEditBlock();
    editor->setTextCursor(cursor);
    editor->ensureCursorVisible();
    m_returnEnd = edit.from + 1;
}

void Indenter::tabKey(QPlainTextEdit* editor)
{
    SettingsManager& sm = SettingsManager::instance();
    int width = sm.getIndentWidth();
    bool tabs = sm.getIndentTabs();
    QTextCursor cursor = editor->textCursor();
    QString text = editor->document()->toPlainText();
    TextIndentRules rules = rulesFor(text);
    bool computes = sm.getAutoIndent() && textIndentComputes(rules);
    int anchor = cursor.anchor();
    int position = cursor.position();
    QTextBlock block = cursor.block();
    int line = block.blockNumber();
    QString lineText = block.text();
    int offset = position - block.position();
    int lead = 0;
    while (lead < lineText.length() && (lineText[lead] == ' ' || lineText[lead] == '\t')) lead++;
    TextFillEdit edit;
    if (cursor.hasSelection()) {
        int first = 0;
        int last = 0;
        selectedLines(cursor, &first, &last);
        if (computes) {
            apply(editor, textReindent(text, first, last, rules, width, tabs, anchor, position),
                  stepName(QT_TRANSLATE_NOOP("Indenter", "indent region")));
        } else {
            apply(editor, textShift(text, first, last, width, tabs, anchor, position),
                  stepName(QT_TRANSLATE_NOOP("Indenter", "indent")));
        }
        return;
    }
    if (computes) {
        QVector<int> levels = textIndentLevels(text, line, rules, width);
        if (levels.isEmpty()) return;
        int target = levels.first();
        bool again = line == m_tabBlock && editor->document()->revision() == m_tabRevision &&
                     position == m_tabPosition;
        if (again && levels.size() > 1) {
            int at = levels.indexOf(textLineIndent(lineText));
            target = levels[at < 0 ? 0 : (at + 1) % levels.size()];
        }
        apply(editor, textSetIndent(text, line, target, tabs, anchor, position),
              stepName(QT_TRANSLATE_NOOP("Indenter", "indent")));
        m_tabBlock = line;
        m_tabRevision = editor->document()->revision();
        m_tabPosition = editor->textCursor().position();
        return;
    }
    int column = textColumns(lineText.left(offset));
    // Indent automatically off: the whitespace to the next stop at the cursor
    bool relative = sm.getAutoIndent();
    if (relative && offset <= lead) column = textLineIndent(lineText);
    int target = textNextStop(column, width);
    if (relative) {
        for (int point : textIndentPoints(text, line)) {
            if (point > column) {
                target = point;
                break;
            }
        }
    }
    if (relative && offset <= lead) {
        edit = textSetIndent(text, line, target, tabs, anchor, position);
    } else {
        edit.changed = true;
        edit.from = edit.to = position;
        edit.text = textIndentRun(column, target, tabs);
        edit.anchor = edit.cursor = position + edit.text.length();
    }
    apply(editor, edit, stepName(QT_TRANSLATE_NOOP("Indenter", "indent")));
}

void Indenter::backtabKey(QPlainTextEdit* editor)
{
    SettingsManager& sm = SettingsManager::instance();
    int width = sm.getIndentWidth();
    bool tabs = sm.getIndentTabs();
    QTextCursor cursor = editor->textCursor();
    QString text = editor->document()->toPlainText();
    TextIndentRules rules = rulesFor(text);
    int first = cursor.blockNumber();
    int last = first;
    if (cursor.hasSelection()) selectedLines(cursor, &first, &last);
    QString step = stepName(QT_TRANSLATE_NOOP("Indenter", "unindent"));
    if (cursor.hasSelection() || textIndentComputes(rules)) {
        apply(editor, textShift(text, first, last, -width, tabs, cursor.anchor(), cursor.position()), step);
        return;
    }
    // back to the previous indent point or stop
    int column = textLineIndent(cursor.block().text());
    int target = textPreviousStop(column, width);
    for (int point : textIndentPoints(text, first)) {
        if (point < column) target = qMax(target, point);
    }
    apply(editor, textSetIndent(text, first, target, tabs, cursor.anchor(), cursor.position()), step);
}

void Indenter::apply(QPlainTextEdit* editor, const TextFillEdit& edit, const QString& step)
{
    if (!edit.changed) return;
    if (nameStep && !step.isEmpty()) nameStep(step);
    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();
    cursor.setPosition(edit.from);
    cursor.setPosition(edit.to, QTextCursor::KeepAnchor);
    cursor.insertText(edit.text);
    cursor.endEditBlock();
    if (nameStep && !step.isEmpty()) nameStep(QString());
    cursor.setPosition(edit.anchor);
    cursor.setPosition(edit.cursor, QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
    editor->ensureCursorVisible();
}

IndentFilter::IndentFilter(QPlainTextEdit* editor, const TextIndentRules& rules, CodeRole role):
    QObject(editor), m_editor(editor)
{
    m_indenter.setRules(rules);
    m_indenter.setHeaderRole(role);
    editor->installEventFilter(this);
}

// the indenting keys are done here, the others go on to the editor
bool IndentFilter::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        if (m_indenter.key(static_cast<QKeyEvent*>(event), m_editor)) return true;
    } else if (event->type() == QEvent::ShortcutOverride) {
        // Tab and Shift+Tab stay in the text, not the focus chain of the dialog
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab) key->accept();
    }
    return QObject::eventFilter(watched, event);
}
