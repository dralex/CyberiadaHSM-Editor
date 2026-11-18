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

#include <QByteArray>
#include <QImage>
#include <QRect>
#include <QString>

// docs/MARKDOWN.md, Tools; EDIT-MD-2..4

// the folder an image is saved into when no other was used (markdown/imageFolder)
extern const char* const MARKDOWN_IMAGE_FOLDER;
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

#endif
