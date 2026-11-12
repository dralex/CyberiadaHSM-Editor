/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The md4c Markdown reader: the document built the way Qt's reader builds it
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

#include <QFontDatabase>
#include <QGuiApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QStack>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>
#include <QTextTable>
#include <QVector>

#include "markdown_importer.h"
#include "markdown_objects.h"
#include "md4c.h"

namespace {

const unsigned MD_FLAGS = MD_DIALECT_GITHUB | MD_FLAG_LATEXMATHSPANS;
// the values of Qt's reader (QTextMarkdownImporter)
const int BLOCK_QUOTE_INDENT = 40;
const int HEADING_SIZE_BASE = 4;
const int PARAGRAPH_MARGIN_NUMERATOR = 2;
const int PARAGRAPH_MARGIN_DENOMINATOR = 3;

QString attribute(const MD_ATTRIBUTE& a)
{
    return QString::fromUtf8(a.text, int(a.size));
}

Qt::Alignment cellAlignment(MD_ALIGN align)
{
    switch (align) {
    case MD_ALIGN_CENTER: return Qt::AlignHCenter | Qt::AlignVCenter;
    case MD_ALIGN_RIGHT:  return Qt::AlignRight | Qt::AlignVCenter;
    default:              return Qt::AlignLeft | Qt::AlignVCenter;
    }
}

class Md4cReader {
public:
    Md4cReader(QTextDocument* document, const QString& base): doc(document), cursor(document), baseDir(base)
    {
        QFont font = doc->defaultFont();
        paragraphMargin = font.pointSize() * PARAGRAPH_MARGIN_NUMERATOR / PARAGRAPH_MARGIN_DENOMINATOR;
        monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        if (font.pointSize() == -1) monoFont.setPixelSize(font.pixelSize());
        else monoFont.setPointSize(font.pointSize());
    }

    void read(const QString& text)
    {
        doc->clear();
        cursor = QTextCursor(doc);
        QByteArray bytes = text.toUtf8();
        MD_PARSER parser = {0, MD_FLAGS, enterBlock, leaveBlock, enterSpan, leaveSpan, onText, nullptr, nullptr};
        md_parse(bytes.constData(), MD_SIZE(bytes.size()), &parser, this);
    }

private:
    static int enterBlock(MD_BLOCKTYPE type, void* detail, void* self)
    { return static_cast<Md4cReader*>(self)->onEnterBlock(type, detail); }
    static int leaveBlock(MD_BLOCKTYPE type, void* detail, void* self)
    { return static_cast<Md4cReader*>(self)->onLeaveBlock(type, detail); }
    static int enterSpan(MD_SPANTYPE type, void* detail, void* self)
    { return static_cast<Md4cReader*>(self)->onEnterSpan(type, detail); }
    static int leaveSpan(MD_SPANTYPE type, void* detail, void* self)
    { return static_cast<Md4cReader*>(self)->onLeaveSpan(type, detail); }
    static int onText(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* self)
    { return static_cast<Md4cReader*>(self)->onText(type, QString::fromUtf8(text, int(size))); }

