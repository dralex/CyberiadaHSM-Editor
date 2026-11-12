/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Markdown readers: the text, the formulas and the images into a document
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
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QVector>
#include <algorithm>

#include "markdown_importer.h"
#include "markdown_objects.h"
#include "md4c.h"

namespace {

const unsigned MD_FLAGS = MD_DIALECT_GITHUB | MD_FLAG_LATEXMATHSPANS;
// the formula placeholder of the Qt reader and its alt text: Qt drops an image
// without one
const char* const MATH_PREFIX = "cyb-math:";
const char* const MATH_PLACEHOLDER = "![$](cyb-math:%1)";
const char* const INJECTED_ALT = "_";

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

struct ScanState {
    const char*  doc = nullptr;
    int          size = 0;
    // the byte spans of the formulas: the content between the dollars
    bool         inMath = false;
    bool         display = false;
    int          first = -1;
    int          last = -1;
    int          imageDepth = 0;
    struct Bytes { int first; int last; bool display; };
    QList<Bytes> math;
    QList<ImageSpan> images;
    // the byte of each image source in the text, -1 when md4c built it elsewhere
    QList<int>   srcBytes;
    QList<int>   voidTagEnds;
    QList<int>   foreignTags;
};

bool inDoc(const ScanState* st, const MD_CHAR* p)
{
    return p >= st->doc && p < st->doc + st->size;
}

QString attribute(const MD_ATTRIBUTE& a)
{
    return QString::fromUtf8(a.text, int(a.size));
}

int scanEnterBlock(MD_BLOCKTYPE, void*, void*) { return 0; }
int scanLeaveBlock(MD_BLOCKTYPE, void*, void*) { return 0; }

int scanEnterSpan(MD_SPANTYPE type, void* detail, void* user)
{
    ScanState* st = static_cast<ScanState*>(user);
    if (type == MD_SPAN_IMG) {
        if (st->imageDepth++ > 0) return 0;
        const MD_SPAN_IMG_DETAIL* d = static_cast<const MD_SPAN_IMG_DETAIL*>(detail);
        ImageSpan img;
        img.source = attribute(d->src);
        img.title = attribute(d->title);
        st->images.append(img);
        st->srcBytes.append(d->src.size > 0 && inDoc(st, d->src.text) ? int(d->src.text - st->doc) : -1);
    } else if ((type == MD_SPAN_LATEXMATH || type == MD_SPAN_LATEXMATH_DISPLAY) && st->imageDepth == 0) {
        st->inMath = true;
        st->display = type == MD_SPAN_LATEXMATH_DISPLAY;
        st->first = st->last = -1;
    }
    return 0;
}

int scanLeaveSpan(MD_SPANTYPE type, void*, void* user)
{
    ScanState* st = static_cast<ScanState*>(user);
    if (type == MD_SPAN_IMG) {
        st->imageDepth--;
    } else if ((type == MD_SPAN_LATEXMATH || type == MD_SPAN_LATEXMATH_DISPLAY) && st->inMath) {
        if (st->first >= 0) st->math.append(ScanState::Bytes{st->first, st->last, st->display});
        st->inMath = false;
    }
    return 0;
}

int scanText(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* user)
{
    ScanState* st = static_cast<ScanState*>(user);
    if (st->imageDepth > 0 && !st->images.isEmpty()) {
        ImageSpan& img = st->images.last();
        img.alt += type == MD_TEXT_SOFTBR || type == MD_TEXT_BR ? QString(" ") : QString::fromUtf8(text, int(size));
        img.chunks++;
    } else if (type == MD_TEXT_HTML && inDoc(st, text)) {
        QString html = QString::fromUtf8(text, int(size));
        if (isOpenVoidTag(html)) st->voidTagEnds.append(int(text - st->doc) + int(size) - 1);
        else if (isForeignTag(html)) st->foreignTags.append(int(text - st->doc));
    } else if (st->inMath && type == MD_TEXT_LATEXMATH && inDoc(st, text)) {
        int b = int(text - st->doc);
        if (st->first < 0 || b < st->first) st->first = b;
        if (b + int(size) > st->last) st->last = b + int(size);
    }
    return 0;
}

// the place of the alt text of an inline image without one: the ']' of "![](src"
int emptyAltPlace(const QByteArray& bytes, int src)
{
    int i = src - 1;
    while (i >= 0 && (bytes.at(i) == ' ' || bytes.at(i) == '\t' || bytes.at(i) == '\n' || bytes.at(i) == '<')) i--;
    if (i < 3 || bytes.at(i) != '(' || bytes.at(i - 1) != ']' || bytes.at(i - 2) != '[' || bytes.at(i - 3) != '!')
        return -1;
    return i - 1;
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
// an edit of the text for the Qt reader, applied from the end
struct SourceEdit {
    int     position;
    int     length;
    QString text;
};
#endif

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

} // namespace

namespace {
const char* const VOID_ELEMENTS =
    "area|base|br|col|embed|hr|img|input|link|meta|param|source|track|wbr";
const char* const TEXT_ELEMENTS =
    "a|abbr|b|big|cite|code|del|dfn|em|font|i|ins|kbd|mark|q|s|samp|small|span|"
    "strike|strong|sub|sup|tt|u|var";
}

bool isForeignTag(const QString& html)
{
    static const QRegularExpression tag("^</?([a-zA-Z][a-zA-Z0-9-]*)\\b[^>]*>$");
    static const QRegularExpression known(QString("^(?:%1|%2)$").arg(VOID_ELEMENTS, TEXT_ELEMENTS),
                                          QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m = tag.match(html);
    return m.hasMatch() && !known.match(m.captured(1)).hasMatch();
}

bool isOpenVoidTag(const QString& html)
{
    static const QRegularExpression re(QString("^<(?:%1)\\b[^>]*>$").arg(VOID_ELEMENTS),
                                       QRegularExpression::CaseInsensitiveOption);
    return re.match(html).hasMatch() && !html.endsWith("/>");
}

QString closeVoidTags(const QString& html)
{
    static const QRegularExpression re(QString("<((?:%1)\\b[^>]*?)\\s*/?>").arg(VOID_ELEMENTS),
                                       QRegularExpression::CaseInsensitiveOption);
    QString out = html;
    out.replace(re, "<\\1/>");
    return out;
}

MarkdownScan scanMarkdown(const QString& text)
{
    QByteArray bytes = text.toUtf8();
    ScanState st;
    st.doc = bytes.constData();
    st.size = bytes.size();
    MD_PARSER parser = {0, MD_FLAGS, scanEnterBlock, scanLeaveBlock, scanEnterSpan, scanLeaveSpan,
                        scanText, nullptr, nullptr};
    md_parse(bytes.constData(), MD_SIZE(bytes.size()), &parser, &st);

    QVector<int> offsets = utf16Offsets(bytes);
    MarkdownScan scan;
    for (const ScanState::Bytes& m : st.math) {
        // the dollars around the content: one inline, two display
        int open = m.first, close = m.last;
        for (int k = 0; k < 2 && open > 0 && bytes.at(open - 1) == '$'; k++) open--;
        for (int k = 0; k < 2 && close < bytes.size() && bytes.at(close) == '$'; k++) close++;
        MathSpan span;
        span.begin = offsets.at(open);
        span.end = offsets.at(close);
        span.display = m.display;
        span.tex = text.mid(offsets.at(m.first), offsets.at(m.last) - offsets.at(m.first));
        span.tex.replace('\n', ' ');
        scan.math.append(span);
    }
    for (int end : st.voidTagEnds) scan.openVoidTags.append(offsets.at(end));
    for (int start : st.foreignTags) scan.foreignTags.append(offsets.at(start));
    for (int i = 0; i < st.images.size(); i++) {
        ImageSpan img = st.images.at(i);
        if (img.chunks == 0 && st.srcBytes.at(i) >= 0) {
            int place = emptyAltPlace(bytes, st.srcBytes.at(i));
            if (place >= 0) img.emptyAlt = offsets.at(place);
        }
        scan.images.append(img);
    }
    return scan;
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
void readMarkdownQt(QTextDocument* doc, const QString& text, const QString& baseDir)
{
    // the formulas become placeholder images; an image without an alt text gets
    // one, Qt would drop it
    MarkdownScan scan = scanMarkdown(text);
    QList<SourceEdit> edits;
    for (int i = 0; i < scan.math.size(); i++) {
        const MathSpan& m = scan.math.at(i);
        edits.append(SourceEdit{m.begin, m.end - m.begin, QString(MATH_PLACEHOLDER).arg(i)});
    }
    for (const ImageSpan& img : scan.images)
        if (img.emptyAlt >= 0) edits.append(SourceEdit{img.emptyAlt, 0, INJECTED_ALT});
    for (int end : scan.openVoidTags) edits.append(SourceEdit{end, 0, "/"});
    for (int start : scan.foreignTags) edits.append(SourceEdit{start, 1, "&lt;"});
    std::sort(edits.begin(), edits.end(),
              [](const SourceEdit& a, const SourceEdit& b) { return a.position > b.position; });
    QString source = text;
    for (const SourceEdit& e : edits) source.replace(e.position, e.length, e.text);
    doc->setMarkdown(source, QTextDocument::MarkdownDialectGitHub);

    // the images in the document order: Qt makes one per text piece of an alt
    struct Image { int position; QTextCharFormat format; };
    QList<Image> found;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it) {
            QTextFragment fragment = it.fragment();
            if (!fragment.charFormat().isImageFormat()) continue;
            for (int k = 0; k < fragment.length(); k++)
                found.append(Image{fragment.position() + k, fragment.charFormat()});
        }
    }
    struct Change { int position; QTextCharFormat format; };
    QList<Change> changes;
    QList<int> removed;
    int next = 0;
    for (int i = 0; i < found.size(); i++) {
        QTextImageFormat img = found.at(i).format.toImageFormat();
        QString name = img.name();
        if (name.startsWith(MATH_PREFIX)) {
            bool ok = false;
            int n = name.mid(int(qstrlen(MATH_PREFIX))).toInt(&ok);
            if (ok && n >= 0 && n < scan.math.size())
                changes.append(Change{found.at(i).position,
                                      markdownMathFormat(img, scan.math.at(n).tex, scan.math.at(n).display)});
            continue;
        }
        if (next < scan.images.size() && scan.images.at(next).source == name) {
            const ImageSpan& span = scan.images.at(next++);
            QTextCharFormat f = markdownImageFormat(img, span.source, span.alt, baseDir);
            if (!span.title.isEmpty()) f.setToolTip(span.title);
            changes.append(Change{found.at(i).position, f});
            for (int k = 1; k < span.chunks && i + 1 < found.size() &&
                            found.at(i + 1).position == found.at(i).position + 1 &&
                            found.at(i + 1).format.toImageFormat().name() == name; k++)
                removed.append(found.at(++i).position);
        } else {
            changes.append(Change{found.at(i).position,
                                  markdownImageFormat(img, name, img.stringProperty(QTextFormat::ImageAltText),
                                                      baseDir)});
        }
    }
    QTextCursor cursor(doc);
    for (const Change& c : changes) {
        cursor.setPosition(c.position);
        cursor.setPosition(c.position + 1, QTextCursor::KeepAnchor);
        cursor.setCharFormat(c.format);
    }
    std::sort(removed.begin(), removed.end(), [](int a, int b) { return a > b; });
    for (int p : removed) {
        cursor.setPosition(p);
        cursor.deleteChar();
    }
}
#endif

void importMarkdown(QTextDocument* doc, const QString& text, const QString& baseDir)
{
    installMarkdownObjects(doc);
    // the document keeps no history of its own rendering
    bool undo = doc->isUndoRedoEnabled();
    doc->setUndoRedoEnabled(false);
#ifdef CYBERIADA_MD_READER_QT
    readMarkdownQt(doc, text, baseDir);
#else
    readMarkdownMd4c(doc, text, baseDir);
#endif
    alignDisplayFormulas(doc);
    doc->setUndoRedoEnabled(undo);
}
