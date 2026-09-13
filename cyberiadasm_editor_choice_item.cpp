/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The State Machine Editor Choice Item
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

#include "cyberiada_constants.h"
#include "cyberiadasm_editor_choice_item.h"
#include "cyberiadasm_editor_scene.h"
#include "myassert.h"
#include "settings_manager.h"

/* -----------------------------------------------------------------------------
 * Choice Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorChoiceItem::CyberiadaSMEditorChoiceItem(CyberiadaSMModel* model,
                                                         Cyberiada::Element* element,
                                                         QGraphicsItem* parent):
    CyberiadaSMEditorAbstractItem(model, element, parent)
{
    choice = static_cast<const Cyberiada::ChoicePseudostate*>(element);

    if (choice->has_geometry()) {
        Cyberiada::Rect r = choice->get_geometry_rect();
        setPos(QPointF(r.x, r.y));
    }

    setAcceptHoverEvents(true);
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);

    initializeDots();
    setDotsPosition();
    hideDots();
    // begin a transition from the four rhombus tips (or a body drag)
    enableTransitionSourceDots(QList<int>() << GrabberTop << GrabberBottom
                               << GrabberLeft << GrabberRight);
}

QPainterPath CyberiadaSMEditorChoiceItem::shape() const
{
    // the hit/attach shape is the rhombus, not the bounding rect, so the source
    // boxes and the transition endpoints match the drawn diamond
    QRectF r = boundingRect();
    QPolygonF diamond;
    diamond << QPointF(r.left() + r.width() / 2.0, r.top())
            << QPointF(r.right(), r.top() + r.height() / 2.0)
            << QPointF(r.left() + r.width() / 2.0, r.bottom())
            << QPointF(r.left(), r.top() + r.height() / 2.0);
    QPainterPath path;
    path.addPolygon(diamond);
    path.closeSubpath();
    return path;
}

void CyberiadaSMEditorChoiceItem::syncFromModel()
{
    prepareGeometryChange();
    if (choice->has_geometry()) {
        Cyberiada::Rect r = choice->get_geometry_rect();
        setPos(QPointF(r.x, r.y));
    }
    CyberiadaSMEditorAbstractItem::syncFromModel();
}

QRectF CyberiadaSMEditorChoiceItem::boundingRect() const
{
    if (!choice->has_geometry()) {
        return QRectF(- CHOICE_DEFAULT_SIZE / 2.0,
                      - CHOICE_DEFAULT_SIZE / 2.0,
                      CHOICE_DEFAULT_SIZE,
                      CHOICE_DEFAULT_SIZE);
    }
    Cyberiada::Rect r = choice->get_geometry_rect();
    return QRectF(- r.width / 2, - r.height / 2, r.width, r.height);
}

void CyberiadaSMEditorChoiceItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    QColor color(Qt::black);
    if (isSelected()) {
        color = SettingsManager::instance().getSelectionColor();
    }
    painter->setPen(QPen(color, 1, Qt::SolidLine));

    QRectF r = boundingRect();
    const QPointF points[] = {
        QPointF(r.left() + r.width() / 2.0, r.top()),
        QPointF(r.right(), r.top() + r.height() / 2.0),
        QPointF(r.left() + r.width() / 2.0, r.bottom()),
        QPointF(r.left(), r.top() + r.height() / 2.0)
    };

    painter->drawConvexPolygon(points, 4);
}

void CyberiadaSMEditorChoiceItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
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
        Cyberiada::Rect r = choice->get_geometry_rect();
        model->updateGeometry(model->elementToIndex(element),
                              Cyberiada::Rect(pos().x(), pos().y(), r.width, r.height));
    }

    QGraphicsItem::mouseMoveEvent(event);
    emit geometryChanged();
}

void CyberiadaSMEditorChoiceItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (!isSelected() || !isEditable()) {
        event->ignore();
        return;
    }

    setCursor(QCursor(Qt::SizeAllCursor));
    QGraphicsItem::hoverMoveEvent(event);
}
