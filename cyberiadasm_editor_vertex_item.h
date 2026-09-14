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

#ifndef CYBERIADASMEDITORVERTEXITEM_H
#define CYBERIADASMEDITORVERTEXITEM_H

#include "cyberiadasm_editor_items.h"
#include "editable_text_item.h"
#include "dotsignal.h"

class CyberiadaSMEditorVertexItem;

/* -----------------------------------------------------------------------------
 * Vertex Name
 * ----------------------------------------------------------------------------- */

// the editable name drawn under a pseudostate/final point; a press falls through
// to the point (so it drags), a double click edits it, and the empty name is a
// valid state (the vertex simply shows none)
class VertexTitle : public EditableTextItem {
public:
    VertexTitle(const QString& text, CyberiadaSMEditorVertexItem* parent);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
};

/* -----------------------------------------------------------------------------
 * Vertex Item
 * ----------------------------------------------------------------------------- */

class CyberiadaSMEditorVertexItem: public CyberiadaSMEditorAbstractItem {
public:
    CyberiadaSMEditorVertexItem(CyberiadaSMModel* model,
                                Cyberiada::Element* element,
                                QGraphicsItem* parent = NULL);

    virtual int type() const { return VertexItem; }
    void syncFromModel() override;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event);

private:
    QRectF fullCircle() const;
    QRectF partialCircle() const;
    void setTitlePosition();

    VertexTitle* title = nullptr;
};


#endif // CYBERIADASMEDITORVERTEXITEM_H
