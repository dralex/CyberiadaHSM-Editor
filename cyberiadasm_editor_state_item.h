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
#include "cyberiadasm_editor_transition_item.h"

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
    friend class StateTitle;

public:
    explicit CyberiadaSMEditorStateItem(QObject *parent_object,
                       CyberiadaSMModel *model,
                       Cyberiada::Element *element,
                       QGraphicsItem *parent = NULL);
    ~CyberiadaSMEditorStateItem();

    virtual int type() const { return StateItem; }

    QPainterPath shape() const override;
    void setPreviousPosition(const QPointF previousPosition);

    void setRect(qreal x, qreal y, qreal w, qreal h);
    void setRect(const QRectF &rect);
    QRectF rect() const;
    qreal x() const;
    qreal y() const;
    qreal width() const;
    qreal height() const;
    QString name() const;

    // a bare state - only a name, no actions and no nested states - shows the name
    // centred in the box; adding any action or child moves it to the top header
    bool isNameOnly() const;

    StateRegion* getRegion();
    // create the region on demand when the state turned composite
    StateRegion* ensureRegion();
    void updateRegion();
    // the vertical space the title and the entry/exit action blocks take out of
    // the child region (updated by updateRegion); a nested child needs the
    // parent taller by this much
    qreal actionInset() const { return region_action_inset; }

    QRectF boundingRect() const override;

    // a composite floors at its content plus the action inset (a simple state
    // has no children and keeps the base minimum)
    qreal minimumWidth() const override;
    qreal minimumHeight() const override;
    // a composite reserves the title / entry / exit / internal blocks as an inset
    // the children may not enter
    QMarginsF contentInset() const override;
    qreal minSpanHeight() const override;

    void syncFromModel() override;

    void setTextPosition();

    // the action a double click on the free space adds, or -1
    int missingActionType() const;

private:
    void initializeActions();
    void addAction(Cyberiada::ActionType type);
    void addInternalTransition();
    // a state can self-loop, so it draws with the self-loop-then-retarget seed
    void startTransition() override;
    void updateSizeToFitChildren(CyberiadaSMEditorAbstractItem* child) override;

signals:
    void rectChanged(CyberiadaSMEditorStateItem *rect);
    // void previousPositionChanged();
    // void clicked(CyberiadaSMEditorStateItem *rect);
    // void signalMove(QGraphicsItem *item, qreal dx, qreal dy);
    void aboutToDelete();

private slots:
    void onTextItemSizeChanged();
    void onActionDeleted(StateAction* signalOwner);
    void onActionChanged(StateAction* signalOwner);

    void slotInspectorModeChanged(bool on) override;
    void slotServiceObjectsChanged(bool on) override;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    StateTitle* title;
    StateAction* entry = nullptr;
    StateAction* exit = nullptr;

    QRectF m_rect;
    StateRegion* region = nullptr;
    qreal region_action_inset = 0;
    qreal m_topInset = 0;      // title + entry + internal-transition blocks
    qreal m_bottomInset = 0;   // exit block
    const Cyberiada::State* state;
    std::vector<StateAction*> actions;
    // the internal-transition action blocks (type actionTransition), tracked so
    // their height is reserved in the region inset and they can be laid out
    std::vector<StateAction*> internalActions;
    
    void updateParent(CyberiadaSMEditorAbstractItem* newParent);

    void setGrabbersPosition();
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
        QGraphicsRectItem(parent) {
        // the region spans the composite's full width and sits above it, so it
        // must never intercept the border press that drives resize
        setAcceptedMouseButtons(Qt::NoButton);
        setAcceptHoverEvents(false);
    }

    bool getTopLine() { return topLine; }
    bool getBottomLine() { return bottomLine; }

    void setTopLine(bool topLine) {
        this->topLine = topLine;
    }

    void setBottomLine(bool bottomLine) {
        this->bottomLine = bottomLine;
    }

    void setVisibleRegon(bool on) {
        visible = on;
    }

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    bool topLine = false;
    bool bottomLine = false;
    bool visible = true;
};

/* -----------------------------------------------------------------------------
 * State Title Item
 * ----------------------------------------------------------------------------- */

class StateTitle : public EditableTextItem
{
public:
    explicit StateTitle(const QString &text,
                        QGraphicsItem *parent = nullptr);

protected:
    void focusOutEvent(QFocusEvent *event) override;
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
    QString getBehavior();
    // an internal transition is edited whole ("EVENT [guard] / behaviour"); its
    // trigger and guard come back parsed, unlike the entry/exit keyword header
    QString getTrigger();
    QString getGuard();
    bool isTransition() const { return transition; }
    int protectedLength() const override { return typeText.length(); }

signals:
    void actionDeleted(StateAction* signalOwner);
    void actionUpdated(StateAction* signalOwner);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    QString typeText;
    bool transition = false;
};

#endif // CYBERIADASMEDITORSTATEITEM_H
