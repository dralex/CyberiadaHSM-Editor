/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Code Syntax Highlighting by the Platform Language
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

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextDocument>

#include "code_highlighter.h"
#include "settings_manager.h"

static void initSyntaxResource()
{
    // the resources live in the static core library (see FontManager)
    Q_INIT_RESOURCE(smeditor);
}

static QStringList jsonStrings(const QJsonValue& value)
{
    QStringList result;
    for (const QJsonValue& v : value.toArray()) {
        result.append(v.toString());
    }
    return result;
}

CodeStyle::CodeStyle()
{
    initSyntaxResource();
    loadLanguages();
    connect(&SettingsManager::instance(), &SettingsManager::highlightSettingsChanged,
            this, &CodeStyle::styleChanged);
}

void CodeStyle::loadLanguages()
{
    QDir dir(":/Syntax/syntax");
    for (const QString& file : dir.entryList(QStringList("*.json"), QDir::Files, QDir::Name)) {
        QFile f(dir.filePath(file));
        if (!f.open(QIODevice::ReadOnly)) continue;
        QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        if (o.value("name").toString().isEmpty()) continue;
        CodeLanguage lang;
        lang.name = o.value("name").toString();
        lang.aliases = jsonStrings(o.value("aliases"));
        lang.lineComment = o.value("lineComment").toString();
        QStringList block = jsonStrings(o.value("blockComment"));
        if (block.size() == 2) {
            lang.blockOpen = block[0];
            lang.blockClose = block[1];
        }
        lang.quotes = o.value("quotes").toString();
        lang.preprocessor = o.value("preprocessor").toBool();
        for (const QString& k : jsonStrings(o.value("keywords"))) lang.keywords.insert(k);
        for (const QString& t : jsonStrings(o.value("types"))) lang.types.insert(t);
        registry.append(lang);
    }
}

const CodeLanguage* CodeStyle::find(const QString& name) const
{
    QString key = name.trimmed();
    if (key.isEmpty()) return nullptr;
    for (const CodeLanguage& lang : registry) {
        if (lang.name.compare(key, Qt::CaseInsensitive) == 0) return &lang;
        for (const QString& alias : lang.aliases) {
            if (alias.compare(key, Qt::CaseInsensitive) == 0) return &lang;
        }
    }
    return nullptr;
}

bool CodeStyle::isActive() const
{
    const SettingsManager& sm = SettingsManager::instance();
    if (!sm.getHighlightCode()) return false;
    return !exportPlain || sm.getHighlightInExports();
}

const CodeLanguage* CodeStyle::effective(const CodeLanguage* lang) const
{
    return isActive() ? lang : nullptr;
}

CodeMarkup CodeStyle::findMarkup(const QString& value, bool* known)
{
    QString key = value.trimmed().toLower();
    if (known) *known = true;
    if (key == "markdown" || key == "md") return codeMarkupMarkdown;
    if (known) *known = key.isEmpty() || key == "plain";
    return codeMarkupPlain;
}

CodeMarkup CodeStyle::commentMarkup(const QString& own, CodeMarkup document)
{
    return own.trimmed().isEmpty() ? document : findMarkup(own);
}

QString CodeStyle::markupValue(CodeMarkup markup)
{
    return markup == codeMarkupMarkdown ? QString("markdown") : QString();
}

QStringList CodeStyle::languages() const
{
    QStringList result;
    for (const CodeLanguage& lang : registry) result.append(lang.name);
    return result;
}

void CodeStyle::overrideExportPlain(bool on)
{
    if (exportPlain == on) return;
    exportPlain = on;
    emit styleChanged();
}

