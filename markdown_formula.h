/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The formulas of a Markdown text: found, written and picked from the palette
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

#ifndef CYBERIADA_MARKDOWN_FORMULA
#define CYBERIADA_MARKDOWN_FORMULA

#include <QChar>
#include <QString>

// docs/MARKDOWN.md, Insert Formula; EDIT-MD-24..27

// a formula of a Markdown text: [begin, end) with its dollars, null without one
struct MarkdownFormula {
    int     begin = -1;
    int     end = -1;
    QString tex;
    // two dollars
    bool    display = false;
    bool    isNull() const { return begin < 0; }
};

// the formula holding the character at the position, never in code (markdownSourceRanges)
MarkdownFormula markdownFormulaAt(const QString& text, int position);
// the formula a cursor stands in: the characters on both sides of it are the formula's
MarkdownFormula markdownFormulaAround(const QString& text, int cursor);
// the character before or after a place, in its line; null at a line end
QChar markdownCharBefore(const QString& text, int position);
QChar markdownCharAfter(const QString& text, int position);

// the TeX as a formula's text holds it (EDIT-MD-26): the lines without their trailing spaces,
// the blank ones dropped, a $ without a \ before it escaped, trimmed; empty for a blank TeX
QString markdownFormulaTex(const QString& tex);
// the text of a formula (EDIT-MD-26): before and after are the characters around it in its
// line, null at a line end or for a paragraph of its own; empty for a blank TeX
QString markdownFormulaText(const QString& tex, bool display, QChar before = QChar(), QChar after = QChar());

#endif
