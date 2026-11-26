/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The indentation of a source text: the columns of its lines and the edits
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

#ifndef CYBERIADA_TEXT_INDENT
#define CYBERIADA_TEXT_INDENT

#include <QString>
#include <QStringList>
#include <QVector>
#include "text_fill.h"

struct CodeLanguage;

// how a text indents (EDIT-INDENT-1/3/5, docs/INDENT.md)
enum TextIndentStyle {
    textIndentNone = 0,
    // no column to compute: Markdown, Plain text, a language without rules
    textIndentRelative,
    // the C family: the brackets, the blocks of statements, the labels
    textIndentBraces,
    // the brackets only (JSON)
    textIndentBrackets,
    // the brackets and the blocks opened by a colon (Python)
    textIndentOffside
};

struct TextIndentRules {
    TextIndentStyle style = textIndentNone;
    // relative: the units of the prefix a new line takes, none for a language
    TextFillKind    fill = textFillNone;
    QString         lineComment;
    QString         blockOpen;
    QString         blockClose;
    QString         quotes;
    bool            preprocessor = false;
    QStringList     labels;
    QStringList     flat;
    // the text before it is a header, not code (EDIT-INDENT-8)
    int             codeStart = 0;
};

// the rules of a language, of a fill kind (Markdown, Plain text)
TextIndentRules textIndentRules(const CodeLanguage* language);
TextIndentRules textIndentRules(TextFillKind kind);
// the style computes the columns of its lines
bool    textIndentComputes(const TextIndentRules& rules);

// the columns of the leading whitespace of a line
int     textLineIndent(const QString& line);
// the whitespace of an indent in the form (EDIT-INDENT-4): spaces, or a tab per stop and spaces;
// a run from a column to another the same way
QString textIndentString(int column, bool tabs);
QString textIndentRun(int from, int to, bool tabs);
// the next stop of the width after a column, the last one before it (0 at least)
int     textNextStop(int column, int width);
int     textPreviousStop(int column, int width);

// the column of a line from 0 with the lines above as they stand; -1 when the line stays (it
// starts in a block comment, a string, a continued preprocessor line or the header)
int          textIndentColumn(const QString& text, int line, const TextIndentRules& rules, int width);
// the levels a repeated Tab cycles (offside): the column, then each lower block level; the
// column alone for another style
QVector<int> textIndentLevels(const QString& text, int line, const TextIndentRules& rules, int width);
// the indent points of indent-relative: the indent and the word starts of the nearest
// non-blank line above, ascending; none without one
QVector<int> textIndentPoints(const QString& text, int line);
// the typed character before the position makes its line move (EDIT-INDENT-2)
bool         textIndentElectric(const QString& text, int position, const TextIndentRules& rules);

// Return at the position (EDIT-INDENT-1/3): the whitespace around it and the line feed
TextFillEdit textReturnEdit(const QString& text, int position, const TextIndentRules& rules, int width,
                            bool tabs);
// the lines first..last re-indented one after another, each with those above as re-indented;
// an empty or blank line emptied (Indent Region, EDIT-INDENT-6)
TextFillEdit textReindent(const QString& text, int first, int last, const TextIndentRules& rules,
                          int width, bool tabs, int anchor, int cursor);
// the line at the column, its whitespace rewritten in the form; a line already there in the
// form is no edit; the anchor and the cursor at their characters, from the whitespace at the text
TextFillEdit textSetIndent(const QString& text, int line, int column, bool tabs, int anchor, int cursor);
// each non-blank line of first..last moved by delta columns, 0 at least (EDIT-INDENT-1/3)
TextFillEdit textShift(const QString& text, int first, int last, int delta, bool tabs, int anchor,
                       int cursor);
// each tab of [from, to) turned into the spaces up to its stop (EDIT-INDENT-6)
TextFillEdit textUntabify(const QString& text, int from, int to, int anchor, int cursor);

#endif
