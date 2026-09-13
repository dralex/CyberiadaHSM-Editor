/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The State Machine Editor Transition Item
 *
 * Copyright (C) 2025 Anastasia Viktorova <viktorovaa.04@gmail.com>
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

#include <cstdio>
#include <QPainter>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QtMath>
#include <QTextDocument>
#include <QTextOption>

// transition action includes
#include <QRegularExpression>

#include "myassert.h"
#include "cyberiadasm_editor_transition_item.h"
#include "cyberiada_constants.h"
#include "cyberiadasm_editor_scene.h"
#include "settings_manager.h"

// QLineF::intersect was deprecated in favour of intersects in Qt 5.14
static QLineF::IntersectType lineIntersect(const QLineF& a, const QLineF& b, QPointF* point)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    return a.intersects(b, point);
#else
    return a.intersect(b, point);
#endif
}

/* -----------------------------------------------------------------------------
 * Transition Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorTransitionItem::CyberiadaSMEditorTransitionItem(QObject *parent_object,
                                                                 CyberiadaSMModel *model,
                                                                 Cyberiada::Element *element,
                                                                 QGraphicsItem *parent,
                                                                 QMap<Cyberiada::ID, QGraphicsItem*>& elementItem) :
    CyberiadaSMEditorAbstractItem(model, element, parent),
    // QObject(parent_object),
    elementIdToItemMap(elementItem)
{
    setAcceptHoverEvents(true);
    setFlags(ItemIsSelectable|ItemSendsGeometryChanges);

    isMouseTraking = false;
    isSourceTraking = false;
    isTargetTraking = false;

    transition = static_cast<const Cyberiada::Transition*>(element);

    actionItem = new TransitionAction(actionText(), this);
    setActionVisibility(SettingsManager::instance().getShowTransitionText());
    connect(&SettingsManager::instance(), &SettingsManager::showTransitionTextChanged, this, &CyberiadaSMEditorTransitionItem::setActionVisibility);
    // the label is centered on the polyline, so it follows its own size
    connect(actionItem, &EditableTextItem::sizeChanged, this, &CyberiadaSMEditorTransitionItem::updateActionPosition);

    connect(target(), &CyberiadaSMEditorAbstractItem::geometryChanged, this, &CyberiadaSMEditorTransitionItem::onTargetGeomertyChanged);
    connect(source(), &CyberiadaSMEditorAbstractItem::geometryChanged, this, &CyberiadaSMEditorTransitionItem::onSourceGeomertyChanged);
    connect(target(), &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorTransitionItem::onTargetSizeChanged);
    connect(source(), &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorTransitionItem::onSourceSizeChanged);

    updateActionPosition();
    initializeDots();
    hideDots();
}

CyberiadaSMEditorTransitionItem::~CyberiadaSMEditorTransitionItem()
{
    if(actionItem) { delete actionItem; }

    if(!listDots.isEmpty()){
        foreach (DotSignal *dot, listDots) {
            dot->deleteLater();
        }
        listDots.clear();
    }
}

QRectF CyberiadaSMEditorTransitionItem::boundingRect() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    return path().boundingRect().adjusted(-10, -10, 10, 10); // enlarge the area for the arrowhead
}

void CyberiadaSMEditorTransitionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPen pen = QPen(Qt::black, 2, Qt::SolidLine);
    if (isSelected()) {
        SettingsManager& sm = SettingsManager::instance();
        pen.setColor(sm.getSelectionColor());
        pen.setWidth(sm.getSelectionBorderWidth());
    }

    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path());

    drawArrow(painter);
}

QPainterPath CyberiadaSMEditorTransitionItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(5);
    return stroker.createStroke(path());
}

CyberiadaSMEditorAbstractItem *CyberiadaSMEditorTransitionItem::source() const
{
    return static_cast<CyberiadaSMEditorAbstractItem*>(elementIdToItemMap.value(transition->source_element_id()));
}

void CyberiadaSMEditorTransitionItem::setSource(CyberiadaSMEditorAbstractItem *newSource)
{
    if (source() == newSource) return;

    movePolyline(sourceCenter() - newSource->sceneBoundingRect().center());

    disconnect(source(), &CyberiadaSMEditorAbstractItem::geometryChanged, this, &CyberiadaSMEditorTransitionItem::onSourceGeomertyChanged);
    disconnect(source(), &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorTransitionItem::onSourceSizeChanged);

    connect(newSource, &CyberiadaSMEditorAbstractItem::geometryChanged, this, &CyberiadaSMEditorTransitionItem::onSourceGeomertyChanged);
    connect(newSource, &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorTransitionItem::onSourceSizeChanged);

    model->updateGeometry(model->elementToIndex(element), newSource->getId(), targetId());
}


QPointF CyberiadaSMEditorTransitionItem::attachedPoint(const CyberiadaSMEditorAbstractItem* item,
                                                       const QPointF& toward) const
{
    if (!item) return QPointF();
    QPointF centre = item->sceneBoundingRect().center();
    if (toward == centre) return QPointF();
    bool has = false;
    QPointF p = findIntersectionWithItem(item, centre, toward, &has, true);
    return has ? p - centre : QPointF();
}

QPointF CyberiadaSMEditorTransitionItem::sourcePoint() const
{
    if (transition->has_geometry_source_point()) {
        return QPointF(transition->get_source_point().x, transition->get_source_point().y);
    }
    if (isArcLoop()) return QPointF();
    QPointF toward;
    if (transition->has_polyline() && !transition->get_geometry_polyline().empty()) {
        const Cyberiada::Point& v = transition->get_geometry_polyline().front();
        toward = QPointF(v.x, v.y) + sourceCenter();
    } else if (transition->has_geometry_target_point()) {
        toward = QPointF(transition->get_target_point().x, transition->get_target_point().y) + targetCenter();
    } else {
        toward = targetCenter();
    }
    return attachedPoint(source(), toward);
}

// the other end keeps its stored point, or stays unplaced: the displayed
// attachment is never written back
void CyberiadaSMEditorTransitionItem::setSourcePoint(const QPointF &point)
{
    if(sourcePoint() == point) {
        return;
    }
    model->updateGeometry(model->elementToIndex(element), Cyberiada::Point(point.x(), point.y()),
                          transition->get_target_point());
}

void CyberiadaSMEditorTransitionItem::setSourcePoint(const Cyberiada::Point &point)
{
    model->updateGeometry(model->elementToIndex(element), point, transition->get_target_point());
}

QPointF CyberiadaSMEditorTransitionItem::sourceCenter() const
{
    Cyberiada::ID id = transition->source_element_id();

    if(!elementIdToItemMap.value(id)) return QPoint(); // workaround
    MY_ASSERT(elementIdToItemMap.value(id));
    return (elementIdToItemMap.value(id))->sceneBoundingRect().center();
}

CyberiadaSMEditorAbstractItem *CyberiadaSMEditorTransitionItem::target() const
{
    return static_cast<CyberiadaSMEditorAbstractItem*>(elementIdToItemMap.value(transition->target_element_id()));
}

void CyberiadaSMEditorTransitionItem::setTarget(CyberiadaSMEditorAbstractItem *newTarget)
{
    if (target() == newTarget) return;

    disconnect(target(), &CyberiadaSMEditorAbstractItem::geometryChanged, this, &CyberiadaSMEditorTransitionItem::onTargetGeomertyChanged);
    disconnect(target(), &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorTransitionItem::onTargetSizeChanged);

    connect(newTarget, &CyberiadaSMEditorAbstractItem::geometryChanged, this, &CyberiadaSMEditorTransitionItem::onTargetGeomertyChanged);
    connect(newTarget, &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorTransitionItem::onTargetSizeChanged);

    model->updateGeometry(model->elementToIndex(element), sourceId(), newTarget->getId());

    isMouseTraking = false;
}

QPointF CyberiadaSMEditorTransitionItem::targetPoint() const
{
    if (transition->has_geometry_target_point()) {
        return QPointF(transition->get_target_point().x, transition->get_target_point().y);
    }
    if (isArcLoop()) return QPointF();
    QPointF toward;
    if (transition->has_polyline() && !transition->get_geometry_polyline().empty()) {
        const Cyberiada::Point& v = transition->get_geometry_polyline().back();
        toward = QPointF(v.x, v.y) + sourceCenter();
    } else if (transition->has_geometry_source_point()) {
        toward = QPointF(transition->get_source_point().x, transition->get_source_point().y) + sourceCenter();
    } else {
        toward = sourceCenter();
    }
    return attachedPoint(target(), toward);
}

void CyberiadaSMEditorTransitionItem::setTargetPoint(const QPointF &point)
{
    if(targetPoint() == point) {
        return;
    }
    model->updateGeometry(model->elementToIndex(element), transition->get_source_point(),
                          Cyberiada::Point(point.x(), point.y()));
}

QPointF CyberiadaSMEditorTransitionItem::targetCenter() const
{
    if(!elementIdToItemMap.value(transition->target_element_id())) return QPoint(); // workaround
    MY_ASSERT(elementIdToItemMap.value(transition->target_element_id()));
    return (elementIdToItemMap.value(transition->target_element_id()))->sceneBoundingRect().center();
}

// the loop is drawn as an arc only while it carries no polyline of its own
bool CyberiadaSMEditorTransitionItem::isArcLoop() const
{
    return source() == target() && !transition->has_polyline();
}

QPainterPath CyberiadaSMEditorTransitionItem::path() const
{
    MY_ASSERT(model);
    QPainterPath path = QPainterPath();

    // loop
    if (isArcLoop() && !(isSourceTraking || isTargetTraking)) {
        QPointF p1 = sourcePoint() + sourceCenter();
        QPointF p2 = targetPoint() + targetCenter();

        QPointF v1 = p1 - sourceCenter();
        QPointF v2 = p2 - sourceCenter();

        double cross = v1.x() * v2.y() - v1.y() * v2.x();

        bool clockwise = (cross > 0);
        // clockwise = !clockwise;

        QPointF center = (p1 + p2) / 2;
        qreal radius = QLineF(p1, p2).length() / 2;

        qreal MIN_RADIUS = 30;

        if (radius <= MIN_RADIUS) {
            QPointF dir = p2 - p1;
            if (dir.isNull()) {
                dir = QPointF(1, 0);
            }

            QLineF norm(dir, QPointF());
            norm.setLength(1.0);

            QPointF normal(-norm.dy(), norm.dx());

            qreal offset = std::sqrt(MIN_RADIUS * MIN_RADIUS - radius * radius);

            center += normal * offset;
            radius = MIN_RADIUS;
        }

        QRectF rect(center.x() - radius, center.y() - radius,
                    radius * 2, radius * 2);


        qreal angle1 = QLineF(center, p1).angle();
        qreal angle2 = QLineF(center, p2).angle();

        if(angle1 == angle2) {
            angle2 += 360;
        }

        qreal spanAngle = angle2 - angle1;
        if (clockwise) {
            if (spanAngle > 0) spanAngle -= 360;
        } else {
            if (spanAngle < 0) spanAngle += 360;
        }

        path.moveTo(p1);
        path.arcTo(rect, angle1, spanAngle);

        return path;
    }

    // polyline
    if (isSourceTraking) {
        path.moveTo(prevPosition);
    } else {
        path.moveTo(sourcePoint() + sourceCenter());
    }

    if(transition->has_polyline()) {
        for (const auto& point : transition->get_geometry_polyline()) {
            path.lineTo(QPointF(point.x, point.y) + sourceCenter());
        }
    }

    if (isTargetTraking) {
        path.lineTo(prevPosition);
    } else {
        path.lineTo(targetPoint() + targetCenter());
    }

    return path;
}

DotSignal *CyberiadaSMEditorTransitionItem::getDot(int index)
{
    if (index < 0) return nullptr;
    if (listDots.size() <= index) return nullptr;
    return listDots.at(index);
}

void CyberiadaSMEditorTransitionItem::drawArrow(QPainter* painter)
{
    SettingsManager& sm = SettingsManager::instance();

    QPen pen(Qt::black, 1);
    if (isSelected()) {
        pen.setColor(sm.getSelectionColor());
        pen.setWidth(sm.getSelectionBorderWidth());
    }
    painter->setPen(pen);

    double angle;

    QPointF p1 = sourcePoint() + sourceCenter();
    QPointF p2 = targetPoint() + targetCenter();
    if (isSourceTraking) {
        p1 = prevPosition;
    }
    if (isTargetTraking) {
        p2 = prevPosition;
    }

    if (isArcLoop() && !(isTargetTraking || isSourceTraking)) {
        QPointF center = (p1 + p2) / 2;
        angle = qDegreesToRadians(QLineF(center, p2).angle());
        QPointF v1 = p1 - sourceCenter();
        QPointF v2 = p2 - sourceCenter();

        double cross = v1.x() * v2.y() - v1.y() * v2.x();

        bool clockwise = (cross > 0);

        if (clockwise) {
            angle -= M_PI / 2;
        } else {
            angle += M_PI / 2;
        }
    } else {
        if(transition->has_polyline()) {
            Cyberiada::Polyline polyline = transition->get_geometry_polyline();
            p1 = QPointF(polyline.back().x, polyline.back().y) + sourceCenter();
        }

        QLineF line(p1, p2);
        angle = std::atan2(-line.dy(), line.dx());
    }

    qreal arrowSize = 10.0;
    QPointF arrowP1 = p2 + QPointF(-arrowSize * std::cos(angle - M_PI / 6),
                                   arrowSize * std::sin(angle - M_PI / 6));
    QPointF arrowP2 = p2 + QPointF(-arrowSize * std::cos(angle + M_PI / 6),
                                   arrowSize * std::sin(angle + M_PI / 6));

    QPolygonF arrowHead;
    arrowHead << p2 << arrowP1 << arrowP2;

    painter->setBrush(QBrush(pen.color()));
    painter->drawPolygon(arrowHead);
}

CyberiadaSMEditorAbstractItem *CyberiadaSMEditorTransitionItem::itemUnderCursor()
{
    QList<QGraphicsItem*> items = scene()->items(prevPosition);

    CyberiadaSMEditorAbstractItem* cItem = nullptr;
    for (QGraphicsItem* item : items) {
        cItem = dynamic_cast<CyberiadaSMEditorAbstractItem*>(item);
        if (!cItem) { continue; }

        if (cItem->type() == CyberiadaSMEditorAbstractItem::StateItem ||
            cItem->type() == CyberiadaSMEditorAbstractItem::CompositeStateItem ||
            cItem->type() == CyberiadaSMEditorAbstractItem::VertexItem ||
            cItem->type() == CyberiadaSMEditorAbstractItem::ChoiceItem) {
            return cItem;
        }
    }
    return nullptr;
}

QPointF CyberiadaSMEditorTransitionItem::findIntersectionWithItem(const CyberiadaSMEditorAbstractItem *item,
                                                                  const QPointF& start, const QPointF& end,
                                                                  bool* hasIntersections,
                                                                  bool forwardOnly) const
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

void CyberiadaSMEditorTransitionItem::updateCoordinates(CornerFlags side, QPointF& point, qreal d)
{
    if (point.x() < 0 && (side & CyberiadaSMEditorAbstractItem::CornerFlags::Right) == CyberiadaSMEditorAbstractItem::CornerFlags::Right) {
        point -= QPointF(d, 0);
    }
    if (point.x() > 0 && (side & CyberiadaSMEditorAbstractItem::CornerFlags::Right) == CyberiadaSMEditorAbstractItem::CornerFlags::Right) {
        point += QPointF(d, 0);
    }
    if (point.y() < 0 && (side & CyberiadaSMEditorAbstractItem::CornerFlags::Bottom) == CyberiadaSMEditorAbstractItem::CornerFlags::Bottom) {
        point -= QPointF(0, d);
    }
    if (point.y() > 0 && (side & CyberiadaSMEditorAbstractItem::CornerFlags::Bottom) == CyberiadaSMEditorAbstractItem::CornerFlags::Bottom) {
        point += QPointF(0, d);
    }
}

void CyberiadaSMEditorTransitionItem::movePolyline(QPointF delta)
{
    if(!transition->has_polyline()) { return; }

    Cyberiada::Polyline pol = transition->get_geometry_polyline();
    for (size_t i = 0; i < pol.size(); ++i) {
        pol[i] = Cyberiada::Point(pol[i].x + delta.x(), pol[i].y + delta.y());
    }

    model->updateGeometry(model->elementToIndex(element), pol);
}

QString CyberiadaSMEditorTransitionItem::actionText() const
{
    if(transition->has_action()){
        QStringList parts;

        if (transition->get_action().has_trigger()) {
            parts << QString(transition->get_action().get_trigger().c_str());
        }

        if (transition->get_action().has_guard()) {
            parts << "[" + QString(transition->get_action().get_guard().c_str()) + "]";
        }

        if (transition->get_action().has_behavior()) {
            parts << "/ " + QString(transition->get_action().get_behavior().c_str());
        }

        QString result = parts.join(" ");
        return result;
    }

    return QString();
}

void CyberiadaSMEditorTransitionItem::updateAction()
{
    actionItem->setPlainText(actionText());
    updateActionPosition();
}

void CyberiadaSMEditorTransitionItem::updateActionPosition() {
    if (!actionItem) return;

    // while the user drags the label, leave it where the drag puts it: a reflow
    // triggered mid-drag must not snap it back to the auto midpoint
    if (actionItem->isDragging()) return;

    // a moved label keeps its stored position, relative to the source centre
    if (transition->has_geometry_label_point()) {
        const Cyberiada::Point& lp = transition->get_label_point();
        actionItem->setPos(QPointF(lp.x, lp.y) + sourceCenter() - actionItem->boundingRect().center());
        update();
        return;
    }

    QPointF lastPoint = sourcePoint();
    if(transition->has_polyline() && transition->get_geometry_polyline().size() > 0) {
        Cyberiada::Polyline polyline = transition->get_geometry_polyline();
        Cyberiada::Point lastPolylinePoint = *(std::next(polyline.begin(), polyline.size() - 1));
        lastPoint = QPointF(lastPolylinePoint.x, lastPolylinePoint.y);
    }
    QPointF textPos = (lastPoint + (targetPoint() + targetCenter() - sourceCenter())) / 2 + sourceCenter() - actionItem->boundingRect().center();

    actionItem->setPos(textPos);
    update();
}

void CyberiadaSMEditorTransitionItem::setActionVisibility(bool visible) {
    actionItem->setVisible(visible && SettingsManager::instance().getShowText());
}

void CyberiadaSMEditorTransitionItem::syncFromModel()
{
    prepareGeometryChange();
    if (transition->has_action()) {
        updateAction();
    }
    updateDots();
    setDotsPosition();
    CyberiadaSMEditorAbstractItem::syncFromModel();
}

void CyberiadaSMEditorTransitionItem::onSourceGeomertyChanged()
{
    prepareGeometryChange();
    updateActionPosition();
    setDotsPosition();
}

void CyberiadaSMEditorTransitionItem::onTargetGeomertyChanged()
{
    prepareGeometryChange();
    updateActionPosition();
    setDotsPosition();
}

void CyberiadaSMEditorTransitionItem::onSourceSizeChanged(CyberiadaSMEditorAbstractItem::CornerFlags side, qreal d)
{
    prepareGeometryChange();
    QPointF newPosition = sourcePoint();
    updateCoordinates(side, newPosition, d);
    setSourcePoint(newPosition);
}

void CyberiadaSMEditorTransitionItem::onTargetSizeChanged(CyberiadaSMEditorAbstractItem::CornerFlags side, qreal d)
{
    prepareGeometryChange();
    QPointF newPosition = targetPoint();
    updateCoordinates(side, newPosition, d);
    setTargetPoint(newPosition);
}

void CyberiadaSMEditorTransitionItem::slotMoveDot(QGraphicsItem *signalOwner, qreal dx, qreal dy, QPointF p)
{
    if (SettingsManager::instance().getInspectorMode()) { return; }
    prepareGeometryChange();

    // with Ctrl held the endpoint locks to the adjacent point on the dominant
    // axis, so the segment next to it stays strictly horizontal or vertical
    DotSignal* movedDot = dynamic_cast<DotSignal*>(signalOwner);
    Qt::KeyboardModifiers mods = movedDot ? movedDot->modifiers() : Qt::NoModifier;

    prevPosition = p;

    for(int i = 0; i < listDots.size(); i++){
        if(listDots.at(i) == signalOwner){
            // first point
            if(i == 0) {
                QPointF nextPoint = targetPoint() + targetCenter();
                if(transition->has_polyline() && transition->get_geometry_polyline().size() > 0) {
                    Cyberiada::Polyline polyline = transition->get_geometry_polyline();
                    nextPoint = QPointF(polyline.front().x, polyline.front().y) + sourceCenter();
                }
                if (mods & Qt::ControlModifier) {
                    if (qAbs(p.x() - nextPoint.x()) <= qAbs(p.y() - nextPoint.y())) p.setX(nextPoint.x());
                    else                                                            p.setY(nextPoint.y());
                    prevPosition = p;
                }

                CyberiadaSMEditorAbstractItem* cItem = itemUnderCursor();
                if (cItem) {
                    QPointF newPoint;
                    bool hasIntersections;

                    // loop
                    if (cItem == target()) {
                        setSource(cItem);
                        hasIntersections = false;
                        QPointF newPoint = findIntersectionWithItem(source(), p, sourceCenter(), &hasIntersections) -
                                           sourceCenter();
                        isSourceTraking = false;
                        setSourcePoint(newPoint);
                        return;
                    }

                    //
                    if (cItem == source()) {
                        hasIntersections = false;
                        QPointF newPoint = findIntersectionWithItem(source(), p, nextPoint, &hasIntersections) -
                                           sourceCenter();
                        if (hasIntersections) {
                            isSourceTraking = false;
                            setSourcePoint(newPoint);
                            return;
                        }
                    }

                    newPoint = findIntersectionWithItem(cItem, p, nextPoint, &hasIntersections) -
                               cItem->sceneBoundingRect().center();
                    // intersectoin with item under cursor
                    if (hasIntersections) {
                        isSourceTraking = false;
                        setSource(cItem);
                        setSourcePoint(newPoint);
                        return;
                    }

                    return;
                }
                bool hasIntersections = false;

                if (isArcLoop()) {
                    // loop
                    nextPoint = sourceCenter();
                }

                QPointF newPoint = findIntersectionWithItem(source(), p, nextPoint, &hasIntersections) -
                           sourceCenter();
                // intersection with source in adjusted bounding rect
                if (hasIntersections) {
                    isSourceTraking = false;
                    setSourcePoint(newPoint);
                    return;
                }


                // mouse traking
                isSourceTraking = true;
                setDotsPosition();
                break;
            }
            // last point
            if(i == listDots.size() - 1) {
                QPointF nextPoint = sourcePoint() + sourceCenter();
                if(transition->has_polyline() && transition->get_geometry_polyline().size() > 0) {
                    Cyberiada::Polyline polyline = transition->get_geometry_polyline();
                    nextPoint = QPointF(polyline.back().x, polyline.back().y) + sourceCenter();
                }
                if (mods & Qt::ControlModifier) {
                    if (qAbs(p.x() - nextPoint.x()) <= qAbs(p.y() - nextPoint.y())) p.setX(nextPoint.x());
                    else                                                            p.setY(nextPoint.y());
                    prevPosition = p;
                }

                CyberiadaSMEditorAbstractItem* cItem = itemUnderCursor();
                if (cItem) {
                    QPointF newPoint;
                    bool hasIntersections;
                    if (cItem == source()) {
                        // loop
                        setTarget(cItem);
                        hasIntersections = false;
                        QPointF newPoint = findIntersectionWithItem(target(), p, targetCenter(), &hasIntersections) -
                                           targetCenter();
                        isTargetTraking = false;
                        setTargetPoint(newPoint);
                        return;
                    }

                    if (cItem == target()) {
                        hasIntersections = false;
                        QPointF newPoint = findIntersectionWithItem(target(), p, nextPoint, &hasIntersections) -
                                           targetCenter();
                        if (hasIntersections) {
                            isTargetTraking = false;
                            setTargetPoint(newPoint);
                            return;
                        }
                    }

                    newPoint = findIntersectionWithItem(cItem, p, nextPoint, &hasIntersections) -
                               cItem->sceneBoundingRect().center();
                    if (hasIntersections) {
                        isTargetTraking = false;
                        setTarget(cItem);
                        setTargetPoint(newPoint);
                        return;
                    }
                    return;
                }

                bool hasIntersections = false;

                if (isArcLoop()) {
                    // loop
                    nextPoint = sourceCenter();
                }

                QPointF newPoint = findIntersectionWithItem(target(), p, nextPoint, &hasIntersections) -
                                   targetCenter();

                if (hasIntersections) {
                    isTargetTraking = false;
                    setTargetPoint(newPoint);
                    return;
                }

                isTargetTraking = true;
                setDotsPosition();
                break;
            }

            // polyline points
            Cyberiada::Polyline pol = transition->get_geometry_polyline();
            Cyberiada::Point pt = pol.at(i - 1);
            pt.x += dx;
            pt.y += dy;
            if (mods & Qt::ControlModifier) {
                // like the endpoints: snap the point onto the axis of its
                // previous neighbour so the incoming segment stays orthogonal
                // (the dots and the polyline share the source-centre origin)
                QPointF neigh = listDots.at(i - 1)->scenePos() - sourceCenter();
                if (qAbs(pt.x - neigh.x()) <= qAbs(pt.y - neigh.y())) pt.x = neigh.x();
                else                                                  pt.y = neigh.y();
            }
            pol.at(i - 1) = pt;
            model->updateGeometry(model->elementToIndex(element), pol);
            break;
        }
    }
}

// true when anc is item's element or one of its ancestors in the model tree
bool CyberiadaSMEditorTransitionItem::isAncestorOf(CyberiadaSMEditorAbstractItem *anc,
                                                   CyberiadaSMEditorAbstractItem *item) const
{
    if (!anc || !item) return false;
    Cyberiada::Element* target = anc->getElement();
    for (Cyberiada::Element* e = item->getElement(); e; e = e->get_parent()) {
        if (e == target) return true;
    }
    return false;
}

void CyberiadaSMEditorTransitionItem::slotMouseReleaseDot(QGraphicsItem *signalOwner, QPointF p)
{
    isSourceTraking = false;
    isTargetTraking = false;

    if (SettingsManager::instance().getInspectorMode()) { return; }

    // a single-drag transition draw (or a quick drag-to-release) seeds a
    // self-loop and delivers no move to the grabbed endpoint dot, so the loop
    // survives. Bind the loose endpoint here if it was released over a
    // different item; released over the same one it stays a self-loop.
    if (source() != target()) { return; }
    if (listDots.isEmpty()) { return; }

    int idx = listDots.indexOf(dynamic_cast<DotSignal*>(signalOwner));
    if (idx != 0 && idx != listDots.size() - 1) { return; }

    prevPosition = p;
    CyberiadaSMEditorAbstractItem* cItem = itemUnderCursor();
    if (!cItem) { return; }

    prepareGeometryChange();
    bool hasIntersections = false;
    if (idx == listDots.size() - 1) {
        // a small drag that ends over an ancestor of the source (the parent SM
        // it sits in) is a self-loop being placed, not a retarget to the parent
        if (cItem == target() || isAncestorOf(cItem, source())) { return; }
        QPointF newPoint = findIntersectionWithItem(cItem, p, sourceCenter(), &hasIntersections) -
                           cItem->sceneBoundingRect().center();
        if (!hasIntersections) { return; }
        isTargetTraking = false;
        setTarget(cItem);
        setTargetPoint(newPoint);
    } else {
        if (cItem == source() || isAncestorOf(cItem, target())) { return; }
        QPointF newPoint = findIntersectionWithItem(cItem, p, targetCenter(), &hasIntersections) -
                           cItem->sceneBoundingRect().center();
        if (!hasIntersections) { return; }
        isSourceTraking = false;
        setSource(cItem);
        setSourcePoint(newPoint);
    }
    setDotsPosition();
}

void CyberiadaSMEditorTransitionItem::slotDeleteDot(QGraphicsItem *signalOwner)
{
    if (SettingsManager::instance().getInspectorMode()) { return; }
    if (isArcLoop()) { return; }

    for(int i = 0; i < listDots.size(); i++){
        if(listDots.at(i) == signalOwner){
            removePolylineVertex(i);
            break;
        }
    }
}

// interior dots map to polyline[dotIndex-1]; the endpoints have no vertex
void CyberiadaSMEditorTransitionItem::removePolylineVertex(int dotIndex)
{
    if (!transition->has_polyline()) { return; }
    if (dotIndex <= 0 || dotIndex >= listDots.size() - 1) { return; }

    Cyberiada::Polyline pol = transition->get_geometry_polyline();
    if (dotIndex - 1 >= int(pol.size())) { return; }
    pol.erase(pol.begin() + dotIndex - 1);
    model->updateGeometry(model->elementToIndex(element), pol);
}

// the two 10px diagonal probe crosses of a local point against each segment
int CyberiadaSMEditorTransitionItem::segmentAt(const QPointF& localPos) const
{
    QLineF checkLineFirst(localPos.x() - 5, localPos.y() - 5, localPos.x() + 5, localPos.y() + 5);
    QLineF checkLineSecond(localPos.x() + 5, localPos.y() - 5, localPos.x() - 5, localPos.y() + 5);
    QPainterPath oldPath = path();
    for(int i = 0; i < oldPath.elementCount() - 1; i++){
        QLineF checkableLine(oldPath.elementAt(i), oldPath.elementAt(i+1));
        if(lineIntersect(checkableLine, checkLineFirst, 0) == 1 ||
           lineIntersect(checkableLine, checkLineSecond, 0) == 1){
            return i;
        }
    }
    return -1;
}

void CyberiadaSMEditorTransitionItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (SettingsManager::instance().getInspectorMode()) { return; }

    // a double click edits the transition label; a vertex is added by dragging
    // a segment instead
    actionItem->setVisible(true);
    actionItem->startEditing();
    QGraphicsItem::mouseDoubleClickEvent(event);
}

void CyberiadaSMEditorTransitionItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (SettingsManager::instance().getInspectorMode()) { return; }

    // offer to remove a polyline vertex when the click is on an interior dot
    int vertexDot = -1;
    for (int i = 1; i < listDots.size() - 1; i++) {
        if (QLineF(listDots.at(i)->scenePos(), event->scenePos()).length() <= 6.0) {
            vertexDot = i;
            break;
        }
    }
    if (vertexDot < 0) { return; }

    QMenu menu;
    QAction* remove = menu.addAction(tr("Delete point"));
    if (menu.exec(event->screenPos()) == remove) {
        removePolylineVertex(vertexDot);
    }
    event->accept();
}

void CyberiadaSMEditorTransitionItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }
    if (SettingsManager::instance().getInspectorMode()) {
        QGraphicsItem::mousePressEvent(event);
        return;
    }

    showDots();
    if (event->button() & Qt::LeftButton) {
        isLeftMouseButtonPressed = true;
        // a press on a segment arms a vertex insertion, sprung by the drag
        pressPos = event->pos();
        pendingSegment = isArcLoop() ? 0 : segmentAt(pressPos);
        vertexDragArmed = (pendingSegment >= 0);
    }
    QGraphicsItem::mousePressEvent(event);
}

void CyberiadaSMEditorTransitionItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select ||
        SettingsManager::instance().getInspectorMode()) {
        event->ignore();
        return;
    }

    // once the drag leaves the press spot, insert a vertex on the pressed
    // segment; the same gesture then carries it
    if (vertexDragArmed &&
        QLineF(pressPos, event->pos()).length() > 3.0) {
        vertexDragArmed = false;
        QPointF p = event->pos() - sourceCenter();
        Cyberiada::Point cybP(p.x(), p.y());
        Cyberiada::Polyline pol;
        if (transition->has_polyline()) {
            pol = transition->get_geometry_polyline();
            pol.insert(pol.begin() + pendingSegment, cybP);
        } else {
            pol.push_back(cybP);
        }
        model->updateGeometry(model->elementToIndex(element), pol);
        draggingVertex = pendingSegment;
    }
    // drag the inserted vertex with the rest of the gesture
    if (draggingVertex >= 0 && transition->has_polyline()) {
        Cyberiada::Polyline pol = transition->get_geometry_polyline();
        if (draggingVertex < int(pol.size())) {
            QPointF p = event->pos() - sourceCenter();
            pol.at(draggingVertex) = Cyberiada::Point(p.x(), p.y());
            model->updateGeometry(model->elementToIndex(element), pol);
        }
    }
    QGraphicsItem::mouseMoveEvent(event);
}

void CyberiadaSMEditorTransitionItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
   if (event->button() & Qt::LeftButton) {
       isLeftMouseButtonPressed = false;
       vertexDragArmed = false;
       pendingSegment = -1;
       draggingVertex = -1;
   }
   QGraphicsItem::mouseReleaseEvent(event);
}

void CyberiadaSMEditorTransitionItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select ||
        SettingsManager::instance().getInspectorMode()) {
        event->ignore();
        return;
    }

    if (isSelected()) {
        showDots();
        setDotsPosition();
    }
    QGraphicsItem::hoverEnterEvent(event);
}

void CyberiadaSMEditorTransitionItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }

    QGraphicsItem::hoverMoveEvent(event);
}

void CyberiadaSMEditorTransitionItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }

    hideDots();
    QGraphicsItem::hoverLeaveEvent(event);
}

void CyberiadaSMEditorTransitionItem::initializeDots()
{
    if (isArcLoop()) {
        // source
        DotSignal *dotS = new DotSignal(sourcePoint() + sourceCenter(), this);
        connect(dotS, &DotSignal::signalMove, this, &CyberiadaSMEditorTransitionItem::slotMoveDot);
        connect(dotS, &DotSignal::signalMouseRelease, this, &CyberiadaSMEditorTransitionItem::slotMouseReleaseDot);
        connect(dotS, &DotSignal::signalDelete, this, &CyberiadaSMEditorTransitionItem::slotDeleteDot);
        dotS->setDotFlags(DotSignal::Movable);
        dotS->setDeleteable(true);
        listDots.append(dotS);
        // target
        DotSignal *dotT = new DotSignal(targetPoint() + targetCenter(), this);
        connect(dotT, &DotSignal::signalMove, this, &CyberiadaSMEditorTransitionItem::slotMoveDot);
        connect(dotT, &DotSignal::signalMouseRelease, this, &CyberiadaSMEditorTransitionItem::slotMouseReleaseDot);
        connect(dotT, &DotSignal::signalDelete, this, &CyberiadaSMEditorTransitionItem::slotDeleteDot);
        dotT->setDotFlags(DotSignal::Movable);
        dotT->setDeleteable(true);
        listDots.append(dotT);
        return;
    }
    // polyline
    QPainterPath linePath = path();
    for(int i = 0; i < linePath.elementCount(); i++) {
        QPointF point = linePath.elementAt(i);
        DotSignal *dot = new DotSignal(point, this);
        connect(dot, &DotSignal::signalMove, this, &CyberiadaSMEditorTransitionItem::slotMoveDot);
        connect(dot, &DotSignal::signalMouseRelease, this, &CyberiadaSMEditorTransitionItem::slotMouseReleaseDot);
        connect(dot, &DotSignal::signalDelete, this, &CyberiadaSMEditorTransitionItem::slotDeleteDot);
        dot->setDotFlags(DotSignal::Movable);
        dot->setDeleteable(true);
        listDots.append(dot);
    }
}

void CyberiadaSMEditorTransitionItem::updateDots()
{
    int n = 2;
    if(!isArcLoop() && transition->has_polyline()) {
        n += transition->get_geometry_polyline().size();
    }

    if(listDots.size() == n) {
        return;
    }

    if (listDots.size() > 2) {
        for (int i = 1; i < listDots.size() - 1; ++i) {
            listDots[i]->deleteLater();
        }

        DotSignal *first = listDots.first();
        DotSignal *last = listDots.last();

        listDots.clear();
        listDots.append(first);
        listDots.append(last);
    }

    if(!transition->has_polyline()) {
        return;
    }

    // polyline
    Cyberiada::Polyline pol = transition->get_geometry_polyline();
    for(int i = 0; i < n - 2; i++) {
        QPointF point = QPointF(pol.at(i).x, pol.at(i).y);
        DotSignal *dot = new DotSignal(point, this);
        connect(dot, &DotSignal::signalMove, this, &CyberiadaSMEditorTransitionItem::slotMoveDot);
        connect(dot, &DotSignal::signalMouseRelease, this, &CyberiadaSMEditorTransitionItem::slotMouseReleaseDot);
        connect(dot, &DotSignal::signalDelete, this, &CyberiadaSMEditorTransitionItem::slotDeleteDot);
        dot->setDotFlags(DotSignal::Movable);
        dot->setDeleteable(true);
        listDots.insert(i + 1, dot);
    }
}

void CyberiadaSMEditorTransitionItem::showDots()
{
    foreach( DotSignal* dot, listDots ) {
        dot->setVisible(true);
    }
}

void CyberiadaSMEditorTransitionItem::hideDots()
{
    foreach( DotSignal* dot, listDots ) {
        dot->setVisible(false);
    }
}

void CyberiadaSMEditorTransitionItem::setDotsPosition()
{
    QPainterPath linePath = path();
    if (isArcLoop()) {
        listDots.at(0)->setPos(sourcePoint() + sourceCenter());
        listDots.at(1)->setPos(targetPoint() + targetCenter());
        return;
    }
    for(int i = 0; i < linePath.elementCount(); i++) {
        QPointF point = linePath.elementAt(i);
        listDots.at(i)->setPos(point);
    }
}


/* -----------------------------------------------------------------------------
 * Transition Action Item
 * ----------------------------------------------------------------------------- */

