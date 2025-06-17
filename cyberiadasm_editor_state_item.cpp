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

#include <QPainter>
#include <QDebug>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <math.h>

#include "myassert.h"
#include "dotsignal.h"
#include "cyberiada_constants.h"
#include "cyberiadasm_editor_state_item.h"
#include "cyberiadasm_editor_scene.h"
#include "dialogs/stateactiondialog.h"

// state action includes
#include <QTextCursor>
#include <QKeyEvent>

/* -----------------------------------------------------------------------------
 * State Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorStateItem::CyberiadaSMEditorStateItem(QObject *parent_object,
                     CyberiadaSMModel *model,
                     Cyberiada::Element *element,
                     QGraphicsItem *parent) :
    CyberiadaSMEditorAbstractItem(model, element, parent)
{
    setAcceptHoverEvents(true);
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);

    state = static_cast<const Cyberiada::State*>(element);

    setPos(QPointF(x(), y()));

    title = new StateTitle(state->get_name().c_str(), this);
    title->setFontBoldness(true);

    initializeActions();

    if (state->is_composite_state()) {
        connect(title, &EditableTextItem::sizeChanged, this, &CyberiadaSMEditorStateItem::onTextItemSizeChanged);
        region = new StateRegion(this);
        updateRegion();
    }

    initializeDots();
    setDotsPosition();
    hideDots();
}

// TODO
CyberiadaSMEditorStateItem::~CyberiadaSMEditorStateItem()
{
    // emit aboutToDelete();

    for (StateAction* action : actions) {
        delete action;
    }
    actions.clear();

    if (title != nullptr) { delete title; }

    // if(!element->has_geometry())
    //     return;
    // for(int i = 0; i < 8; i++){
    //     delete cornerGrabber[i];
    // }
}

void CyberiadaSMEditorStateItem::setRect(qreal x, qreal y, qreal w, qreal h)
{
    setRect(QRectF(x, y, w, h));
}

void CyberiadaSMEditorStateItem::setRect(const QRectF &rect)
{
    prepareGeometryChange();
    /*
    if (rect.width() >= 320 && rect.height() >= 180){
        QGraphicsRectItem::setRect(rect);
    }
    QRectF r = rect;
    if (r.height() < 180) {
        r.setHeight(180);
    }
    if (r.width() < 360){
        r.setWidth(360);
    }
    if (r.width() >= 320 && r.height() >= 180) {

    QGraphicsRectItem::setRect(rect);
    }
*/
    m_rect = rect;
}

QRectF CyberiadaSMEditorStateItem::rect() const {
    Cyberiada::Rect model_rect = state->get_geometry_rect();
    QRectF rect = QRectF(-model_rect.width / 2, -model_rect.height / 2, model_rect.width, model_rect.height);
    return rect;
}

qreal CyberiadaSMEditorStateItem::x() const
{
    Cyberiada::Rect model_rect = state->get_geometry_rect();
    return model_rect.x;
}

qreal CyberiadaSMEditorStateItem::y() const
{
    Cyberiada::Rect model_rect = state->get_geometry_rect();
    return model_rect.y;
}

qreal CyberiadaSMEditorStateItem::width() const
{
    Cyberiada::Rect model_rect = state->get_geometry_rect();
    return model_rect.width;
}

qreal CyberiadaSMEditorStateItem::height() const
{
    Cyberiada::Rect model_rect = state->get_geometry_rect();
    return model_rect.height;
}

StateRegion *CyberiadaSMEditorStateItem::getRegion()
{
    return region;
}

void CyberiadaSMEditorStateItem::updateRegion()
{
    // draw region based on state title and actions
    // qreal top_delta = title->boundingRect().height();
    // qreal bottom_delta = 0;
    // region->setTopLine(false);
    // region->setBottomLine(false);

    // if (entry) {
    //     top_delta += entry->boundingRect().height();
    //     region->setTopLine(true);
    // }
    // if (exit) {
    //     bottom_delta += exit->boundingRect().height();
    //     region->setBottomLine(true);
    // }

    // region->setRect(-width()/2, -(height() - top_delta - bottom_delta) / 2, width(), height() - top_delta - bottom_delta);
    // region->setPos(0, (top_delta - bottom_delta)/2 );

    if (state->has_region_geometry()) {
        Cyberiada::Rect r = state->get_region_geometry_rect();
        region->setRect(QRectF(- r.width / 2,
                             - r.height / 2,
                             r.width,
                             r.height));
    } else {
        region->setRect(rect());
    }
}

QRectF CyberiadaSMEditorStateItem::boundingRect() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    return rect();
}

