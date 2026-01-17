/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Temporary Transition Item
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

#include "temporary_transition.h"
#include <cstdio>
#include <QPainter>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QtMath>
#include <QTextDocument>
#include <QTextOption>

#include "myassert.h"
#include "cyberiadasm_editor_transition_item.h"
#include "cyberiada_constants.h"
#include "cyberiadasm_editor_scene.h"
#include "settings_manager.h"

/* -----------------------------------------------------------------------------
 * Temporary Transition Item
 * ----------------------------------------------------------------------------- */

TemporaryTransition::TemporaryTransition(QObject *parent_object,
                                         CyberiadaSMModel *model,
                                         QGraphicsItem *parent,
                                         QMap<Cyberiada::ID, QGraphicsItem*>& elementItem,
                                         CyberiadaSMEditorAbstractItem* source,
                                         QPointF& targetPoint) :
    // QObject(parent_object),
    QGraphicsItem(parent),
    model(model),
    elementIdToItemMap(elementItem),
    source(source),
    target(source),
    targetPoint(targetPoint)
{
    setAcceptHoverEvents(true);
    setFlags(ItemIsSelectable|ItemSendsGeometryChanges);

    isSourceTraking = false;
    isTargetTraking = true;

    // TODO start source point
    sourcePoint = QPointF();

    initializeDots();
}

TemporaryTransition::~TemporaryTransition()
{
    if(!listDots.isEmpty()){
        foreach (DotSignal *dot, listDots) {
            dot->deleteLater();
        }
        listDots.clear();
    }
}

QRectF TemporaryTransition::boundingRect() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    return path().boundingRect().adjusted(-10, -10, 10, 10);
}

void TemporaryTransition::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
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

CyberiadaSMEditorAbstractItem *TemporaryTransition::getSource() const
{
    return source;
}

void TemporaryTransition::setSource(CyberiadaSMEditorAbstractItem *newSource)
{
    source = newSource;
}

Cyberiada::Element *TemporaryTransition::getSourceElement() const
{
    return source->getElement();
}

Cyberiada::Point TemporaryTransition::getSourcePoint() const
{
    return Cyberiada::Point(sourcePoint.x(), sourcePoint.y());
}

void TemporaryTransition::setSourcePoint(const QPointF &point)
{
    sourcePoint = point;
    setDotsPosition();
}

QPointF TemporaryTransition::sourceCenter() const
{
    Cyberiada::ID id = source->getId();
    if(!elementIdToItemMap.value(id)) return QPoint();
    MY_ASSERT(elementIdToItemMap.value(id));
    return (elementIdToItemMap.value(id))->sceneBoundingRect().center();
}

CyberiadaSMEditorAbstractItem *TemporaryTransition::getTarget() const
{
    return target;
}

void TemporaryTransition::setTarget(CyberiadaSMEditorAbstractItem *newTarget)
{
    target = newTarget;
}

Cyberiada::Element *TemporaryTransition::getTargetElement() const
{
    return target->getElement();
}

Cyberiada::Point TemporaryTransition::getTargetPoint() const
{
    return Cyberiada::Point(targetPoint.x(), targetPoint.y());
}

void TemporaryTransition::setTargetPoint(const QPointF &point)
{
    targetPoint = point;
    setDotsPosition();
}

QPointF TemporaryTransition::targetCenter() const
{
    Cyberiada::ID id = target->getId();
    if(!elementIdToItemMap.value(id)) return QPoint();
    MY_ASSERT(elementIdToItemMap.value(id));
    return (elementIdToItemMap.value(id))->sceneBoundingRect().center();
}

