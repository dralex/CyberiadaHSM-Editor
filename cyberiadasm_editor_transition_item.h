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

class TransitionText;

class CyberiadaSMEditorTransitionItem : public CyberiadaSMEditorAbstractItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    explicit CyberiadaSMEditorTransitionItem(QObject *parent_object,
                        CyberiadaSMModel *model,
                        Cyberiada::Element *element,
                        QGraphicsItem *parent,
                        QMap<Cyberiada::ID, QGraphicsItem*> &elementItem);
    ~CyberiadaSMEditorTransitionItem();


    virtual int type() const { return TransitionItem; }

    CyberiadaSMEditorAbstractItem *source() const;
    // void setSource(Rectangle *source);
    QPointF sourcePoint() const;
    // void setSourcePoint(const QPointF& point);
    QPointF sourceCenter() const;

    CyberiadaSMEditorAbstractItem *target() const;
    // void setTarget(Rectangle *target);
    QPointF targetPoint() const;
    // void setTargetPoint(const QPointF& point);
    QPointF targetCenter() const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr);
    QPainterPath shape() const override; // для обнаружения столкновений

    QPainterPath path() const;
    // void setPath(const QPainterPath &path);
    void updatePath();

    void drawArrow(QPainter* painter);

    QVector<QPointF> points() const;
    // void setPoints(const QVector<QPointF>& points);

    QString text() const;
    void setText(const QString& text);
    void setTextPosition(const QPointF& pos);
    void updateTextPosition();

    // QPointF findIntersectionWithRect(const State* state);

signals:
    void pathChanged();
    //    void rectChanged();
    //    void pointsChanged();
    void clicked(CyberiadaSMEditorTransitionItem *rect);
    void signalMove(QGraphicsItem *item, qreal dx, qreal dy);

private slots:
    void onSourceGeomertyChanged();
    // void slotMoveSource(QGraphicsItem *item, qreal dx, qreal dy);
    // void slotMoveTarget(QGraphicsItem *item, qreal dx, qreal dy);
    // void slotMove();
    // void slotStateResized(State *rect, State::CornerFlags side);
    void slotMoveDot(QGraphicsItem *signalOwner, qreal dx, qreal dy);

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    // void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    // void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;

private:
    // void updateCoordinates(State *state, State::CornerFlags side, QPointF *point, QPointF* previousCenterPos);

    const Cyberiada::Transition* transition;

    TransitionText *actionItem = nullptr;
    QPointF textPosition;

    // QPointF previousPosition;
    QPainterPath m_path;
    QVector<QPointF> m_points;

    QList<DotSignal *> listDots;

    QMap<Cyberiada::ID, QGraphicsItem*>& elementIdToItemMap;

    bool isLeftMouseButtonPressed;
    bool isMouseTraking;

    void initializeDots() override;
    void updateDots();
    void showDots() override;
    void hideDots() override;
    void setDotsPosition() override;
    void setPointPos(int pointIndex, qreal dx, qreal dy);
};

/* -----------------------------------------------------------------------------
 * Transition Text Item
 * ----------------------------------------------------------------------------- */

class TransitionText : public EditableTextItem
{
    Q_OBJECT
public:
    explicit TransitionText(const QString &text, QGraphicsItem *parent = nullptr);

    void paint( QPainter *painter, const QStyleOptionGraphicsItem *o, QWidget *w) override;
};

#endif // CYBERIADASMEDITORTRANSITIONITEM_H
