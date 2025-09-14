/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The State Machine Editor State Machine Item
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
#include "cyberiadasm_editor_sm_item.h"
#include "myassert.h"
#include "settings_manager.h".

/* -----------------------------------------------------------------------------
 * State Machine Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorSMItem::CyberiadaSMEditorSMItem(CyberiadaSMModel* model,
                                                 Cyberiada::Element* element,
                                                 QGraphicsItem* parent):
    CyberiadaSMEditorAbstractItem(model, element, parent)
{
    QRectF rect = toQtRect(element->get_bound_rect(*(model->rootDocument())));

    if(element->has_geometry()) {
        setPos(rect.x(), rect.y());
    }
    setFlags(ItemIsSelectable);

    isHighlighted = false;

    initializeDots();
    setDotsPosition();
    hideDots();
}

QRectF CyberiadaSMEditorSMItem::boundingRect() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    MY_ASSERT(element);

    Cyberiada::Rect r = element->get_bound_rect(*(model->rootDocument()));
    QRectF rect = toQtRect(r);

    if(!element->has_geometry()) {
        return QRectF(rect.x() - rect.width() / 2, rect.y() - rect.height() / 2, rect.width(), rect.height());
    }
    return QRectF(- rect.width() / 2, - rect.height() / 2, rect.width(), rect.height());
}


void CyberiadaSMEditorSMItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    if (!element->has_geometry()) return;

    QPen pen = QPen(Qt::black, 2, Qt::SolidLine);
    if (isSelected() || isHighlighted) {
        SettingsManager& sm = SettingsManager::instance();
        pen.setColor(sm.getSelectionColor());
        pen.setWidth(sm.getSelectionBorderWidth());
        QColor fillColor = sm.getSelectionColor();
        fillColor.setAlpha(50);
        painter->setBrush(QBrush(fillColor));
    }

    painter->setPen(pen);
    QRectF r = boundingRect();
    painter->drawRect(r);
    const QPointF name_frame[] = {
        QPointF(r.left(), r.top()),
        QPointF(r.left() + 50, r.top()),
        QPointF(r.left() + 50, r.top() + 20),
        QPointF(r.left() + 40, r.top() + 30),
        QPointF(r.left(), r.top() + 30)
    };
    painter->drawConvexPolygon(name_frame, 5);
}



void CyberiadaSMEditorSMItem::updateSizeToFitChildren(CyberiadaSMEditorAbstractItem *child)
{
    // TODO copy from state
}
