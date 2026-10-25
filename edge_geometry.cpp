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

#include <limits>
#include <QPainterPath>
#include <QPolygonF>
#include <QRectF>
#include <QFontMetricsF>
#include <QGraphicsTextItem>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QAbstractTextDocumentLayout>

#include "edge_geometry.h"
#include "cyberiadasm_editor_items.h"
#include "cyberiada_constants.h"

QLineF::IntersectType EdgeGeometry::lineIntersect(const QLineF& a, const QLineF& b, QPointF* point)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    return a.intersects(b, point);
#else
    return a.intersect(b, point);
#endif
}

QPointF EdgeGeometry::findIntersectionWithItem(const CyberiadaSMEditorAbstractItem *item,
                                               const QPointF& start, const QPointF& end,
                                               bool* hasIntersections,
                                               bool forwardOnly)
{
    if (!item) return QPointF();

    int margin = 60;
    QRectF adjustedRect = item->sceneBoundingRect().adjusted(-margin, -margin, margin, margin);

    if (!adjustedRect.contains(start)) {
        *hasIntersections = false;
        return QPointF();
    }

    QPointF dir = end - start;
    if (dir.isNull()) {
        return QPointF();
    }

    // a pseudostate (initial/final/terminate) is a circle: attach on its border,
    // on the side facing the other endpoint, not at the centre
    if (item->type() == CyberiadaSMEditorAbstractItem::VertexItem) {
        QPointF c = item->sceneBoundingRect().center();
        // the connected element is the ray end farther from the centre (the
        // caller passes either centre->toward or an external point->centre)
        QPointF ext = QLineF(start, c).length() >= QLineF(end, c).length() ? start : end;
        QPointF facing = ext - c;
        *hasIntersections = true;
        qreal len = QLineF(QPointF(0, 0), facing).length();
        if (len == 0) return c;
        return c + facing / len * VERTEX_POINT_RADIUS;
    }

    // a choice is drawn as a rhombus inscribed in its rect; a transition binds
    // to the tip (a rect-edge midpoint) that faces its other end, whatever the
    // rect proportions are
    if (item->type() == CyberiadaSMEditorAbstractItem::ChoiceItem) {
        QRectF r = item->sceneBoundingRect();
        QPointF c = r.center();
        const QPointF tips[4] = {
            QPointF(c.x(), r.top()),      // top
            QPointF(r.right(), c.y()),    // right
            QPointF(c.x(), r.bottom()),   // bottom
            QPointF(r.left(), c.y())      // left
        };
        // the connected element is the ray end farther from the centre: the
        // caller passes either centre->toward or an external point->centre
        QPointF ext = QLineF(start, c).length() >= QLineF(end, c).length() ? start : end;
        QPointF facing = ext - c;
        if (facing.isNull()) {
            *hasIntersections = true;
            return c;
        }
        int best = 0;
        qreal bestDot = -std::numeric_limits<qreal>::max();
        for (int i = 0; i < 4; ++i) {
            QPointF td = tips[i] - c;
            qreal len = QLineF(c, tips[i]).length();
            if (len == 0) continue;
            qreal dot = QPointF::dotProduct(td, facing) / len;
            if (dot > bestDot) {
                bestDot = dot;
                best = i;
            }
        }
        *hasIntersections = true;
        return tips[best];
    }

    QLineF rayForward(start, start + dir * 1e5);
    QLineF rayBackward(start, start - dir * 1e5);

    QPainterPath shape = item->shape();
    QPainterPath sceneShape = item->sceneTransform().map(shape);
    QPolygonF polygon = sceneShape.toFillPolygon();

    QPointF closestIntersection;
    qreal minDist = std::numeric_limits<qreal>::max();

    for (int i = 0; i < polygon.size(); ++i) {
        QPointF p1 = polygon[i];
        QPointF p2 = polygon[(i + 1) % polygon.size()];

        QLineF edge(p1, p2);
        QPointF intersectionPoint;

        // find closest intersection to start point
        if (lineIntersect(rayForward, edge, &intersectionPoint) == QLineF::BoundedIntersection) {
            qreal dist = QLineF(start, intersectionPoint).length();
            if (dist < minDist) {
                minDist = dist;
                closestIntersection = intersectionPoint;
            }
        }
        if (!forwardOnly &&
            lineIntersect(rayBackward, edge, &intersectionPoint) == QLineF::BoundedIntersection) {
            qreal dist = QLineF(start, intersectionPoint).length();
            if (dist < minDist) {
                minDist = dist;
                closestIntersection = intersectionPoint;
            }
        }
    }

    if (minDist < std::numeric_limits<qreal>::max()) {
        *hasIntersections = true;
        return closestIntersection;
    } else {
        *hasIntersections = false;
        return QPointF();
    }
}

