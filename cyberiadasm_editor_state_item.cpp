/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The State Machine Editor State Item
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

#include <QPainter>
#include <QDebug>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <math.h>

#include "myassert.h"
#include "dotsignal.h"
#include "cyberiada_constants.h"
#include "cyberiadasm_editor_state_item.h"
#include "cyberiadasm_editor_scene.h"

/* -----------------------------------------------------------------------------
 * State Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorStateItem::CyberiadaSMEditorStateItem(QObject *parent_object,
                     CyberiadaSMModel *model,
                     Cyberiada::Element *element,
                     QGraphicsItem *parent) :
    CyberiadaSMEditorAbstractItem(model, element, parent)
    // m_cornerFlags(0)
{
    setAcceptHoverEvents(true);
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);

    state = static_cast<const Cyberiada::State*>(element);

    setPos(QPointF(x(), y()));

    title = new StateTitle(state->get_name().c_str(), this);
    title->setFontBoldness(true);

    actions = state->get_actions();
    // std::vector<Cyberiada::Action> actions = state->get_actions();
    for (std::vector<Cyberiada::Action>::const_iterator i = actions.begin(); i != actions.end(); i++) {
        Cyberiada::ActionType type = i->get_type();
        if (type == Cyberiada::actionEntry) {
            // TODO "exit" or "entry" and "/" are constants from cyberiadamlpp
            entry = new StateAction(QString("entry / ") + QString(i->get_behavior().c_str()), this);
            connect(entry, &EditableTextItem::sizeChanged, this, &CyberiadaSMEditorStateItem::onTextItemSizeChanged);
        } else if (type == Cyberiada::actionExit) {
            exit = new StateAction(QString("exit / ") + QString(i->get_behavior().c_str()), this);
            connect(exit, &EditableTextItem::sizeChanged, this, &CyberiadaSMEditorStateItem::onTextItemSizeChanged);
        }
    }

    if (state->is_composite_state()) {
        connect(title, &EditableTextItem::sizeChanged, this, &CyberiadaSMEditorStateItem::onTextItemSizeChanged);
        area = new StateArea(this);
        updateArea();
    }

    initializeDots();
    setDotsPosition();
    hideDots();
    setPositionText();
}

// TODO
CyberiadaSMEditorStateItem::~CyberiadaSMEditorStateItem()
{
    emit aboutToDelete();

    // qDebug() << "elem" << element;
    // qDebug() << "has geom" << element->has_geometry();

    // if(!element->has_geometry())
    //     return;
    // for(int i = 0; i < 8; i++){
    //     delete cornerGrabber[i];
    // }
}

void CyberiadaSMEditorStateItem::setRect(qreal x, qreal y, qreal w, qreal h)
{
    setRect(QRectF(x, y, w, h));
}

void CyberiadaSMEditorStateItem::setRect(const QRectF &rect)
{
    prepareGeometryChange();
    /*
    if (rect.width() >= 320 && rect.height() >= 180){
        QGraphicsRectItem::setRect(rect);
    }
    QRectF r = rect;
    if (r.height() < 180) {
        r.setHeight(180);
    }
    if (r.width() < 360){
        r.setWidth(360);
    }
    if (r.width() >= 320 && r.height() >= 180) {

    QGraphicsRectItem::setRect(rect);
    }
*/
    m_rect = rect;
}

QRectF CyberiadaSMEditorStateItem::rect() const {
    Cyberiada::Rect model_rect = state->get_geometry_rect();
    QRectF rect = QRectF(-model_rect.width / 2, -model_rect.height / 2, model_rect.width, model_rect.height);
    return rect;
}

qreal CyberiadaSMEditorStateItem::x() const
{
    Cyberiada::Rect model_rect = state->get_geometry_rect();
    return model_rect.x;
}

qreal CyberiadaSMEditorStateItem::y() const
{
    Cyberiada::Rect model_rect = state->get_geometry_rect();
    return model_rect.y;
}

qreal CyberiadaSMEditorStateItem::width() const
{
    Cyberiada::Rect model_rect = state->get_geometry_rect();
    return model_rect.width;
}

qreal CyberiadaSMEditorStateItem::height() const
{
    Cyberiada::Rect model_rect = state->get_geometry_rect();
    return model_rect.height;
}

StateArea *CyberiadaSMEditorStateItem::getArea()
{
    return area;
}

