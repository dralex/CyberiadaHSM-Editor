/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The diagram an exported SVG is drawn from, recorded in its metadata
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

#ifndef CYBERIADA_SVG_SOURCE
#define CYBERIADA_SVG_SOURCE

#include <QByteArray>
#include <QString>

// EDIT-IO-8, docs/MARKDOWN.md, Tools: an RDF description of the SVG in its <metadata>,
// Dublin Core dc:source and dc:format, the digest and the font in the editor's namespace
extern const char* const SVG_SOURCE_NAMESPACE;

// what an SVG records about the diagram it is drawn from
struct SvgSource {
    // the diagram file, absolute; empty when the SVG records none
    QString graphml;
    // sha256:<hex> of the diagram file drawn; empty without one
    QString digest;
    // cyb:font, else the first font-family the drawing names
    QString font;
};

// sha256:<hex> of the file, empty when it cannot be read
QString fileDigest(const QString& path);
// the SVG of QSvgGenerator with the title, the description and the metadata of its source;
// the source path is written relative to the SVG
QByteArray recordSvgSource(const QByteArray& svg, const QString& svgPath, const QString& graphml,
                           const QString& title, const QString& font);
SvgSource readSvgSource(const QString& svgPath);

#endif