QTextCharFormat CodeStyle::format(CodeTokenKind kind) const
{
    // colour only: a weight or a slant would change the text size and the layout
    static const QColor colors[codeTokenKindsCount] = {
        QColor(0x00, 0x33, 0xb3),   // keyword
        QColor(0x00, 0x7f, 0x7f),   // type
        QColor(0x06, 0x7d, 0x17),   // string
        QColor(0x17, 0x50, 0xeb),   // number
        QColor(0x8c, 0x8c, 0x8c),   // comment
        QColor(0x87, 0x10, 0x94),   // heading
        QColor(0x9e, 0x5a, 0x00),   // emphasis
        QColor(0x06, 0x7d, 0x17),   // code
        QColor(0x17, 0x50, 0xeb),   // link
        QColor(0x8c, 0x8c, 0x8c)    // marker
    };
    QTextCharFormat fmt;
    fmt.setForeground(colors[kind]);
    return fmt;
}

// the [start, end) parts of the text that hold code
static QVector<QPair<int, int>> codeSegments(const QString& text, CodeRole role)
{
    QVector<QPair<int, int>> result;
    int n = text.length();
    switch (role) {
    case codeRoleFormalComment:
        result.append(qMakePair(0, n));
        break;
    case codeRoleBehaviour: {
        static const QRegularExpression prefix(R"(^\s*(entry|exit|do)\s*/)");
        QRegularExpressionMatch m = prefix.match(text);
        result.append(qMakePair(m.hasMatch() ? m.capturedEnd(0) : 0, n));
        break;
    }
    case codeRoleTransition: {
        // the event runs up to [ or / as in TransitionAction::parseLabel
        int i = 0;
        while (i < n && text[i] != '[' && text[i] != '/' && text[i] != ']') i++;
        if (i < n && text[i] == '[') {
            int close = text.indexOf(']', i + 1);
            int end = close < 0 ? n : close;
            result.append(qMakePair(i + 1, end));
            i = close < 0 ? n : close + 1;
        }
        int slash = text.indexOf('/', i);
        if (slash >= 0) result.append(qMakePair(slash + 1, n));
        break;
    }
    default:
        break;
    }
    return result;
}

static bool isWordChar(QChar c)
{
    return c.isLetterOrNumber() || c == '_';
}

QVector<CodeSpan> CodeStyle::spans(const QString& text, CodeRole role, const CodeLanguage& lang)
{
    QVector<CodeSpan> result;
    for (const QPair<int, int>& seg : codeSegments(text, role)) {
        int i = seg.first;
        int end = seg.second;
        while (i < end) {
            QChar c = text[i];
            int j = i + 1;
            CodeTokenKind kind = codeTokenKindsCount;
            if (!lang.lineComment.isEmpty() && text.midRef(i).startsWith(lang.lineComment)) {
                j = text.indexOf('\n', i);
                if (j < 0 || j > end) j = end;
                kind = codeTokenComment;
            } else if (!lang.blockOpen.isEmpty() && text.midRef(i).startsWith(lang.blockOpen)) {
                j = text.indexOf(lang.blockClose, i + lang.blockOpen.length());
                j = (j < 0 || j >= end) ? end : j + lang.blockClose.length();
                kind = codeTokenComment;
            } else if (lang.quotes.contains(c)) {
                while (j < end && text[j] != c && text[j] != '\n') {
                    if (text[j] == '\\') j++;
                    j++;
                }
                if (j < end && text[j] == c) j++;
                j = qMin(j, end);
                kind = codeTokenString;
            } else if (c.isDigit() && (i == 0 || !isWordChar(text[i - 1]))) {
                while (j < end && (isWordChar(text[j]) || text[j] == '.')) j++;
                kind = codeTokenNumber;
            } else if (c.isLetter() || c == '_' || (lang.preprocessor && c == '#')) {
                while (j < end && isWordChar(text[j])) j++;
                QString word = text.mid(i, j - i);
                if (word.startsWith('#') || lang.keywords.contains(word)) {
                    kind = codeTokenKeyword;
                } else if (lang.types.contains(word)) {
                    kind = codeTokenType;
                }
            }
            if (kind != codeTokenKindsCount) {
                result.append(CodeSpan{i, j - i, kind});
            }
            i = j;
        }
    }
    return result;
}