QPointF EdgeGeometry::attachedPoint(const CyberiadaSMEditorAbstractItem* item, const QPointF& toward,
                                    bool sideWhenCentred)
{
    if (!item) return QPointF();
    QPointF centre = item->sceneBoundingRect().center();
    QPointF to = toward;
    if (toward == centre) {
        if (!sideWhenCentred) return QPointF();
        // coincident boxes give no direction: the right side, not the centre
        to = centre + QPointF(1.0, 0.0);
    }
    bool has = false;
    QPointF p = findIntersectionWithItem(item, centre, to, &has, true);
    return has ? p - centre : QPointF();
}

// a candidate foot at t of the segment a + t * d within [lo, hi], off the avoided rect
static void footCandidate(const QPointF& a, const QPointF& d, qreal t, qreal lo, qreal hi,
                          const QPointF& p, const QRectF& avoid, qreal* bestDist, QPointF* best)
{
    const qreal eps = 1e-6;
    if (t < lo - eps || t > hi + eps) return;
    QPointF q = a + d * t;
    if (!avoid.isNull() && avoid.adjusted(eps, eps, -eps, -eps).contains(q)) return;
    qreal dist = QPointF::dotProduct(q - p, q - p);
    if (dist < *bestDist) {
        *bestDist = dist;
        *best = q;
    }
}

QPointF EdgeGeometry::nearestOnPath(const QPainterPath& path, const QPointF& p, qreal endMargin,
                                    const QRectF& avoid)
{
    QPolygonF points;
    const QList<QPolygonF> parts = path.toSubpathPolygons();
    for (const QPolygonF& part : parts) points += part;
    if (points.isEmpty()) return p;
    QPointF best = points.first();
    qreal bestDist = std::numeric_limits<qreal>::max();
    for (int i = 0; i + 1 < points.size(); i++) {
        QPointF a = points[i], d = points[i + 1] - a;
        qreal len2 = QPointF::dotProduct(d, d);
        qreal lo = i == 0 ? endMargin : 0.0;
        qreal hi = i + 2 == points.size() ? 1.0 - endMargin : 1.0;
        qreal t = len2 > 0.0 ? QPointF::dotProduct(p - a, d) / len2 : 0.0;
        footCandidate(a, d, qBound(lo, t, hi), lo, hi, p, avoid, &bestDist, &best);
        if (avoid.isNull()) continue;
        // the nearest point under the rect: its border crossings and the ends
        footCandidate(a, d, lo, lo, hi, p, avoid, &bestDist, &best);
        footCandidate(a, d, hi, lo, hi, p, avoid, &bestDist, &best);
        if (qAbs(d.x()) > 1e-6) {
            footCandidate(a, d, (avoid.left() - a.x()) / d.x(), lo, hi, p, avoid, &bestDist, &best);
            footCandidate(a, d, (avoid.right() - a.x()) / d.x(), lo, hi, p, avoid, &bestDist, &best);
        }
        if (qAbs(d.y()) > 1e-6) {
            footCandidate(a, d, (avoid.top() - a.y()) / d.y(), lo, hi, p, avoid, &bestDist, &best);
            footCandidate(a, d, (avoid.bottom() - a.y()) / d.y(), lo, hi, p, avoid, &bestDist, &best);
        }
    }
    if (bestDist == std::numeric_limits<qreal>::max() && !avoid.isNull()) {
        // the rect covers the free middle: beside it, else the plain nearest
        if (endMargin > 0.0) {
            QPointF beside = nearestOnPath(path, p, 0.0, avoid);
            if (!avoid.adjusted(1e-6, 1e-6, -1e-6, -1e-6).contains(beside)) return beside;
        }
        return nearestOnPath(path, p, endMargin);
    }
    return best;
}

void EdgeGeometry::clampOntoBorder(QPointF& p, qreal hw, qreal hh)
{
    if (hw <= 0.0 || hh <= 0.0) return;
    qreal x = qBound(-hw, p.x(), hw);
    qreal y = qBound(-hh, p.y(), hh);
    if ((hh - qAbs(y)) <= (hw - qAbs(x))) {   // nearer the top/bottom edge -> snap y
        y = (y >= 0.0) ? hh : -hh;
    } else {                                  // nearer a side edge -> snap x
        x = (x >= 0.0) ? hw : -hw;
    }
    p = QPointF(x, y);
}

void EdgeGeometry::rebaseOnResize(int side, QPointF& point, qreal d, const CyberiadaSMEditorAbstractItem* item,
                                  bool link)
{
    if (!item) return;
    // 'd' is the CENTRE displacement on the flagged axis (not the edge delta, and
    // it does not say which edge moved). Keeping the endpoint's ABSOLUTE scene
    // position is rel -= centreDelta (abs = old_centre + rel; new_centre =
    // old_centre + d). The handle path sends Right/Bottom with signed d; the
    // containment-grow paths send the real side (incl. Left/Top) with positive d.
    qreal cx = 0.0, cy = 0.0;
    if (side & CyberiadaSMEditorAbstractItem::CornerFlags::Right)  cx += d;
    if (side & CyberiadaSMEditorAbstractItem::CornerFlags::Left)   cx -= d;
    if (side & CyberiadaSMEditorAbstractItem::CornerFlags::Bottom) cy += d;
    if (side & CyberiadaSMEditorAbstractItem::CornerFlags::Top)    cy -= d;
    point.rx() -= cx;
    point.ry() -= cy;
    // re-attach to the NEW border: a point on an unmoved (e.g. perpendicular)
    // border keeps its place, while a point whose edge moved away is pulled back
    QRectF r = item->sceneBoundingRect();   // already the new size at emit time
    if (link && item->type() == CyberiadaSMEditorAbstractItem::ChoiceItem) {
        point = rhombusNearest(point, r.width() / 2.0, r.height() / 2.0);
        return;
    }
    clampOntoBorder(point, r.width() / 2.0, r.height() / 2.0);
}

