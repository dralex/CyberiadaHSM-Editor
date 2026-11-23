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

#include <QRegularExpression>
#include <QStringList>

#include "markdown_formula.h"
#include "markdown_importer.h"
#include "markdown_lines.h"

namespace {

const QChar DOLLAR('$');
const QChar BACKSLASH('\\');
const QChar NEWLINE('\n');
const char* const DISPLAY_DOLLARS = "$$";

// a dollar md4c would take for a mark written \$
QString escapedDollars(const QString& line)
{
    QString result;
    for (int i = 0; i < line.length(); i++) {
        if (line[i] == DOLLAR && (i == 0 || line[i - 1] != BACKSLASH)) result += BACKSLASH;
        result += line[i];
    }
    return result;
}

// md4c opens a formula only after, and closes it only before, a space, a punctuation or a
// symbol character (Unicode P and S); a dollar beside would join the formula's dollars
bool needsSpace(QChar c)
{
    if (c.isNull() || c.isSpace()) return false;
    if (c == DOLLAR) return true;
    return !c.isPunct() && !c.isSymbol();
}

QString firstWord(const QString& line)
{
    static const QRegularExpression SPACES("\\s+");
    return line.trimmed().section(SPACES, 0, 0);
}

} // namespace

MarkdownFormula markdownFormulaAt(const QString& text, int position)
{
    MarkdownFormula formula;
    if (position < 0 || position >= text.length()) return formula;
    for (const MarkdownMathRange& m : markdownSourceRanges(text).math) {
        if (m.begin <= position && position < m.end) {
            formula.begin = m.begin;
            formula.end = m.end;
            formula.tex = text.mid(m.texBegin, m.texEnd - m.texBegin);
            formula.display = text.midRef(m.begin, 2) == QLatin1String(DISPLAY_DOLLARS);
            break;
        }
    }
    return formula;
}

MarkdownFormula markdownFormulaAround(const QString& text, int cursor)
{
    MarkdownFormula formula = markdownFormulaAt(text, cursor);
    return formula.isNull() || formula.begin >= cursor ? MarkdownFormula() : formula;
}

QChar markdownCharBefore(const QString& text, int position)
{
    return position > 0 && position <= text.length() && text[position - 1] != NEWLINE ? text[position - 1] : QChar();
}

QChar markdownCharAfter(const QString& text, int position)
{
    return position >= 0 && position < text.length() && text[position] != NEWLINE ? text[position] : QChar();
}

QString markdownFormulaTex(const QString& tex)
{
    QString source = tex;
    source.replace("\r\n", "\n").replace(QChar::ParagraphSeparator, NEWLINE).replace('\r', NEWLINE);
    // a blank line would end the paragraph
    QStringList lines;
    for (QString line : source.split(NEWLINE)) {
        int n = line.length();
        while (n > 0 && line[n - 1].isSpace()) n--;
        line.truncate(n);
        if (!line.trimmed().isEmpty()) lines << escapedDollars(line);
    }
    if (lines.isEmpty()) return QString();
    lines.first() = lines.first().trimmed();
    return lines.join(NEWLINE);
}

QString markdownFormulaText(const QString& tex, bool display, QChar before, QChar after)
{
    QString normalized = markdownFormulaTex(tex);
    if (normalized.isEmpty()) return QString();
    QStringList lines = normalized.split(NEWLINE);
    QString result;
    if (!display || lines.size() == 1) {
        for (QString& line : lines) line = line.trimmed();
        const char* dollars = display ? DISPLAY_DOLLARS : "$";
        result = dollars + lines.join(' ') + dollars;
    } else {
        // a line that would open a block goes on the line before it, the opening $$ too
        QStringList written;
        written << DISPLAY_DOLLARS;
        for (const QString& line : lines) {
            if (markdownOpensBlock(firstWord(line))) written.last() += ' ' + line.trimmed();
            else written << line;
        }
        written << DISPLAY_DOLLARS;
        result = written.join(NEWLINE);
    }
    if (needsSpace(before)) result.prepend(' ');
    if (needsSpace(after)) result.append(' ');
    return result;
}
