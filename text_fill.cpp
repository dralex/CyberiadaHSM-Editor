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

#include <QRegularExpression>
#include <QStringList>
#include <QVector>
#include <algorithm>

#include "cyberiada_constants.h"
#include "markdown_lines.h"
#include "text_fill.h"

namespace {

// the spaces after a list marker that still belong to it; more make the content indented code
const int LIST_MARKER_SPACES_MAX = 4;
// the trailing spaces of a hard break
const int HARD_BREAK_SPACES = 2;

// a line as its quote marks, as written, and the rest
struct LineParts {
    QString quote;
    int     depth = 0;
    QString body;
};

// the lines of one paragraph filled together (EDIT-MD-18/23)
struct Unit {
    int     first = 0;
    int     last = 0;
    bool    item = false;
    int     depth = 0;
    QString quote;
    // the start of the first line kept as written, the prefix of the next lines
    QString firstPrefix;
    QString prefix;
    bool    prefixSet = false;
    // the trailing spaces of the hard break that ends the unit
    QString tail;
};

LineParts splitQuote(const QString& line)
{
    static const QRegularExpression QUOTE(R"(^(?: {0,3}> ?)+)");
    LineParts parts;
    QRegularExpressionMatch m = QUOTE.match(line);
    if (m.hasMatch()) {
        parts.quote = m.captured(0);
        parts.depth = parts.quote.count('>');
    }
    parts.body = line.mid(parts.quote.length());
    return parts;
}

QString leadingSpace(const QString& s)
{
    int i = 0;
    while (i < s.length() && (s[i] == ' ' || s[i] == '\t')) i++;
    return s.left(i);
}

// the start of the content of a list item: its indent, its marker and the spaces after it;
// the number of an ordered one
int itemContent(const QString& body, int* number)
{
    int i = 0;
    while (i < body.length() && body[i] == ' ') i++;
    *number = -1;
    if (body[i] == '-' || body[i] == '*' || body[i] == '+') {
        i++;
    } else {
        int digits = i;
        while (i < body.length() && body[i].isDigit()) i++;
        *number = body.mid(digits, i - digits).toInt();
        i++;
    }
    int spaces = 0;
    while (i + spaces < body.length() && body[i + spaces] == ' ') spaces++;
    if (spaces > LIST_MARKER_SPACES_MAX) spaces = 1;
    return i + spaces;
}

// two trailing spaces or a backslash end the line with a hard break; the spaces as its tail
bool hardBreak(const QString& line, QString* tail)
{
    int n = 0;
    while (n < line.length() && line[line.length() - 1 - n] == ' ') n++;
    if (n >= HARD_BREAK_SPACES) {
        *tail = line.right(n);
        return true;
    }
    if (n) return false;
    int slashes = 0;
    while (slashes < line.length() && line[line.length() - 1 - slashes] == '\\') slashes++;
    return slashes % 2;
}

QList<Unit> plainUnits(const QStringList& lines)
{
    QList<Unit> units;
    for (int i = 0; i < lines.size(); i++) {
        if (lines[i].trimmed().isEmpty()) continue;
        Unit u;
        u.first = i;
        while (i + 1 < lines.size() && !lines[i + 1].trimmed().isEmpty()) i++;
        u.last = i;
        u.firstPrefix = leadingSpace(lines[u.first]);
        u.prefix = u.last > u.first ? leadingSpace(lines[u.first + 1]) : u.firstPrefix;
        units << u;
    }
    return units;
}

// the units of a Markdown text: the blocks that are never filled are skipped (EDIT-MD-18)
QList<Unit> markdownUnits(const QStringList& lines, QVector<LineParts>* split)
{
    static const QRegularExpression ATX(R"(^ {0,3}#{1,6}(?:[ \t]|$))");
    static const QRegularExpression THEMATIC(R"(^ {0,3}([-*_])(?:[ \t]*\1){2,}[ \t]*$)");
    static const QRegularExpression SETEXT(R"(^ {0,3}(?:=+|-+)[ \t]*$)");
    static const QRegularExpression DELIMITER(R"(^ {0,3}\|?[ \t]*:?-+:?[ \t]*(?:\|[ \t]*:?-+:?[ \t]*)*\|?[ \t]*$)");
    static const QRegularExpression TAG(R"(^ {0,3}<[A-Za-z/!?])");
    static const QRegularExpression REFERENCE(R"(^ {0,3}\[[^\]]+\]:)");

    QVector<LineParts>& parts = *split;
    parts.resize(lines.size());
    for (int i = 0; i < lines.size(); i++) parts[i] = splitQuote(lines[i]);
    // the code lines of the text, and of each run of quoted lines by their bodies
    QVector<bool> code = markdownCodeLines(lines);
    for (int i = 0; i < lines.size();) {
        if (!parts[i].depth) {
            i++;
            continue;
        }
        QStringList bodies;
        int j = i;
        while (j < lines.size() && parts[j].depth) bodies << parts[j++].body;
        QVector<bool> quoted = markdownCodeLines(bodies);
        for (int k = i; k < j; k++) code[k] = quoted[k - i];
        i = j;
    }

    enum Block { blockNone, blockOpaque, blockComment, blockFormula };
    Block block = blockNone;
    QList<Unit> units;
    bool open = false;
    for (int i = 0; i < lines.size(); i++) {
        const QString& body = parts[i].body;
        QString trimmed = body.trimmed();
        if (block == blockComment || block == blockFormula) {
            if (lines[i].contains(block == blockComment ? "-->" : "$$")) block = blockNone;
            open = false;
            continue;
        }
        if (trimmed.isEmpty()) {
            // an HTML block and a table end at a blank line
            block = blockNone;
            open = false;
            continue;
        }
        if (code[i] || block == blockOpaque || ATX.match(body).hasMatch()) {
            open = false;
            continue;
        }
        if (open && body.contains('|') && DELIMITER.match(body).hasMatch() &&
            units.last().last == i - 1 && lines[i - 1].contains('|')) {
            // the line before is the header of a table
            if (units.last().first == i - 1) units.removeLast();
            else units.last().last = i - 2;
            block = blockOpaque;
            open = false;
            continue;
        }
        if (open && !units.last().item && units.last().depth == parts[i].depth &&
            SETEXT.match(body).hasMatch()) {
            // the paragraph is the text of a heading
            units.removeLast();
            open = false;
            continue;
        }
        if (THEMATIC.match(body).hasMatch()) {
            open = false;
            continue;
        }
        if (trimmed.startsWith("$$")) {
            if (trimmed.count("$$") % 2) block = blockFormula;
            open = false;
            continue;
        }
        if (trimmed.startsWith("<!--")) {
            if (!trimmed.contains("-->")) block = blockComment;
            open = false;
            continue;
        }
        bool item = markdownListItem(body);
        int number = -1;
        int content = item ? itemContent(body, &number) : 0;
        // an ordered list interrupts a paragraph outside a list from 1 only
        if (item && open && !units.last().item && number > 1 &&
            leadingSpace(units.last().firstPrefix.mid(units.last().quote.length())).isEmpty()) item = false;
        bool continued = open && !item && (!parts[i].depth || parts[i].depth == units.last().depth);
        if (continued) {
            Unit& u = units.last();
            u.last = i;
            if (!u.prefixSet) {
                u.prefix = u.quote + leadingSpace(body);
                u.prefixSet = true;
            }
        } else {
            if (!item && (TAG.match(body).hasMatch() || REFERENCE.match(body).hasMatch())) {
                // an HTML block to a blank line, a link reference definition
                if (TAG.match(body).hasMatch()) block = blockOpaque;
                open = false;
                continue;
            }
            Unit u;
            u.first = u.last = i;
            u.item = item;
            u.depth = parts[i].depth;
            u.quote = parts[i].quote;
            if (item) {
                u.firstPrefix = u.quote + body.left(content);
                u.prefix = u.quote + QString(content, ' ');
                u.prefixSet = true;
            } else {
                u.firstPrefix = u.quote + leadingSpace(body);
                u.prefix = u.firstPrefix;
            }
            units << u;
            open = true;
        }
        if (hardBreak(lines[i], &units.last().tail)) open = false;
    }
    return units;
}

QList<Unit> fillUnits(const QStringList& lines, TextFillKind kind, QVector<LineParts>* parts)
{
    if (kind == textFillMarkdown) return markdownUnits(lines, parts);
    parts->clear();
    return plainUnits(lines);
}

// where the content of a line of a unit starts: past its prefix
int contentStart(const Unit& u, int index, const QString& line, TextFillKind kind)
{
    if (index == u.first) return u.firstPrefix.length();
    if (kind == textFillMarkdown) {
        LineParts parts = splitQuote(line);
        return parts.quote.length() + leadingSpace(parts.body).length();
    }
    return leadingSpace(line).length();
}

// the positions of a text where a break would cut a code span, a formula, a tag or a link
// target; a display formula inside a line too
QVector<bool> protectedSpaces(const QString& s, TextFillKind kind)
{
    if (kind != textFillMarkdown) return QVector<bool>(s.length() + 1, false);
    QVector<bool> protect = markdownInlineOpen(s, s.length());
    int open = -1;
    for (int i = 0; i + 1 < s.length(); i++) {
        if (s[i] != '$' || s[i + 1] != '$') continue;
        if (open < 0) {
            open = i;
        } else {
            for (int k = open; k <= i; k++) protect[k] = true;
            open = -1;
        }
        i++;
    }
    return protect;
}

// a unit filled: its new text and the places of its characters that are no spaces, of the
// old text and of the new one (from the start of the unit)
struct FilledUnit {
    QString     text;
    QVector<int> oldPlaces;
    QVector<int> newPlaces;
};

FilledUnit fillUnit(const Unit& u, const QStringList& lines, const QVector<int>& starts, int width,
                    TextFillKind kind, bool unfill)
{
    // the contents of the lines joined by a space; the place of each character in the text
    QString s;
    QVector<int> places;
    for (int i = u.first; i <= u.last; i++) {
        const QString& line = lines[i];
        int start = contentStart(u, i, line, kind);
        int end = line.length();
        while (end > start && line[end - 1].isSpace()) end--;
        if (!s.isEmpty()) {
            s += ' ';
            places << -1;
        }
        for (int k = start; k < end; k++) {
            s += line[k];
            places << starts[i] + k;
        }
    }
    QVector<bool> protect = protectedSpaces(s, kind);
    QStringList words;
    FilledUnit filled;
    for (int i = 0; i < s.length();) {
        while (i < s.length() && s[i].isSpace()) i++;
        if (i >= s.length()) break;
        int begin = i;
        while (i < s.length() && !(s[i].isSpace() && !protect[i])) i++;
        words << s.mid(begin, i - begin);
        for (int k = begin; k < i; k++) {
            if (!s[k].isSpace()) filled.oldPlaces << places[k];
        }
    }

    int n = words.size();
    for (int i = 0; i < n;) {
        bool first = i == 0;
        const QString& prefix = first ? u.firstPrefix : u.prefix;
        int j = i + 1;
        if (unfill) {
            j = n;
        } else {
            int columns = textColumns(prefix) + textColumns(words[i]);
            while (j < n && columns + 1 + textColumns(words[j]) <= width) {
                columns += 1 + textColumns(words[j]);
                j++;
            }
            if (kind == textFillMarkdown && j < n) {
                // a word that would open a block goes back with the words before it
                int k = j;
                while (k < n && k - i > 1 && markdownOpensBlock(words[k])) k--;
                if (k < n && markdownOpensBlock(words[k])) {
                    k = j;
                    while (k < n && markdownOpensBlock(words[k])) k++;
                }
                j = k;
            }
        }
        if (!first) filled.text += '\n';
        filled.text += prefix;
        for (int w = i; w < j; w++) {
            if (w > i) filled.text += ' ';
            for (QChar c : words[w]) {
                if (!c.isSpace()) filled.newPlaces << filled.text.length();
                filled.text += c;
            }
        }
        i = j;
    }
    filled.text += u.tail;
    return filled;
}

// a place of the old text inside a unit at the same character of the new one (EDIT-MD-20)
int unitPlace(const FilledUnit& filled, const Unit& u, int unitStart, int place)
{
    if (place < unitStart + u.firstPrefix.length()) return place - unitStart;
    int k = std::lower_bound(filled.oldPlaces.begin(), filled.oldPlaces.end(), place) - filled.oldPlaces.begin();
    if (k < filled.oldPlaces.size() && filled.oldPlaces[k] == place) return filled.newPlaces[k];
    return k > 0 ? filled.newPlaces[k - 1] + 1 : u.firstPrefix.length();
}

QVector<int> lineStarts(const QStringList& lines)
{
    QVector<int> starts;
    int at = 0;
    for (const QString& line : lines) {
        starts << at;
        at += line.length() + 1;
    }
    return starts;
}

int lineOf(const QVector<int>& starts, int place)
{
    return int(std::upper_bound(starts.begin(), starts.end(), place) - starts.begin()) - 1;
}

} // namespace

