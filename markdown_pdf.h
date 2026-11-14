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

#ifndef CYBERIADA_MARKDOWN_PDF
#define CYBERIADA_MARKDOWN_PDF

#include <QString>

// the rendered text on A4 pages with their numbers (EDIT-MODE-18, docs/MARKDOWN.md, PDF), its
// images relative to baseDir; the number of the pages written, 0 with the error set when the
// file cannot be written
int exportMarkdownPdf(const QString& text, const QString& baseDir, const QString& title,
                      const QString& path, QString* error);

#endif
