/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The geometry of the edges: the transitions and the comment links
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

#ifndef CYBERIADA_EDGE_GEOMETRY_H
#define CYBERIADA_EDGE_GEOMETRY_H

#include <QLineF>
#include <QPointF>
#include <QPainterPath>
#include <QVector>

class CyberiadaSMEditorAbstractItem;
class QGraphicsTextItem;

namespace EdgeGeometry {

// QLineF::intersect was deprecated in favour of intersects in Qt 5.14
QLineF::IntersectType lineIntersect(const QLineF& a, const QLineF& b, QPointF* point);

// the border point of a node on the ray start -> end in scene coordinates (forwardOnly:
// not behind start); a vertex attaches to its circle, a choice to the rhombus tip that
// faces the other end
QPointF findIntersectionWithItem(const CyberiadaSMEditorAbstractItem* item,
                                 const QPointF& start, const QPointF& end,
                                 bool* hasIntersections, bool forwardOnly = false);

// the attachment of an end without a stored point: the node border toward a scene
// point, relative to the node centre; a point at the centre gives the centre, or the
// right side with sideWhenCentred (a comment link stays on the border, EDIT-EDGE-17)
QPointF attachedPoint(const CyberiadaSMEditorAbstractItem* item, const QPointF& toward,
                      bool sideWhenCentred = false);

// put a centre-relative point exactly on the border of a rect of the given
// half-extents (clamp inside, then snap to the nearest edge)
void clampOntoBorder(QPointF& p, qreal hw, qreal hh);

// re-base a centre-relative end after its node was resized: side and d tell the
// centre displacement; the absolute place is kept and the end put back on the new border
// a link end on a choice goes back onto its rhombus (EDIT-EDGE-17)
void rebaseOnResize(int side, QPointF& point, qreal d, const CyberiadaSMEditorAbstractItem* item,
                    bool link = false);

// a centre-relative bend of a self-loop after its node was resized from the old to the
// new half-extents: a bend beyond a side moves with that side, a bend within the node's
// span on an axis moves by -shift (the centre move: it keeps its absolute place) (EDIT-EDGE-22)
QPointF rebaseLoopBend(const QPointF& v, qreal oldHw, qreal oldHh, qreal newHw, qreal newHh,
                       const QPointF& shift);

// the border point of a node nearest to a scene point: a rect node clamps it onto
// its border, a vertex or a choice attaches toward it
QPointF borderNearest(const CyberiadaSMEditorAbstractItem* item, const QPointF& p);

// the rhombus of a choice inscribed in a rect of the given half-extents, centre-relative:
// the border point on the ray toward a point, and the border point nearest to a point
QPointF rhombusToward(const QPointF& toward, qreal hw, qreal hh);
QPointF rhombusNearest(const QPointF& p, qreal hw, qreal hh);

// a comment link end: as attachedPoint (the side when centred) and borderNearest, except
// on a choice, whose comment point lies anywhere on the rhombus border, not only on the
// vertices a transition takes (EDIT-EDGE-4/17)
QPointF linkAttachedPoint(const CyberiadaSMEditorAbstractItem* item, const QPointF& toward);
QPointF linkBorderNearest(const CyberiadaSMEditorAbstractItem* item, const QPointF& p);

// the point of a path nearest to p; endMargin of the first and the last segment
// and the avoided rect stay free (a link foot keeps off the arrow and the label,
// as libhtreegeom places it)
QPointF nearestOnPath(const QPainterPath& path, const QPointF& p, qreal endMargin = 0.0,
                      const QRectF& avoid = QRectF());

// the underline of the characters [from, to) of a text item in scene coordinates,
// one segment per laid-out line the range covers (a fragment, EDIT-EDGE-21)
QVector<QLineF> textRangeLines(const QGraphicsTextItem* item, int from, int to);

}

#endif