QPainterPath TemporaryTransition::path() const
{
    MY_ASSERT(model);
    QPainterPath path = QPainterPath();

    // loop
    if (source == target && !(isSourceTraking || isTargetTraking)) {
        QPointF p1 = sourcePoint + sourceCenter();
        QPointF p2 = targetPoint + targetCenter();

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

    if (isSourceTraking) {
        path.moveTo(prevPosition);
    } else {
        path.moveTo(sourcePoint + sourceCenter());
    }

    if (isTargetTraking) {
        path.lineTo(prevPosition);
    } else {
        path.lineTo(targetPoint + targetCenter());
    }

    return path;
}

DotSignal *TemporaryTransition::getDot(int index)
{
    if (index < 0) return nullptr;
    if (listDots.size() <= index) return nullptr;
    return listDots.at(index);
}

void TemporaryTransition::drawArrow(QPainter* painter)
{
    SettingsManager& sm = SettingsManager::instance();

    QPen pen(Qt::black, 1);
    if (isSelected()) {
        pen.setColor(sm.getSelectionColor());
        pen.setWidth(sm.getSelectionBorderWidth());
    }
    painter->setPen(pen);

    double angle;

    QPointF p1;
    QPointF p2;
    if (isSourceTraking) {
        p1 = prevPosition;
    } else {
        p1 = sourcePoint + sourceCenter();
    }
    if (isTargetTraking) {
        p2 = prevPosition;
    } else {
        p2 = targetPoint + targetCenter();
    }

    if (source == target && !(isTargetTraking || isSourceTraking)) {
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

CyberiadaSMEditorAbstractItem *TemporaryTransition::itemUnderCursor()
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

QPointF TemporaryTransition::findIntersectionWithItem(const CyberiadaSMEditorAbstractItem *item,
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

    if (item->type() == CyberiadaSMEditorAbstractItem::VertexItem) {
        *hasIntersections = true;
        return item->sceneBoundingRect().center();
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

void TemporaryTransition::slotMoveDot(QGraphicsItem *signalOwner, qreal dx, qreal dy, QPointF p)
{
    prepareGeometryChange();
    prevPosition = p;

    for(int i = 0; i < listDots.size(); i++){
        if(listDots.at(i) == signalOwner){
            // first point
            // if(i == 0) {
            //     QPointF nextPoint = targetPoint + targetCenter();

            //     CyberiadaSMEditorAbstractItem* cItem = itemUnderCursor();
            //     if (cItem) {
            //         QPointF newPoint;
            //         bool hasIntersections;

            //         // loop
            //         if (cItem == target) {
            //             setSource(cItem);
            //             hasIntersections = false;
            //             QPointF newPoint = findIntersectionWithItem(source, p, sourceCenter(), &hasIntersections) -
            //                                sourceCenter();
            //             isSourceTraking = false;
            //             setSourcePoint(newPoint);
            //             return;
            //         }

            //         if (cItem == source) {
            //             hasIntersections = false;
            //             QPointF newPoint = findIntersectionWithItem(source, p, nextPoint, &hasIntersections) -
            //                                sourceCenter();
            //             if (hasIntersections) {
            //                 isSourceTraking = false;
            //                 setSourcePoint(newPoint);
            //                 return;
            //             }
            //         }

            //         newPoint = findIntersectionWithItem(cItem, p, nextPoint, &hasIntersections) -
            //                    cItem->sceneBoundingRect().center();
            //         // intersectoin with item under cursor
            //         if (hasIntersections) {
            //             isSourceTraking = false;
            //             setSource(cItem);
            //             setSourcePoint(newPoint);
            //             return;
            //         }
            //         return;
            //     }
            //     bool hasIntersections = false;

            //     if (source == target) {
            //         // loop
            //         nextPoint = sourceCenter();
            //     }

            //     QPointF newPoint = findIntersectionWithItem(source, p, nextPoint, &hasIntersections) -
            //                        sourceCenter();
            //     // intersection with source in adjusted bounding rect
            //     if (hasIntersections) {
            //         isSourceTraking = false;
            //         setSourcePoint(newPoint);
            //         return;
            //     }

            //     // mouse traking
            //     isSourceTraking = true;
            //     setDotsPosition();
            //     break;
            // }
            // last point
            if(i == listDots.size() - 1) {
                QPointF nextPoint = sourcePoint + sourceCenter();

                CyberiadaSMEditorAbstractItem* cItem = itemUnderCursor();
                if (cItem) {
                    QPointF newPoint;
                    bool hasIntersections;
                    if (cItem == source) {
                        // loop
                        setTarget(cItem);
                        hasIntersections = false;
                        QPointF newPoint = findIntersectionWithItem(target, p, targetCenter(), &hasIntersections) -
                                           targetCenter();
                        isTargetTraking = false;
                        setTargetPoint(newPoint);
                        return;
                    }

                    if (cItem == target) {
                        hasIntersections = false;
                        QPointF newPoint = findIntersectionWithItem(target, p, nextPoint, &hasIntersections) -
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

                if (source == target) {
                    // loop
                    nextPoint = sourceCenter();
                }

                QPointF newPoint = findIntersectionWithItem(target, p, nextPoint, &hasIntersections) -
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
            break;
        }
    }
}

void TemporaryTransition::slotMouseReleaseDot()
{
    if (isSourceTraking || isTargetTraking) {
        emit signalReady(this, false);
        prepareGeometryChange();
        return;
    }
    emit signalReady(this, true);
}

void TemporaryTransition::initializeDots()
{
    // source
    DotSignal *dotS = new DotSignal(sourcePoint + sourceCenter(), this);
    connect(dotS, &DotSignal::signalMove, this, &TemporaryTransition::slotMoveDot);
    connect(dotS, &DotSignal::signalMouseRelease, this, &TemporaryTransition::slotMouseReleaseDot);
    dotS->setDotFlags(DotSignal::Movable);
    dotS->setDeleteable(true);
    listDots.append(dotS);
    // target
    DotSignal *dotT = new DotSignal(QPointF(), this);
    connect(dotT, &DotSignal::signalMove, this, &TemporaryTransition::slotMoveDot);
    connect(dotT, &DotSignal::signalMouseRelease, this, &TemporaryTransition::slotMouseReleaseDot);
    dotT->setDotFlags(DotSignal::Movable);
    dotT->setDeleteable(true);
    listDots.append(dotT);
}

void TemporaryTransition::showDots()
{
    foreach( DotSignal* dot, listDots ) {
        dot->setVisible(true);
    }
}

void TemporaryTransition::hideDots()
{
    foreach( DotSignal* dot, listDots ) {
        dot->setVisible(false);
    }
}

void TemporaryTransition::setDotsPosition()
{
    if (isSourceTraking) {
        listDots.first()->setPos(prevPosition);
        return;
    }
    if (isTargetTraking) {
        listDots.last()->setPos(prevPosition);
        return;
    }
    listDots.at(0)->setPos(sourcePoint + sourceCenter());
    listDots.at(1)->setPos(targetPoint + targetCenter());
}

