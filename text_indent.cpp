/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The indentation of a source text: the columns of its lines and the edits
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
#include <climits>
#include <QStringList>
#include <algorithm>

#include "code_highlighter.h"
#include "cyberiada_constants.h"
#include "markdown_lines.h"
#include "text_indent.h"

namespace {

// a string as the scanner sees it: no bracket, no sign
const QChar MASK_STRING('x');
const QChar MASK_BLANK(' ');
// the length of a triple quote; the longest delimiter of a raw string of C++
const int TRIPLE = 3;
const int RAW_DELIMITER_MAX = 16;

// the words of a header waiting for its (...), of a statement whose next one is its body
const QStringList HEADER_WORDS = { "if", "for", "while", "switch", "catch", "foreach", "using", "lock" };
const QStringList BODY_WORDS = { "else", "do", "try", "finally" };
// Python: the statements after which the block ends, the words of a block's header
const QStringList DEDENT_AFTER = { "return", "pass", "break", "continue", "raise" };
const QStringList BLOCK_WORDS = { "def", "class", "if", "elif", "while", "for", "with", "async", "except" };

// a Python dedenter and the words of the lines it lines up with
QStringList dedenterOpeners(const QString& word)
{
    if (word == "else") return { "if", "elif", "for", "while", "try", "except" };
    if (word == "elif") return { "if", "elif" };
    if (word == "except") return { "try", "except" };
    if (word == "finally") return { "try", "except", "else" };
    return QStringList();
}

bool isWordChar(QChar c)
{
    return c.isLetterOrNumber() || c == '_';
}

bool isBlank(QChar c)
{
    return c == ' ' || c == '\t';
}

int leadLength(const QString& s)
{
    int i = 0;
    while (i < s.length() && isBlank(s[i])) i++;
    return i;
}

// the first word of a text from a place, empty when it starts with no letter
QString wordAt(const QString& s, int from)
{
    if (from < 0 || from >= s.length() || !(s[from].isLetter() || s[from] == '_')) return QString();
    int end = from;
    while (end < s.length() && isWordChar(s[end])) end++;
    return s.mid(from, end - from);
}

QChar closerOf(QChar open)
{
    if (open == '(') return ')';
    if (open == '[') return ']';
    return '}';
}

// a colon not part of ::
bool singleColon(const QString& s, int i)
{
    return s[i] == ':' && !(i > 0 && s[i - 1] == ':') && !(i + 1 < s.length() && s[i + 1] == ':');
}

// the state of the lexer from a line to the next
struct LexState {
    bool    block = false;
    QChar   triple;
    // a raw string of C++ until its )delimiter", a verbatim string of C# until a single quote
    QString raw;
    bool    verbatim = false;
    bool    directive = false;
    bool    inString() const { return !triple.isNull() || !raw.isEmpty() || verbatim; }
};

// a line as the scanner reads it
struct LineInfo {
    // the comments and the header blank, the strings masked
    QString      code;
    // the column before each character of the line as written, and at its end
    QVector<int> columns;
    int          indent = 0;
    int          first = -1;
    // the column of the first comment of the line, -1 without one
    int          comment = -1;
    // it starts in a block comment, a string, a continued directive or the header: it stays
    bool         kept = false;
    bool         directive = false;
    bool         backslash = false;
    // it ends in a string
    bool         open = false;
};

LineInfo lexLine(const QString& raw, int lineStart, const TextIndentRules& rules, LexState& st)
{
    LineInfo info;
    int n = raw.length();
    info.code = raw;
    info.columns.resize(n + 1);
    int column = 0;
    for (int i = 0; i < n; i++) {
        info.columns[i] = column;
        QChar c = raw[i];
        if (c == '\t') column = (column / EDITOR_TAB_SPACES + 1) * EDITOR_TAB_SPACES;
        else if (!c.isLowSurrogate() && !c.isMark()) column++;
    }
    info.columns[n] = column;
    info.indent = info.columns[leadLength(raw)];
    bool header = lineStart < rules.codeStart;
    bool continued = st.directive;
    info.kept = st.block || st.inString() || continued || header;
    auto mask = [&info](int from, int to, QChar with) {
        for (int k = from; k < to; k++) {
            if (!isBlank(info.code[k])) info.code[k] = with;
        }
    };
    int i = 0;
    if (header) {
        i = qMin(n, rules.codeStart - lineStart);
        mask(0, i, MASK_BLANK);
    }
    while (i < n) {
        if (st.block) {
            int close = raw.indexOf(rules.blockClose, i);
            int end = close < 0 ? n : close + rules.blockClose.length();
            mask(i, end, MASK_BLANK);
            if (close >= 0) st.block = false;
            i = end;
            continue;
        }
        if (!st.raw.isEmpty()) {
            int close = raw.indexOf(st.raw, i);
            int end = close < 0 ? n : close + st.raw.length();
            mask(i, end, MASK_STRING);
            if (close >= 0) st.raw.clear();
            i = end;
            continue;
        }
        if (st.verbatim) {
            int j = i;
            while (j < n) {
                if (raw[j] == '"' && j + 1 < n && raw[j + 1] == '"') {
                    j += 2;
                    continue;
                }
                if (raw[j] == '"') break;
                j++;
            }
            int end = qMin(j + 1, n);
            mask(i, end, MASK_STRING);
            if (j < n) st.verbatim = false;
            i = end;
            continue;
        }
        if (!st.triple.isNull()) {
            QString close(TRIPLE, st.triple);
            int j = i;
            bool found = false;
            while (j < n) {
                if (raw[j] == '\\') {
                    j += 2;
                    continue;
                }
                if (raw.midRef(j, TRIPLE) == close) {
                    j += TRIPLE;
                    found = true;
                    break;
                }
                j++;
            }
            j = qMin(j, n);
            mask(i, j, MASK_STRING);
            if (found) st.triple = QChar();
            i = j;
            continue;
        }
        QChar c = raw[i];
        if (!rules.lineComment.isEmpty() && raw.midRef(i).startsWith(rules.lineComment)) {
            if (info.comment < 0) info.comment = info.columns[i];
            mask(i, n, MASK_BLANK);
            break;
        }
        if (!rules.blockOpen.isEmpty() && !rules.blockClose.isEmpty() && raw.midRef(i).startsWith(rules.blockOpen)) {
            if (info.comment < 0) info.comment = info.columns[i];
            mask(i, i + rules.blockOpen.length(), MASK_BLANK);
            i += rules.blockOpen.length();
            st.block = true;
            continue;
        }
        if (c == '"' && rules.quotes.contains(c) && rules.style == textIndentBraces && i > 0) {
            // R"delimiter( ... )delimiter" of C++, @" ... " of C#, over the lines
            int paren = raw.indexOf('(', i + 1);
            if (raw[i - 1] == 'R' && paren > i && paren - i - 1 <= RAW_DELIMITER_MAX) {
                st.raw = ")" + raw.mid(i + 1, paren - i - 1) + "\"";
                mask(i, paren + 1, MASK_STRING);
                i = paren + 1;
                continue;
            }
            if (raw[i - 1] == '@') {
                mask(i, i + 1, MASK_STRING);
                st.verbatim = true;
                i++;
                continue;
            }
        }
        if (rules.quotes.contains(c)) {
            if (rules.style == textIndentOffside && raw.midRef(i, TRIPLE) == QString(TRIPLE, c)) {
                mask(i, i + TRIPLE, MASK_STRING);
                i += TRIPLE;
                st.triple = c;
                continue;
            }
            // to the same quote or the line end, as the highlighter reads it
            int j = i + 1;
            while (j < n && raw[j] != c) {
                if (raw[j] == '\\') j++;
                j++;
            }
            j = qMin(j + 1, n);
            mask(i, j, MASK_STRING);
            i = j;
            continue;
        }
        i++;
    }
    for (int k = 0; k < n; k++) {
        if (!info.code[k].isSpace()) {
            info.first = k;
            break;
        }
    }
    info.directive = continued || (rules.preprocessor && info.first >= 0 && info.code[info.first] == '#');
    st.directive = info.directive && raw.endsWith('\\');
    int last = info.code.length() - 1;
    while (last >= 0 && info.code[last].isSpace()) last--;
    info.backslash = last >= 0 && info.code[last] == '\\';
    info.open = st.inString();
    return info;
}

// an open bracket and, for a block of statements, the statement in progress
struct Frame {
    // null for the root
    QChar   open;
    int     column = 0;
    int     lineIndent = 0;
    // the column of the code after the opener on its line, -1 when the line ends there
    int     align = -1;
    bool    block = false;
    bool    list = false;
    bool    flat = false;
    int     base = 0;
    bool    inStatement = false;
    int     statementIndent = 0;
    bool    sub = false;
    bool    header = false;
    bool    label = false;
    bool    assign = false;
    // a ( read in the statement at this level
    bool    call = false;
    QString word;
    int     statementLine = -1;
    // the columns a continued line lines up with, -1 for none (as the line-up functions of
    // Emacs): the code after the first assignment and after return on their line, the last ?,
    // the first << or >>, the start of the argument in progress
    int     assignAt = -1;
    int     returnAt = -1;
    int     question = -1;
    int     stream = -1;
    int     argument = -1;
    // the value of the keyword argument in progress, name=value
    int     value = -1;
    // a bracket ending the header line of a Python block: its lines go two widths in (PEP 8)
    bool    wide = false;
    // the template arguments open on the line: their commas part no arguments
    int     angle = 0;
    // the last code character at this level
    QChar   last;
};

// a Python statement read
struct Logical {
    int     indent = 0;
    QString word;
    QChar   last;
};

// the state from the start of the text to a line: the column of the line, then its characters
class Scanner {
public:
    Scanner(const TextIndentRules& rules, int width): m_rules(rules), m_width(width)
    {
        Frame root;
        root.block = rules.style == textIndentBraces;
        m_frames.append(root);
    }

