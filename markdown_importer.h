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

#ifndef CYBERIADA_MARKDOWN_IMPORTER
#define CYBERIADA_MARKDOWN_IMPORTER

#include <QString>

class QTextDocument;

// a tag of an element that is neither void nor text formatting (<rundir>): text
bool isForeignTag(const QString& html);
// the void elements closed (<br> -> <br/>), the other tags as they are
QString closeVoidTags(const QString& html);

// the md4c reader (docs/MARKDOWN.md, Reading): GitHub dialect with the formulas,
// the formats of Qt's own reader
void readMarkdownMd4c(QTextDocument* doc, const QString& text, const QString& baseDir);

// the document filled by the md4c reader (EDIT-MODE-17), the image paths taken
// relative to baseDir, a display formula alone in its paragraph centred
void importMarkdown(QTextDocument* doc, const QString& text, const QString& baseDir);

#endif
