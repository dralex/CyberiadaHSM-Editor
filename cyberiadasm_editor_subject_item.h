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
#include <QList>
#include <QPointer>
#include <QPainterPath>
#include <QVector>
#include <QLineF>

#include <cyberiada/cyberiadamlpp.h>

#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_items.h"

class CyberiadaSMEditorTransitionItem;
class DotSignal;

// a subject is not an element: the item is no abstract element item, it is found by
// its comment and the subject index, and the ends are looked up by id when drawn
class CyberiadaSMEditorSubjectItem : public QGraphicsObject
{
    Q_OBJECT

public:
    enum { Type = CyberiadaSMEditorAbstractItem::TransitionItem + 1 };

    CyberiadaSMEditorSubjectItem(CyberiadaSMModel* model, Cyberiada::Comment* comment, int index,
                                 QMap<Cyberiada::ID, QGraphicsItem*>& elementItem);
    ~CyberiadaSMEditorSubjectItem();

    int type() const override { return Type; }
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    Cyberiada::Comment* getComment() const { return comment; }
    int getIndex() const { return index; }
    const Cyberiada::CommentSubject& subject() const;
    // false once the subject is gone: the item waits for its removal and draws nothing
    bool hasSubject() const;
    // leave the ends and forget the comment before the model frees it: the item
    // waits for its deletion and reads nothing
    void detach();

    // the drawn ends in scene coordinates: the stored points, or the border
    // attachments toward the other end (EDIT-EDGE-17)
    QPointF sourceEnd() const;
    QPointF targetEnd() const;
    QPainterPath path() const;
    // the underline of a fragment in the drawn target text, a segment per line, in
    // scene coordinates; empty when the fragment is not drawn (EDIT-EDGE-21)
    QVector<QLineF> underline() const;

    // a link is drawn only while both its ends are (EDIT-EDGE-19)
    static bool drawable(const Cyberiada::Comment* comment, const Cyberiada::CommentSubject& subject,
                         const QMap<Cyberiada::ID, QGraphicsItem*>& elementItem);

    // the end handles show on the selected link under the selection tool (EDIT-EDGE-20)
    void refreshDots();
    DotSignal* sourceDot() const { return dots[0]; }
    DotSignal* targetDot() const { return dots[1]; }
    // remove the subject, never the comment
    bool deleteSubject();

public slots:
    void updatePath();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private slots:
    void slotMoveDot(QGraphicsItem* owner, qreal dx, qreal dy, QPointF p);
    void slotReleaseDot(QGraphicsItem* owner, QPointF p);
    // a stored end keeps its place over a resize of its node and stays on the border
    void onCommentSizeChanged(CyberiadaSMEditorAbstractItem::CornerFlags side, qreal d);
    void onTargetSizeChanged(CyberiadaSMEditorAbstractItem::CornerFlags side, qreal d);

private:
    bool editable() const;
    // the element under a dragged target end that may take the subject, not the
    // current target; null when none
    CyberiadaSMEditorAbstractItem* retargetUnder(const QPointF& p) const;
    // where a dragged end lands: on the comment border, on the target border or path,
    // or loose over a new target
    QPointF dragEnd(int end, const QPointF& p) const;
    QModelIndex commentIndex() const;
    CyberiadaSMEditorAbstractItem* commentItem() const;
    // the target node, or the commented transition and its source node, whose
    // centre is the frame of the target point (PNST 1044 9.1.1)
    CyberiadaSMEditorAbstractItem* targetNode() const;
    CyberiadaSMEditorTransitionItem* targetTransition() const;
    // the item of the target itself: the node or the transition
    CyberiadaSMEditorAbstractItem* targetItem() const;
    QPointF frameCentre() const;
    QPointF commentCentre() const;
    // follow the comment, the target node (the frame of a transition target) and the
    // other end of a commented transition
    void connectEnds();

    CyberiadaSMModel* model;
    Cyberiada::Comment* comment;
    int index;
    QMap<Cyberiada::ID, QGraphicsItem*>& elementIdToItemMap;
    DotSignal* dots[2] = { nullptr, nullptr };
    QList<QPointer<CyberiadaSMEditorAbstractItem>> connectedEnds;
    Cyberiada::ID connectedTarget;
    // the end being dragged (0 the source, 1 the target, -1 none) and where it is
    int dragging = -1;
    QPointF dragPoint;
};

#endif // CYBERIADASMEDITORSUBJECTITEM_H