    int column(const LineInfo& line) const
    {
        if (line.kept) return -1;
        if (line.directive) return m_rules.margin;
        // a comment under the comment after the code above stays with it (c-lineup-comment)
        if (line.first < 0 && line.comment >= 0 && line.comment == m_trailing) return line.comment;
        QChar c = line.first >= 0 ? line.code[line.first] : QChar();
        const Frame& f = m_frames.last();
        if (m_rules.style == textIndentOffside) return offsideColumn(line, c);
        if (!f.open.isNull() && (!f.block || f.align >= 0)) return bracketColumn(f, line, c);
        if (m_rules.style != textIndentBraces) return m_rules.margin;
        return blockColumn(line, c);
    }

    QVector<int> levels(const LineInfo& line) const
    {
        int col = column(line);
        if (col < 0) return QVector<int>();
        QVector<int> result { col };
        if (m_rules.style != textIndentOffside || m_frames.size() > 1 || m_continues) return result;
        // a dedenter at each opener it may close, the body of a block at its column alone
        QStringList openers = dedenterOpeners(wordAt(line.code, line.first));
        if (!openers.isEmpty()) {
            QVector<int> closed = dedenterLevels(openers);
            return closed.isEmpty() ? result : closed;
        }
        if (!m_done.isEmpty() && m_done.last().last == ':') return result;
        return lower(result, m_done.size(), [this](int k) { return m_done[k].indent; });
    }

