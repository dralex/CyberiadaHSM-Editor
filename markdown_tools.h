/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The authoring tools of a Markdown text: the paths, the copies and the lines
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

#ifndef CYBERIADA_MARKDOWN_TOOLS
#define CYBERIADA_MARKDOWN_TOOLS

#include <functional>

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QRect>
#include <QString>

#include "svg_source.h"

// docs/MARKDOWN.md, Tools; EDIT-MD-2..4

// the folder an image is saved into when no other was used (markdown/imageFolder)
extern const char* const MARKDOWN_IMAGE_FOLDER;
// the folder a diagram is copied into when no other was used (markdown/diagramFolder)
extern const char* const MARKDOWN_DIAGRAM_FOLDER;
// the name of a pasted image: image-<n>.png
extern const char* const MARKDOWN_PASTED_IMAGE;
// the quality of an edited JPEG
const int MARKDOWN_JPEG_QUALITY = 90;
// an image turns by quarters
const int IMAGE_QUARTER_TURN = 90;
const int IMAGE_FULL_TURN = 360;

// a path in the directory or in a subfolder of it
bool isInsideDirectory(const QString& path, const QString& dir);
// the path relative to the directory of the text with '/'; <path> with a space or a parenthesis
QString markdownPath(const QString& path, const QString& baseDir);
// an alt or a link text with its '\', '[' and ']' escaped
QString markdownLabel(const QString& text);
// ![alt](path), the path relative to the directory of the text
QString markdownImageLine(const QString& alt, const QString& path, const QString& baseDir);
// the first free dir/<base>-<n>.<suffix>, n from 1
QString numberedFileName(const QString& dir, const QString& base, const QString& suffix);

// an image the dialog edits: a format Qt reads, not an SVG (EDIT-MD-3)
bool isEditableImage(const QString& path);
bool isSvgFile(const QString& path);
// an image file Insert Image takes: an editable one or an SVG
bool isInsertableImage(const QString& path);
// the file dialog filter of the images Insert Image takes
QString imageFileFilter();

// the edits of a raster image, applied in this order: the rotation, the crop of the
// rotated image, the width limit
struct ImageEdits {
    // clockwise, a multiple of 90
    int   rotation = 0;
    // null: the whole image
    QRect crop;
    // 0: no limit
    int   maxWidth = 0;
    bool  isEmpty() const { return rotation % IMAGE_FULL_TURN == 0 && crop.isNull() && maxWidth <= 0; }
};
QImage applyImageEdits(const QImage& image, const ImageEdits& edits);

// what Insert Image writes and inserts
struct ImageInsert {
    // the image file; empty for a pasted image
    QString    source;
    QImage     pasted;
    ImageEdits edits;
    // the directory of the text
    QString    baseDir;
    // the file the line names: the source when it is linked in place
    QString    target;
    QString    alt;
    bool       isPasted() const { return source.isEmpty(); }
    // a new file is written: a pasted or an edited image, or one outside the directory
    bool       writesFile() const;
};

// the file Insert Image proposes (EDIT-MD-2/3): the source itself when it lies inside the
// directory and is not edited; an edited one beside it as <name>-<n>.<ext>; else
// <folder>/<name> of the directory, a pasted one image-<n>.png
QString proposeImageTarget(const ImageInsert& insert, const QString& folder);
// the bytes written: the source as it is, or the edited image in the target's format
// (PNG for a format Qt cannot write)
bool imageInsertData(const ImageInsert& insert, QByteArray* data, QString* format, QString* error);
// the target exists with another content than data
bool imageTargetClashes(const ImageInsert& insert, const QByteArray& data);
// write the target (its folder made), nothing when it is the source or holds data already
bool writeImageInsert(const ImageInsert& insert, const QByteArray& data, QString* error);

// two files of the same bytes
bool sameFileContent(const QString& a, const QString& b);
// the file copied over the target (its folder made), nothing when it holds the same bytes
bool copyFileOver(const QString& source, const QString& target, QString* error);

// an image of a Markdown text, `![alt](target)`, perhaps the whole text of a link
struct MarkdownImage {
    // of the '!', or of the '[' of the link holding it, from 1
    int     line = 0;
    int     column = 0;
    QString alt;
    // as written, without <>
    QString target;
    // the target of a link holding the image alone, empty without one
    QString link;
};
// the images of the text; the code fences and the code spans are skipped
QList<MarkdownImage> markdownImages(const QString& text);

// a diagram image of a text (EDIT-MD-6): the diagram of its link, else the one its SVG
// records, else the .graphml beside the SVG of its base name
struct DiagramImage {
    MarkdownImage image;
    QString       svg;
    QString       graphml;
    // what the SVG records about its source
    SvgSource     recorded;
    // the recorded digest is the one of the diagram file
    bool          upToDate = false;
};
QList<DiagramImage> markdownDiagramImages(const QString& text, const QString& baseDir);
// [![alt](svg)](graphml), the paths relative to the directory of the text (EDIT-MD-5)
QString markdownDiagramLine(const QString& alt, const QString& svg, const QString& graphml,
                            const QString& baseDir);
// the name a diagram file proposes for its image: the document name, else the title of
// its first state machine, else the file's base name
QString diagramFileTitle(const QString& graphml);
// the diagram file drawn into the SVG off any tab, in the font (the export font when empty),
// the SVG recording its source (EDIT-IO-8, EDIT-MD-5/7)
bool renderDiagramFile(const QString& graphml, const QString& svg, const QString& font, QString* error);

// what Update Diagram Images did to an image (EDIT-MD-7)
struct DiagramUpdate {
    DiagramImage image;
    bool         updated = false;
    QString      error;
};
// the out-of-date images, every one with all, exported again once per SVG, each in the font it
// records, else in the font given
QList<DiagramUpdate> updateDiagramImages(const QList<DiagramImage>& images, bool all, const QString& font);

// a target of a Markdown text: [text](target), ![alt](target) or [label]: target
struct MarkdownLink {
    // of the '[' or the '!', from 1
    int     line = 0;
    int     column = 0;
    // as written, without <>
    QString target;
    bool    image = false;
};
// the links, the images and the link definitions; the code fences and the code spans skipped
QList<MarkdownLink> markdownLinks(const QString& text);

// a heading of a Markdown text, its anchor numbered as the rendered view numbers it
struct MarkdownHeading {
    int     level = 0;
    QString title;
    QString anchor;
    // from 1
    int     line = 0;
};
// the ATX and the setext headings outside the code fences
QList<MarkdownHeading> markdownHeadings(const QString& text);

// a broken link of a Markdown text (EDIT-MD-8)
struct MarkdownFinding {
    int     line = 0;
    int     column = 0;
    QString message;
};
// the missing files and images, the anchors no heading has, the out-of-date diagram images and
// the ones linked to another diagram than they record; openText gives the text of a file's
// open tab, a null string without one; a site is never fetched
QList<MarkdownFinding> markdownCheck(const QString& text, const QString& baseDir,
                                     const std::function<QString(const QString&)>& openText);

#endif
