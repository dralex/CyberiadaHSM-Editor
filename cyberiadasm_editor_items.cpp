/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Editor Scene Items
 *
 * Copyright (C) 2024 Alexey Fedoseev <aleksey@fedoseev.net>
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

#include <QDebug>
#include <QPainter>
#include <QColor>
#include <QCursor>

#include "cyberiadasm_editor_items.h"
#include "cyberiadasm_editor_state_item.h"
#include "myassert.h"


/* -----------------------------------------------------------------------------
 * Abstract Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorAbstractItem::CyberiadaSMEditorAbstractItem(CyberiadaSMModel* _model,
															 Cyberiada::Element* _element,
                                                             QGraphicsItem* parent):
    // QObject(nullptr)
    QGraphicsItem(parent),
    model(_model),
    element(_element),
    m_cornerFlags(0)
{
    if(parent) {
        CyberiadaSMEditorAbstractItem* newParent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parent);
        if(newParent) {
            connect(newParent, &CyberiadaSMEditorAbstractItem::geometryChanged,
                    this, &CyberiadaSMEditorAbstractItem::onParentGeometryChanged);
        }
        StateArea* stateArea = dynamic_cast<StateArea*>(parent);
        if(stateArea) {
            newParent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(stateArea->parentItem());
            if(newParent) {
                connect(newParent, &CyberiadaSMEditorAbstractItem::geometryChanged,
                        this, &CyberiadaSMEditorAbstractItem::onParentGeometryChanged);
            }
        }
    }
}

QVariant CyberiadaSMEditorAbstractItem::data(int key) const
{
	if (key == 0) {
		return QString(element->get_id().c_str());
	} else {
		return QVariant();
	}
}

void CyberiadaSMEditorAbstractItem::setPreviousPosition(const QPointF previousPosition)
{
    if (m_previousPosition == previousPosition)
        return;

    m_previousPosition = previousPosition;
    emit previousPositionChanged();
}

void CyberiadaSMEditorAbstractItem::onParentGeometryChanged() {
    update();
    emit geometryChanged();
}

QVariant CyberiadaSMEditorAbstractItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged || change == ItemTransformHasChanged) {
        emit geometryChanged();
    }
    if (change == ItemParentHasChanged) {
        handleParentChange();
    }
    return QGraphicsItem::itemChange(change, value);
}

void CyberiadaSMEditorAbstractItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() & Qt::LeftButton) {
        m_leftMouseButtonPressed = true;
        setPreviousPosition(event->scenePos());
        // emit clicked(this);
    }
    QGraphicsItem::mousePressEvent(event);
    showDots();
}

void CyberiadaSMEditorAbstractItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    /*
    QPointF pt = event->pos();
    switch (m_cornerFlags) {
    case Top:
        resizeTop(pt);
        break;
    case Bottom:
        resizeBottom(pt);
        break;
    case Left:
        resizeLeft(pt);
        break;
    case Right:
        resizeRight(pt);
        break;
    case TopLeft:
        resizeTop(pt);
        resizeLeft(pt);
        break;
    case TopRight:
        resizeTop(pt);
        resizeRight(pt);
        break;
    case BottomLeft:
        resizeBottom(pt);
        resizeLeft(pt);
        break;
    case BottomRight:
        resizeBottom(pt);
        resizeRight(pt);
        break;
    default:
    */
    if (m_leftMouseButtonPressed) {
        setCursor(Qt::ClosedHandCursor);
        setFlag(ItemIsMovable);
    }
    // break;
    // }

    QGraphicsItem::mouseMoveEvent(event);
    emit geometryChanged();
}

void CyberiadaSMEditorAbstractItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() & Qt::LeftButton) {
        m_leftMouseButtonPressed = false;
        setFlag(ItemIsMovable, false);
    }
    QGraphicsItem::mouseReleaseEvent(event);
}


void CyberiadaSMEditorAbstractItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    setDotsPosition();
    showDots();
    QGraphicsItem::hoverEnterEvent(event);
}

void CyberiadaSMEditorAbstractItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_cornerFlags = 0;
    hideDots();
    setCursor(Qt::ArrowCursor);
    QGraphicsItem::hoverLeaveEvent( event );
}

void CyberiadaSMEditorAbstractItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    /*
    QPointF pt = event->pos();              // The current position of the mouse
    qreal drx = pt.x() - rect().right();    // Distance between the mouse and the right
    qreal dlx = pt.x() - rect().left();     // Distance between the mouse and the left

    qreal dby = pt.y() - rect().top();      // Distance between the mouse and the top
    qreal dty = pt.y() - rect().bottom();   // Distance between the mouse and the bottom

    If the mouse position is within a radius of 7
    to a certain side( top, left, bottom or right)
    we set the Flag in the Corner Flags Register

    m_cornerFlags = 0;
    if( dby < 7 && dby > -7 ) m_cornerFlags |= Top;       // Top side
    if( dty < 7 && dty > -7 ) m_cornerFlags |= Bottom;    // Bottom side
    if( drx < 7 && drx > -7 ) m_cornerFlags |= Right;     // Right side
    if( dlx < 7 && dlx > -7 ) m_cornerFlags |= Left;      // Left side

    switch (m_cornerFlags) {
    case Top:
    case Bottom:
        setCursor(QCursor(Qt::SizeVerCursor));
        break;

    case Left:
    case Right:
        setCursor(QCursor(Qt::SizeHorCursor));
        break;

    case TopRight:
    case BottomLeft:
        setCursor(QCursor(Qt::SizeBDiagCursor));
        break;

    case TopLeft:
    case BottomRight:
        setCursor(QCursor(Qt::SizeFDiagCursor));
        break;

    default:
*/
    setCursor(Qt::OpenHandCursor);
        // break;
    // }
    QGraphicsItem::hoverMoveEvent(event);
}

void CyberiadaSMEditorAbstractItem::handleParentChange() {
    if (auto oldParent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem())) {
        disconnect(oldParent, &CyberiadaSMEditorAbstractItem::geometryChanged,
                   this, &CyberiadaSMEditorAbstractItem::onParentGeometryChanged);
    }

    if (auto newParent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem())) {
        connect(newParent, &CyberiadaSMEditorAbstractItem::geometryChanged,
                this, &CyberiadaSMEditorAbstractItem::onParentGeometryChanged);
    }
}

void CyberiadaSMEditorAbstractItem::setDotsPosition()
{
    QRectF tmpRect = boundingRect();
    cornerGrabber[GrabberTop]->setPos(tmpRect.left() + tmpRect.width()/2, tmpRect.top());
    cornerGrabber[GrabberBottom]->setPos(tmpRect.left() + tmpRect.width()/2, tmpRect.bottom());
    cornerGrabber[GrabberLeft]->setPos(tmpRect.left(), tmpRect.top() + tmpRect.height()/2);
    cornerGrabber[GrabberRight]->setPos(tmpRect.right(), tmpRect.top() + tmpRect.height()/2);
    cornerGrabber[GrabberTopLeft]->setPos(tmpRect.topLeft().x(), tmpRect.topLeft().y());
    cornerGrabber[GrabberTopRight]->setPos(tmpRect.topRight().x(), tmpRect.topRight().y());
    cornerGrabber[GrabberBottomLeft]->setPos(tmpRect.bottomLeft().x(), tmpRect.bottomLeft().y());
    cornerGrabber[GrabberBottomRight]->setPos(tmpRect.bottomRight().x(), tmpRect.bottomRight().y());
}

void CyberiadaSMEditorAbstractItem::showDots()
{
    if(!isSelected()) return;
    for(int i = 0; i < 8; i++){
        cornerGrabber[i]->setVisible(true);
    }
}

void CyberiadaSMEditorAbstractItem::hideDots()
{
    for(int i = 0; i < 8; i++){
        cornerGrabber[i]->setVisible(false);
    }
}