    QVector<int> cycle(const LineInfo& line) const
    {
        int col = column(line);
        if (col < 0) return QVector<int>();
        if (m_rules.style == textIndentOffside && m_frames.size() == 1 && !m_continues) {
            QVector<int> closed = dedenterLevels(dedenterOpeners(wordAt(line.code, line.first)));
            if (!closed.isEmpty()) return closed;
        }
        return lower(QVector<int>({ col }), m_indents.size(), [this](int k) { return m_indents[k]; });
    }

    void advance(const LineInfo& line)
    {
        m_trailing = line.first >= 0 && line.comment > line.columns[line.first] ? line.comment : -1;
        if (line.first >= 0 && !line.kept && !line.directive) m_indents.append(line.indent);
        if (line.directive) return;
        if (m_rules.style == textIndentOffside) {
            advanceOffside(line);
            return;
        }
        const QString& code = line.code;
        int n = code.length();
        bool firstToken = true;
        for (int i = 0; i < n; i++) {
            QChar ch = code[i];
            if (ch.isSpace()) continue;
            int end = i + 1;
            if (isWordChar(ch)) {
                while (end < n && isWordChar(code[end])) end++;
            }
            bool shift = (ch == '<' || ch == '>') && end < n && code[end] == ch && !(end + 1 < n && code[end + 1] == '=');
            if (shift) end++;
            QString word = wordAt(code, i);
            if (m_frames.last().block) statementToken(word, ch, line, i, firstToken);
            firstToken = false;
            lineUp(ch, line, i, shift);
            if (ch == '(' || ch == '[' || ch == '{') {
                m_frames.last().last = ch;
                if (ch == '(') m_frames.last().call = true;
                push(ch, line, i);
            } else if (ch == ')' || ch == ']' || ch == '}') {
                close(ch);
                m_frames.last().last = ch;
            } else {
                Frame& f = m_frames.last();
                if (f.block) {
                    if (ch == ';' || (ch == ',' && f.list) || (f.label && singleColon(code, i))) endStatement(f);
                    else if (ch == '=') {
                        if (!f.assign && assignment(code, i)) f.assignAt = codeAfter(line, i + 1);
                        f.assign = true;
                    }
                }
                f.last = code[end - 1];
            }
            m_before = end - i > 1 ? code[end - 2] : m_last;
            m_last = code[end - 1];
            i = end - 1;
        }
        for (Frame& open : m_frames) open.angle = 0;
        // a macro without its semicolon, Q_OBJECT, is a statement of its own
        Frame& f = m_frames.last();
        if (f.block && f.inStatement && f.statementLine == m_line && macroLine(line)) endStatement(f);
        m_line++;
    }

private:
    // the levels after the last of result: each smaller indent of count indents, the last one
    // nearest, the left edge at the end
    template <typename Indent>
    QVector<int> lower(QVector<int> result, int count, Indent indent) const
    {
        int edge = m_rules.margin;
        int last = result.last();
        for (int k = count - 1; k >= 0 && last > edge; k--) {
            int at = indent(k);
            if (at < last && at >= edge) {
                last = at;
                result.append(last);
            }
        }
        if (last > edge) result.append(edge);
        return result;
    }

    int bracketColumn(const Frame& f, const LineInfo& line, QChar c) const
    {
        if (c == closerOf(f.open)) return f.align >= 0 ? f.column : f.lineIndent;
        int lined = linedUp(f, line);
        if (lined >= 0) return lined;
        // an argument continued, under its start; the next one, the next part of a for or a call
        // cascaded, .arg(), under the first
        bool continued = !f.last.isNull() && f.last != ',' && f.last != ';' && !cascadeLine(line);
        // Python lines up with the opening delimiter (PEP 8), a keyword argument under its value
        if (continued && f.value >= 0) return f.value;
        if (continued && f.argument >= 0 && m_rules.style != textIndentOffside) return f.argument;
        return f.align >= 0 ? f.align : f.lineIndent + (f.wide ? 2 * m_width : m_width);
    }