    int onEnterBlock(MD_BLOCKTYPE type, void* detail)
    {
        blockType = type;
        switch (type) {
        case MD_BLOCK_P:
            needsInsertBlock = true;
            break;
        case MD_BLOCK_QUOTE:
            blockQuoteDepth++;
            break;
        case MD_BLOCK_CODE: {
            const MD_BLOCK_CODE_DETAIL* d = static_cast<const MD_BLOCK_CODE_DETAIL*>(detail);
            codeBlock = true;
            codeLanguage = attribute(d->lang);
            codeFence = d->fence_char ? QString(QChar::fromLatin1(d->fence_char)) : QString();
            needsInsertBlock = true;
            break;
        }
        case MD_BLOCK_H: {
            const MD_BLOCK_H_DETAIL* d = static_cast<const MD_BLOCK_H_DETAIL*>(detail);
            QTextBlockFormat bf;
            bf.setHeadingLevel(int(d->level));
            QTextCharFormat cf;
            cf.setProperty(QTextFormat::FontSizeAdjustment, HEADING_SIZE_BASE - int(d->level));
            cf.setFontWeight(QFont::Bold);
            needsInsertBlock = false;
            if (doc->isEmpty()) {
                cursor.setBlockFormat(bf);
                cursor.setCharFormat(cf);
            } else {
                cursor.insertBlock(bf, cf);
            }
            break;
        }
        case MD_BLOCK_LI: {
            const MD_BLOCK_LI_DETAIL* d = static_cast<const MD_BLOCK_LI_DETAIL*>(detail);
            needsInsertBlock = true;
            listItem = true;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            markerType = !d->is_task ? QTextBlockFormat::MarkerType::NoMarker
                         : d->task_mark == ' ' ? QTextBlockFormat::MarkerType::Unchecked
                                               : QTextBlockFormat::MarkerType::Checked;
#else
            Q_UNUSED(d);
#endif
            break;
        }
        case MD_BLOCK_UL:
        case MD_BLOCK_OL:
            // a list nested in an empty item is made at once
            if (needsInsertList) lists.push(cursor.insertList(listFormat));
            else needsInsertList = true;
            listFormat = QTextListFormat();
            listFormat.setIndent(lists.count() + 1);
            if (type == MD_BLOCK_UL) {
                const MD_BLOCK_UL_DETAIL* d = static_cast<const MD_BLOCK_UL_DETAIL*>(detail);
                listFormat.setStyle(d->mark == '*' ? QTextListFormat::ListCircle
                                    : d->mark == '+' ? QTextListFormat::ListSquare
                                                     : QTextListFormat::ListDisc);
            } else {
                const MD_BLOCK_OL_DETAIL* d = static_cast<const MD_BLOCK_OL_DETAIL*>(detail);
                listFormat.setStyle(QTextListFormat::ListDecimal);
                listFormat.setNumberSuffix(QString(QChar::fromLatin1(d->mark_delimiter)));
            }
            break;
        case MD_BLOCK_TABLE:
            tableColumns = 0;
            tableRows = 0;
            table = cursor.insertTable(1, 1);
            break;
        case MD_BLOCK_TR:
            tableRows++;
            nonEmptyCells.clear();
            if (table->rows() < tableRows) table->appendRows(1);
            tableCol = -1;
            break;
        case MD_BLOCK_TH:
        case MD_BLOCK_TD: {
            const MD_BLOCK_TD_DETAIL* d = static_cast<const MD_BLOCK_TD_DETAIL*>(detail);
            tableCol++;
            if (table->columns() <= tableCol) table->appendColumns(1);
            tableColumns = qMax(tableColumns, tableCol + 1);
            QTextTableCell cell = table->cellAt(tableRows - 1, tableCol);
            if (!cell.isValid()) return 1;
            cursor = cell.firstCursorPosition();
            if (type == MD_BLOCK_TH) {
                QTextCharFormat f = cell.format();
                f.setFontWeight(QFont::Bold);
                cell.setFormat(f);
            }
            // the header cells too, as GitHub aligns them
            QTextBlockFormat bf = cursor.blockFormat();
            bf.setAlignment(cellAlignment(d->align));
            cursor.setBlockFormat(bf);
            break;
        }
        case MD_BLOCK_HR: {
            QTextBlockFormat bf;
            bf.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth, 1);
            cursor.insertBlock(bf, QTextCharFormat());
            break;
        }
        default:
            break;
        }
        return 0;
    }

    int onLeaveBlock(MD_BLOCKTYPE type, void*)
    {
        // the HTML of a block never waits past it, closed or not
        flushHtml(true);
        switch (type) {
        case MD_BLOCK_P:
            listItem = false;
            break;
        case MD_BLOCK_UL:
        case MD_BLOCK_OL:
            if (needsInsertList) lists.push(cursor.createList(listFormat));
            needsInsertList = false;
            if (!lists.isEmpty()) lists.pop();
            break;
        case MD_BLOCK_TR: {
            // a run of empty cells joins the filled one before it
            int mergeEnd = -1, mergeBegin = -1;
            for (int col = tableCol; col >= 0; col--) {
                if (nonEmptyCells.contains(col)) {
                    if (mergeEnd >= 0 && mergeBegin >= 0)
                        table->mergeCells(tableRows - 1, mergeBegin - 1, 1, mergeEnd - mergeBegin + 2);
                    mergeEnd = mergeBegin = -1;
                } else if (mergeEnd < 0) {
                    mergeEnd = col;
                } else {
                    mergeBegin = col;
                }
            }
            break;
        }
        case MD_BLOCK_TABLE:
            table = nullptr;
            cursor.movePosition(QTextCursor::End);
            break;
        case MD_BLOCK_H:
            cursor.setCharFormat(QTextCharFormat());
            break;
        case MD_BLOCK_QUOTE:
            blockQuoteDepth--;
            needsInsertBlock = true;
            break;
        case MD_BLOCK_CODE:
            codeBlock = false;
            codeLanguage.clear();
            codeFence.clear();
            needsInsertBlock = true;
            break;
        default:
            break;
        }
        return 0;
    }

