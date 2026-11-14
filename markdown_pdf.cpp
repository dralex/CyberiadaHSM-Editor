/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The rendered Markdown text exported to PDF
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

#include <QAbstractTextDocumentLayout>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPicture>
#include <QTextDocument>

#include "markdown_pdf.h"
#include "markdown_importer.h"
#include "markdown_objects.h"

namespace {

const qreal PAGE_MARGIN_MM = 20;
// the band of the page number under the text, in lines of the text font
const qreal NUMBER_BAND_LINES = 2;

} // namespace

int exportMarkdownPdf(const QString& text, const QString& baseDir, const QString& title,
                      const QString& path, QString* error)
{
    QPdfWriter writer(path);
    writer.setTitle(title);
    writer.setCreator(QCoreApplication::applicationName());
    // the resolution of the screen, the one the formula pictures are recorded at
    writer.setResolution(QPicture().logicalDpiY());
    writer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Portrait,
                                     QMarginsF(PAGE_MARGIN_MM, PAGE_MARGIN_MM,
                                               PAGE_MARGIN_MM, PAGE_MARGIN_MM),
                                     QPageLayout::Millimeter));
    QPainter painter;
    if (!painter.begin(&writer)) {
        if (error) *error = QCoreApplication::translate("MarkdownPdf", "cannot write the PDF %1").arg(path);
        return 0;
    }
    QRectF paint = writer.pageLayout().paintRectPixels(writer.resolution());

    // laid out for the writer, the pages its paint rect less the number band
    QTextDocument doc;
    doc.documentLayout()->setPaintDevice(&writer);
    doc.setDocumentMargin(0);
    qreal band = QFontMetricsF(doc.defaultFont(), &writer).height() * NUMBER_BAND_LINES;
    QSizeF page(paint.width(), paint.height() - band);
    doc.setPageSize(page);
    setMarkdownColors(&doc, Qt::black, Qt::white);
    importMarkdown(&doc, text, baseDir);

    // the text black whatever the theme of the application
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, Qt::black);
    int pages = doc.pageCount();
    for (int i = 0; i < pages; i++) {
        if (i > 0) writer.newPage();
        QRectF clip(QPointF(0, i * page.height()), page);
        painter.save();
        painter.translate(0, -clip.top());
        painter.setClipRect(clip);
        context.clip = clip;
        doc.documentLayout()->draw(&painter, context);
        drawMarkdownQuotes(&painter, &doc);
        painter.restore();
        painter.setPen(Qt::black);
        painter.setFont(doc.defaultFont());
        painter.drawText(QRectF(0, page.height(), page.width(), band), Qt::AlignCenter,
                         QString::number(i + 1));
    }
    painter.end();
    if (QFileInfo(path).size() <= 0) {
        if (error) *error = QCoreApplication::translate("MarkdownPdf", "cannot write the PDF %1").arg(path);
        return 0;
    }
    return pages;
}