    static bool cascadeLine(const LineInfo& line)
    {
        if (line.first < 0) return false;
        QChar c = line.code[line.first];
        return c == '.' || (c == '-' && line.first + 1 < line.code.length() && line.code[line.first + 1] == '>');
    }

    // a : under the ?, a << under the first <<
    int linedUp(const Frame& f, const LineInfo& line) const
    {
        if (line.first < 0) return -1;
        const QString& code = line.code;
        QChar c = code[line.first];
        if (f.question >= 0 && singleColon(code, line.first)) return f.question;
        bool shift = (c == '<' || c == '>') && line.first + 1 < code.length() && code[line.first + 1] == c;
        if (f.stream >= 0 && shift) return f.stream;
        return -1;
    }

    // the columns of this level a later line may line up with
    void lineUp(QChar ch, const LineInfo& line, int i, bool shift)
    {
        Frame& f = m_frames.last();
        // the < after a name opens template arguments, a > closes them, -> aside
        if (ch == '<' && !shift && i > 0 && isWordChar(line.code[i - 1])) f.angle++;
        else if (ch == '>' && !shift && f.angle > 0 && !(i > 0 && line.code[i - 1] == '-')) f.angle--;
        if ((ch == ',' && f.angle == 0) || (ch == ';' && !f.block)) {
            f.argument = -1;
            f.value = -1;
            if (!f.block) {
                f.question = -1;
                f.stream = -1;
            }
            return;
        }
        if (f.argument < 0 && ch != closerOf(f.open)) f.argument = line.columns[i];
        if (ch == '?') f.question = line.columns[i];
        else if (shift && f.stream < 0) f.stream = line.columns[i];
        else if (ch == '=' && !f.block && f.value < 0 && assignment(line.code, i)) f.value = codeAfter(line, i + 1);
    }

    // the column of the code after a place of the line, -1 when the line ends
    static int codeAfter(const LineInfo& line, int from)
    {
        for (int k = from; k < line.code.length(); k++) {
            if (!line.code[k].isSpace()) return line.columns[k];
        }
        return -1;
    }

    // an = that assigns: not ==, !=, <=, >=, =>
    static bool assignment(const QString& code, int i)
    {
        QChar before = i > 0 ? code[i - 1] : QChar(' ');
        QChar after = i + 1 < code.length() ? code[i + 1] : QChar(' ');
        return after != '=' && after != '>' && !QString("=!<>").contains(before);
    }

    static bool macroLine(const LineInfo& line)
    {
        static const QRegularExpression MACRO(R"(^\s*[A-Z_][A-Z0-9_]*\s*(\(.*\))?\s*$)");
        return MACRO.match(line.code).hasMatch();
    }

    // a line of a block of statements, the root among them
    int blockColumn(const LineInfo& line, QChar c) const
    {
        const Frame& f = m_frames.last();
        bool root = f.open.isNull();
        int statements = root ? m_rules.margin : (f.flat ? f.base : f.base + m_width);
        if (!root && c == '}') return f.base;
        if (!f.inStatement) return labelLine(line) ? (root ? m_rules.margin : f.base) : statements;
        // a substatement or a statement continued; its block brace at its first line
        if (c == '{') return f.statementIndent;
        // a call cascaded, .arg() or ->next(), goes a width in from the statement
        bool cascade = cascadeLine(line);
        if (!f.sub && !cascade) {
            int lined = linedUp(f, line);
            if (lined >= 0) return lined;
            if (f.assignAt >= 0) return f.assignAt;
            if (f.returnAt >= 0) return f.returnAt;
            // a declaration of the top level over its lines, a return type above its function;
            // the declaration under a template header, in a class too
            if (root && !f.assign && !f.call) return f.statementIndent;
            if (f.word == "template" && f.last == '>') return f.statementIndent;
        }
        return f.statementIndent + m_width;
    }

    bool labelLine(const LineInfo& line) const
    {
        QString word = wordAt(line.code, line.first);
        if (word.isEmpty() || !m_rules.labels.contains(word)) return false;
        for (int k = line.first + word.length(); k < line.code.length(); k++) {
            QChar c = line.code[k];
            if (c == ';' || c == '{') return false;
            if (singleColon(line.code, k)) return true;
        }
        return false;
    }