    int onEnterSpan(MD_SPANTYPE type, void* detail)
    {
        QTextCharFormat f = spanFormats.isEmpty() ? QTextCharFormat() : spanFormats.top();
        switch (type) {
        case MD_SPAN_EM:
            f.setFontItalic(true);
            break;
        case MD_SPAN_STRONG:
            f.setFontWeight(QFont::Bold);
            break;
        case MD_SPAN_DEL:
            f.setFontStrikeOut(true);
            break;
        case MD_SPAN_U:
            f.setFontUnderline(true);
            break;
        case MD_SPAN_A: {
            const MD_SPAN_A_DETAIL* d = static_cast<const MD_SPAN_A_DETAIL*>(detail);
            f.setAnchor(true);
            f.setAnchorHref(attribute(d->href));
            f.setForeground(QGuiApplication::palette().link());
            if (d->title.size > 0) f.setToolTip(attribute(d->title));
            break;
        }
        case MD_SPAN_CODE:
            f.setFont(monoFont);
            f.setFontFixedPitch(true);
            break;
        case MD_SPAN_IMG: {
            if (imageDepth++ == 0) {
                const MD_SPAN_IMG_DETAIL* d = static_cast<const MD_SPAN_IMG_DETAIL*>(detail);
                imageSource = attribute(d->src);
                imageTitle = attribute(d->title);
                imageAlt.clear();
            }
            break;
        }
        case MD_SPAN_LATEXMATH:
        case MD_SPAN_LATEXMATH_DISPLAY:
            if (imageDepth == 0) {
                inMath = true;
                mathDisplay = type == MD_SPAN_LATEXMATH_DISPLAY;
                mathTex.clear();
            }
            break;
        default:
            break;
        }
        spanFormats.push(f);
        cursor.setCharFormat(f);
        return 0;
    }

    int onLeaveSpan(MD_SPANTYPE type, void*)
    {
        if (!spanFormats.isEmpty()) spanFormats.pop();
        QTextCharFormat f = spanFormats.isEmpty() ? QTextCharFormat() : spanFormats.top();
        if (type == MD_SPAN_IMG && --imageDepth == 0) {
            insertPendingBlock();
            QTextCharFormat object = markdownImageFormat(f, imageSource, imageAlt, baseDir);
            if (!imageTitle.isEmpty()) object.setToolTip(imageTitle);
            cursor.insertText(QString(QChar::ObjectReplacementCharacter), object);
            markCellFilled();
        } else if ((type == MD_SPAN_LATEXMATH || type == MD_SPAN_LATEXMATH_DISPLAY) && inMath) {
            inMath = false;
            insertPendingBlock();
            cursor.insertText(QString(QChar::ObjectReplacementCharacter),
                              markdownMathFormat(f, mathTex, mathDisplay));
            markCellFilled();
        }
        cursor.setCharFormat(f);
        return 0;
    }

    int onText(MD_TEXTTYPE type, QString s)
    {
        if (imageDepth > 0) {
            imageAlt += type == MD_TEXT_SOFTBR || type == MD_TEXT_BR ? QString(" ") : s;
            return 0;
        }
        if (inMath) {
            mathTex += type == MD_TEXT_SOFTBR || type == MD_TEXT_BR ? QString(" ") : s;
            return 0;
        }
        insertPendingBlock();
        if (type == MD_TEXT_HTML && !isForeignTag(s)) {
            // the tags are counted and the HTML inserted once they close; a void
            // element closes itself
            s = closeVoidTags(s);
            static const QRegularExpression opening("<[a-zA-Z]");
            static const QRegularExpression closing("(/>|</)");
            htmlDepth += s.count(opening) - s.count(closing);
            htmlBuffer += s;
            flushHtml(false);
            return 0;
        }
        if (htmlDepth > 0) {
            // inside the tags every text waits for them in its order
            htmlBuffer += htmlText(type, s);
            return 0;
        }
        switch (type) {
        case MD_TEXT_NULLCHAR:
            s = QString(QChar(QChar::ReplacementCharacter));
            break;
        case MD_TEXT_BR:
            s = QString("\n");
            break;
        case MD_TEXT_SOFTBR:
            s = QString(" ");
            break;
        case MD_TEXT_ENTITY:
            cursor.insertHtml(s);
            s.clear();
            break;
        default:
            // a tag that is not text formatting stays as it is written
            break;
        }
        if (!s.isEmpty()) {
            cursor.insertText(s);
            markCellFilled();
        }
        if (!lists.isEmpty()) {
            QTextBlockFormat bf = cursor.blockFormat();
            bf.setIndent(0);
            cursor.setBlockFormat(bf);
        }
        return 0;
    }

