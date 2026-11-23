/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The lines of a Markdown source: the code blocks and the inline spans
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

#ifndef CYBERIADA_MARKDOWN_LINES
#define CYBERIADA_MARKDOWN_LINES

#include <QString>
#include <QStringList>
#include <QVector>

// the fence a line opens or closes: its character and length, 0 when it is none
int  markdownFenceOf(const QString& line, QChar* mark);
// four spaces or a tab before some text
bool markdownIndented(const QString& line);
// a bullet or an ordered list item: its marker and a space
bool markdownListItem(const QString& line);
// a word that opens a block at the start of a line (EDIT-MD-19/26)
bool markdownOpensBlock(const QString& word);
// the lines inside a fenced or an indented code block, the fences too (EDIT-AUTO-4)
QVector<bool> markdownCodeLines(const QStringList& lines);
// for each position from 0 to `to` of a line, a code span, a formula, a tag or a link
// target is open before it (EDIT-AUTO-4, EDIT-MD-19)
QVector<bool> markdownInlineOpen(const QString& line, int to);
// [from, to) of the line is in a code span, a formula, a tag, a link target or a URL
bool markdownInlineCodeAt(const QString& line, int from, int to);

#endif