    int offsideColumn(const LineInfo& line, QChar c) const
    {
        const Frame& f = m_frames.last();
        if (!f.open.isNull()) return bracketColumn(f, line, c);
        // after a backslash; a block header two widths in, apart from its body (PEP 8)
        if (m_continues) return m_open.indent + (BLOCK_WORDS.contains(m_open.word) ? 2 * m_width : m_width);
        int col = m_rules.margin;
        if (!m_done.isEmpty()) {
            const Logical& p = m_done.last();
            if (p.last == ':') {
                col = p.indent + m_width;
            } else if (DEDENT_AFTER.contains(p.word)) {
                col = m_rules.margin;
                for (int k = m_done.size() - 2; k >= 0; k--) {
                    if (m_done[k].indent < p.indent) {
                        col = m_done[k].indent;
                        break;
                    }
                }
            } else {
                col = p.indent;
            }
        }
        QStringList openers = dedenterOpeners(wordAt(line.code, line.first));
        if (openers.isEmpty()) return col;
        QVector<int> closed = dedenterLevels(openers);
        return closed.isEmpty() ? col : closed.first();
    }

    // the levels of the blocks around the line whose statement a dedenter may follow: the
    // nearest line at each lower level above, when it is one of the openers; nearest first
    QVector<int> dedenterLevels(const QStringList& openers) const
    {
        QVector<int> levels;
        int last = INT_MAX;
        for (int k = m_done.size() - 1; k >= 0 && last > m_rules.margin; k--) {
            if (m_done[k].indent >= last) continue;
            last = m_done[k].indent;
            if (openers.contains(m_done[k].word)) levels.append(last);
        }
        return levels;
    }

    // a token of a block of statements: one may start a statement or the body of a header
    void statementToken(const QString& word, QChar ch, const LineInfo& line, int at, bool firstToken)
    {
        Frame& f = m_frames.last();
        if (ch == '}' || ch == ')' || ch == ']') return;
        if (!f.inStatement) {
            endStatement(f);
            f.inStatement = true;
            f.statementIndent = line.indent;
            f.statementLine = m_line;
            f.label = m_rules.labels.contains(word);
        } else if (f.sub && ch != '{') {
            // the body of a header: a statement of its own, on its line or after the header
            endStatement(f);
            f.inStatement = true;
            if (firstToken) f.statementIndent = line.indent;
        } else {
            return;
        }
        f.word = word;
        f.header = HEADER_WORDS.contains(word);
        f.sub = BODY_WORDS.contains(word);
        if (word == "return") f.returnAt = codeAfter(line, at + word.length());
    }

    void endStatement(Frame& f)
    {
        f.inStatement = false;
        f.sub = false;
        f.header = false;
        f.label = false;
        f.assign = false;
        f.call = false;
        f.assignAt = -1;
        f.returnAt = -1;
        f.question = -1;
        f.stream = -1;
    }

    void push(QChar open, const LineInfo& line, int at)
    {
        Frame parent = m_frames.last();
        Frame f;
        f.open = open;
        f.column = line.columns[at];
        f.lineIndent = line.indent;
        for (int k = at + 1; k < line.code.length(); k++) {
            if (!line.code[k].isSpace()) {
                f.align = line.columns[k];
                break;
            }
        }
        if (open == '{' && m_rules.style == textIndentBraces) {
            f.block = true;
            // a function, a lambda, a control block; else a list of values
            bool body = m_last == ')' || (m_last == '>' && m_before == '=');
            if (!parent.block) {
                f.list = !body;
            } else {
                f.list = !body && (parent.assign || parent.word == "enum" || parent.word == "return" ||
                                   m_last == ',' || m_last == '=');
            }
            f.flat = parent.block && m_rules.flat.contains(parent.word);
            f.base = parent.block && parent.inStatement ? parent.statementIndent : line.indent;
            Frame& p = m_frames.last();
            p.sub = false;
            p.header = false;
        }
        m_frames.append(f);
    }

    void close(QChar closer)
    {
        QChar open = closer == ')' ? QChar('(') : closer == ']' ? QChar('[') : QChar('{');
        int k = m_frames.size() - 1;
        while (k > 0 && m_frames[k].open != open) k--;
        if (k == 0) return;
        m_frames.resize(k);
        Frame& top = m_frames.last();
        if (!top.block) return;
        if (open == '{') endStatement(top);
        else if (top.header) {
            top.header = false;
            top.sub = true;
        }
    }

    void advanceOffside(const LineInfo& line)
    {
        if (line.first < 0) return;
        if (m_frames.size() == 1 && !m_continues && !line.kept) {
            m_open = Logical();
            m_open.indent = line.indent;
            m_open.word = wordAt(line.code, line.first);
        }
        const QString& code = line.code;
        for (int i = 0; i < code.length(); i++) {
            QChar ch = code[i];
            if (ch.isSpace()) continue;
            bool header = m_frames.size() == 1 && BLOCK_WORDS.contains(m_open.word);
            lineUp(ch, line, i, false);
            if (ch == '(' || ch == '[' || ch == '{') {
                m_frames.last().last = ch;
                push(ch, line, i);
                Frame& f = m_frames.last();
                f.wide = header && f.align < 0;
            } else if (ch == ')' || ch == ']' || ch == '}') {
                close(ch);
                m_frames.last().last = ch;
            } else {
                m_frames.last().last = ch;
            }
            m_open.last = ch;
        }
        m_continues = m_frames.size() > 1 || line.backslash || line.open;
        if (!m_continues) m_done.append(m_open);
    }

