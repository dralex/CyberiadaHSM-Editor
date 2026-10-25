/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The State Machine Editor Transition Item
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

#ifndef CYBERIADASMEDITORTRANSITIONITEM_H
#define CYBERIADASMEDITORTRANSITIONITEM_H


#include <QGraphicsItem>
#include <QObject>
#include <QPainterPath>
#include <QVector>
#include <QPointF>
#include <QGraphicsTextItem>
#include <QString>
#include <QPainter>

#include "cyberiadasm_editor_items.h"
#include "dotsignal.h"
#include "editable_text_item.h"


/* -----------------------------------------------------------------------------
 * Transition Item
 * ----------------------------------------------------------------------------- */

class TransitionAction;

class CyberiadaSMEditorTransitionItem : public CyberiadaSMEditorAbstractItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

    friend class TransitionAction;

public:
    explicit CyberiadaSMEditorTransitionItem(QObject *parent_object,
                        CyberiadaSMModel *model,
                        Cyberiada::Element *element,
                        QGraphicsItem *parent,
                        QMap<Cyberiada::ID, QGraphicsItem*> &elementItem);
    ~CyberiadaSMEditorTransitionItem();

    virtual int type() const { return TransitionItem; }

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr);
    QPainterPath shape() const override;

    // convert a stored label POINT to a label RECT sized from the current text, so
    // an old point-label diagram becomes a rect one in edit mode; a no-op in
    // inspection mode and for labels that are already rects or auto-placed. Silent
    // (a direct element write): it neither pushes an undo step nor marks the
    // document modified, so opening a file does not dirty it.
    void migrateLabelToRect();

    CyberiadaSMEditorAbstractItem *source() const;
    void setSource(CyberiadaSMEditorAbstractItem *newSource);
    QPointF sourcePoint() const;
    void setSourcePoint(const QPointF& point);
    void setSourcePoint(const Cyberiada::Point& point);
    QPointF sourceCenter() const;
    Cyberiada::ID sourceId() const {
        return transition->source_element_id();
    }

    CyberiadaSMEditorAbstractItem *target() const;
    void setTarget(CyberiadaSMEditorAbstractItem *newTarget);
    QPointF targetPoint() const;
    void setTargetPoint(const QPointF& point);
    QPointF targetCenter() const;
    Cyberiada::ID targetId() const {
        return transition->target_element_id();
    }

    // seed a placed self-loop with a default orthogonal border-to-border
    // polyline; the two sides follow the initial and final drag direction, the
    // modifiers of the placing gesture decide the snap (EDIT-EDGE-16)
    void seedDefaultLoop(const QPointF& startDir, const QPointF& endDir,
                         Qt::KeyboardModifiers mods);

    // the coordinate a Ctrl drag of an endpoint locks to its neighbour
    enum LockedAxis { LockNone, LockX, LockY };
    // EDIT-EDGE-15: a border point on a straight edge of a rect node slid along
    // the edge to the nearest grid line within it; unchanged otherwise
    QPointF snapAlongBorder(const CyberiadaSMEditorAbstractItem* item, const QPointF& p,
                            Qt::KeyboardModifiers mods, LockedAxis locked) const;

    QPainterPath path() const;
    void updatePath();

    DotSignal* getDot(int index);

    // the scene rect of the shown label, null when hidden (a comment link foot keeps off it)
    QRectF labelSceneRect() const;

    QString actionText() const;
    void updateAction();
    void updateActionPosition();
    void setActionVisibility(bool visible);

    void syncFromModel() override;
    // the label draws the action fields (EDIT-EDGE-21)
    EditableTextItem* fieldText(const SubjectFragment::Field& field, int* start) const override;

signals:
    // void clicked(CyberiadaSMEditorTransitionItem *rect);
    // void signalMove(QGraphicsItem *item, qreal dx, qreal dy);

private slots:
    void onSourceGeomertyChanged();
    void onTargetGeomertyChanged();
    void onSourceSizeChanged(CyberiadaSMEditorAbstractItem::CornerFlags side, qreal d);
    void onTargetSizeChanged(CyberiadaSMEditorAbstractItem::CornerFlags side, qreal d);
    void slotMoveDot(QGraphicsItem *signalOwner, qreal dx, qreal dy, QPointF p);
    void slotMouseReleaseDot(QGraphicsItem *signalOwner, QPointF p);
    bool isAncestorOf(CyberiadaSMEditorAbstractItem *anc, CyberiadaSMEditorAbstractItem *item) const;
    void slotDeleteDot(QGraphicsItem *signalOwner);
    // the label-box corner handles: directional width resize, opposite side anchored
    void slotMoveLabelHandle(QGraphicsItem *signalOwner, qreal dx, qreal dy, QPointF p);
    void slotReleaseLabelHandle(QGraphicsItem *signalOwner, QPointF p);

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;

