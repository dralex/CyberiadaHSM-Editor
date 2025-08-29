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
#include "settings_manager.h"
#include "myassert.h"


/* -----------------------------------------------------------------------------
 * Abstract Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorAbstractItem::CyberiadaSMEditorAbstractItem(CyberiadaSMModel* _model,
                                                             Cyberiada::Element* _element,
                                                             QGraphicsItem* parent):
    // QObject(nullptr),
    QGraphicsItem(parent),
    model(_model),
    element(_element),
    cornerFlags(0)
{
    connect(&SettingsManager::instance(), &SettingsManager::inspectorModeChanged, this, &CyberiadaSMEditorAbstractItem::slotInspectorModeChanged);
    connect(&SettingsManager::instance(), &SettingsManager::selectionSettingsChanged, this, &CyberiadaSMEditorAbstractItem::slotSelectionSettingsChanged);

    if(parent) {
        CyberiadaSMEditorAbstractItem* newParent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parent);
        if(newParent) {
            // change in parent geometry
            // to change transition action position when parent of state/target changes
            connect(newParent, &CyberiadaSMEditorAbstractItem::geometryChanged,
                    this, &CyberiadaSMEditorAbstractItem::onParentGeometryChanged);
            // change in parent size
            // to change position of children
            connect(newParent, &CyberiadaSMEditorAbstractItem::sizeChanged,
                    this, &CyberiadaSMEditorAbstractItem::onParentSizeChanged);
            // change in this item geometry
            // to change size of parent when child moves inside
            // connect(this, &CyberiadaSMEditorAbstractItem::geometryChanged,
            //         newParent, &CyberiadaSMEditorAbstractItem::onChildGeometryChanged);
        }
        StateRegion* stateArea = dynamic_cast<StateRegion*>(parent);
        if(stateArea) {
            newParent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(stateArea->parentItem());
            if(newParent) {
                // change in parent geometry
                connect(newParent, &CyberiadaSMEditorAbstractItem::geometryChanged,
                        this, &CyberiadaSMEditorAbstractItem::onParentGeometryChanged);
                // change in parent size
                // to change position of children
                connect(newParent, &CyberiadaSMEditorAbstractItem::sizeChanged,
                        this, &CyberiadaSMEditorAbstractItem::onParentSizeChanged);
                // change in this item geometry
                // connect(this, &CyberiadaSMEditorAbstractItem::geometryChanged,
                //         newParent, &CyberiadaSMEditorAbstractItem::onChildGeometryChanged);
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
    setDotsPosition();
    update();
}

void CyberiadaSMEditorAbstractItem::onParentGeometryChanged() {
    update();
    emit geometryChanged();
}

void CyberiadaSMEditorAbstractItem::onParentSizeChanged(CornerFlags side, qreal delta)
{
    if(auto collection = dynamic_cast<Cyberiada::ElementCollection*>(element)) {
        Cyberiada::Rect r = collection->get_geometry_rect();
        QRectF tmpR = QRectF(r.x, r.y, r.width, r.height);
        switch (side) {
        case CornerFlags::Right:
            tmpR = QRectF(r.x - delta, r.y, r.width, r.height);
            break;
        case CornerFlags::Left:
            tmpR = QRectF(r.x + delta, r.y, r.width, r.height);
            break;
        case CornerFlags::Bottom:
            tmpR = QRectF(r.x, r.y - delta, r.width, r.height);
            break;
        case CornerFlags::Top:
            tmpR = QRectF(r.x, r.y + delta, r.width, r.height);
            break;
        default:
            break;
        }

        Cyberiada::Rect oldR = collection->get_geometry_rect();
        qDebug() << "--child old:" << oldR.x << oldR.y << oldR.width << oldR.height;
        qDebug() << "--new:" << tmpR.x() << tmpR.y() << tmpR.width() << tmpR.height();
        qDebug() << "--delta" << oldR.x - tmpR.x();

        Cyberiada::Rect newR = Cyberiada::Rect(tmpR.x(),
                                            tmpR.y(),
                                            tmpR.width(),
                                            tmpR.height());
        model->updateGeometry(model->elementToIndex(element), newR);
    }
}

void CyberiadaSMEditorAbstractItem::onChildGeometryChanged()
{
    // update();
    // emit geometryChanged();
}

void CyberiadaSMEditorAbstractItem::updateSizeToFitChildren(CyberiadaSMEditorAbstractItem* child)
{

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
        dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select ) {
        event->ignore();
        return;
    }

    if (SettingsManager::instance().getInspectorMode()) {
        QGraphicsItem::mousePressEvent(event);
    }

    if (event->button() & Qt::LeftButton) {
        isLeftMouseButtonPressed = true;
        // setPreviousPosition(event->scenePos());
        // setPreviousPosition(event->pos());
        // emit clicked(this);
    }
    QGraphicsItem::mousePressEvent(event);
    showDots();
}

void CyberiadaSMEditorAbstractItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!element->has_geometry() ||
        dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select ||
        SettingsManager::instance().getInspectorMode()) {
        event->ignore();
        return;
    }

    if (!isLeftMouseButtonPressed) {
        return;
    }

    QPointF pt = event->pos();

    switch (cornerFlags) {
    case Top:
        updatePosGeometry();
        break;
    case Bottom:
        resizeBottom(pt);
        break;
    case Left:
        updatePosGeometry();
        break;
    case Right:
        resizeRight(pt);
        break;
    case TopLeft:
        updatePosGeometry();
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

    if (parentItem()) {
        CyberiadaSMEditorAbstractItem* parent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem());
        if(parent) {
            parent->updateSizeToFitChildren(this);
        }
        StateRegion* stateArea = dynamic_cast<StateRegion*>(parentItem());
        if(stateArea) {
            parent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(stateArea->parentItem());
            if(parent) {
                parent->updateSizeToFitChildren(this);
            }
        }
    }
    // emit geometryChanged();
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
        dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select ||
        SettingsManager::instance().getInspectorMode()) {
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

void CyberiadaSMEditorAbstractItem::slotInspectorModeChanged(bool on)
{
    update();
}

void CyberiadaSMEditorAbstractItem::slotSelectionSettingsChanged()
{
    update();
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

    QPointF pt = event->pos();                      // The current position of the mouse
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
        setCursor(QCursor(Qt::SizeAllCursor));
        break;
    case Left:
        setCursor(QCursor(Qt::SizeAllCursor));
        break;
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
        // TODO
        setCursor(QCursor(Qt::SizeAllCursor));
        break;
    case BottomLeft:
        // setCursor(QCursor(Qt::SizeBDiagCursor));
        setCursor(QCursor(Qt::SizeAllCursor));
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
    qreal delta = (widthOffset - boundingRect().width()) / 2;
    Cyberiada::Rect r = Cyberiada::Rect(pos().x() + delta,
                                        pos().y(),
                                        tmpRect.width(),
                                        tmpRect.height());
    model->updateGeometry(model->elementToIndex(element), r);
    emit sizeChanged(CornerFlags::Right, delta);
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
    qreal delta = (heightOffset - boundingRect().height()) / 2;
    Cyberiada::Rect r = Cyberiada::Rect(pos().x(),
                                        pos().y() + delta,
                                        tmpRect.width(),
                                        tmpRect.height());
    model->updateGeometry(model->elementToIndex(element), r);
    emit sizeChanged(CornerFlags::Bottom, delta);
}

void CyberiadaSMEditorAbstractItem::updatePosGeometry()
{
    // change the model data
    setFlag(ItemIsMovable);

    Cyberiada::Rect r = Cyberiada::Rect(pos().x(),
                                        pos().y(),
                                        boundingRect().width(),
                                        boundingRect().height());
    if (type() == StateItem || type() == CompositeStateItem) {
        auto coll = dynamic_cast<Cyberiada::ElementCollection*>(element);
        Cyberiada::Rect oldR = coll->get_geometry_rect();
        QRectF parR = mapRectToParent(QRectF(boundingRect()));
        qDebug() << "++child old:" << oldR.x << oldR.y << oldR.width << oldR.height;
        qDebug() << "++new:" << r.x << r.y << r.width << r.height;
        qDebug() << "++delta" << oldR.x - r.x;
        qDebug() << "++mapBR" << parR;
    }
    model->updateGeometry(model->elementToIndex(element), r);
}

void CyberiadaSMEditorAbstractItem::initializeDots()
{
    if (!element->has_geometry()) return;
    for (int i = 0; i < 8; i++){
        cornerGrabber[i] = new DotSignal(this);
    }
}

// change of parent
void CyberiadaSMEditorAbstractItem::handleParentChange() {
    // if (auto oldParent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem())) {
    //     disconnect(oldParent, &CyberiadaSMEditorAbstractItem::geometryChanged,
    //                this, &CyberiadaSMEditorAbstractItem::onParentGeometryChanged);
    // }

    // if (auto newParent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem())) {
    //     connect(newParent, &CyberiadaSMEditorAbstractItem::geometryChanged,
    //             this, &CyberiadaSMEditorAbstractItem::onParentGeometryChanged);
    // }
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


