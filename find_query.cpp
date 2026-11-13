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

#include "find_query.h"

QRegularExpression FindQuery::expression() const
{
    QString pattern = regexp ? text : QRegularExpression::escape(text);
    // an end of the match never falls inside a word: not \b, which would refuse a
    // field starting or ending with a non-word character after or before a word
    if (wholeWords) {
        const QString edge = QStringLiteral("(?:(?<!\\w)|(?!\\w))");
        pattern = edge + QStringLiteral("(?:") + pattern + QStringLiteral(")") + edge;
    }
    // the Unicode properties make the case and the word characters of any script count
    QRegularExpression::PatternOptions options = QRegularExpression::UseUnicodePropertiesOption;
    if (!matchCase) options |= QRegularExpression::CaseInsensitiveOption;
    return QRegularExpression(pattern, options);
}

bool FindQuery::operator==(const FindQuery& other) const
{
    return text == other.text && matchCase == other.matchCase &&
        wholeWords == other.wholeWords && regexp == other.regexp;
}

FindRanges findRanges(const QString& text, const QRegularExpression& expression, int limit,
                      bool* more)
{
    FindRanges ranges;
    if (more) *more = false;
    if (!expression.isValid() || expression.pattern().isEmpty()) return ranges;
    QRegularExpressionMatchIterator i = expression.globalMatch(text);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        // an empty match marks nothing (EDIT-FIND-2)
        if (match.capturedLength() == 0) continue;
        if (ranges.size() >= limit) {
            if (more) *more = true;
            break;
        }
        FindRange range = { match.capturedStart(), match.capturedEnd() };
        ranges.append(range);
    }
    return ranges;
}
