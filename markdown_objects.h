/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The formulas and the images of a rendered Markdown document
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

#ifndef CYBERIADA_MARKDOWN_OBJECTS
#define CYBERIADA_MARKDOWN_OBJECTS

#include <QColor>
#include <QString>
#include <QTextCharFormat>
#include <QTextFormat>

class QTextDocument;

// the objects the Markdown readers put into a document (docs/MARKDOWN.md)
enum MarkdownObjectType {
    MarkdownMathObject = QTextFormat::UserObject + 1,
    MarkdownImageObject
};

enum MarkdownObjectProperty {
    MarkdownTex = QTextFormat::UserProperty + 1,
    MarkdownDisplay,
    // the absolute path of the image file, empty when it is not found
    MarkdownImagePath,
    // the image as the text names it, and its alt text
    MarkdownImageSource,
    MarkdownImageAlt
};

// a formula or an image in the format of the text around it
QTextCharFormat markdownMathFormat(const QTextCharFormat& base, const QString& tex, bool display);
QTextCharFormat markdownImageFormat(const QTextCharFormat& base, const QString& source,
                                    const QString& alt, const QString& baseDir);

// the absolute path of an image the text names, relative to baseDir; empty for a
// URL that is not a file and for a relative path without a base directory
QString markdownImagePath(const QString& source, const QString& baseDir);

// the drawing of the objects on the layout of the document, set once per layout
void installMarkdownObjects(QTextDocument* doc);

// the colour of the formulas whose text sets none: the text colour of the view
void setMarkdownTextColor(QTextDocument* doc, const QColor& color);

#endif
