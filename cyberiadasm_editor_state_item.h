/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The State Machine Editor State Item
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

#ifndef CYBERIADASMEDITORSTATEITEM_H
#define CYBERIADASMEDITORSTATEITEM_H


#include <QObject>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <QPainter>
#include <QDebug>

#include "dotsignal.h"
#include "editable_text_item.h"
#include "cyberiadasm_editor_items.h"

/* -----------------------------------------------------------------------------
 * State Item
 * ----------------------------------------------------------------------------- */

class StateRegion;
class StateTitle;
class StateAction;

class CyberiadaSMEditorStateItem : public CyberiadaSMEditorAbstractItem //, ItemWithText
{
    Q_OBJECT
    //Q_PROPERTY(QPointF previousPosition READ previousPosition WRITE setPreviousPosition NOTIFY previousPositionChanged)

public:
    explicit CyberiadaSMEditorStateItem(QObject *parent_object,
                       CyberiadaSMModel *model,
                       Cyberiada::Element *element,
                       QGraphicsItem *parent = NULL);
    ~CyberiadaSMEditorStateItem();

    virtual int type() const { return StateItem; }

    void setPreviousPosition(const QPointF previousPosition);

    void setRect(qreal x, qreal y, qreal w, qreal h);
    void setRect(const QRectF &rect);
    QRectF rect() const;
    qreal x() const;
    qreal y() const;
    qreal width() const;
    qreal height() const;

    StateRegion* getRegion();
    void updateRegion();

    QRectF boundingRect() const override;

    void syncFromModel() override;

    void setTextPosition();

private:
    void initializeActions();
    void addAction(Cyberiada::ActionType type);

signals:
    void rectChanged(CyberiadaSMEditorStateItem *rect);
    // void previousPositionChanged();
    void clicked(CyberiadaSMEditorStateItem *rect);
    void signalMove(QGraphicsItem *item, qreal dx, qreal dy);
    void aboutToDelete();

private slots:
    void onTextItemSizeChanged();
    void onActionDeleted(StateAction* signalOwner);
    void onActionChanged(StateAction* signalOwner);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    StateTitle* title;
    StateAction* entry = nullptr;
    StateAction* exit = nullptr;

    QRectF m_rect;
    StateRegion* region;
    const Cyberiada::State* state;
    std::vector<StateAction*> actions;

    void resizeLeft( const QPointF &pt);
    void resizeRight( const QPointF &pt);
    void resizeBottom(const QPointF &pt);
    void resizeTop(const QPointF &pt);

    void setPositionGrabbers();
    void showGrabbers();
    void hideGrabbers();
};


/* -----------------------------------------------------------------------------
 * State Region Item
 * ----------------------------------------------------------------------------- */

class StateRegion : public QGraphicsRectItem
{
public:
    explicit StateRegion(QGraphicsItem *parent = NULL):
        QGraphicsRectItem(parent) {}

    bool getTopLine() { return topLine; }
    bool getBottomLine() { return bottomLine; }

    void setTopLine(bool topLine) {
        this->topLine = topLine;
    }

    void setBottomLine(bool bottomLine) {
        this->bottomLine = bottomLine;
    }

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        Q_UNUSED(option)
        Q_UNUSED(widget)

        if(topLine) painter->drawLine(boundingRect().topLeft(), boundingRect().topRight());
        if(bottomLine) painter->drawLine(boundingRect().bottomLeft(), boundingRect().bottomRight());

        painter->setPen(Qt::blue);
        painter->drawRect(rect());

        painter->setBrush(Qt::blue);
        painter->drawEllipse(QPointF(0, 0), 2, 2); // Центр системы координат
    }

private:
    bool topLine = false;
    bool bottomLine = false;
};

/* -----------------------------------------------------------------------------
 * State Title Items
 * ----------------------------------------------------------------------------- */

class StateTitle : public EditableTextItem
{
public:
    explicit StateTitle(const QString &text,
                        QGraphicsItem *parent = nullptr):
        EditableTextItem(text, parent) {
        setTextAlignment(Qt::AlignCenter);
        setTextMargin(0);
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QPointF startPos;
    bool isMoving = false;
    bool isLeftMouseButtonPressed = false;
};

/* -----------------------------------------------------------------------------
 * State Action Item
 * ----------------------------------------------------------------------------- */

class StateAction : public EditableTextItem
{
    Q_OBJECT
public:
    explicit StateAction(const Cyberiada::Action* action,
                         QGraphicsItem *parent = nullptr);

    QString getText();

signals:
    void actionDeleted(StateAction* signalOwner);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    const Cyberiada::Action* action;
    QString typeText;
};

#endif // CYBERIADASMEDITORSTATEITEM_H
