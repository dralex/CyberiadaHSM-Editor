/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The replacement of the typed patterns by their characters
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

#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextDocument>
#include "autoreplace.h"
#include "markdown_lines.h"
#include "settings_manager.h"

namespace {

struct DefaultRule {
    const char* from;
    const char* to;
};

// the Cyrillic letters are the keys of c, r, tm in the Russian layout
const DefaultRule DEFAULT_RULES[] = {
    { "--",     u8"–" },
    { u8"–-", u8"—" },
    { "<<",     u8"«" },
    { ">>",     u8"»" },
    { "...",    u8"…" },
    { "(c)",    u8"©" },
    { u8"(с)", u8"©" },
    { "(r)",    u8"®" },
    { u8"(р)", u8"®" },
    { "(tm)",   u8"™" },
    { u8"(тм)", u8"™" },
    { "->",     u8"→" },
    { "<-",     u8"←" },
    { "=>",     u8"⇒" },
    { "+-",     u8"±" },
    { "!=",     u8"≠" },
    { "<=",     u8"≤" },
    { ">=",     u8"≥" },
};

bool suspended = false;

int longestPattern(const AutoreplaceRules& rules)
{
    int n = 0;
    for (const AutoreplaceRule& rule : rules) n = qMax(n, rule.from.length());
    return n;
}

} // namespace

AutoreplaceRules defaultAutoreplaceRules()
{
    AutoreplaceRules rules;
    for (size_t i = 0; i < sizeof(DEFAULT_RULES) / sizeof(DEFAULT_RULES[0]); i++) {
        rules << AutoreplaceRule(QString::fromUtf8(DEFAULT_RULES[i].from), QString::fromUtf8(DEFAULT_RULES[i].to));
    }
    return rules;
}

int autoreplaceMatch(const QString& before, const AutoreplaceRules& rules)
{
    int found = -1;
    for (int i = 0; i < rules.size(); i++) {
        const AutoreplaceRule& rule = rules[i];
        if (!rule.enabled || rule.from.isEmpty() || !before.endsWith(rule.from)) continue;
        if (found < 0 || rule.from.length() > rules[found].from.length()) found = i;
    }
    return found;
}

bool markdownProseAt(const QString& text, int from, int to)
{
    QString head = text.left(to);
    // an HTML comment not closed before the range
    int comment = head.lastIndexOf("<!--");
    if (comment >= 0 && comment + 4 <= from && head.indexOf("-->", comment + 4) < 0) return false;
    // a display formula open over the lines
    if (head.left(from).count("$$") % 2) return false;

    QStringList lines = head.split('\n');
    int index = lines.size() - 1;
    if (markdownCodeLines(lines)[index]) return false;
    const QString& line = lines[index];
    int start = from - (head.length() - line.length());
    if (start < 0) return true;   // a pattern over the lines is not an element of one
    if (markdownInlineCodeAt(line, start, line.length())) return false;

    // a rule, the underline of a heading, the border of a table
    static const QString BORDER = "-|:= ";
    bool border = true;
    for (QChar c : line) border = border && BORDER.contains(c);
    if (border) return false;
    // the marks of a quote
    bool quote = true;
    for (QChar c : line) quote = quote && (c == '>' || c == ' ');
    return !quote;
}

bool autoreplaceAllowed(AutoreplaceKind kind, const QString& text, int from, int to)
{
    switch (kind) {
    case autoreplaceProse:      return true;
    case autoreplaceMarkdown:   return markdownProseAt(text, from, to);
    default:                    return false;
    }
}

void suspendAutoreplace(bool on)
{
    suspended = on;
}

bool typingKey(const QKeyEvent* event)
{
    QString text = event->text();
    return !text.isEmpty() && text[0].isPrint();
}