    const TextIndentRules& m_rules;
    int                    m_width;
    QVector<Frame>         m_frames;
    // the line read next, from 0 (directives aside)
    int                    m_line = 0;
    // the column of the comment after the code of the line above, -1 without one
    int                    m_trailing = -1;
    // the last two code characters read
    QChar                  m_last;
    QChar                  m_before;
    // the indents of the code lines read (EDIT-INDENT-1, the Tab cycle)
    QVector<int>           m_indents;
    // offside: the statements read, the one still open over the lines
    QVector<Logical>       m_done;
    Logical                m_open;
    bool                   m_continues = false;
};

// the lines of a text and where each starts
struct Lines {
    QStringList  list;
    QVector<int> starts;

    explicit Lines(const QString& text): list(text.split('\n'))
    {
        int start = 0;
        for (const QString& line : list) {
            starts.append(start);
            start += line.length() + 1;
        }
    }
    int lineOf(int position) const
    {
        return int(std::upper_bound(starts.begin(), starts.end(), position) - starts.begin()) - 1;
    }
};

// the place of a position of an old line in its new line: from its whitespace at the text, or
// at the line start when it is a line start of a selection
int newPlace(const QString& before, const QString& after, int k, bool keepStart)
{
    int oldLead = leadLength(before);
    int newLead = leadLength(after);
    if (k == 0 && keepStart) return 0;
    if (k <= oldLead) return newLead;
    return qMin(after.length(), newLead + k - oldLead);
}

// the edit from the old lines to the new ones, which differ in their whitespace only
TextFillEdit linesEdit(const Lines& old, const QStringList& now, int anchor, int cursor)
{
    TextFillEdit edit;
    int first = -1;
    int last = -1;
    for (int i = 0; i < old.list.size(); i++) {
        if (old.list[i] != now[i]) {
            if (first < 0) first = i;
            last = i;
        }
    }
    if (first < 0) return edit;
    edit.changed = true;
    edit.from = old.starts[first];
    edit.to = old.starts[last] + old.list[last].length();
    edit.text = now.mid(first, last - first + 1).join('\n');
    int delta = edit.text.length() - (edit.to - edit.from);
    bool selection = anchor != cursor;
    auto map = [&](int position) {
        if (position < edit.from) return position;
        if (position > edit.to) return position + delta;
        int line = old.lineOf(position);
        int start = edit.from;
        for (int i = first; i < line; i++) start += now[i].length() + 1;
        return start + newPlace(old.list[line], now[line], position - old.starts[line], selection);
    };
    edit.anchor = map(anchor);
    edit.cursor = map(cursor);
    return edit;
}

// the levels of a line read after the lines above: those Indent Region keeps, or the Tab cycle
QVector<int> scanLine(const QString& text, int line, const TextIndentRules& rules, int width, bool cycle)
{
    Lines lines(text);
    if (!textIndentComputes(rules) || line < 0 || line >= lines.list.size()) return QVector<int>();
    LexState st;
    Scanner scanner(rules, width);
    for (int i = 0; i < line; i++) scanner.advance(lexLine(lines.list[i], lines.starts[i], rules, st));
    LineInfo info = lexLine(lines.list[line], lines.starts[line], rules, st);
    return cycle ? scanner.cycle(info) : scanner.levels(info);
}

} // namespace

TextIndentRules textIndentRules(const CodeLanguage* language)
{
    TextIndentRules rules;
    rules.style = textIndentRelative;
    if (!language) return rules;
    rules.lineComment = language->lineComment;
    rules.blockOpen = language->blockOpen;
    rules.blockClose = language->blockClose;
    rules.quotes = language->quotes;
    rules.preprocessor = language->preprocessor;
    rules.labels = language->indentLabels;
    rules.flat = language->indentFlat;
    if (language->indent == "braces") rules.style = textIndentBraces;
    else if (language->indent == "brackets") rules.style = textIndentBrackets;
    else if (language->indent == "offside") rules.style = textIndentOffside;
    return rules;
}

TextIndentRules textIndentRules(TextFillKind kind)
{
    TextIndentRules rules;
    rules.style = textIndentRelative;
    rules.fill = kind;
    rules.blocks = kind == textFillMarkdown;
    return rules;
}

bool textIndentComputes(const TextIndentRules& rules)
{
    return rules.style == textIndentBraces || rules.style == textIndentBrackets ||
           rules.style == textIndentOffside;
}

int textLineIndent(const QString& line)
{
    return textColumns(line.left(leadLength(line)));
}

QString textIndentString(int column, bool tabs)
{
    return textIndentRun(0, column, tabs);
}

QString textIndentRun(int from, int to, bool tabs)
{
    QString run;
    int column = from;
    if (tabs) {
        for (int stop = (column / EDITOR_TAB_SPACES + 1) * EDITOR_TAB_SPACES; stop <= to; stop += EDITOR_TAB_SPACES) {
            run += '\t';
            column = stop;
        }
    }
    if (to > column) run += QString(to - column, ' ');
    return run;
}

