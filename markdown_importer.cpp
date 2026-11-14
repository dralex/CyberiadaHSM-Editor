/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Markdown into a document: the text, the formulas and the images
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

#include <QList>
#include <QPair>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include "markdown_importer.h"
#include "markdown_objects.h"
#include "md4c.h"

namespace {

const char* const VOID_ELEMENTS =
    "area|base|br|col|embed|hr|img|input|link|meta|param|source|track|wbr";
const char* const TEXT_ELEMENTS =
    "a|abbr|b|big|cite|code|del|dfn|em|font|i|ins|kbd|mark|q|s|samp|small|span|"
    "strike|strong|sub|sup|tt|u|var";

void alignDisplayFormulas(QTextDocument* doc)
{
    QTextCursor cursor(doc);
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        if (b.text().trimmed() != QString(QChar::ObjectReplacementCharacter)) continue;
        for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it) {
            QTextCharFormat f = it.fragment().charFormat();
            if (f.objectType() == MarkdownMathObject && f.boolProperty(MarkdownDisplay)) {
                QTextBlockFormat bf = b.blockFormat();
                bf.setAlignment(Qt::AlignHCenter);
                cursor.setPosition(b.position());
                cursor.setBlockFormat(bf);
                break;
            }
        }
    }
}

const unsigned MD_FLAGS = MD_DIALECT_GITHUB | MD_FLAG_LATEXMATHSPANS;

// the UTF-16 offset of each UTF-8 byte of the text
QVector<int> utf16Offsets(const QByteArray& utf8)
{
    QVector<int> map(utf8.size() + 1);
    int u = 0;
    int i = 0;
    while (i < utf8.size()) {
        uchar c = uchar(utf8.at(i));
        int len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xe ? 3 : (c >> 3) == 0x1e ? 4 : 1;
        for (int k = 0; k < len && i + k < utf8.size(); k++) map[i + k] = u;
        u += len == 4 ? 2 : 1;
        i += len;
    }
    map[utf8.size()] = u;
    return map;
}

// the byte ranges of the TeX of the formulas and of the HTML comments md4c reports
struct MathScan {
    const char* doc = nullptr;
    int size = 0;
    int imageDepth = 0;
    bool inMath = false;
    int first = -1;
    int last = -1;
    QVector<QPair<int, int>> tex;
    int commentStart = -1;
    int htmlEnd = -1;
    QVector<QPair<int, int>> comments;
};

int mathEnterBlock(MD_BLOCKTYPE, void*, void*) { return 0; }

// an unclosed comment ends with its block, as the reader drops it there
int mathLeaveBlock(MD_BLOCKTYPE, void*, void* user)
{
    MathScan* st = static_cast<MathScan*>(user);
    if (st->commentStart >= 0) {
        st->comments.append(qMakePair(st->commentStart, st->htmlEnd));
        st->commentStart = -1;
    }
    return 0;
}

int mathEnterSpan(MD_SPANTYPE type, void*, void* user)
{
    MathScan* st = static_cast<MathScan*>(user);
    if (type == MD_SPAN_IMG) st->imageDepth++;
    else if ((type == MD_SPAN_LATEXMATH || type == MD_SPAN_LATEXMATH_DISPLAY) && st->imageDepth == 0) {
        st->inMath = true;
        st->first = st->last = -1;
    }
    return 0;
}

int mathLeaveSpan(MD_SPANTYPE type, void*, void* user)
{
    MathScan* st = static_cast<MathScan*>(user);
    if (type == MD_SPAN_IMG) st->imageDepth--;
    else if ((type == MD_SPAN_LATEXMATH || type == MD_SPAN_LATEXMATH_DISPLAY) && st->inMath) {
        if (st->first >= 0) st->tex.append(qMakePair(st->first, st->last));
        st->inMath = false;
    }
    return 0;
}

