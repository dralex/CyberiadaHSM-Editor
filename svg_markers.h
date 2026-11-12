/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The SVG markers drawn as plain groups for QtSvg
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

#ifndef CYBERIADA_SVG_MARKERS
#define CYBERIADA_SVG_MARKERS

#include <QByteArray>
#include <QPointF>
#include <QString>

// a vertex of a path where a marker stands and the direction of the path there
struct SvgPathEnd {
    QPointF point;
    QPointF direction;
    bool    valid = false;
};

// the first and the last vertex of the path data (M L H V C S Q T A Z, absolute
// and relative), the directions taken from the nearest segment of non-zero length
void svgPathEnds(const QString& d, SvgPathEnd* start, SvgPathEnd* end);

// Qt 5 draws SVG Tiny, which has no markers: each marker-start and marker-end of a
// path, line, polyline or polygon becomes a group after that element, placed and
// turned as the marker would be (docs/MARKDOWN.md, SVG images); a file without
// markers is returned as it is
QByteArray expandSvgMarkers(const QByteArray& svg);

#endif
