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

#ifndef CYBERIADASM_EDITOR_SM_ITEM_H
#define CYBERIADASM_EDITOR_SM_ITEM_H

#include "cyberiadasm_editor_items.h"

/* -----------------------------------------------------------------------------
 * State Machine Item
 * ----------------------------------------------------------------------------- */

class CyberiadaSMEditorSMItem: public CyberiadaSMEditorAbstractItem {
public:
    CyberiadaSMEditorSMItem(CyberiadaSMModel* model,
                            Cyberiada::Element* element,
                            QGraphicsItem* parent = NULL);

    virtual int type() const { return SMItem; }

    // virtual QRectF boundingRect() const;
    QRectF boundingRect() const override;

    // virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
};


#endif // CYBERIADASM_EDITOR_SM_ITEM_H