// a fenced block: code by its language, or one code span
static void addFenceBody(QVector<CodeSpan>& result, const QString& text, int from, int to,
                         const CodeLanguage* lang)
{
    if (from >= to) return;
    if (!lang) {
        result.append(CodeSpan{from, to - from, codeTokenCode});
        return;
    }
    for (const CodeSpan& span : CodeStyle::spans(text.mid(from, to - from), codeRoleFormalComment, *lang)) {
        result.append(CodeSpan{span.start + from, span.length, span.kind});
    }
}

// the inline elements of a line part starting at base; an earlier element wins an overlap
static void addInlineSpans(QVector<CodeSpan>& result, const QString& part, int base)
{
    static const QRegularExpression code(R"((`+)(.+?)\1)");
    static const QRegularExpression link(R"(!?\[[^\]]*\]\([^)\s]*(\s+"[^"]*")?\)|<(https?|ftp|mailto):[^>\s]+>|\bhttps?://[^\s<>()]+)");
    static const QRegularExpression strong(R"((\*\*|__|~~)(?=\S)(.+?)(?<=\S)\1)");
    static const QRegularExpression em(R"((?<![*\w])\*(?=[^\s*])(.+?)(?<=[^\s*])\*(?!\*)|(?<![_\w])_(?=[^\s_])(.+?)(?<=[^\s_])_(?![_\w]))");
    const QRegularExpression* rules[] = { &code, &link, &strong, &em };
    const CodeTokenKind kinds[] = { codeTokenCode, codeTokenLink, codeTokenEmphasis, codeTokenEmphasis };
    QVector<QPair<int, int>> taken;
    for (size_t r = 0; r < sizeof(rules) / sizeof(rules[0]); r++) {
        QRegularExpressionMatchIterator it = rules[r]->globalMatch(part);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            int from = m.capturedStart(0), to = m.capturedEnd(0);
            bool overlaps = false;
            for (const QPair<int, int>& t : taken) {
                if (from < t.second && t.first < to) { overlaps = true; break; }
            }
            if (overlaps) continue;
            taken.append(qMakePair(from, to));
            result.append(CodeSpan{base + from, to - from, kinds[r]});
        }
    }
}

QVector<CodeSpan> CodeStyle::markdownSpans(const QString& text, const CodeLanguage* fallback) const
{
    static const QRegularExpression fence(R"(^ {0,3}(`{3,}|~{3,})\s*([^\s`]*)\s*$)");
    static const QRegularExpression heading(R"(^ {0,3}#{1,6}(\s|$))");
    static const QRegularExpression rule(R"(^ {0,3}([-*_])( *\1){2,} *$)");
    static const QRegularExpression quote(R"(^ {0,3}(> ?)+)");
    static const QRegularExpression list(R"(^\s*([-*+]|\d{1,9}[.)])(\s|$))");

    QVector<CodeSpan> result;
    int n = text.length();
    int pos = 0;
    // the open fence: its marker, its language and where its body starts
    QString fenceMarker;
    const CodeLanguage* fenceLang = nullptr;
    int bodyStart = -1;
    while (pos <= n) {
        int eol = text.indexOf('\n', pos);
        if (eol < 0) eol = n;
        QString line = text.mid(pos, eol - pos);
        QRegularExpressionMatch f = fence.match(line);
        if (bodyStart >= 0) {
            // a closing fence: the same character, at least as long, no tag
            if (f.hasMatch() && f.captured(1).at(0) == fenceMarker.at(0) &&
                f.captured(1).length() >= fenceMarker.length() && f.captured(2).isEmpty()) {
                addFenceBody(result, text, bodyStart, pos, fenceLang);
                result.append(CodeSpan{pos, line.length(), codeTokenMarker});
                bodyStart = -1;
            }
        } else if (f.hasMatch()) {
            fenceMarker = f.captured(1);
            const CodeLanguage* tagged = find(f.captured(2));
            fenceLang = tagged ? tagged : fallback;
            result.append(CodeSpan{pos, line.length(), codeTokenMarker});
            bodyStart = qMin(eol + 1, n);
        } else if (heading.match(line).hasMatch()) {
            result.append(CodeSpan{pos, line.length(), codeTokenHeading});
        } else if (rule.match(line).hasMatch()) {
            result.append(CodeSpan{pos, line.length(), codeTokenMarker});
        } else {
            int start = 0;
            QRegularExpressionMatch q = quote.match(line);
            if (q.hasMatch()) {
                result.append(CodeSpan{pos, q.capturedLength(0), codeTokenMarker});
                start = q.capturedEnd(0);
            }
            QRegularExpressionMatch l = list.match(line.mid(start));
            if (l.hasMatch()) {
                result.append(CodeSpan{pos + start + l.capturedStart(1), l.capturedLength(1), codeTokenMarker});
                start += l.capturedEnd(1);
            }
            addInlineSpans(result, line.mid(start), pos + start);
        }
        if (eol >= n) break;
        pos = eol + 1;
    }
    // an unclosed fence runs to the end
    if (bodyStart >= 0) addFenceBody(result, text, bodyStart, n, fenceLang);
    return result;
}

