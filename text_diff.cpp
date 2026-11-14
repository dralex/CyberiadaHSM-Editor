/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The line diff of two texts for a text diff tab
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

#include <QHash>
#include <vector>

#include "text_diff.h"

using namespace TextDiff;

namespace {

// a changed base line pairs with a target line this similar (EDIT-MODE-20)
const double PAIR_SIMILARITY = 0.5;
// the target lines a changed base line looks at for its pair
const int PAIR_LOOKAHEAD = 8;
// the steps of one comparison; the parts left after it are replaced whole, so a
// large text of no common lines does not stall the window
const long long WORK_BUDGET = 20000000;

// the Myers difference in linear space (the middle snake), as git diff by default:
// marks the removed items of a and the inserted ones of b
class Myers {
public:
    Myers(const QVector<int>& first, const QVector<int>& second):
        a(first), b(second), removed(size_t(first.size()), 0), inserted(size_t(second.size()), 0) {}

    void run() { compare(0, a.size(), 0, b.size()); }

    const QVector<int>& a;
    const QVector<int>& b;
    std::vector<char> removed;
    std::vector<char> inserted;

private:
    long long work = 0;

    void replace(int aLo, int aHi, int bLo, int bHi)
    {
        for (int i = aLo; i < aHi; i++) removed[size_t(i)] = 1;
        for (int j = bLo; j < bHi; j++) inserted[size_t(j)] = 1;
    }

    void compare(int aLo, int aHi, int bLo, int bHi)
    {
        while (aLo < aHi && bLo < bHi && a[aLo] == b[bLo]) {
            aLo++;
            bLo++;
        }
        while (aLo < aHi && bLo < bHi && a[aHi - 1] == b[bHi - 1]) {
            aHi--;
            bHi--;
        }
        int x = 0, y = 0;
        if (aLo == aHi || bLo == bHi || !bisect(aLo, aHi, bLo, bHi, &x, &y) ||
            (x == aLo && y == bLo) || (x == aHi && y == bHi)) {
            replace(aLo, aHi, bLo, bHi);
            return;
        }
        compare(aLo, x, bLo, y);
        compare(x, aHi, y, bHi);
    }

