/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
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

    QPainterPath path() const;
    void updatePath();

    DotSignal* getDot(int index);

    QString actionText() const;
    void updateAction();
    void updateActionPosition();
    void setActionVisibility(bool visible);

    void syncFromModel() override;

signals:
    // void clicked(CyberiadaSMEditorTransitionItem *rect);
    // void signalMove(QGraphicsItem *item, qreal dx, qreal dy);

private slots:
    void onSourceGeomertyChanged();
    void onTargetGeomertyChanged();
    void onSourceSizeChanged(CyberiadaSMEditorAbstractItem::CornerFlags side, qreal d);
    void onTargetSizeChanged(CyberiadaSMEditorAbstractItem::CornerFlags side, qreal d);
    void slotMoveDot(QGraphicsItem *signalOwner, qreal dx, qreal dy, QPointF p);
    void slotMouseReleaseDot();
    void slotDeleteDot(QGraphicsItem *signalOwner);

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;

private:
    void drawArrow(QPainter* painter);
    CyberiadaSMEditorAbstractItem* itemUnderCursor();
    QPointF findIntersectionWithItem(const CyberiadaSMEditorAbstractItem *item,
                                     const QPointF& start, const QPointF& end,
                                     bool* hasIntersections);
    void updateCoordinates(CyberiadaSMEditorAbstractItem::CornerFlags side,
                           QPointF& point, qreal d);
    void movePolyline(QPointF delta);

    const Cyberiada::Transition* transition;

    TransitionAction *actionItem = nullptr;
    QPointF textPosition;

    QPointF prevPosition;

    QList<DotSignal *> listDots;

    QMap<Cyberiada::ID, QGraphicsItem*>& elementIdToItemMap;

    bool isLeftMouseButtonPressed;
    bool isMouseTraking;
    bool isSourceTraking;
    bool isTargetTraking;

    void initializeDots() override;
    void updateDots();
    void showDots() override;
    void hideDots() override;
    void setDotsPosition() override;
};


/* -----------------------------------------------------------------------------
 * Transition Action Item
 * ----------------------------------------------------------------------------- */

class TransitionAction : public EditableTextItem
{
    Q_OBJECT
public:
    explicit TransitionAction(const QString &text, QGraphicsItem *parent = nullptr);

    QString getTrigger() const;
    QString getGuard() const;
    QString getBehaviour() const;

protected:
    void paint( QPainter *painter, const QStyleOptionGraphicsItem *o, QWidget *w) override;

    void focusOutEvent(QFocusEvent *event) override;
};

#endif // CYBERIADASMEDITORTRANSITIONITEM_H