int textNextStop(int column, int width)
{
    return (column / width + 1) * width;
}

int textPreviousStop(int column, int width)
{
    return column <= 0 ? 0 : ((column - 1) / width) * width;
}

int textIndentColumn(const QString& text, int line, const TextIndentRules& rules, int width)
{
    QVector<int> levels = textIndentLevels(text, line, rules, width);
    return levels.isEmpty() ? -1 : levels.first();
}

QVector<int> textIndentLevels(const QString& text, int line, const TextIndentRules& rules, int width)
{
    return scanLine(text, line, rules, width, false);
}

QVector<int> textIndentCycle(const QString& text, int line, const TextIndentRules& rules, int width)
{
    return scanLine(text, line, rules, width, true);
}

QVector<int> textIndentPoints(const QString& text, int line)
{
    Lines lines(text);
    QVector<int> points;
    int above = qMin(line, lines.list.size()) - 1;
    while (above >= 0 && lines.list[above].trimmed().isEmpty()) above--;
    if (above < 0) return points;
    const QString& s = lines.list[above];
    for (int i = 0; i < s.length(); i++) {
        if (isBlank(s[i]) || (i > 0 && !isBlank(s[i - 1]))) continue;
        int column = textColumns(s.left(i));
        if (!points.contains(column)) points.append(column);
    }
    std::sort(points.begin(), points.end());
    return points;
}

bool textIndentElectric(const QString& text, int position, const TextIndentRules& rules)
{
    if (!textIndentComputes(rules) || position < 1 || position > text.length()) return false;
    QChar c = text[position - 1];
    int lineStart = position > 1 ? text.lastIndexOf('\n', position - 2) + 1 : 0;
    QString before = text.mid(lineStart, position - 1 - lineStart);
    bool first = before.trimmed().isEmpty();
    if (first && (c == '}' || c == ')' || c == ']')) return true;
    if (first && c == '{' && rules.style == textIndentBraces) return true;
    if (first && c == '#' && rules.preprocessor) return true;
    if (c != ':') return false;
    QString word = wordAt(before, leadLength(before));
    if (rules.style == textIndentOffside) return !dedenterOpeners(word).isEmpty();
    if (rules.style != textIndentBraces || !rules.labels.contains(word)) return false;
    // the colon of the label: a single one, none before it on the line
    QString line = before + c + text.mid(position, 1);
    if (!singleColon(line, before.length())) return false;
    for (int k = 0; k < before.length(); k++) {
        if (singleColon(before, k)) return false;
    }
    return true;
}

TextFillEdit textReturnEdit(const QString& text, int position, const TextIndentRules& rules, int width,
                            bool tabs)
{
    TextFillEdit edit;
    position = qBound(0, position, text.length());
    int lineStart = position > 0 ? text.lastIndexOf('\n', position - 1) + 1 : 0;
    int lineEnd = text.indexOf('\n', position);
    if (lineEnd < 0) lineEnd = text.length();
    int before = position;
    while (before > lineStart && isBlank(text[before - 1])) before--;
    int after = position;
    while (after < lineEnd && isBlank(text[after])) after++;
    QString line = text.mid(lineStart, lineEnd - lineStart);
    int index = text.left(lineStart).count('\n');
    int column = -1;
    if (textIndentComputes(rules)) {
        QString broken = text.left(before) + '\n' + text.mid(after);
        TextIndentRules moved = rules;
        if (after <= moved.codeStart) moved.codeStart += before + 1 - after;
        else if (before < moved.codeStart) moved.codeStart = before + 1;
        column = textIndentColumn(broken, index + 1, moved, width);
        edit.from = before;
    } else {
        // a line left blank is emptied, the trailing spaces of a hard break stay
        edit.from = before == lineStart ? lineStart : position;
        QString prefix = textFillPrefix(text, index, rules.fill);
        if (!prefix.isNull()) column = textColumns(prefix.left(leadLength(prefix)));
    }
    if (column < 0) column = textLineIndent(line);
    edit.to = after;
    edit.text = "\n" + textIndentString(column, tabs);
    edit.changed = true;
    edit.anchor = edit.cursor = edit.from + edit.text.length();
    return edit;
}

TextFillEdit textReindent(const QString& text, int first, int last, const TextIndentRules& rules,
                          int width, bool tabs, int anchor, int cursor)
{
    Lines lines(text);
    if (!textIndentComputes(rules)) return TextFillEdit();
    first = qMax(0, first);
    last = qMin(last, lines.list.size() - 1);
    QStringList now = lines.list;
    LexState st;
    Scanner scanner(rules, width);
    int start = 0;
    for (int i = 0; i <= last; i++) {
        if (i >= first) {
            LexState probe = st;
            LineInfo info = lexLine(now[i], start, rules, probe);
            int column = scanner.column(info);
            // Python: the indent tells the blocks, a line at one of its levels keeps it, another
            // goes to the nearest level, the deeper one of two
            if (rules.style == textIndentOffside && column >= 0) {
                int indent = info.indent;
                for (int level : scanner.levels(info)) {
                    if (qAbs(level - indent) < qAbs(column - indent)) column = level;
                }
            }
            QString body = now[i].mid(leadLength(now[i]));
            if (column >= 0) now[i] = body.isEmpty() ? QString() : textIndentString(column, tabs) + body;
        }
        scanner.advance(lexLine(now[i], start, rules, st));
        start += now[i].length() + 1;
    }
    return linesEdit(lines, now, anchor, cursor);
}

