/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
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

void CodeStyle::setLanguage(const QString& name)
{
    if (name == currentName) return;
    currentName = name;
    current = find(name);
    emit styleChanged();
}

const CodeLanguage* CodeStyle::activeLanguage() const
{
    const SettingsManager& sm = SettingsManager::instance();
    if (!sm.getHighlightCode()) return nullptr;
    if (exportPlain && !sm.getHighlightInExports()) return nullptr;
    return current;
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
        QColor(0x8c, 0x8c, 0x8c)    // comment
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

CodeHighlighter::CodeHighlighter(QTextDocument* document, CodeRole role):
    QSyntaxHighlighter(document), role(role)
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

void CodeHighlighter::highlightBlock(const QString& text)
{
    const CodeLanguage* lang = CodeStyle::instance().activeLanguage();
    if (!lang || role == codeRoleNone) {
        setCurrentBlockState(0);
        return;
    }
    // a span may cross the lines (a block comment, a guard), so the whole text
    // is scanned; the texts are short
    const CodeStyle& style = CodeStyle::instance();
    int blockStart = currentBlock().position();
    int blockEnd = blockStart + text.length();
    uint tail = 0;
    for (const CodeSpan& span : CodeStyle::spans(document()->toPlainText(), role, *lang)) {
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
