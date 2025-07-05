/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * Dot Signal for the State Machine Editor Scene Items
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

#include <QBrush>
#include <QColor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include "dotsignal.h"

#include <QDebug>

DotSignal::DotSignal(QGraphicsItem *parentItem, QObject *parent) :
    QObject(parent)
{
    setParentItem(parentItem);
    setAcceptHoverEvents(true);
    setBrush(QBrush(Qt::green));
    setRect(-4,-4,8,8);
    setDotFlags(0);
    setDeleteable(false);
}

DotSignal::DotSignal(QPointF pos, QGraphicsItem *parentItem, QObject *parent) :
    QObject(parent)
{
    setParentItem(parentItem);
    setAcceptHoverEvents(true);
    setBrush(QBrush(Qt::green));
    setRect(-4,-4,8,8);
    setPos(pos);
    setPreviousPosition(pos);
    setDotFlags(0);
    setDeleteable(false);
}

DotSignal::~DotSignal()
{

}

QPointF DotSignal::getPreviousPosition() const
{
    return previousPosition;
}

void DotSignal::setPreviousPosition(const QPointF newPreviousPosition)
{
    if (previousPosition == newPreviousPosition)
        return;

    previousPosition = newPreviousPosition;
    emit previousPositionChanged();
}

void DotSignal::setDotFlags(unsigned int newFlags)
{
    flags = newFlags;
}

void DotSignal::setDeleteable(bool on)
{
    deleteable = on;
    setFlag(QGraphicsItem::ItemIsFocusable, on);
    setFlag(QGraphicsItem::ItemIsSelectable, on);
}

void DotSignal::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if(flags & Movable){
        auto dx = event->scenePos().x() - previousPosition.x();
        auto dy = event->scenePos().y() - previousPosition.y();
        moveBy(dx,dy);
        setPreviousPosition(event->scenePos());
        emit signalMove(this, dx, dy, event->scenePos());
    } else {
        QGraphicsItem::mouseMoveEvent(event);
    }
}

void DotSignal::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    emit signalPress(this);
    if(flags & Movable){
        setPreviousPosition(event->scenePos());
    } else {
        QGraphicsItem::mousePressEvent(event);
    }
}

void DotSignal::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    emit signalMouseRelease();
    QGraphicsItem::mouseReleaseEvent(event);
}

void DotSignal::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    setBrush(QBrush(Qt::red));
}

void DotSignal::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    if (!hasFocus()) {
        setBrush(QBrush(Qt::green));
    }
}

void DotSignal::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        emit signalDelete(this);
    } else {
        QGraphicsItem::keyPressEvent(event);
    }
}
