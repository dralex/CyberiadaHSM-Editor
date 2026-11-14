/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The JSON texts: the strict check of a text tab
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

#include <QCoreApplication>
#include <QRegularExpression>
#include <QVector>

#include "json_text.h"

// the editor's own reading, not Qt's: QJsonDocument takes "\q", calls a missing comma an
// unterminated object and gives UTF-8 offsets (docs/MODES.md, JSON)

namespace {

enum JsonTokenKind {
    tokenBeginObject,
    tokenEndObject,
    tokenBeginArray,
    tokenEndArray,
    tokenColon,
    tokenComma,
    tokenString,   // in double quotes; not valid when broken
    tokenNumber,   // a run of the number characters; valid when it is a JSON number
    tokenWord,     // a run of letters; valid when it is a literal
    tokenQuoted,   // in single quotes, never valid
    tokenComment,  // "//" or "/*"
    tokenOther,    // any other character
    tokenEnd
};

struct JsonToken {
    JsonTokenKind kind = tokenEnd;
    int start = 0;
    int length = 0;
    bool valid = true;
    // the first fault of a broken string
    int errorStart = 0;
    int errorLength = 0;
    QString error;
};

bool isJsonSpace(QChar c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool isLineEnd(QChar c)
{
    return c == '\n' || c == '\r';
}

bool isHexDigit(QChar c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

class JsonLexer {
public:
    explicit JsonLexer(const QString& text): text(text), size(text.length())
    {
        // a byte order mark at the start is no text
        if (size > 0 && text[0] == QChar(0xFEFF)) pos = 1;
    }

    JsonToken next()
    {
        while (pos < size && isJsonSpace(text[pos])) pos++;
        JsonToken token;
        token.start = pos;
        if (pos >= size) return token;
        QChar c = text[pos];
        token.length = 1;
        switch (c.unicode()) {
        case '{': token.kind = tokenBeginObject; break;
        case '}': token.kind = tokenEndObject; break;
        case '[': token.kind = tokenBeginArray; break;
        case ']': token.kind = tokenEndArray; break;
        case ':': token.kind = tokenColon; break;
        case ',': token.kind = tokenComma; break;
        case '"': scanString(&token); break;
        case '\'': {
            token.kind = tokenQuoted;
            token.valid = false;
            int end = pos + 1;
            while (end < size && text[end] != '\'' && !isLineEnd(text[end])) end++;
            token.length = (end < size && text[end] == '\'' ? end + 1 : end) - pos;
            break;
        }
        case '/':
            if (pos + 1 < size && (text[pos + 1] == '/' || text[pos + 1] == '*')) {
                token.kind = tokenComment;
                token.length = 2;
            } else {
                token.kind = tokenOther;
            }
            break;
        default:
            if (c.isDigit() || c == '-' || c == '+' || c == '.') {
                static const QRegularExpression number(
                    "\\A-?(?:0|[1-9][0-9]*)(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?\\z");
                token.kind = tokenNumber;
                token.length = runEnd(true) - pos;
                token.valid = number.match(text.mid(pos, token.length)).hasMatch();
            } else if (c.isLetter() || c == '_') {
                token.kind = tokenWord;
                token.length = runEnd(false) - pos;
                QString word = text.mid(pos, token.length);
                token.valid = word == "true" || word == "false" || word == "null";
            } else {
                token.kind = tokenOther;
                if (c.isHighSurrogate() && pos + 1 < size) token.length = 2;
            }
            break;
        }
        pos += token.length;
        return token;
    }

private:
    // the end of a run of letters, digits and _, of a number with . + - too
    int runEnd(bool number) const
    {
        int end = pos + 1;
        while (end < size) {
            QChar c = text[end];
            if (!c.isLetterOrNumber() && c != '_' &&
                !(number && (c == '.' || c == '+' || c == '-'))) break;
            end++;
        }
        return end;
    }

    // the length of the escape at i; negative when it is invalid
    int escapeLength(int i) const
    {
        if (i + 1 >= size || isLineEnd(text[i + 1])) return -1;
        QChar e = text[i + 1];
        if (QString("\"\\/bfnrt").contains(e)) return 2;
        if (e != 'u') return -2;
        int digits = 0;
        while (digits < 4 && i + 2 + digits < size && isHexDigit(text[i + 2 + digits])) digits++;
        return digits == 4 ? 6 : -(2 + digits);
    }

    void fail(JsonToken* token, int start, int length, const QString& error) const
    {
        if (!token->valid) return;
        token->valid = false;
        token->errorStart = start;
        token->errorLength = length;
        token->error = error;
    }

    // up to the closing quote, else to the line end; the first fault is kept
    void scanString(JsonToken* token) const
    {
        token->kind = tokenString;
        int i = pos + 1;
        while (i < size && text[i] != '"' && !isLineEnd(text[i])) {
            if (text[i] == '\\') {
                int length = escapeLength(i);
                if (length < 0) {
                    fail(token, i, -length,
                         QCoreApplication::translate("JsonText", "invalid escape sequence"));
                }
                i += qAbs(length);
                continue;
            }
            if (text[i].unicode() < 0x20) {
                fail(token, i, 1, QCoreApplication::translate("JsonText", "control character in a string"));
            }
            i++;
        }
        if (i < size && text[i] == '"') {
            token->length = i + 1 - pos;
            return;
        }
        token->length = i - pos;
        // an unterminated string is the fault, whatever is inside
        token->valid = true;
        fail(token, pos, token->length, QCoreApplication::translate("JsonText", "unterminated string"));
    }

    const QString& text;
    int size;
    int pos = 0;
};

enum JsonExpect {
    expectValue,
    expectValueOrEnd,  // after [
    expectKeyOrEnd,    // after {
    expectKey,         // after a comma in an object
    expectColon,
    expectCommaOrEnd,
    expectNothing      // after the value of the text
};

struct JsonOpen {
    QChar bracket;
    int position;
};

JsonCheck failure(const QString& text, int position, int length, const QString& message)
{
    JsonCheck check;
    check.ok = false;
    check.position = position;
    check.length = qMax(1, length);
    check.line = text.leftRef(position).count('\n') + 1;
    int lineStart = position > 0 ? text.lastIndexOf('\n', position - 1) + 1 : 0;
    check.column = position - lineStart + 1;
    check.message = message;
    return check;
}

QString tokenText(const QString& text, const JsonToken& token)
{
    return text.mid(token.start, token.length);
}

bool startsValue(const JsonToken& token)
{
    switch (token.kind) {
    case tokenBeginObject:
    case tokenBeginArray:
    case tokenString:
    case tokenNumber:
    case tokenWord:
    case tokenQuoted:
        return true;
    default:
        return false;
    }
}

// a token where a value is due that is not one
JsonCheck valueFailure(const QString& text, const JsonToken& token)
{
    switch (token.kind) {
    case tokenString:
        return failure(text, token.errorStart, token.errorLength, token.error);
    case tokenNumber:
        return failure(text, token.start, token.length,
                       QCoreApplication::translate("JsonText", "invalid number"));
    case tokenWord:
        return failure(text, token.start, token.length,
                       QCoreApplication::translate("JsonText", "'%1' is not a JSON value")
                       .arg(tokenText(text, token)));
    case tokenQuoted:
        return failure(text, token.start, token.length,
                       QCoreApplication::translate("JsonText", "a string must be in double quotes"));
    case tokenComma:
    case tokenEndObject:
    case tokenEndArray:
        return failure(text, token.start, token.length,
                       QCoreApplication::translate("JsonText", "missing value"));
    default:
        return failure(text, token.start, token.length,
                       QCoreApplication::translate("JsonText", "unexpected '%1'")
                       .arg(tokenText(text, token)));
    }
}

} // namespace

JsonCheck checkJson(const QString& text)
{
    JsonLexer lexer(text);
    QVector<JsonOpen> open;
    JsonExpect expect = expectValue;
    // the comma before the token, -1 for none
    int comma = -1;
    for (;;) {
        JsonToken token = lexer.next();
        if (token.kind == tokenEnd) {
            if (open.isEmpty()) return JsonCheck();
            return failure(text, open.last().position, 1, open.last().bracket == '{' ?
                           QCoreApplication::translate("JsonText", "the object is not closed") :
                           QCoreApplication::translate("JsonText", "the array is not closed"));
        }
        if (token.kind == tokenComment) {
            return failure(text, token.start, token.length,
                           QCoreApplication::translate("JsonText", "comments are not allowed in JSON"));
        }
        int previousComma = comma;
        comma = -1;
        bool value = false;
        switch (expect) {
        case expectValue:
        case expectValueOrEnd:
            if (token.kind == tokenEndArray && expect == expectValueOrEnd) {
                open.removeLast();
                value = true;
            } else if (token.kind == tokenEndArray && previousComma >= 0) {
                return failure(text, previousComma, 1, QCoreApplication::translate("JsonText", "trailing comma"));
            } else if (token.kind == tokenBeginObject) {
                open.append(JsonOpen{'{', token.start});
                expect = expectKeyOrEnd;
            } else if (token.kind == tokenBeginArray) {
                open.append(JsonOpen{'[', token.start});
                expect = expectValueOrEnd;
            } else if (startsValue(token) && token.valid) {
                value = true;
            } else {
                return valueFailure(text, token);
            }
            break;
        case expectKeyOrEnd:
        case expectKey:
            if (token.kind == tokenEndObject) {
                if (previousComma >= 0) {
                    return failure(text, previousComma, 1, QCoreApplication::translate("JsonText", "trailing comma"));
                }
                open.removeLast();
                value = true;
            } else if (token.kind == tokenString) {
                if (!token.valid) return valueFailure(text, token);
                expect = expectColon;
            } else if (token.kind == tokenWord || token.kind == tokenNumber || token.kind == tokenQuoted) {
                return failure(text, token.start, token.length,
                               QCoreApplication::translate("JsonText", "a key must be a string in double quotes"));
            } else if (token.kind == tokenEndArray) {
                return failure(text, token.start, token.length,
                               QCoreApplication::translate("JsonText", "expected '%1'").arg('}'));
            } else {
                return failure(text, token.start, token.length,
                               QCoreApplication::translate("JsonText", "unexpected '%1'")
                               .arg(tokenText(text, token)));
            }
            break;
        case expectColon:
            if (token.kind != tokenColon) {
                return failure(text, token.start, token.length,
                               QCoreApplication::translate("JsonText", "missing colon"));
            }
            expect = expectValue;
            break;
        case expectCommaOrEnd: {
            QChar close = open.last().bracket == '{' ? '}' : ']';
            if (token.kind == tokenComma) {
                comma = token.start;
                expect = close == '}' ? expectKey : expectValue;
            } else if (token.kind == tokenEndObject || token.kind == tokenEndArray) {
                if (tokenText(text, token) != QString(close)) {
                    return failure(text, token.start, token.length,
                                   QCoreApplication::translate("JsonText", "expected '%1'").arg(close));
                }
                open.removeLast();
                value = true;
            } else if (startsValue(token)) {
                return failure(text, token.start, token.length,
                               QCoreApplication::translate("JsonText", "missing comma"));
            } else {
                return failure(text, token.start, token.length,
                               QCoreApplication::translate("JsonText", "unexpected '%1'")
                               .arg(tokenText(text, token)));
            }
            break;
        }
        case expectNothing:
            return failure(text, token.start, token.length,
                           QCoreApplication::translate("JsonText", "extra text after the value"));
        }
        if (value) expect = open.isEmpty() ? expectNothing : expectCommaOrEnd;
    }
}
