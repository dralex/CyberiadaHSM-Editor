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

#ifndef CYBERIADA_MARKDOWN_IMPORTER
#define CYBERIADA_MARKDOWN_IMPORTER

#include <QList>
#include <QString>

class QTextDocument;

// a formula of the text: [begin, end) with its dollars, the TeX between them
struct MathSpan {
    int     begin = 0;
    int     end = 0;
    bool    display = false;
    QString tex;
};

// an image of the text in the document order; emptyAlt is the place for an alt
// text when the image has none and its source is written inline, else -1
struct ImageSpan {
    QString source;
    QString title;
    QString alt;
    int     chunks = 0;
    int     emptyAlt = -1;
};

struct MarkdownScan {
    QList<MathSpan>  math;
    QList<ImageSpan> images;
    // the '>' of each inline void element written without '/' (<br>) and the '<' of
    // each tag of an element that is not HTML text formatting (<rundir>): Qt's
    // reader waits for their closing tags and loses the text after them
    QList<int>       openVoidTags;
    QList<int>       foreignTags;
};

// an HTML void element written as an opening tag (<br>, <img src=x>)
bool isOpenVoidTag(const QString& html);
// a tag of an element that is neither void nor text formatting: shown as text
bool isForeignTag(const QString& html);
// the void elements closed (<br> -> <br/>), the other tags as they are
QString closeVoidTags(const QString& html);

// the formulas and the images as md4c reads them (GitHub dialect, the math
// spans); the code is never a formula
MarkdownScan scanMarkdown(const QString& text);

// the readers (docs/MARKDOWN.md, Reading); both give the same structure
void readMarkdownMd4c(QTextDocument* doc, const QString& text, const QString& baseDir);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
void readMarkdownQt(QTextDocument* doc, const QString& text, const QString& baseDir);
#endif

// the document filled by the reader of the build (EDIT-MODE-17), the image paths
// taken relative to baseDir, a display formula alone in its paragraph centred
void importMarkdown(QTextDocument* doc, const QString& text, const QString& baseDir);

#endif