QPointF EdgeGeometry::borderNearest(const CyberiadaSMEditorAbstractItem* item, const QPointF& p)
{
    if (!item) return p;
    QRectF r = item->sceneBoundingRect();
    if (item->type() == CyberiadaSMEditorAbstractItem::VertexItem ||
        item->type() == CyberiadaSMEditorAbstractItem::ChoiceItem) {
        return attachedPoint(item, p) + r.center();
    }
    QPointF rel = p - r.center();
    clampOntoBorder(rel, r.width() / 2.0, r.height() / 2.0);
    return rel + r.center();
}

QPointF EdgeGeometry::rhombusToward(const QPointF& toward, qreal hw, qreal hh)
{
    // the ray c + t * d crosses |x|/hw + |y|/hh = 1 at t = 1 / s
    qreal s = hw > 0.0 && hh > 0.0 ? qAbs(toward.x()) / hw + qAbs(toward.y()) / hh : 0.0;
    return s > 0.0 ? toward / s : QPointF();
}

QPointF EdgeGeometry::rhombusNearest(const QPointF& p, qreal hw, qreal hh)
{
    if (hw <= 0.0 || hh <= 0.0) return p;
    const QPointF v[4] = { QPointF(hw, 0.0), QPointF(0.0, hh), QPointF(-hw, 0.0), QPointF(0.0, -hh) };
    QPointF best;
    qreal bestDist = std::numeric_limits<qreal>::max();
    for (int i = 0; i < 4; i++) {
        QPointF a = v[i], d = v[(i + 1) % 4] - a;
        qreal t = qBound(0.0, QPointF::dotProduct(p - a, d) / QPointF::dotProduct(d, d), 1.0);
        QPointF q = a + d * t;
        qreal dist = QPointF::dotProduct(p - q, p - q);
        if (dist < bestDist) {
            bestDist = dist;
            best = q;
        }
    }
    return best;
}

QPointF EdgeGeometry::linkAttachedPoint(const CyberiadaSMEditorAbstractItem* item, const QPointF& toward)
{
    if (!item || item->type() != CyberiadaSMEditorAbstractItem::ChoiceItem) {
        return attachedPoint(item, toward, true);
    }
    QRectF r = item->sceneBoundingRect();
    QPointF d = toward - r.center();
    // coincident centres give no direction: the right side
    if (d.isNull()) d = QPointF(1.0, 0.0);
    return rhombusToward(d, r.width() / 2.0, r.height() / 2.0);
}

QPointF EdgeGeometry::linkBorderNearest(const CyberiadaSMEditorAbstractItem* item, const QPointF& p)
{
    if (!item || item->type() != CyberiadaSMEditorAbstractItem::ChoiceItem) return borderNearest(item, p);
    QRectF r = item->sceneBoundingRect();
    return rhombusNearest(p - r.center(), r.width() / 2.0, r.height() / 2.0) + r.center();
}

QVector<QLineF> EdgeGeometry::textRangeLines(const QGraphicsTextItem* item, int from, int to)
{
    QVector<QLineF> lines;
    if (!item || from >= to) return lines;
    QTextDocument* doc = item->document();
    // the underline sits where the font draws its own, below the baseline
    qreal under = QFontMetricsF(item->font()).underlinePos();
    for (QTextBlock block = doc->findBlock(from); block.isValid() && block.position() < to; block = block.next()) {
        // the block bounding rect lays the block out first
        doc->documentLayout()->blockBoundingRect(block);
        QTextLayout* layout = block.layout();
        if (!layout) continue;
        QPointF origin = layout->position();
        int bp = block.position();
        for (int i = 0; i < layout->lineCount(); i++) {
            QTextLine line = layout->lineAt(i);
            int a = qMax(from, bp + line.textStart());
            int b = qMin(to, bp + line.textStart() + line.textLength());
            if (a >= b) continue;
            qreal x1 = line.cursorToX(a - bp);
            qreal x2 = line.cursorToX(b - bp);
            // a range holding only a line break or a wrap space draws nothing
            if (x2 - x1 < 0.5) continue;
            qreal y = line.y() + line.ascent() + under;
            lines.append(QLineF(item->mapToScene(origin + QPointF(x1, y)),
                                item->mapToScene(origin + QPointF(x2, y))));
        }
    }
    return lines;
}
