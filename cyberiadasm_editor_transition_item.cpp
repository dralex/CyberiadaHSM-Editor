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
#include <QtMath>
#include <QTextDocument>
#include <QTextOption>

// transition action includes
#include <QRegularExpression>

#include "myassert.h"
#include "cyberiadasm_editor_transition_item.h"
#include "cyberiada_constants.h"
#include "cyberiadasm_editor_scene.h"

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
    return path().boundingRect().adjusted(-10, -10, 10, 10); // Увеличиваем область для стрелки
}

void CyberiadaSMEditorTransitionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{   QColor color(Qt::black);
    if (isSelected()) {
        color.setRgb(255, 0, 0);
    }
    painter->setPen(QPen(color, 2, Qt::SolidLine));
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


QPointF CyberiadaSMEditorTransitionItem::sourcePoint() const
{
    if (transition->has_geometry_source_point()) {
        return QPointF(transition->get_source_point().x, transition->get_source_point().y);
    }
    return QPointF();
}

void CyberiadaSMEditorTransitionItem::setSourcePoint(const QPointF &point)
{
    if(sourcePoint() == point) {
        return;
    }
    model->updateGeometry(model->elementToIndex(element), Cyberiada::Point(point.x(), point.y()),
                          Cyberiada::Point(targetPoint().x(), targetPoint().y()));
}

void CyberiadaSMEditorTransitionItem::setSourcePoint(const Cyberiada::Point &point)
{
    model->updateGeometry(model->elementToIndex(element), point,
                          Cyberiada::Point(targetPoint().x(), targetPoint().y()));
}

QPointF CyberiadaSMEditorTransitionItem::sourceCenter() const
{
    Cyberiada::ID id = transition->source_element_id();

    if(!elementIdToItemMap.value(id)) return QPoint(); //костыль
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
    return QPointF();
}

void CyberiadaSMEditorTransitionItem::setTargetPoint(const QPointF &point)
{
    if(targetPoint() == point) {
        return;
    }
    model->updateGeometry(model->elementToIndex(element), Cyberiada::Point(sourcePoint().x(), sourcePoint().y()),
                          Cyberiada::Point(point.x(), point.y()));
}

QPointF CyberiadaSMEditorTransitionItem::targetCenter() const
{
    if(!elementIdToItemMap.value(transition->target_element_id())) return QPoint(); //костыль
    MY_ASSERT(elementIdToItemMap.value(transition->target_element_id()));
    return (elementIdToItemMap.value(transition->target_element_id()))->sceneBoundingRect().center();
}

QPainterPath CyberiadaSMEditorTransitionItem::path() const
{
    MY_ASSERT(model);
    QPainterPath path = QPainterPath();

    // loop
    if (source() == target() && !(isSourceTraking || isTargetTraking)) {
        QPointF p1 = sourcePoint() + sourceCenter();
        QPointF p2 = targetPoint() + targetCenter();

        QPointF center = (p1 + p2) / 2;
        qreal radius = QLineF(p1, p2).length() / 2;

        QRectF rect(center.x() - radius, center.y() - radius,
                    radius * 2, radius * 2);

        QPointF v1 = p1 - sourceCenter();
        QPointF v2 = p2 - sourceCenter();

        double cross = v1.x() * v2.y() - v1.y() * v2.x();

        bool clockwise = (cross > 0);
        // clockwise = !clockwise;

        qreal angle1 = QLineF(center, p1).angle();
        qreal angle2 = QLineF(center, p2).angle();

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

void CyberiadaSMEditorTransitionItem::drawArrow(QPainter* painter)
{
    QPen pen(Qt::black, 1);
    if (isSelected()) {
        pen.setColor(Qt::red);
        pen.setWidth(2);
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

    if (source() == target() && !(isTargetTraking || isSourceTraking)) {
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
            cItem->type() == CyberiadaSMEditorAbstractItem::VertexItem) {
            return cItem;
        }
    }
    return nullptr;
}

QPointF CyberiadaSMEditorTransitionItem::findIntersectionWithItem(const CyberiadaSMEditorAbstractItem *item,
                                                                  const QPointF& start, const QPointF& end,
                                                                  bool* hasIntersections)
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
        if (rayForward.intersect(edge, &intersectionPoint) == QLineF::BoundedIntersection) {
            qreal dist = QLineF(start, intersectionPoint).length();
            if (dist < minDist) {
                minDist = dist;
                closestIntersection = intersectionPoint;
            }
        }
        if (rayBackward.intersect(edge, &intersectionPoint) == QLineF::BoundedIntersection) {
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
    actionItem->setVisible(visible);
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
    if (!isSelected()) return;
    prepareGeometryChange();
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

                CyberiadaSMEditorAbstractItem* cItem = itemUnderCursor();
                if (cItem) {
                    QPointF newPoint;
                    bool hasIntersections;
                    if (cItem == target()) {
                        // loop
                        setSource(cItem);
                        hasIntersections = false;
                        QPointF newPoint = findIntersectionWithItem(source(), p, sourceCenter(), &hasIntersections) -
                                           sourceCenter();
                        isSourceTraking = false;
                        setSourcePoint(newPoint);
                        return;
                    }

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
                    if (hasIntersections) {
                        isSourceTraking = false;
                        setSource(cItem);
                        setSourcePoint(newPoint);
                        return;
                    }
                    return;
                }
                bool hasIntersections = false;
                QPointF newPoint = findIntersectionWithItem(source(), p, nextPoint, &hasIntersections) -
                                   sourceCenter();

                if (hasIntersections) {
                    isSourceTraking = false;
                    setSourcePoint(newPoint);
                    return;
                }

                isSourceTraking = true;
                break;
            }
            // last point
            if(i == listDots.size() - 1) {
                QPointF nextPoint = sourcePoint() + sourceCenter();
                if(transition->has_polyline() && transition->get_geometry_polyline().size() > 0) {
                    Cyberiada::Polyline polyline = transition->get_geometry_polyline();
                    nextPoint = QPointF(polyline.back().x, polyline.back().y) + sourceCenter();
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
                QPointF newPoint = findIntersectionWithItem(target(), p, nextPoint, &hasIntersections) -
                                   targetCenter();

                if (hasIntersections) {
                    isTargetTraking = false;
                    setTargetPoint(newPoint);
                    return;
                }

                isTargetTraking = true;
                break;
            }

            // polyline points
            Cyberiada::Polyline pol = transition->get_geometry_polyline();
            Cyberiada::Point p = pol.at(i - 1);
            p.x += dx;
            p.y += dy;
            pol.at(i - 1) = p;
            model->updateGeometry(model->elementToIndex(element), pol);
            break;
        }
    }
}

void CyberiadaSMEditorTransitionItem::slotMouseReleaseDot()
{
    isSourceTraking = false;
    isTargetTraking = false;
}

void CyberiadaSMEditorTransitionItem::slotDeleteDot(QGraphicsItem *signalOwner)
{
    // TODO
    if (source() == target()) { return; }
    QPainterPath linePath = path();

    for(int i = 0; i < linePath.elementCount(); i++){
        if(listDots.at(i) == signalOwner){
            // first or last
            if(i == 0 || i == linePath.elementCount() - 1) {
                break;
            }

            Cyberiada::Polyline pol = transition->get_geometry_polyline();
            pol.erase(pol.begin() + i - 1);
            model->updateGeometry(model->elementToIndex(element), pol);
            break;
        }
    }
}

void CyberiadaSMEditorTransitionItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (source() == target()) { return; }

    QPointF clickPos = event->pos();
    QLineF checkLineFirst(clickPos.x() - 5, clickPos.y() - 5, clickPos.x() + 5, clickPos.y() + 5);
    QLineF checkLineSecond(clickPos.x() + 5, clickPos.y() - 5, clickPos.x() - 5, clickPos.y() + 5);
    QPainterPath oldPath = path();
    for(int i = 0; i < oldPath.elementCount() - 1; i++){
        QLineF checkableLine(oldPath.elementAt(i), oldPath.elementAt(i+1));
        if(checkableLine.intersect(checkLineFirst,0) == 1 || checkableLine.intersect(checkLineSecond,0) == 1){
            QPointF p = clickPos - source()->sceneBoundingRect().center();
            Cyberiada::Point cybP = Cyberiada::Point(p.x(), p.y());
            Cyberiada::Polyline pol;
            if(transition->has_polyline()) {
                pol = transition->get_geometry_polyline();
                pol.insert(pol.begin() + i, cybP);
            } else {
                pol.push_back(cybP);
            }
            model->updateGeometry(model->elementToIndex(element), pol);
            break;
        }
    }
    QGraphicsItem::mouseDoubleClickEvent(event);
}

void CyberiadaSMEditorTransitionItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }

    showDots();
    if (event->button() & Qt::LeftButton) {
        isLeftMouseButtonPressed = true;
       //        setPreviousPosition(event->scenePos());
        // emit clicked(this);
    }
    QGraphicsItem::mousePressEvent(event);
}

void CyberiadaSMEditorTransitionItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }

   if (isLeftMouseButtonPressed) {
//        auto dx = event->scenePos().x() - m_previousPosition.x();
//        auto dy = event->scenePos().y() - m_previousPosition.y();
//        moveBy(dx,dy);
//        //        setPreviousPosition(event->scenePos());
//        emit signalMove(this, dx, dy);
   }
   QGraphicsItem::mouseMoveEvent(event);
}

void CyberiadaSMEditorTransitionItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
   if (event->button() & Qt::LeftButton) {
       isLeftMouseButtonPressed = false;
   }
   QGraphicsItem::mouseReleaseEvent(event);
}

void CyberiadaSMEditorTransitionItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
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
    if (source() == target()) {
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
    if(source() != target() && transition->has_polyline()) {
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

    if(!transition->has_polyline() && source() != target()) {
        return;
    }

    // polyline
    if(source() != target() && transition->has_polyline()) {
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
        return;
    }

    // loop
    if(source() == target()) {
        // for(int i = 1; i < n -2; i++) {
        //     QPointF point = QPointF(path().elementAt(i).x, path().elementAt(i).y);
        //     DotSignal *dot = new DotSignal(point, this);
        //     connect(dot, &DotSignal::signalMove, this, &CyberiadaSMEditorTransitionItem::slotMoveDot);
        //     connect(dot, &DotSignal::signalMouseRelease, this, &CyberiadaSMEditorTransitionItem::slotMouseReleaseDot);
        //     connect(dot, &DotSignal::signalDelete, this, &CyberiadaSMEditorTransitionItem::slotDeleteDot);
        //     dot->setDotFlags(DotSignal::Movable);
        //     dot->setDeleteable(true);
        //     listDots.insert(i, dot);
        // }
        return;
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
    if (source() == target()) {
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
    setTextWidthEnabled(false);
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
    QRegularExpression re(R"(\[\s*(.*?)\s*\])");  // всё между [ и ]
    QRegularExpressionMatch match = re.match(toPlainText());
    if (match.hasMatch())
        return match.captured(1).trimmed();
    return QString();
}

QString TransitionAction::getBehaviour() const
{
    QRegularExpression re(R"(\/\s*(.+)$)");  // всё после /
    QRegularExpressionMatch match = re.match(toPlainText());
    if (match.hasMatch())
        return match.captured(1).trimmed();
    return QString();
}

void TransitionAction::paint(QPainter *painter, const QStyleOptionGraphicsItem *o, QWidget *w) {
    if (!toPlainText().isEmpty()) {
        painter->fillRect(boundingRect(), painter->background());
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
