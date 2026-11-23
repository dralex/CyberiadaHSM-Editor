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

#include <QRegularExpression>

#include "markdown_lines.h"

namespace {

const int FENCE_LENGTH = 3;
const int CODE_INDENT = 4;

} // namespace

int markdownFenceOf(const QString& line, QChar* mark)
{
    int i = 0;
    while (i < line.length() && line[i] == ' ') i++;
    if (i >= CODE_INDENT || i >= line.length() || (line[i] != '`' && line[i] != '~')) return 0;
    *mark = line[i];
    int n = 0;
    while (i + n < line.length() && line[i + n] == *mark) n++;
    return n >= FENCE_LENGTH ? n : 0;
}

bool markdownIndented(const QString& line)
{
    if (line.startsWith('\t')) return true;
    int i = 0;
    while (i < line.length() && line[i] == ' ') i++;
    return i >= CODE_INDENT && i < line.length();
}

bool markdownListItem(const QString& line)
{
    QString t = line.trimmed();
    if (t.length() < 2) return false;
    if ((t[0] == '-' || t[0] == '*' || t[0] == '+') && t[1] == ' ') return true;
    int i = 0;
    while (i < t.length() && t[i].isDigit()) i++;
    return i > 0 && i + 1 < t.length() && (t[i] == '.' || t[i] == ')') && t[i + 1] == ' ';
}

bool markdownOpensBlock(const QString& word)
{
    static const QRegularExpression MARKER(R"(^(?:[-*+]|\d{1,9}[.)]|#{1,6}|=+|-+)$)");
    if (MARKER.match(word).hasMatch()) return true;
    if (word.startsWith('>') || word.startsWith("```") || word.startsWith("~~~")) return true;
    return word.length() > 1 && word[0] == '<' && (word[1].isLetter() || word[1] == '/' || word[1] == '!');
}

QVector<bool> markdownCodeLines(const QStringList& lines)
{
    QVector<bool> code(lines.size(), false);
    QChar open;
    int openLength = 0;
    // the block before the last blank line holds a list item; the current block does
    bool listBeforeBlank = false;
    bool blockList = false;
    for (int i = 0; i < lines.size(); i++) {
        const QString& line = lines[i];
        QChar mark;
        int n = markdownFenceOf(line, &mark);
        if (openLength) {
            code[i] = true;
            // the closing line is code too
            if (n && mark == open && n >= openLength && line.trimmed().length() == n) openLength = 0;
            continue;
        }
        if (n) {
            open = mark;
            openLength = n;
            code[i] = true;
            continue;
        }
        bool blank = line.trimmed().isEmpty();
        if (blank) {
            listBeforeBlank = blockList;
            blockList = false;
            continue;
        }
        if (markdownListItem(line)) blockList = true;
        // an indented line is code after an empty one or another code line, not in a list
        if (!markdownIndented(line)) continue;
        if (i == 0) code[i] = true;
        else if (lines[i - 1].trimmed().isEmpty()) code[i] = !listBeforeBlank;
        else if (markdownIndented(lines[i - 1])) code[i] = code[i - 1];
    }
    return code;
}

QVector<bool> markdownInlineOpen(const QString& line, int to)
{
    QVector<bool> open(to + 1, false);
    int ticks = 0;        // the length of the open code span, 0 outside one
    bool math = false;
    int tag = -1;         // the open '<' of a tag
    int target = -1;      // the open "](" of a link
    int i = 0;
    while (i < to) {
        open[i] = ticks || math || tag >= 0 || target >= 0;
        QChar c = line[i];
        int step = 1;
        if (c == '\\' && !ticks) {
            step = 2;
        } else if (c == '`') {
            int n = 1;
            while (i + n < line.length() && line[i + n] == '`') n++;
            if (!ticks) ticks = n;
            else if (ticks == n) ticks = 0;
            step = n;
        } else if (!ticks) {
            if (c == '$') {
                // a dollar opens a formula before a character that is no space or digit
                if (math) math = false;
                else if (i + 1 < line.length() && !line[i + 1].isSpace() && !line[i + 1].isDigit() &&
                         (i == 0 || !line[i - 1].isLetterOrNumber())) math = true;
            } else if (!math) {
                if (c == '<' && i + 1 < line.length() &&
                    (line[i + 1].isLetter() || line[i + 1] == '/' || line[i + 1] == '!')) tag = i;
                else if (c == '>') tag = -1;
                else if (c == '(' && i > 0 && line[i - 1] == ']') target = i;
                else if (c == ')') target = -1;
            }
        }
        // the escaped character and the rest of a run of ticks are as the first one
        for (int k = i + 1; k < i + step && k < to; k++) open[k] = open[i];
        i += step;
    }
    open[to] = ticks || math || tag >= 0 || target >= 0;
    return open;
}

bool markdownInlineCodeAt(const QString& line, int from, int to)
{
    if (markdownInlineOpen(line, from).last()) return true;
    // a URL: the word around the range
    int begin = from;
    while (begin > 0 && !line[begin - 1].isSpace()) begin--;
    QString word = line.mid(begin, to - begin);
    return word.contains("://") || word.startsWith("www.");
}