void CyberiadaSMEditorStateItem::setTextPosition()
{
    // TODO refactor
    QRectF oldRect = rect();
    QRectF titleRect = title->boundingRect();
    title->setPos(oldRect.x() + (oldRect.width() - titleRect.width()) / 2 , oldRect.y());

    // simple state
    if (state->is_simple_state()) {
        if (entry != nullptr && exit != nullptr) {
            float delta = (entry->boundingRect().height() + exit->boundingRect().height());
            entry->setPos(oldRect.x() + 15, oldRect.y() + (height() + titleRect.height() - delta) / 2);
            exit->setPos(entry->pos() + QPointF(0, entry->boundingRect().height()));
            return;
        }
        if (entry != nullptr) {
            entry->setPos(oldRect.x() + 15, oldRect.y() + (height() + titleRect.height() - entry->boundingRect().height()) / 2);
        }
        if (exit != nullptr) {
            exit->setPos(oldRect.x() + 15, oldRect.y() + (height() + titleRect.height() - exit->boundingRect().height()) / 2);
        }
        return;
    }

    // composite state
    if (entry != nullptr) {
        entry->setPos(oldRect.x() + 15, oldRect.y() + titleRect.height());
    }
    if (exit != nullptr) {
        exit->setPos(oldRect.x() + 15, oldRect.bottom() - exit->boundingRect().height());
    }
    // setPositionGrabbers();
}

void CyberiadaSMEditorStateItem::syncFromModel()
{
    qDebug() << "sync" << state->get_id().c_str() << state->get_name().c_str();
    initializeActions();
    update();
}

void CyberiadaSMEditorStateItem::initializeActions()
{
    // TODO
    for (StateAction* action : actions) {
        delete action;
    }
    actions.clear();
    entry = nullptr;
    exit = nullptr;

    int actionIndex = 0;
    for (std::vector<Cyberiada::Action>::const_iterator i = state->get_actions().begin(); i != state->get_actions().end(); i++) {
        StateAction* action = new StateAction(&(*i), this);
        Cyberiada::ActionType type = i->get_type();
        if (type == Cyberiada::actionEntry) {
            // TODO "exit", "entry" and "/" are constants from cyberiadamlpp
            entry = action;
        } else if (type == Cyberiada::actionExit) {
            exit = action;
        }
        connect(action, &EditableTextItem::sizeChanged, this, &CyberiadaSMEditorStateItem::onTextItemSizeChanged);
        connect(action, &StateAction::actionDeleted, this, &CyberiadaSMEditorStateItem::onActionDeleted);
        // connect(action, &EditableTextItem::editingFinished, this, &CyberiadaSMEditorStateItem::onActionChanged);
        actions.push_back(action);
        actionIndex++;
    }

    setTextPosition();
}

void CyberiadaSMEditorStateItem::addAction(Cyberiada::ActionType type)
{
    StateActionDialog dialog;

    if (dialog.exec() == QDialog::Accepted) {
        // QString trigger = dialog.getTrigger();
        // QString guard = dialog.getGuard();
        QString trigger = QString("");
        QString guard = QString("");
        QString behaviour = dialog.getBehaviour();
        model->newAction(model->elementToIndex(element), type, trigger, guard, behaviour);
    }
}

void CyberiadaSMEditorStateItem::onTextItemSizeChanged()
{
    if (state->is_composite_state()) updateRegion();
    setTextPosition();
}

void CyberiadaSMEditorStateItem::onActionDeleted(StateAction* signalOwner)
{
    int i;
    for(i = 0; i < actions.size(); i++){
        if(actions.at(i) == signalOwner){
            break;
        }
    }

    model->deleteAction(model->elementToIndex(element), i);
}

void CyberiadaSMEditorStateItem::onActionChanged(StateAction* signalOwner) //, )
{
    // TODO
    // int i;
    // for(i = 0; i < actions.size(); i++){
    //     if(actions.at(i) == signalOwner){
    //         break;
    //     }
    // }

    // model->updateAction(model->elementToIndex(element), i, );
}

void CyberiadaSMEditorStateItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget){
    Q_UNUSED(option)
    Q_UNUSED(widget)

    qreal titleHeight = title->boundingRect().height();

    QPen pen = QPen(Qt::black, 2, Qt::SolidLine);
    if (isSelected()) {
        pen.setColor(Qt::red);
        pen.setWidth(2);
        painter->setBrush(QBrush(QColor(255, 0, 0, 50)));
        // painter->setBrush(QBrush(QColor(66, 170, 255, 50)));
    }

    painter->setPen(pen);

    QPainterPath path;
    QRectF tmpRect = rect();
    path.addRoundedRect(tmpRect, ROUNDED_RECT_RADIUS, ROUNDED_RECT_RADIUS);
    painter->drawLine(QPointF(tmpRect.x(), tmpRect.y() + titleHeight), QPointF(tmpRect.right(), tmpRect.y() + titleHeight));
    painter->drawPath(path);

    painter->setBrush(Qt::red);
    painter->drawEllipse(QPointF(0, 0), 2, 2); // The center of the coordinate system
}

void CyberiadaSMEditorStateItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;

    // QAction *renameAction = menu.addAction("Переименовать");
    QAction *deleteAction = menu.addAction("Удалить");
    QAction *addTransitionAction = menu.addAction("Добавить переход");
    QAction *addEntryAction = menu.addAction("Добавить entry");
    QAction *addExitAction = menu.addAction("Добавить exit");
    QAction *addDoAction = menu.addAction("Добавить do");

    if(entry != nullptr) {
        addEntryAction->setEnabled(false);
    }
    if(exit != nullptr) {
        addExitAction->setEnabled(false);
    }

    QAction *selectedAction = menu.exec(event->screenPos());

    if (selectedAction == deleteAction) {
        // static_cast<CyberiadaSMEditorScene*>(scene())->removeSMItem(element);
        // delete this;
    } else if (selectedAction == addTransitionAction) {

    } else if (selectedAction == addEntryAction) {
        addAction(Cyberiada::ActionType::actionEntry);
    } else if (selectedAction == addExitAction) {
        addAction(Cyberiada::ActionType::actionExit);
    } else if (selectedAction == addDoAction) {

    }

    event->accept();
}

/* -----------------------------------------------------------------------------
 * State Title Item
 * ----------------------------------------------------------------------------- */

void StateTitle::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    ToolType currentTool = dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool();
    if(currentTool != ToolType::Select) {
        event->ignore();
        return;
    }

    if (event->button() == Qt::LeftButton and !hasFocus()) {
        startPos = event->scenePos();
        isMoving = true;
        isLeftMouseButtonPressed = true;
        event->accept();
        return;
    }
    QGraphicsTextItem::mousePressEvent(event);
}

void StateTitle::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    ToolType currentTool = dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool();
    if(currentTool != ToolType::Select) {
        event->ignore();
        return;
    }

    if (isLeftMouseButtonPressed && !hasFocus()) {
        if (isMoving && parentItem()) {
            QPointF delta = event->scenePos() - startPos;
            parentItem()->setPos(parentItem()->pos() + delta);
            startPos = event->scenePos();
        }
        return;
    }
    QGraphicsTextItem::mouseMoveEvent(event);
}

void StateTitle::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    ToolType currentTool = dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool();
    if(currentTool != ToolType::Select) {
        event->ignore();
        return;
    }

    if (event->button() == Qt::LeftButton and !hasFocus()) {
        isMoving = false;
        isLeftMouseButtonPressed = false;
        return;
    }
    QGraphicsTextItem::mouseReleaseEvent(event);
}

/* -----------------------------------------------------------------------------
 * State Region Item
 * ----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
 * State Action Item
 * ----------------------------------------------------------------------------- */

StateAction::StateAction(const Cyberiada::Action* action, QGraphicsItem *parent):
    EditableTextItem(parent),
    action(action) {
    setTextMargin(30);

    Cyberiada::ActionType type = action->get_type();
    switch(type) {
    case Cyberiada::ActionType::actionEntry:
        typeText = QString("entry / ");
        break;
    case Cyberiada::ActionType::actionExit:
        typeText = QString("exit / ");
        break;
    default:
        typeText = QString("");
    }

    setPlainText(typeText + QString(action->get_behavior().c_str()));
}

void StateAction::keyPressEvent(QKeyEvent *event)
{
    if (!isEdit) return;

    QTextCursor cursor = textCursor();
    if (cursor.position() <= typeText.length() &&
        (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Left)) {
        // Blocking deletion/relocation to a protected area
        event->ignore();
        return;
    }

    QGraphicsTextItem::keyPressEvent(event);
}

void StateAction::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsTextItem::mousePressEvent(event);

    // Protect against setting the cursor to the beginning of the type
    QTextCursor cursor = textCursor();
    if (cursor.position() < typeText.length()) {
        cursor.setPosition(typeText.length());
        setTextCursor(cursor);
    }
}

void StateAction::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
    QMenu menu;

    QAction *deleteAction = menu.addAction("Удалить");
    QAction *editAction = menu.addAction("Редактировать текст");

    QAction *selectedAction = menu.exec(event->screenPos());

    if (selectedAction == deleteAction) {
        emit actionDeleted(this);
    } else if (selectedAction == editAction) {
        // Switch to text editing mode
        setTextInteractionFlags(Qt::TextEditorInteraction);
        setFocus();
    }

    event->accept();
}
