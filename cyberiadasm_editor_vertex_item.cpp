/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The State Machine Editor Vertex Item
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

#include <QDebug>
#include <QPainter>
#include <QColor>
#include <math.h>

#include "myassert.h"
#include "cyberiada_constants.h"
#include "cyberiadasm_editor_vertex_item.h"
#include "cyberiadasm_editor_state_item.h"
#include "cyberiadasm_editor_scene.h"
#include "settings_manager.h"


/* -----------------------------------------------------------------------------
 * Vertex Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorVertexItem::CyberiadaSMEditorVertexItem(CyberiadaSMModel* model,
                                                         Cyberiada::Element* element,
                                                         QGraphicsItem* parent):
    CyberiadaSMEditorAbstractItem(model, element, parent)
{
    Cyberiada::Rect r = element->get_bound_rect(*(model->rootDocument()));
    setPos(r.x, r.y);

    setAcceptHoverEvents(true);
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);

    initializeDots();
    setDotsPosition();
    hideDots();
    // begin a transition from the four cardinal boxes (or a body drag)
    enableTransitionSourceDots(QList<int>() << GrabberTop << GrabberBottom
                               << GrabberLeft << GrabberRight);
}

void CyberiadaSMEditorVertexItem::syncFromModel()
{
    Cyberiada::Rect r = element->get_bound_rect(*(model->rootDocument()));
    setPos(r.x, r.y);
    CyberiadaSMEditorAbstractItem::syncFromModel();
}

QRectF CyberiadaSMEditorVertexItem::boundingRect() const
{
    return fullCircle();
}

QPainterPath CyberiadaSMEditorVertexItem::shape() const {
    QPainterPath path;
    // Cyberiada::ElementType type = element->get_type();
    // if (type == Cyberiada::elementInitial) {
    //     path.addEllipse(fullCircle());
    // } else if (type == Cyberiada::elementFinal) {
    //     path.addEllipse(partialCircle());
    // } else {
    //     MY_ASSERT(type == Cyberiada::elementTerminate);
    //     path.addEllipse(fullCircle());
    // }
    path.addEllipse(fullCircle());
    return path;
}

QRectF CyberiadaSMEditorVertexItem::fullCircle() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    return QRectF(- VERTEX_POINT_RADIUS,
                  - VERTEX_POINT_RADIUS,
                  VERTEX_POINT_RADIUS * 2,
                  VERTEX_POINT_RADIUS * 2);
}

QRectF CyberiadaSMEditorVertexItem::partialCircle() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    // Cyberiada::Rect r = element->get_bound_rect(*(model->rootDocument()));
    return QRectF(- VERTEX_POINT_RADIUS * 2.0 / 3.0,
                  - VERTEX_POINT_RADIUS * 2.0 / 3.0,
                  VERTEX_POINT_RADIUS * 4.0 / 3.0,
                  VERTEX_POINT_RADIUS * 4.0 / 3.0);
}

void CyberiadaSMEditorVertexItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    QColor color(Qt::black);
    if (isSelected()) {
        SettingsManager& sm = SettingsManager::instance();
        color = sm.getSelectionColor();
    }
    painter->setPen(QPen(color, 1, Qt::SolidLine));
    Cyberiada::ElementType type = element->get_type();
    if (type == Cyberiada::elementInitial) {
        painter->setBrush(QBrush(color));
        painter->drawEllipse(fullCircle());
    } else if (type == Cyberiada::elementFinal) {
        painter->setBrush(painter->background());
        painter->drawEllipse(fullCircle());
        painter->setBrush(QBrush(color));
        painter->drawEllipse(partialCircle());
    } else {
        MY_ASSERT(type == Cyberiada::elementTerminate);

        painter->setPen(QPen(color, 2, Qt::SolidLine));
        QRectF r = fullCircle();
        painter->drawEllipse(fullCircle());
        painter->drawLine(r.left(), r.top(), r.right(), r.bottom());
        painter->drawLine(r.right(), r.top(), r.left(), r.bottom());
    }
}

void CyberiadaSMEditorVertexItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (creatingOfTrans) {
        creatingOfTrans = false;
        startTransition();
        return;
    }

    if (!isEditable()) {
        event->ignore();
        return;
    }

    if (isLeftMouseButtonPressed) {
        setFlag(ItemIsMovable);
        model->updateGeometry(model->elementToIndex(element),
                              Cyberiada::Point(pos().x(), pos().y()));
    }

    QGraphicsItem::mouseMoveEvent(event);

    // a pseudostate dragged to the parent edge extends the parent, like a state
    if (parentItem()) {
        CyberiadaSMEditorAbstractItem* parent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem());
        if (parent) {
            parent->updateSizeToFitChildren(this);
        }
        StateRegion* stateArea = dynamic_cast<StateRegion*>(parentItem());
        if (stateArea) {
            parent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(stateArea->parentItem());
            if (parent) {
                parent->updateSizeToFitChildren(this);
            }
        }
    }

    emit geometryChanged();
}

void CyberiadaSMEditorVertexItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (!isSelected() || !isEditable()) {
        event->ignore();
        return;
    }

    setCursor(QCursor(Qt::SizeAllCursor));
    QGraphicsItem::hoverMoveEvent(event);
}