namespace {

class CursorTarget: public AutoreplaceTarget {
public:
    explicit CursorTarget(const QTextCursor& cursor): m_cursor(cursor) {}
    int     position() const override { return m_cursor.position(); }
    bool    hasSelection() const override { return m_cursor.hasSelection(); }
    QString mid(int from, int length) const override {
        QTextCursor c = m_cursor;
        c.setPosition(from);
        c.setPosition(from + length, QTextCursor::KeepAnchor);
        return c.selectedText();
    }
    QString all() const override { return m_cursor.document()->toPlainText(); }
    void    replace(int from, int to, const QString& with) override {
        m_cursor.beginEditBlock();
        m_cursor.setPosition(from);
        m_cursor.setPosition(to, QTextCursor::KeepAnchor);
        m_cursor.insertText(with);
        m_cursor.endEditBlock();
    }
private:
    QTextCursor m_cursor;
};

class LineTarget: public AutoreplaceTarget {
public:
    explicit LineTarget(QLineEdit* line): m_line(line) {}
    int     position() const override { return m_line->cursorPosition(); }
    bool    hasSelection() const override { return m_line->hasSelectedText(); }
    QString mid(int from, int length) const override { return m_line->text().mid(from, length); }
    QString all() const override { return m_line->text(); }
    // an edit of the user: the line says textEdited, which commits a property row
    void    replace(int from, int to, const QString& with) override {
        m_line->setSelection(from, to - from);
        m_line->insert(with);
    }
private:
    QLineEdit* m_line;
};

} // namespace

bool Autoreplacer::before(QKeyEvent* event, QTextCursor cursor)
{
    CursorTarget text(cursor);
    return before(event, text);
}

bool Autoreplacer::typed(QTextCursor cursor, AutoreplaceKind kind)
{
    CursorTarget text(cursor);
    return typed(text, kind);
}

bool Autoreplacer::before(QKeyEvent* event, AutoreplaceTarget& text)
{
    int place = m_position;
    m_position = -1;
    if (place < 0 || event->key() != Qt::Key_Backspace || event->modifiers() != Qt::NoModifier ||
        text.hasSelection() || text.position() != place) return false;
    int from = place - m_to.length();
    if (text.mid(from, m_to.length()) != m_to) return false;
    text.replace(from, place, m_from);
    return true;
}

bool Autoreplacer::typed(AutoreplaceTarget& text, AutoreplaceKind kind)
{
    SettingsManager& sm = SettingsManager::instance();
    if (suspended || kind == autoreplaceNone || !sm.getAutoreplaceEnabled() || text.hasSelection()) return false;
    const AutoreplaceRules& rules = sm.getAutoreplaceRules();
    int to = text.position();
    int look = qMax(0, to - longestPattern(rules));
    int found = autoreplaceMatch(text.mid(look, to - look), rules);
    if (found < 0) return false;
    const AutoreplaceRule& rule = rules[found];
    int from = to - rule.from.length();
    // a pattern given back is not replaced again (EDIT-AUTO-6)
    if (m_start >= 0 && from < m_start + m_from.length() && to > m_start &&
        text.mid(m_start, m_from.length()) == m_from) return false;
    // the whole text is read only for a pattern typed
    if (kind == autoreplaceMarkdown && !autoreplaceAllowed(kind, text.all(), from, to)) return false;
    text.replace(from, to, rule.to);
    m_position = text.position();
    m_start = from;
    m_from = rule.from;
    m_to = rule.to;
    return true;
}

AutoreplaceFilter::AutoreplaceFilter(QLineEdit* editor, AutoreplaceKind kind):
    QObject(editor), m_line(editor), m_kind(kind)
{
    editor->installEventFilter(this);
    connect(editor, &QLineEdit::textEdited, this, [this]() { changed(); });
}

AutoreplaceFilter::AutoreplaceFilter(QPlainTextEdit* editor, AutoreplaceKind kind):
    QObject(editor), m_text(editor), m_kind(kind)
{
    editor->installEventFilter(this);
    connect(editor, &QPlainTextEdit::textChanged, this, [this]() { changed(); });
}

// the key is the editor's; what it typed is read when the text has changed
bool AutoreplaceFilter::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        bool done;
        if (m_line) {
            LineTarget text(m_line);
            done = m_replacer.before(key, text);
        } else {
            done = m_replacer.before(key, m_text->textCursor());
        }
        if (done) return true;
        m_armed = typingKey(key);
    } else if (event->type() == QEvent::InputMethod) {
        m_armed = !static_cast<QInputMethodEvent*>(event)->commitString().isEmpty();
    } else if (event->type() == QEvent::KeyRelease) {
        m_armed = false;
    }
    return QObject::eventFilter(watched, event);
}

void AutoreplaceFilter::changed()
{
    if (!m_armed) return;
    m_armed = false;
    if (m_line) {
        LineTarget text(m_line);
        m_replacer.typed(text, m_kind);
    } else {
        m_replacer.typed(m_text->textCursor(), m_kind);
    }
}