void CyberiadaSMEditorStateItem::updateArea()
{
    qreal top_delta = title->boundingRect().height();
    qreal bottom_delta = 0;

    if (entry) {
        top_delta += entry->boundingRect().height();
        area->setTopLine(true);
    }
    if (exit) {
        bottom_delta += entry->boundingRect().height();
        area->setBottomLine(true);
    }

    area->setRect(-width()/2, -(height() - top_delta - bottom_delta) / 2, width(), height() - top_delta - bottom_delta);
    area->setPos(0, (top_delta - bottom_delta)/2 );
}

QRectF CyberiadaSMEditorStateItem::boundingRect() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    return rect();
}

void CyberiadaSMEditorStateItem::setPositionText()
{
    QRectF oldRect = rect();
    QRectF titleRect = title->boundingRect();
    title->setPos(oldRect.x() + (oldRect.width() - titleRect.width()) / 2 , oldRect.y());

    // simple state
    if (state->is_simple_state()) {
        if (entry != nullptr && exit != nullptr) {
            float delta = (entry->boundingRect().height() + exit->boundingRect().height());
            entry->setPos(oldRect.x() + 15, oldRect.y() + (height() + titleRect.height() - delta) / 2);
            exit->setPos(entry->pos() + QPointF(0, entry->boundingRect().height()));
            return;
        }
        if (entry != nullptr) {
            entry->setPos(oldRect.x() + 15, oldRect.y() + (height() + titleRect.height() - entry->boundingRect().height()) / 2);
        }
        if (exit != nullptr) {
            exit->setPos(oldRect.x() + 15, oldRect.y() + (height() + titleRect.height() - exit->boundingRect().height()) / 2);
        }
        return;
    }

    // composite state
    if (entry != nullptr) {
        entry->setPos(oldRect.x() + 15, oldRect.y() + titleRect.height());
    }
    if (exit != nullptr) {
        exit->setPos(oldRect.x() + 15, oldRect.bottom() - exit->boundingRect().height());
    }
    // setPositionGrabbers();
}

void CyberiadaSMEditorStateItem::onTextItemSizeChanged()
{
    if (state->is_composite_state()) updateArea();
    setPositionText();
}

void CyberiadaSMEditorStateItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget){
    Q_UNUSED(option)
    Q_UNUSED(widget)

    qreal titleHeight = title->boundingRect().height();

    QPen pen = QPen(Qt::black, 2, Qt::SolidLine);
    if (isSelected()) {
        pen.setColor(Qt::red);
        pen.setWidth(2);
        painter->setBrush(QBrush(QColor(255, 0, 0, 50)));
        // painter->setBrush(QBrush(QColor(66, 170, 255, 50)));
    }

    painter->setPen(pen);

    QPainterPath path;
    QRectF tmpRect = rect();
    path.addRoundedRect(tmpRect, ROUNDED_RECT_RADIUS, ROUNDED_RECT_RADIUS);
    painter->drawLine(QPointF(tmpRect.x(), tmpRect.y() + titleHeight), QPointF(tmpRect.right(), tmpRect.y() + titleHeight));
    painter->drawPath(path);

    painter->setBrush(Qt::red);
    painter->drawEllipse(QPointF(0, 0), 2, 2); // Центр системы координат
}

/* -----------------------------------------------------------------------------
 * State Title Item
 * ----------------------------------------------------------------------------- */

void StateTitle::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    ToolType currentTool = dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool();
    if(currentTool != ToolType::Select) {
        event->ignore();
        return;
    }

    if (event->button() == Qt::LeftButton and !hasFocus()) {
        startPos = event->scenePos();
        isMoving = true;
        isLeftMouseButtonPressed = true;
        event->accept();
        return;
    }
    QGraphicsTextItem::mousePressEvent(event);
}

void StateTitle::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    ToolType currentTool = dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool();
    if(currentTool != ToolType::Select) {
        event->ignore();
        return;
    }

    if (isLeftMouseButtonPressed && !hasFocus()) {
        if (isMoving && parentItem()) {
            QPointF delta = event->scenePos() - startPos;
            parentItem()->setPos(parentItem()->pos() + delta);
            startPos = event->scenePos();
        }
        return;
    }
    QGraphicsTextItem::mouseMoveEvent(event);
}

void StateTitle::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    ToolType currentTool = dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool();
    if(currentTool != ToolType::Select) {
        event->ignore();
        return;
    }

    if (event->button() == Qt::LeftButton and !hasFocus()) {
        isMoving = false;
        isLeftMouseButtonPressed = false;
        return;
    }
    QGraphicsTextItem::mouseReleaseEvent(event);
}