int textColumns(const QString& line)
{
    int columns = 0;
    for (QChar c : line) {
        if (c == '\t') columns = (columns / EDITOR_TAB_SPACES + 1) * EDITOR_TAB_SPACES;
        else if (!c.isLowSurrogate() && !c.isMark()) columns++;
    }
    return columns;
}

QString textFillPrefix(const QString& text, int line, TextFillKind kind)
{
    if (kind == textFillNone) return QString();
    QStringList lines = text.split('\n');
    QVector<LineParts> parts;
    for (const Unit& u : fillUnits(lines, kind, &parts)) {
        if (u.first <= line && line <= u.last) return u.prefix.isNull() ? QString("") : u.prefix;
    }
    return QString();
}

TextFillEdit textFill(const QString& text, int anchor, int cursor, int width, TextFillKind kind,
                      bool unfill)
{
    TextFillEdit edit;
    edit.anchor = anchor;
    edit.cursor = cursor;
    if (kind == textFillNone) return edit;
    QStringList lines = text.split('\n');
    QVector<int> starts = lineStarts(lines);
    int low = qBound(0, qMin(anchor, cursor), text.length());
    int high = qBound(0, qMax(anchor, cursor), text.length());
    int firstLine = lineOf(starts, low);
    int lastLine = lineOf(starts, high);
    // a selection to the start of a line ends with the line before it
    if (high > low && lastLine > firstLine && high == starts[lastLine]) lastLine--;

    QVector<LineParts> parts;
    QList<Unit> units;
    for (const Unit& u : fillUnits(lines, kind, &parts)) {
        if (u.last >= firstLine && u.first <= lastLine) units << u;
    }
    if (units.isEmpty()) return edit;

    int from = starts[units.first().first];
    int to = starts[units.last().last] + lines[units.last().last].length();
    QString out;
    int oldAt = from;
    int places[2] = { anchor, cursor };
    int mapped[2] = { anchor, cursor };
    bool done[2] = { anchor < from, cursor < from };
    for (const Unit& u : units) {
        int unitStart = starts[u.first];
        int unitEnd = starts[u.last] + lines[u.last].length();
        for (int p = 0; p < 2; p++) {
            if (!done[p] && places[p] < unitStart) {
                mapped[p] = from + out.length() + places[p] - oldAt;
                done[p] = true;
            }
        }
        out += text.mid(oldAt, unitStart - oldAt);
        FilledUnit filled = fillUnit(u, lines, starts, width, kind, unfill);
        for (int p = 0; p < 2; p++) {
            if (!done[p] && places[p] <= unitEnd) {
                mapped[p] = from + out.length() + unitPlace(filled, u, unitStart, places[p]);
                done[p] = true;
            }
        }
        out += filled.text;
        oldAt = unitEnd;
    }
    for (int p = 0; p < 2; p++) {
        if (!done[p]) mapped[p] = places[p] + out.length() - (to - from);
    }
    QString old = text.mid(from, to - from);
    if (out == old) return edit;

    // the least range replaced
    int head = 0;
    while (head < out.length() && head < old.length() && out[head] == old[head]) head++;
    int foot = 0;
    while (foot < out.length() - head && foot < old.length() - head &&
           out[out.length() - 1 - foot] == old[old.length() - 1 - foot]) foot++;
    edit.changed = true;
    edit.from = from + head;
    edit.to = to - foot;
    edit.text = out.mid(head, out.length() - head - foot);
    edit.anchor = mapped[0];
    edit.cursor = mapped[1];
    return edit;
}