TransitionAction::TransitionAction(const QString &text, QGraphicsItem *parent) :
    EditableTextItem(text, parent)
{
    setFontRole(fontRoleTransition);
    setTextAlignment(Qt::AlignCenter);
    setTextMargin(0);
}

QString TransitionAction::getTrigger() const
{
    QRegularExpression re(R"(^\s*([^\[/\]]+))");
    QRegularExpressionMatch match = re.match(toPlainText());
    if (match.hasMatch())
        return match.captured(1).trimmed();
    return QString();
}

QString TransitionAction::getGuard() const
{
    QRegularExpression re(R"(\[\s*(.*?)\s*\])");  // all between [ and ]
    QRegularExpressionMatch match = re.match(toPlainText());
    if (match.hasMatch())
        return match.captured(1).trimmed();
    return QString();
}

QString TransitionAction::getBehaviour() const
{
    QRegularExpression re(R"(\/\s*(.+)$)");  // all after /
    QRegularExpressionMatch match = re.match(toPlainText());
    if (match.hasMatch())
        return match.captured(1).trimmed();
    return QString();
}

void TransitionAction::paint(QPainter *painter, const QStyleOptionGraphicsItem *o, QWidget *w) {
    if (!toPlainText().isEmpty()) {
        QColor color = painter->background().color();
        color.setAlpha(230);
        painter->setBrush(color);
        painter->setPen(Qt::NoPen);
        painter->drawRect(boundingRect());
    }
    QGraphicsTextItem::paint(painter, o, w);
}

