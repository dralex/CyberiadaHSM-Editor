/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The outline of a Markdown text: its headings and its images in the order of the text
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

#ifndef CYBERIADA_MARKDOWN_OUTLINE
#define CYBERIADA_MARKDOWN_OUTLINE

#include <QList>
#include <QString>

class DiagramFileCache;

// docs/MARKDOWN.md, Outline; EDIT-MD-13

// a row of the outline: the document, a heading, an image or a diagram image of the text
struct OutlineEntry {
    enum Kind { outlineDocument = 0, outlineHeading, outlineImage, outlineDiagram };
    Kind    kind = outlineDocument;
    // 1..6 for a heading, 0 otherwise
    int     level = 0;
    // the heading's line, the '!' of an image (the '[' of a link holding it), from 1
    int     line = 0;
    int     column = 0;
    // the document name, the title of a heading, the alt text of an image
    QString title;
    QString anchor;
    // an image as written, without <>; the target of a link holding it alone
    QString target;
    QString link;
    // a diagram image (EDIT-MD-6): its files, absolute, and its SVG drawn from the diagram as it is
    QString svg;
    QString graphml;
    bool    upToDate = false;

    bool    isImage() const { return kind == outlineImage || kind == outlineDiagram; }
    // the text of the row: the title, the file name of an image without an alt text
    QString label() const;
    // what the rows of two readings are matched by (EDIT-MD-14): a heading by its title, an image
    // by its target
    QString key() const;
    // the title of a heading, the target of an image: what a row names (EDIT-MD-16)
    QString name() const;
    bool    operator==(const OutlineEntry& other) const;
    bool    operator!=(const OutlineEntry& other) const { return !(*this == other); }
};

// the headings and the images of the text in its order, an image on the line of a heading after
// it; the diagram files read through the cache when there is one
QList<OutlineEntry> markdownOutline(const QString& text, const QString& baseDir,
                                    DiagramFileCache* cache = nullptr);

#endif