bool textAutoFillBreak(const QString& text, int cursor, int width, TextFillKind kind,
                       TextFillBreak* result)
{
    if (kind == textFillNone || cursor < 1 || cursor > text.length()) return false;
    int key = cursor - 1;
    int lineStart = key > 0 ? text.lastIndexOf('\n', key - 1) + 1 : 0;
    // the line before the key typed
    QString before = text.mid(lineStart, key - lineStart);
    if (textColumns(before) <= width) return false;

    QStringList lines = text.split('\n');
    int index = text.left(lineStart).count('\n');
    QVector<LineParts> parts;
    const Unit* unit = nullptr;
    QList<Unit> units = fillUnits(lines, kind, &parts);
    for (const Unit& u : units) {
        if (u.first <= index && index <= u.last) unit = &u;
    }
    if (!unit) return false;

    int start = contentStart(*unit, index, lines[index], kind);
    QVector<bool> protect = protectedSpaces(before, kind);
    int chosen = -1;
    int chosenEnd = -1;
    for (int i = start + 1; i < before.length(); i++) {
        if (!before[i].isSpace() || before[i - 1].isSpace()) continue;
        int end = i;
        while (end < before.length() && before[end].isSpace()) end++;
        // a run of spaces with a word after it before the key
        if (end >= before.length() || protect[i]) continue;
        if (kind == textFillMarkdown) {
            int wordEnd = end;
            while (wordEnd < before.length() && !before[wordEnd].isSpace()) wordEnd++;
            if (markdownOpensBlock(before.mid(end, wordEnd - end))) continue;
        }
        bool fits = textColumns(before.left(i)) <= width;
        if (fits || chosen < 0) {
            chosen = i;
            chosenEnd = end;
        }
        if (!fits) break;
    }
    if (chosen < 0) return false;
    result->from = lineStart + chosen;
    result->to = lineStart + chosenEnd;
    result->prefix = unit->prefix;
    return true;
}
