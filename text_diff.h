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

#ifndef CYBERIADA_TEXT_DIFF
#define CYBERIADA_TEXT_DIFF

#include <QString>
#include <QStringList>
#include <QVector>

// docs/MODES.md, The text diff tab: the Myers line diff, the changed lines paired
// while similar, their changed characters extended to the words (EDIT-MODE-20..23)
namespace TextDiff {

struct Options {
    // compare the lines with the runs of spaces and tabs as one space, none at the ends
    bool ignoreWhitespace = false;
};

enum RowKind {
    rowEqual = 0,
    rowRemoved,     // a base line, a filler on the target
    rowAdded,       // a target line, a filler on the base
    rowChanged      // a base line and its similar target line
};

// the characters [from, to) of a line
struct Span {
    int from;
    int to;
};
typedef QVector<Span> Spans;

// a row of both sides: the line indexes from 0, -1 for a filler
struct Row {
    RowKind kind = rowEqual;
    int     base = -1;
    int     target = -1;
    Spans   baseSpans;      // a changed pair: its changed characters on each side
    Spans   targetSpans;
};

// a run of rows that are not equal
struct Hunk {
    int firstRow = 0;
    int lastRow = 0;        // past the last row
    int baseLine = 0;       // the first line of each side from 0; where it would stand
    int baseCount = 0;      // when the side has no line in the hunk
    int targetLine = 0;
    int targetCount = 0;
};

struct Result {
    QVector<Row>  rows;
    QVector<Hunk> hunks;
    int added = 0;          // the target lines without a pair
    int removed = 0;        // the base lines without a pair
    int changed = 0;        // the pairs
};

// the lines without their ends (LF, CRLF, CR); a final line end makes no empty line
QStringList splitLines(const QString& text);
// a line as Ignore Whitespace compares it
QString normalise(const QString& line);
// the similarity of two lines: twice their common characters by their lengths, 0..1
double similarity(const QString& a, const QString& b);
Result diffLines(const QStringList& base, const QStringList& target, const Options& options = Options());

}

#endif