TextFillEdit textSetIndent(const QString& text, int line, int column, bool tabs, int anchor, int cursor)
{
    Lines lines(text);
    if (line < 0 || line >= lines.list.size() || column < 0) return TextFillEdit();
    QStringList now = lines.list;
    now[line] = textIndentString(column, tabs) + now[line].mid(leadLength(now[line]));
    return linesEdit(lines, now, anchor, cursor);
}

TextFillEdit textShift(const QString& text, int first, int last, int delta, bool tabs, int anchor,
                       int cursor)
{
    Lines lines(text);
    QStringList now = lines.list;
    first = qMax(0, first);
    last = qMin(last, lines.list.size() - 1);
    for (int i = first; i <= last; i++) {
        if (now[i].trimmed().isEmpty()) continue;
        int column = qMax(0, textLineIndent(now[i]) + delta);
        now[i] = textIndentString(column, tabs) + now[i].mid(leadLength(now[i]));
    }
    return linesEdit(lines, now, anchor, cursor);
}

TextFillEdit textUntabify(const QString& text, int from, int to, int anchor, int cursor)
{
    TextFillEdit edit;
    from = qBound(0, from, text.length());
    to = qBound(from, to, text.length());
    int lineStart = from > 0 ? text.lastIndexOf('\n', from - 1) + 1 : 0;
    int lineEnd = text.indexOf('\n', to);
    if (lineEnd < 0) lineEnd = text.length();
    // the new text of the lines and the place of each old position in it
    QString now;
    QVector<int> place(lineEnd - lineStart + 1);
    int column = 0;
    for (int i = lineStart; i < lineEnd; i++) {
        place[i - lineStart] = now.length();
        QChar c = text[i];
        if (c == '\n') {
            column = 0;
            now += c;
        } else if (c == '\t') {
            int stop = (column / EDITOR_TAB_SPACES + 1) * EDITOR_TAB_SPACES;
            now += (i >= from && i < to) ? QString(stop - column, ' ') : QString(c);
            column = stop;
        } else {
            now += c;
            if (!c.isLowSurrogate() && !c.isMark()) column++;
        }
    }
    place[lineEnd - lineStart] = now.length();
    if (now == text.mid(lineStart, lineEnd - lineStart)) return edit;
    edit.changed = true;
    edit.from = lineStart;
    edit.to = lineEnd;
    edit.text = now;
    int delta = now.length() - (lineEnd - lineStart);
    auto map = [&](int position) {
        if (position < lineStart) return position;
        if (position > lineEnd) return position + delta;
        return lineStart + place[position - lineStart];
    };
    edit.anchor = map(anchor);
    edit.cursor = map(cursor);
    return edit;
}

QVector<TextIndentPart> textIndentParts(const QString& text, int last)
{
    Lines lines(text);
    QVector<TextIndentPart> parts;
    for (const MarkdownFencedBlock& block : markdownFencedBlocks(lines.list, last)) {
        TextIndentPart part;
        part.first = block.open + 1;
        part.last = block.close - 1;
        if (part.first > part.last) continue;
        part.rules = textIndentRules(CodeStyle::instance().find(block.tag));
        if (!textIndentComputes(part.rules)) continue;
        part.rules.codeStart = lines.starts[part.first];
        part.rules.margin = block.indent;
        parts.append(part);
    }
    return parts;
}

TextIndentRules textIndentRulesAt(const QString& text, int line, const TextIndentRules& rules)
{
    if (!rules.blocks) return rules;
    for (const TextIndentPart& part : textIndentParts(text, line)) {
        if (line >= part.first && line <= part.last) return part.rules;
    }
    return rules;
}

TextFillEdit textReindentParts(const QString& text, int first, int last,
                               const QVector<TextIndentPart>& parts, int width, bool tabs, int anchor,
                               int cursor)
{
    Lines lines(text);
    QString now = text;
    for (const TextIndentPart& part : parts) {
        int from = qMax(first, part.first);
        int to = qMin(last, part.last);
        if (from > to) continue;
        // the code starts where the parts before left it
        TextIndentRules rules = part.rules;
        rules.codeStart = Lines(now).starts[part.first];
        TextFillEdit edit = textReindent(now, from, to, rules, width, tabs, 0, 0);
        if (edit.changed) now = now.left(edit.from) + edit.text + now.mid(edit.to);
    }
    return linesEdit(lines, now.split('\n'), anchor, cursor);
}