private:
    // true when the two items resolve to elements of different state machines, so a
    // retarget binding one end there would cross machines (EDIT-STRUCT-4)
    bool crossesMachine(CyberiadaSMEditorAbstractItem* moving, CyberiadaSMEditorAbstractItem* fixed) const;
    bool isArcLoop() const;
    // the border side a drag direction points at
    enum LoopSide { LoopTop, LoopBottom, LoopLeft, LoopRight };
    static LoopSide sideFromDir(const QPointF& dir);
    // the path segment (0-based) that a local point falls on, or -1
    int segmentAt(const QPointF& localPos) const;
    // erase the polyline vertex behind interior dot dotIndex (endpoints ignored)
    void removePolylineVertex(int dotIndex);
    void drawArrow(QPainter* painter);
    // the node under the cursor itself: a snapped point may leave a small node
    CyberiadaSMEditorAbstractItem* itemUnderCursor(const QPointF& cursor);
    // the attachment of an end without a stored point: the node border toward
    // the next point of the edge, a vertex at its centre
    QPointF attachedPoint(const CyberiadaSMEditorAbstractItem* item, const QPointF& toward) const;
    // forwardOnly: the border in the direction of the end alone, not behind
    QPointF findIntersectionWithItem(const CyberiadaSMEditorAbstractItem *item,
                                     const QPointF& start, const QPointF& end,
                                     bool* hasIntersections,
                                     bool forwardOnly = false) const;
    // findIntersectionWithItem with the found point snapped along the border
    QPointF snappedIntersection(const CyberiadaSMEditorAbstractItem* item,
                                const QPointF& start, const QPointF& end,
                                bool* hasIntersections,
                                Qt::KeyboardModifiers mods, LockedAxis locked) const;
    // re-base a centre-relative endpoint after its source/target was resized:
    // keep its absolute scene position, then re-attach it to the new border
    void updateCoordinates(CyberiadaSMEditorAbstractItem::CornerFlags side,
                           QPointF& point, qreal d, const CyberiadaSMEditorAbstractItem* item);
    // put a centre-relative point exactly on the border of a rect of the given
    // half-extents (clamp inside, then snap to the nearest edge)
    static void clampOntoBorder(QPointF& p, qreal hw, qreal hh);
    void movePolyline(QPointF delta);

    const Cyberiada::Transition* transition;

    TransitionAction *actionItem = nullptr;
    QPointF textPosition;

    QPointF prevPosition;

    // the initial drag direction of a self-loop being placed (captured once,
    // used to pick the source-side of the default polyline on release)
    QPointF loopStartDir;
    bool loopStartCaptured = false;

    QList<DotSignal *> listDots;
    // the four label-box corner handles (green boxes), shown when the transition
    // is selected under the selection tool; a drag resizes the label rect
    QList<DotSignal *> labelDots;
    bool labelHandleDragging = false;

    QMap<Cyberiada::ID, QGraphicsItem*>& elementIdToItemMap;

    bool isLeftMouseButtonPressed;
    bool isMouseTraking;
    bool isSourceTraking;
    bool isTargetTraking;

    // a press on a segment arms a vertex insertion; the drag creates it and
    // then carries the new vertex (its polyline index)
    int pendingSegment = -1;
    QPointF pressPos;
    bool vertexDragArmed = false;
    int draggingVertex = -1;

    // an interior dot being dragged and its offset from the cursor: the point
    // follows the cursor, so a snap never swallows the small move steps
    QGraphicsItem* vertexDragDot = nullptr;
    QPointF vertexGrab;

    void initializeDots() override;
    void updateDots();
    // take a handle out of the scene at once, then defer its deletion: the rebuild may
    // run from the dot's own event handler, so it must outlive the call stack, but it
    // must not stay in the draw tree (a repaint would paint a freed item)
    void retireDot(DotSignal* dot);
    void showDots() override;
    void hideDots() override;
    void setDotsPosition() override;
    void initializeLabelDots();
    void setLabelDotsPosition();
};


/* -----------------------------------------------------------------------------
 * Transition Action Item
 * ----------------------------------------------------------------------------- */

class TransitionAction : public EditableTextItem
{
    Q_OBJECT
public:
    explicit TransitionAction(const QString &text, QGraphicsItem *parent = nullptr);

    // split "EVENT [guard] / behaviour" into its parts (any may be empty)
    static void parseLabel(const QString& text, QString& trigger, QString& guard, QString& behaviour);

    QString getTrigger() const;
    QString getGuard() const;
    QString getBehaviour() const;
    bool isDragging() const { return dragging; }
    // the event name is bold when a guard or behaviour follows it (a lonely event
    // stays normal), behind the transition event-name preference
    void applyRichFormat() override;

protected:
    void paint( QPainter *painter, const QStyleOptionGraphicsItem *o, QWidget *w) override;

    void focusOutEvent(QFocusEvent *event) override;
    // the label can be dragged along the scene; the release persists its point
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    void persistLabelPosition(bool refitWidth = false);
    QPointF dragLast;
    bool dragging = false;
};

#endif // CYBERIADASMEDITORTRANSITIONITEM_H
