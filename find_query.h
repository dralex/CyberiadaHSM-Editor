/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The find query and the matching of a text
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

#ifndef CYBERIADA_FIND_QUERY
#define CYBERIADA_FIND_QUERY

#include <QRegularExpression>
#include <QString>
#include <QVector>

// what the find bar looks for (EDIT-FIND-2, docs/FIND.md)
struct FindQuery {
    QString text;
    bool    matchCase = false;
    bool    wholeWords = false;
    bool    regexp = false;

    bool    isEmpty() const { return text.isEmpty(); }
    // one expression of the text and the options; a bad regexp gives an invalid one
    QRegularExpression expression() const;
    bool    operator==(const FindQuery& other) const;
    bool    operator!=(const FindQuery& other) const { return !(*this == other); }
};

// a match [from, to) in the plain text of a QTextDocument
struct FindRange {
    int from;
    int to;
};
typedef QVector<FindRange> FindRanges;

// the non-empty matches of the expression in the text, at most limit; more is set
// when one is left beyond it
FindRanges findRanges(const QString& text, const QRegularExpression& expression, int limit,
                      bool* more = nullptr);

#endif
