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

#include "cyberiadasm_editor_choice_item.h"
#include "myassert.h"

/* -----------------------------------------------------------------------------
 * Choice Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorChoiceItem::CyberiadaSMEditorChoiceItem(CyberiadaSMModel* model,
                                                         Cyberiada::Element* element,
                                                         QGraphicsItem* parent):
    CyberiadaSMEditorAbstractItem(model, element, parent)
{
}

void CyberiadaSMEditorChoiceItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));

    QRectF r = boundingRect();
    const QPointF points[] = {
        QPointF(r.left() + r.width() / 2.0, r.top()),
        QPointF(r.right(), r.top() + r.height() / 2.0),
        QPointF(r.left() + r.width() / 2.0, r.bottom()),
        QPointF(r.left(), r.top() + r.height() / 2.0)
    };

    painter->drawConvexPolygon(points, 4);
}