    // the point where the forward and the backward d-paths meet; false when the
    // parts have nothing in common or the budget is spent
    bool bisect(int aLo, int aHi, int bLo, int bHi, int* xSplit, int* ySplit)
    {
        const int n = aHi - aLo;
        const int m = bHi - bLo;
        const int maxD = (n + m + 1) / 2;
        const int offset = maxD;
        const int length = 2 * maxD + 2;
        std::vector<int> forward(size_t(length), -1);
        std::vector<int> backward(size_t(length), -1);
        forward[size_t(offset + 1)] = 0;
        backward[size_t(offset + 1)] = 0;
        const int delta = n - m;
        // the forward path meets the backward one on odd deltas, else the backward one
        const bool front = (delta % 2 != 0);
        int k1start = 0, k1end = 0, k2start = 0, k2end = 0;
        for (int d = 0; d < maxD; d++) {
            if (work > WORK_BUDGET) return false;
            for (int k1 = -d + k1start; k1 <= d - k1end; k1 += 2) {
                const int k1offset = offset + k1;
                int x1;
                if (k1 == -d || (k1 != d && forward[size_t(k1offset - 1)] < forward[size_t(k1offset + 1)])) {
                    x1 = forward[size_t(k1offset + 1)];
                } else {
                    x1 = forward[size_t(k1offset - 1)] + 1;
                }
                int y1 = x1 - k1;
                while (x1 < n && y1 < m && a[aLo + x1] == b[bLo + y1]) {
                    x1++;
                    y1++;
                }
                work += 1 + x1 - forward[size_t(k1offset)];
                forward[size_t(k1offset)] = x1;
                if (x1 > n) {
                    k1end += 2;
                } else if (y1 > m) {
                    k1start += 2;
                } else if (front) {
                    const int k2offset = offset + delta - k1;
                    if (k2offset >= 0 && k2offset < length && backward[size_t(k2offset)] != -1) {
                        if (x1 >= n - backward[size_t(k2offset)]) {
                            *xSplit = aLo + x1;
                            *ySplit = bLo + y1;
                            return true;
                        }
                    }
                }
            }
            for (int k2 = -d + k2start; k2 <= d - k2end; k2 += 2) {
                const int k2offset = offset + k2;
                int x2;
                if (k2 == -d || (k2 != d && backward[size_t(k2offset - 1)] < backward[size_t(k2offset + 1)])) {
                    x2 = backward[size_t(k2offset + 1)];
                } else {
                    x2 = backward[size_t(k2offset - 1)] + 1;
                }
                int y2 = x2 - k2;
                while (x2 < n && y2 < m && a[aHi - x2 - 1] == b[bHi - y2 - 1]) {
                    x2++;
                    y2++;
                }
                work += 1 + x2 - backward[size_t(k2offset)];
                backward[size_t(k2offset)] = x2;
                if (x2 > n) {
                    k2end += 2;
                } else if (y2 > m) {
                    k2start += 2;
                } else if (!front) {
                    const int k1offset = offset + delta - k2;
                    if (k1offset >= 0 && k1offset < length && forward[size_t(k1offset)] != -1) {
                        const int x1 = forward[size_t(k1offset)];
                        const int y1 = offset + x1 - k1offset;
                        if (x1 >= n - x2) {
                            *xSplit = aLo + x1;
                            *ySplit = bLo + y1;
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }
};

QVector<int> characters(const QString& line)
{
    QVector<int> codes;
    codes.reserve(line.size());
    for (const QChar& c : line) codes << c.unicode();
    return codes;
}

bool isWordChar(const QChar& c)
{
    return c.isLetterOrNumber() || c == '_';
}

// the marked characters as spans, each grown to the words it touches, merged
Spans spansOf(const QString& line, const std::vector<char>& marks, bool ignoreWhitespace)
{
    Spans spans;
    const int size = line.size();
    for (int i = 0; i < size; i++) {
        if (!marks[size_t(i)] || (ignoreWhitespace && line[i].isSpace())) continue;
        int from = i;
        int to = i + 1;
        while (to < size && marks[size_t(to)] && !(ignoreWhitespace && line[to].isSpace())) to++;
        while (from > 0 && isWordChar(line[from]) && isWordChar(line[from - 1])) from--;
        while (to < size && isWordChar(line[to - 1]) && isWordChar(line[to])) to++;
        if (!spans.isEmpty() && from <= spans.last().to) {
            spans.last().to = qMax(spans.last().to, to);
        } else {
            Span span;
            span.from = from;
            span.to = to;
            spans << span;
        }
        i = to - 1;
    }
    return spans;
}

void changedSpans(const QString& base, const QString& target, bool ignoreWhitespace, Row* row)
{
    QVector<int> a = characters(base);
    QVector<int> b = characters(target);
    Myers diff(a, b);
    diff.run();
    row->baseSpans = spansOf(base, diff.removed, ignoreWhitespace);
    row->targetSpans = spansOf(target, diff.inserted, ignoreWhitespace);
}

Row makeRow(RowKind kind, int base, int target)
{
    Row row;
    row.kind = kind;
    row.base = base;
    row.target = target;
    return row;
}

}

QStringList TextDiff::splitLines(const QString& text)
{
    QStringList lines;
    if (text.isEmpty()) return lines;
    int start = 0;
    const int size = text.size();
    for (int i = 0; i < size; i++) {
        if (text[i] != '\n' && text[i] != '\r') continue;
        lines << text.mid(start, i - start);
        if (text[i] == '\r' && i + 1 < size && text[i + 1] == '\n') i++;
        start = i + 1;
    }
    if (start < size) lines << text.mid(start);
    return lines;
}

QString TextDiff::normalise(const QString& line)
{
    QString result;
    result.reserve(line.size());
    bool space = false;
    for (const QChar& c : line) {
        if (c == ' ' || c == '\t') {
            space = true;
            continue;
        }
        if (space && !result.isEmpty()) result += ' ';
        space = false;
        result += c;
    }
    return result;
}

double TextDiff::similarity(const QString& a, const QString& b)
{
    if (a.isEmpty() && b.isEmpty()) return 1.0;
    QVector<int> first = characters(a);
    QVector<int> second = characters(b);
    Myers diff(first, second);
    diff.run();
    int common = 0;
    for (char removed : diff.removed) {
        if (!removed) common++;
    }
    return 2.0 * common / (a.size() + b.size());
}

Result TextDiff::diffLines(const QStringList& base, const QStringList& target, const Options& options)
{
    // the lines as numbers, equal lines one number
    QHash<QString, int> ids;
    QStringList baseCompared, targetCompared;
    auto number = [&ids](const QString& line) {
        auto it = ids.constFind(line);
        if (it != ids.constEnd()) return it.value();
        int id = ids.size();
        ids.insert(line, id);
        return id;
    };
    QVector<int> a, b;
    for (const QString& line : base) {
        baseCompared << (options.ignoreWhitespace ? normalise(line) : line);
        a << number(baseCompared.last());
    }
    for (const QString& line : target) {
        targetCompared << (options.ignoreWhitespace ? normalise(line) : line);
        b << number(targetCompared.last());
    }
    Myers lines(a, b);
    lines.run();

    Result result;
    int i = 0, j = 0;
    while (i < a.size() || j < b.size()) {
        QVector<int> removed, inserted;
        // past the end of one side the other's lines are changes whatever the marks
        while (i < a.size() && (lines.removed[size_t(i)] || j >= b.size())) removed << i++;
        while (j < b.size() && (lines.inserted[size_t(j)] || i >= a.size())) inserted << j++;
        // a run of changes: a base line takes the first similar target line ahead
        int next = 0;
        for (int line : removed) {
            int found = -1;
            for (int m = next; m < inserted.size() && m < next + PAIR_LOOKAHEAD; m++) {
                if (similarity(baseCompared[line], targetCompared[inserted[m]]) >= PAIR_SIMILARITY) {
                    found = m;
                    break;
                }
            }
            if (found < 0) {
                result.rows << makeRow(rowRemoved, line, -1);
                result.removed++;
                continue;
            }
            for (; next < found; next++) {
                result.rows << makeRow(rowAdded, -1, inserted[next]);
                result.added++;
            }
            Row row = makeRow(rowChanged, line, inserted[found]);
            changedSpans(base[line], target[inserted[found]], options.ignoreWhitespace, &row);
            result.rows << row;
            result.changed++;
            next = found + 1;
        }
        for (; next < inserted.size(); next++) {
            result.rows << makeRow(rowAdded, -1, inserted[next]);
            result.added++;
        }
        if (i < a.size() && j < b.size()) result.rows << makeRow(rowEqual, i++, j++);
    }

    // the hunks: the runs of rows that are not equal, with the lines of each side
    int baseNext = 0, targetNext = 0;
    for (int r = 0; r < result.rows.size(); r++) {
        const Row& row = result.rows[r];
        if (row.kind != rowEqual && (result.hunks.isEmpty() || result.hunks.last().lastRow != r)) {
            Hunk hunk;
            hunk.firstRow = r;
            hunk.lastRow = r;
            hunk.baseLine = baseNext;
            hunk.targetLine = targetNext;
            result.hunks << hunk;
        }
        if (row.kind != rowEqual) {
            Hunk& hunk = result.hunks.last();
            hunk.lastRow = r + 1;
            if (row.base >= 0) hunk.baseCount++;
            if (row.target >= 0) hunk.targetCount++;
        }
        if (row.base >= 0) baseNext = row.base + 1;
        if (row.target >= 0) targetNext = row.target + 1;
    }
    return result;
}