void TransitionAction::focusOutEvent(QFocusEvent *event)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    isEdit = false;
    QGraphicsTextItem::focusOutEvent(event);

    CyberiadaSMEditorTransitionItem* transition = dynamic_cast<CyberiadaSMEditorTransitionItem*>(parentItem());

    transition->model->updateAction(transition->model->elementToIndex(transition->element), 0,
                                    getTrigger(), getGuard(), getBehaviour());
}

void TransitionAction::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (isEdit) { EditableTextItem::mousePressEvent(event); return; }
    if (event->button() == Qt::LeftButton) {
        // select the transition and arm a possible label drag
        if (scene()) scene()->clearSelection();
        if (parentItem()) parentItem()->setSelected(true);
        dragLast = event->scenePos();
        dragging = false;
        event->accept();
        return;
    }
    EditableTextItem::mousePressEvent(event);
}

void TransitionAction::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!isEdit && (event->buttons() & Qt::LeftButton)) {
        setPos(pos() + event->scenePos() - dragLast);
        dragLast = event->scenePos();
        dragging = true;
        event->accept();
        return;
    }
    EditableTextItem::mouseMoveEvent(event);
}

void TransitionAction::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (!isEdit && dragging) {
        persistLabelPosition();
        dragging = false;
        event->accept();
        return;
    }
    EditableTextItem::mouseReleaseEvent(event);
}

void TransitionAction::persistLabelPosition()
{
    CyberiadaSMEditorTransitionItem* t = dynamic_cast<CyberiadaSMEditorTransitionItem*>(parentItem());
    if (!t || !t->model) return;
    // the stored point is the label centre relative to the source centre, the
    // same origin updateActionPosition reads it back with
    QPointF lp = pos() + boundingRect().center() - t->sourceCenter();
    t->model->updateLabel(t->model->elementToIndex(t->element), Cyberiada::Point(lp.x(), lp.y()));
}

void TransitionAction::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (SettingsManager::instance().getInspectorMode()) { return; }
    QMenu menu;
    QAction* reset = menu.addAction(QObject::tr("Reset label position"));
    if (menu.exec(event->screenPos()) == reset) {
        CyberiadaSMEditorTransitionItem* t = dynamic_cast<CyberiadaSMEditorTransitionItem*>(parentItem());
        if (t && t->model) {
            // an invalid point clears the stored label, resuming auto-placement
            t->model->updateLabel(t->model->elementToIndex(t->element), Cyberiada::Point());
        }
    }
    event->accept();
}
