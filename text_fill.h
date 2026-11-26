/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The fill of the paragraphs of a Markdown or a Plain text source at a text width
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

#ifndef CYBERIADA_TEXT_FILL
#define CYBERIADA_TEXT_FILL

#include <QString>

// what a text fills as (EDIT-MD-18/23); code never fills
enum TextFillKind {
    textFillNone = 0,
    textFillPlain,
    textFillMarkdown
};

// an edit of a fill: [from, to) of the text replaced by the text, the anchor and the cursor
// after it; nothing changed when the text stays the same
struct TextFillEdit {
    bool    changed = false;
    int     from = 0;
    int     to = 0;
    QString text;
    int     anchor = 0;
    int     cursor = 0;
};

// a break of the fill as you type: the spaces [from, to) become a new line and its prefix
struct TextFillBreak {
    int     from = -1;
    int     to = -1;
    QString prefix;
};

// the units of the lines from the anchor to the cursor filled at the width, or each joined
// into one line (EDIT-MD-18..20/23); the anchor and the cursor kept at their characters
TextFillEdit textFill(const QString& text, int anchor, int cursor, int width, TextFillKind kind,
                      bool unfill);
// the break of the line of the key typed before the cursor, a Space or a Return, at the
// width (EDIT-MD-21); false when the line fits, is never filled or has no break point
bool         textAutoFillBreak(const QString& text, int cursor, int width, TextFillKind kind,
                               TextFillBreak* result);
// the prefix the next lines of the unit holding the line take (EDIT-MD-18/23), the one a break
// gives; null when the line is in no unit (a blank line, a block never filled)
QString      textFillPrefix(const QString& text, int line, TextFillKind kind);
// the columns of a line, a tab to the next stop (EDITOR_TAB_SPACES)
int          textColumns(const QString& line);

#endif
