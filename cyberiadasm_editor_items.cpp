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
#include "cyberiadasm_editor_scene.h"
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
    cornerFlags(0)
{
    if(parent) {
        CyberiadaSMEditorAbstractItem* newParent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parent);
        if(newParent) {
            connect(newParent, &CyberiadaSMEditorAbstractItem::geometryChanged,
                    this, &CyberiadaSMEditorAbstractItem::onParentGeometryChanged);
        }
        StateRegion* stateArea = dynamic_cast<StateRegion*>(parent);
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

QPointF CyberiadaSMEditorAbstractItem::getPreviousPosition() const
{
    return previousPosition;
}

void CyberiadaSMEditorAbstractItem::setPreviousPosition(const QPointF newPreviousPosition)
{
    if (previousPosition == newPreviousPosition)
        return;

    previousPosition = newPreviousPosition;
    emit previousPositionChanged();
}

bool CyberiadaSMEditorAbstractItem::hasGeometry()
{
    return element->has_geometry();
}

void CyberiadaSMEditorAbstractItem::syncFromModel()
{
    update();
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
    if (!element->has_geometry() ||
        dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }

    if (event->button() & Qt::LeftButton) {
        isLeftMouseButtonPressed = true;
        setPreviousPosition(event->scenePos());
        // emit clicked(this);
    }
    QGraphicsItem::mousePressEvent(event);
    showDots();
}

void CyberiadaSMEditorAbstractItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!element->has_geometry() ||
        dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }

    QPointF pt = event->pos();
    switch (cornerFlags) {
    case Top:
        if (isLeftMouseButtonPressed) {
            setFlag(ItemIsMovable);
        }
        break;
    case Bottom:
        resizeBottom(pt);
        break;
    case Left:
        if (isLeftMouseButtonPressed) {
            setFlag(ItemIsMovable);
            // Cyberiada::Rect r = Cyberiada::Rect(boundingRect().x(),
            //                                     boundingRect().y(),
            //                                     boundingRect().height(),
            //                                     boundingRect().width());
            // model->updateGeometry(model->elementToIndex(element), r);
        }
        break;
    case Right:
        resizeRight(pt);
        break;
    case TopLeft:
        if (isLeftMouseButtonPressed) {
            setFlag(ItemIsMovable);
        }
        break;
    // case TopRight:
    //     resizeTop(pt);
    //     resizeRight(pt);
    //     break;
    // case BottomLeft:
    //     resizeBottom(pt);
    //     resizeLeft(pt);
    //     break;
    case BottomRight:
        resizeBottom(pt);
        resizeRight(pt);
        break;
    default:
        // if (isLeftMouseButtonPressed) {
        //     setCursor(Qt::ClosedHandCursor);
        //     setFlag(ItemIsMovable);
        // }
        break;
    }

    QGraphicsItem::mouseMoveEvent(event);
    emit geometryChanged();
}

void CyberiadaSMEditorAbstractItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (!element->has_geometry() ||
        dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }

    if (event->button() & Qt::LeftButton) {
        isLeftMouseButtonPressed = false;
        setFlag(ItemIsMovable, false);
    }
    QGraphicsItem::mouseReleaseEvent(event);
}


void CyberiadaSMEditorAbstractItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    if (!element->has_geometry() ||
        dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }

    setDotsPosition();
    showDots();
    QGraphicsItem::hoverEnterEvent(event);
}

void CyberiadaSMEditorAbstractItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    if (!element->has_geometry() ||
        dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }

    cornerFlags = 0;
    hideDots();
    unsetCursor();
    QGraphicsItem::hoverLeaveEvent( event );
}

void CyberiadaSMEditorAbstractItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    // TODO
    if (!isSelected() ||
        !element->has_geometry() ||
        dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }

    QPointF pt = event->pos();              // The current position of the mouse
    qreal drx = pt.x() - boundingRect().right();    // Distance between the mouse and the right
    qreal dlx = pt.x() - boundingRect().left();     // Distance between the mouse and the left

    qreal dby = pt.y() - boundingRect().top();      // Distance between the mouse and the top
    qreal dty = pt.y() - boundingRect().bottom();   // Distance between the mouse and the bottom

    // If the mouse position is within a radius of 7
    // to a certain side( top, left, bottom or right)
    // we set the Flag in the Corner Flags Register

    cornerFlags = 0;
    if( dby < 7 && dby > -7 ) cornerFlags |= Top;       // Top side
    if( dty < 7 && dty > -7 ) cornerFlags |= Bottom;    // Bottom side
    if( drx < 7 && drx > -7 ) cornerFlags |= Right;     // Right side
    if( dlx < 7 && dlx > -7 ) cornerFlags |= Left;      // Left side

    switch (cornerFlags) {
    case Top:
    case Left:
    case TopLeft:
        setCursor(QCursor(Qt::SizeAllCursor));
        break;

    case Bottom:
        setCursor(QCursor(Qt::SizeVerCursor));
        break;

    case Right:
        setCursor(QCursor(Qt::SizeHorCursor));
        break;

    case TopRight:
    case BottomLeft:
        setCursor(QCursor(Qt::SizeBDiagCursor));
        break;

    case BottomRight:
        setCursor(QCursor(Qt::SizeFDiagCursor));
        break;

    default:
        // setCursor(Qt::ArrowCursor);
        unsetCursor();
        break;
    }
    QGraphicsItem::hoverMoveEvent(event);
}

/*
void CyberiadaSMEditorAbstractItem::resizeLeft(const QPointF &pt)
{
    QRectF tmpRect = rect();
    // if the mouse is on the right side we return
    if( pt.x() > tmpRect.right() )
        return;
    qreal widthOffset =  ( pt.x() - tmpRect.right() );
    // limit the minimum width
    if( widthOffset > -10 )
        return;
    // if it's negative we set it to a positive width value
    if( widthOffset < 0 )
        tmpRect.setWidth( -widthOffset );
    else
        tmpRect.setWidth( widthOffset );
    // Since it's a left side , the rectange will increase in size
    // but keeps the topLeft as it was
    tmpRect.translate( rect().width() - tmpRect.width() , 0 );
    prepareGeometryChange();
    // Set the ne geometry
    setRect( tmpRect );
    // Update to see the result
    update();
    // setPositionGrabbers();
}
*/


void CyberiadaSMEditorAbstractItem::resizeRight(const QPointF &pt)
{
    QRectF tmpRect = boundingRect();
    if( pt.x() < tmpRect.left() )
        return;
    qreal widthOffset =  ( pt.x() - tmpRect.left() );
    if( widthOffset < 10 ) /// limit
        return;
    if( widthOffset < 10)
        tmpRect.setWidth( -widthOffset );
    else
        tmpRect.setWidth( widthOffset );
    prepareGeometryChange();
    // setRect( tmpRect );
    update();
    setDotsPosition();
}



void CyberiadaSMEditorAbstractItem::resizeBottom(const QPointF &pt)
{
    QRectF tmpRect = boundingRect();
    if( pt.y() < tmpRect.top() )
        return;
    qreal heightOffset =  ( pt.y() - tmpRect.top() );
    if( heightOffset < 11 ) /// limit
        return;
    if( heightOffset < 0)
        tmpRect.setHeight( -heightOffset );
    else
        tmpRect.setHeight( heightOffset );
    prepareGeometryChange();
    // setRect( tmpRect );
    update();
    setDotsPosition();
}

void CyberiadaSMEditorAbstractItem::initializeDots()
{
    if (!element->has_geometry()) return;
    for (int i = 0; i < 8; i++){
        cornerGrabber[i] = new DotSignal(this);
    }
}


/*
void CyberiadaSMEditorStateItem::resizeTop(const QPointF &pt)
{
    QRectF tmpRect = rect();
    if( pt.y() > tmpRect.bottom() )
        return;
    qreal heightOffset =  ( pt.y() - tmpRect.bottom() );
    if( heightOffset > -11 ) /// limit
        return;
    if( heightOffset < 0)
        tmpRect.setHeight( -heightOffset );
    else
        tmpRect.setHeight( heightOffset );
    tmpRect.translate( 0 , rect().height() - tmpRect.height() );
    prepareGeometryChange();
    setRect( tmpRect );
    update();
    // setPositionGrabbers();
}
*/

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
    if(!element->has_geometry()) return;
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
    if(!element->has_geometry()) return;
    for(int i = 0; i < 8; i++){
        cornerGrabber[i]->setVisible(true);
    }
}

void CyberiadaSMEditorAbstractItem::hideDots()
{
    if(!element->has_geometry()) return;
    for(int i = 0; i < 8; i++){
        cornerGrabber[i]->setVisible(false);
    }
}