CodeHighlighter::CodeHighlighter(QTextDocument* document, CodeRole role,
                                 const CodeLanguage* language, CodeMarkup markup):
    QSyntaxHighlighter(document), role(role), language(language), markup(markup)
{
    connect(&CodeStyle::instance(), &CodeStyle::styleChanged, this, &QSyntaxHighlighter::rehighlight);
    // the first pass is otherwise deferred to the event loop and the edits
    // before it are not highlighted: an export right after a load or an undo
    // would draw plain text
    QMetaObject::invokeMethod(this, "_q_delayedRehighlight", Qt::DirectConnection);
}

void CodeHighlighter::setRole(CodeRole newRole)
{
    if (role == newRole) return;
    role = newRole;
    rehighlight();
}

void CodeHighlighter::setLanguage(const CodeLanguage* newLanguage)
{
    if (language == newLanguage) return;
    language = newLanguage;
    rehighlight();
}

void CodeHighlighter::setMarkup(CodeMarkup newMarkup)
{
    if (markup == newMarkup) return;
    markup = newMarkup;
    rehighlight();
}

void CodeHighlighter::highlightBlock(const QString& text)
{
    // a span may cross the lines (a block comment, a guard, a fence), so the
    // whole text is scanned; the texts are short
    const CodeStyle& style = CodeStyle::instance();
    QVector<CodeSpan> all;
    if (role == codeRoleMarkup) {
        if (markup == codeMarkupMarkdown && style.isActive()) {
            all = style.markdownSpans(document()->toPlainText(), language);
        }
    } else if (const CodeLanguage* lang = style.effective(language)) {
        all = CodeStyle::spans(document()->toPlainText(), role, *lang);
    }
    if (all.isEmpty()) {
        setCurrentBlockState(0);
        return;
    }
    int blockStart = currentBlock().position();
    int blockEnd = blockStart + text.length();
    uint tail = 0;
    for (const CodeSpan& span : all) {
        int from = qMax(span.start, blockStart);
        int to = qMin(span.start + span.length, blockEnd);
        if (from < to) setFormat(from - blockStart, to - from, style.format(span.kind));
        // the spans past this line, relative to its end: the next line is
        // re-highlighted only when they change
        if (span.start + span.length > blockEnd) {
            tail = tail * 31 + uint(span.start - blockEnd) * 7 + uint(span.length) * 3 + uint(span.kind);
        }
    }
    setCurrentBlockState(int(tail & 0x7fffffff));
}
