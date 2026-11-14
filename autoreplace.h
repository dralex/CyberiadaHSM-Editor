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

#ifndef CYBERIADA_AUTOREPLACE
#define CYBERIADA_AUTOREPLACE

#include <QList>
#include <QString>
#include <QTextCursor>

class QKeyEvent;

// a pattern and the text it becomes when typed (EDIT-AUTO-1, docs/AUTOREPLACE.md)
struct AutoreplaceRule {
    QString from;
    QString to;
    bool    enabled = true;

    AutoreplaceRule() {}
    AutoreplaceRule(const QString& f, const QString& t, bool on = true): from(f), to(t), enabled(on) {}
    bool    operator==(const AutoreplaceRule& other) const {
        return from == other.from && to == other.to && enabled == other.enabled;
    }
};

typedef QList<AutoreplaceRule> AutoreplaceRules;

// what a text is for the rules (EDIT-AUTO-2/3): code and values are never replaced
enum AutoreplaceKind {
    autoreplaceNone = 0,
    autoreplaceProse,
    autoreplaceMarkdown
};

AutoreplaceRules defaultAutoreplaceRules();
// the enabled rule with the longest pattern the text ends with; -1 without one
int  autoreplaceMatch(const QString& before, const AutoreplaceRules& rules);
// [from, to) of a Markdown source is text for people: not code, a formula, an HTML tag
// or comment, a link target, a URL, a rule, a table border, the marks of a quote
// (EDIT-AUTO-4); an unclosed element counts to the end
bool markdownProseAt(const QString& text, int from, int to);
bool autoreplaceAllowed(AutoreplaceKind kind, const QString& text, int from, int to);

// The typing of one text watched for the patterns (EDIT-AUTO-1/5/6): the owner calls
// it around the key it handles, with the rules of the settings
class Autoreplacer {
public:
    // before the key: Backspace right after a replacement gives the typed characters
    // back; true when the key is done
    bool before(QKeyEvent* event, QTextCursor cursor);
    // after a key that typed a character before the cursor: a completed pattern is
    // replaced as one edit block; true when it was
    bool typed(QTextCursor cursor, AutoreplaceKind kind);
    // another text is edited
    void forget() { m_position = m_start = -1; }

private:
    // the place after the last replacement, good until the next key, and what it replaced
    int     m_position = -1;
    // where the last replaced pattern began: found there again, it was given back by
    // Backspace or Undo and stays as typed
    int     m_start = -1;
    QString m_from;
    QString m_to;
};

// the key types a character
bool typingKey(const QKeyEvent* event);
// the keys of a text set as written, not typed (the batch edit-text verb): no rule fires
void suspendAutoreplace(bool on);

#endif