int mathText(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* user)
{
    MathScan* st = static_cast<MathScan*>(user);
    if (type == MD_TEXT_HTML && text >= st->doc && text < st->doc + st->size) {
        QByteArray chunk(text, int(size));
        int base = int(text - st->doc), at = 0;
        while (at < chunk.size()) {
            if (st->commentStart < 0) {
                int open = chunk.indexOf("<!--", at);
                if (open < 0) break;
                st->commentStart = base + open;
                at = open + 4;
            }
            int close = chunk.indexOf("-->", at);
            if (close < 0) break;
            st->comments.append(qMakePair(st->commentStart, base + close + 3));
            st->commentStart = -1;
            at = close + 3;
        }
        st->htmlEnd = base + int(size);
        return 0;
    }
    if (st->inMath && type == MD_TEXT_LATEXMATH && text >= st->doc && text < st->doc + st->size) {
        int b = int(text - st->doc);
        if (st->first < 0 || b < st->first) st->first = b;
        if (b + int(size) > st->last) st->last = b + int(size);
    }
    return 0;
}

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// the text of a quote in its grey; a link and a text of a colour of its own keep theirs
void greyQuotes(QTextDocument* doc)
{
    QList<QPair<int, int>> runs;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        if (b.blockFormat().intProperty(MarkdownQuoteLevel) <= 0) continue;
        for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it) {
            QTextFragment f = it.fragment();
            if (!f.charFormat().hasProperty(QTextFormat::ForegroundBrush))
                runs.append(qMakePair(f.position(), f.length()));
        }
    }
    QTextCharFormat grey;
    grey.setForeground(markdownQuoteColor(doc));
    QTextCursor cursor(doc);
    for (const QPair<int, int>& run : runs) {
        cursor.setPosition(run.first);
        cursor.setPosition(run.first + run.second, QTextCursor::KeepAnchor);
        cursor.mergeCharFormat(grey);
    }
}

} // namespace

MarkdownSourceRanges markdownSourceRanges(const QString& text)
{
    QByteArray bytes = text.toUtf8();
    MathScan st;
    st.doc = bytes.constData();
    st.size = bytes.size();
    MD_PARSER parser = {0, MD_FLAGS, mathEnterBlock, mathLeaveBlock, mathEnterSpan, mathLeaveSpan,
                        mathText, nullptr, nullptr};
    md_parse(bytes.constData(), MD_SIZE(bytes.size()), &parser, &st);
    QVector<int> offsets = utf16Offsets(bytes);
    MarkdownSourceRanges ranges;
    for (const QPair<int, int>& c : st.comments) ranges.comments.append(qMakePair(offsets.at(c.first), offsets.at(c.second)));
    for (const QPair<int, int>& t : st.tex) {
        // the dollars around the TeX, a display one perhaps on lines of their own
        int open = t.first, close = t.second;
        while (open > 0 && isSpace(bytes.at(open - 1))) open--;
        for (int k = 0; k < 2 && open > 0 && bytes.at(open - 1) == '$'; k++) open--;
        while (close < bytes.size() && isSpace(bytes.at(close))) close++;
        for (int k = 0; k < 2 && close < bytes.size() && bytes.at(close) == '$'; k++) close++;
        ranges.math.append(MarkdownMathRange{offsets.at(open), offsets.at(close), offsets.at(t.first), offsets.at(t.second)});
    }
    return ranges;
}

bool isForeignTag(const QString& html)
{
    static const QRegularExpression tag("^</?([a-zA-Z][a-zA-Z0-9-]*)\\b[^>]*>$");
    static const QRegularExpression known(QString("^(?:%1|%2)$").arg(VOID_ELEMENTS, TEXT_ELEMENTS),
                                          QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m = tag.match(html);
    return m.hasMatch() && !known.match(m.captured(1)).hasMatch();
}

QString closeVoidTags(const QString& html)
{
    static const QRegularExpression re(QString("<((?:%1)\\b[^>]*?)\\s*/?>").arg(VOID_ELEMENTS),
                                       QRegularExpression::CaseInsensitiveOption);
    QString out = html;
    out.replace(re, "<\\1/>");
    return out;
}

void importMarkdown(QTextDocument* doc, const QString& text, const QString& baseDir)
{
    installMarkdownObjects(doc);
    // the document keeps no history of its own rendering
    bool undo = doc->isUndoRedoEnabled();
    doc->setUndoRedoEnabled(false);
    readMarkdownMd4c(doc, text, baseDir);
    alignDisplayFormulas(doc);
    greyQuotes(doc);
    doc->setUndoRedoEnabled(undo);
}
