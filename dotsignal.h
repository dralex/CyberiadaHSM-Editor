/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * Dot Signal for the State Machine Editor Scene Items
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

#ifndef DOTSIGNAL_H
#define DOTSIGNAL_H

#include <QObject>
#include <QGraphicsRectItem>

class QGraphicsSceneHoverEventPrivate;
class QGraphicsSceneMouseEvent;

class DotSignal : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
    Q_PROPERTY(QPointF getPreviousPosition READ getPreviousPosition WRITE setPreviousPosition NOTIFY previousPositionChanged)

public:
    explicit DotSignal(QGraphicsItem *parentItem = 0, QObject *parent = 0);
    explicit DotSignal(QPointF pos, QGraphicsItem *parentItem = 0, QObject *parent = 0);
    ~DotSignal();

    enum Flags {
        Movable = 0x01
    };

    QPointF getPreviousPosition() const;
    void setPreviousPosition(const QPointF previousPosition);

    void setDotFlags(unsigned int flags);
    bool isDeleteable() { return deleteable; }
    void setDeleteable(bool on);
    void deleteDot();

signals:
    void previousPositionChanged();
    void signalPress(QGraphicsItem *signalOwner);
    void signalMouseRelease();
    void signalMove(QGraphicsItem *signalOwner, qreal dx, qreal dy, QPointF p);
    void signalDelete(QGraphicsItem *signalOwner);

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

public slots:

private:
    unsigned int flags;
    bool deleteable;
    QPointF previousPosition;
};

#endif // DOTSIGNAL_H
