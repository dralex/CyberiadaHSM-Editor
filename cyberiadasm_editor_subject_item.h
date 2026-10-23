/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The State Machine Editor Comment Link Item: the line from a comment to its
 * subject (PNST 984 7.2.7, EDIT-EDGE-6/17-20)
 *
 * Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
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

#ifndef CYBERIADASMEDITORSUBJECTITEM_H
#define CYBERIADASMEDITORSUBJECTITEM_H

#include <QGraphicsObject>
#include <QMap>
#include <QPainterPath>

#include <cyberiada/cyberiadamlpp.h>

#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_items.h"

class CyberiadaSMEditorTransitionItem;

// a subject is not an element: the item is no abstract element item, it is found by
// its comment and the subject index, and the ends are looked up by id when drawn
class CyberiadaSMEditorSubjectItem : public QGraphicsObject
{
    Q_OBJECT

public:
    enum { Type = CyberiadaSMEditorAbstractItem::TransitionItem + 1 };

    CyberiadaSMEditorSubjectItem(CyberiadaSMModel* model, Cyberiada::Comment* comment, int index,
                                 QMap<Cyberiada::ID, QGraphicsItem*>& elementItem);

    int type() const override { return Type; }
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    Cyberiada::Comment* getComment() const { return comment; }
    int getIndex() const { return index; }
    const Cyberiada::CommentSubject& subject() const;

    // the drawn ends in scene coordinates: the stored points, or the border
    // attachments toward the other end (EDIT-EDGE-17)
    QPointF sourceEnd() const;
    QPointF targetEnd() const;
    QPainterPath path() const;

    // a link is drawn only while both its ends are (EDIT-EDGE-19)
    static bool drawable(const Cyberiada::Comment* comment, const Cyberiada::CommentSubject& subject,
                         const QMap<Cyberiada::ID, QGraphicsItem*>& elementItem);

public slots:
    void updatePath();

private:
    CyberiadaSMEditorAbstractItem* commentItem() const;
    // the target node, or the commented transition and its source node, whose
    // centre is the frame of the target point (PNST 1044 9.1.1)
    CyberiadaSMEditorAbstractItem* targetNode() const;
    CyberiadaSMEditorTransitionItem* targetTransition() const;
    QPointF frameCentre() const;
    QPointF commentCentre() const;
    void connectEnd(QGraphicsItem* item);

    CyberiadaSMModel* model;
    Cyberiada::Comment* comment;
    int index;
    QMap<Cyberiada::ID, QGraphicsItem*>& elementIdToItemMap;
};

#endif // CYBERIADASMEDITORSUBJECTITEM_H