    // a text inside HTML tags as HTML
    static QString htmlText(MD_TEXTTYPE type, const QString& s)
    {
        switch (type) {
        case MD_TEXT_ENTITY:   return s;
        case MD_TEXT_BR:       return QString("<br/>");
        case MD_TEXT_SOFTBR:   return QString(" ");
        case MD_TEXT_NULLCHAR: return QString(QChar(QChar::ReplacementCharacter));
        case MD_TEXT_CODE:     return "<code>" + s.toHtmlEscaped() + "</code>";
        default:               return s.toHtmlEscaped();
        }
    }

    // the held HTML inserted when its tags close, or at once when forced
    void flushHtml(bool force)
    {
        if (htmlBuffer.isEmpty() || (htmlDepth > 0 && !force)) return;
        htmlDepth = 0;
        cursor.insertHtml(htmlBuffer);
        htmlBuffer.clear();
        cursor.setCharFormat(spanFormats.isEmpty() ? QTextCharFormat() : spanFormats.top());
        markCellFilled();
    }

    void markCellFilled()
    {
        if (table && (blockType == MD_BLOCK_TD || blockType == MD_BLOCK_TH) && !nonEmptyCells.contains(tableCol))
            nonEmptyCells.append(tableCol);
    }

    void insertPendingBlock()
    {
        if (needsInsertBlock) insertBlock();
    }

    void insertBlock()
    {
        QTextCharFormat cf = spanFormats.isEmpty() ? QTextCharFormat() : spanFormats.top();
        QTextBlockFormat bf;
        if (!lists.isEmpty() && !needsInsertList && listItem) {
            QTextList* list = lists.top();
            if (list && list->count() > 0) bf = list->item(list->count() - 1).blockFormat();
        }
        if (blockQuoteDepth > 0) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            bf.setProperty(QTextFormat::BlockQuoteLevel, blockQuoteDepth);
#endif
            bf.setLeftMargin(BLOCK_QUOTE_INDENT * blockQuoteDepth);
            bf.setRightMargin(BLOCK_QUOTE_INDENT);
        }
        if (codeBlock) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            bf.setProperty(QTextFormat::BlockCodeLanguage, codeLanguage);
            if (!codeFence.isEmpty()) bf.setProperty(QTextFormat::BlockCodeFence, codeFence);
#endif
            cf.setFont(monoFont);
        } else {
            bf.setTopMargin(paragraphMargin);
            bf.setBottomMargin(paragraphMargin);
        }
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        if (markerType == QTextBlockFormat::MarkerType::NoMarker) bf.clearProperty(QTextFormat::BlockMarker);
        else bf.setMarker(markerType);
#endif
        if (!lists.isEmpty()) bf.setIndent(lists.count());
        if (doc->isEmpty()) {
            cursor.setBlockFormat(bf);
            cursor.setCharFormat(cf);
        } else if (listItem) {
            cursor.insertBlock(bf, QTextCharFormat());
            cursor.setCharFormat(cf);
        } else {
            cursor.insertBlock(bf, cf);
        }
        if (needsInsertList) {
            lists.push(cursor.createList(listFormat));
        } else if (listItem && !lists.isEmpty() && lists.top()) {
            lists.top()->add(cursor.block());
        }
        needsInsertList = false;
        needsInsertBlock = false;
    }

    QTextDocument*          doc;
    QTextCursor             cursor;
    QString                 baseDir;
    QFont                   monoFont;
    qreal                   paragraphMargin = 0;
    QStack<QTextCharFormat> spanFormats;
    QStack<QTextList*>      lists;
    QTextListFormat         listFormat;
    bool                    needsInsertList = false;
    bool                    needsInsertBlock = false;
    bool                    listItem = false;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QTextBlockFormat::MarkerType markerType = QTextBlockFormat::MarkerType::NoMarker;
#endif
    int                     blockQuoteDepth = 0;
    bool                    codeBlock = false;
    QString                 codeLanguage;
    QString                 codeFence;
    MD_BLOCKTYPE            blockType = MD_BLOCK_DOC;
    QTextTable*             table = nullptr;
    int                     tableRows = 0;
    int                     tableColumns = 0;
    int                     tableCol = -1;
    QVector<int>            nonEmptyCells;
    int                     imageDepth = 0;
    QString                 imageSource;
    QString                 imageTitle;
    QString                 imageAlt;
    bool                    inMath = false;
    bool                    mathDisplay = false;
    QString                 mathTex;
    int                     htmlDepth = 0;
    QString                 htmlBuffer;
};

} // namespace

void readMarkdownMd4c(QTextDocument* doc, const QString& text, const QString& baseDir)
{
    Md4cReader reader(doc, baseDir);
    reader.read(text);
}
