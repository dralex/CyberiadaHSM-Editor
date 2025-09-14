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
#include "cyberiadasm_editor_sm_item.h"
#include "cyberiadasm_editor_scene.h"
#include "dialogs/stateactiondialog.h"
#include "settings_manager.h"

// state action includes
#include <QTextCursor>
#include <QKeyEvent>
#include <QTextLayout>
#include <QTextDocument>

#include <QMessageBox>

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
    CyberiadaSMEditorAbstractItem::setPreviousPosition(QPointF(x(), y()));

    title = new StateTitle(name(), this);
    title->setFontBoldness(true);
    connect(title, &EditableTextItem::sizeChanged, this, &CyberiadaSMEditorStateItem::onTextItemSizeChanged);

    initializeActions();

    if (state->is_composite_state()) {
        region = new StateRegion(this);
        region->setVisibleRegon(SettingsManager::instance().getInspectorMode());
        updateRegion();
    }

    isHighlighted = false;
    creatingOfTrans = false;
    trans = nullptr;

    initializeDots();
    setDotsPosition();
    hideDots();
}

// TODO
CyberiadaSMEditorStateItem::~CyberiadaSMEditorStateItem()
{
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

QPainterPath CyberiadaSMEditorStateItem::shape() const {
    QPainterPath path;
    path.addRoundedRect(rect(), ROUNDED_RECT_RADIUS, ROUNDED_RECT_RADIUS);
    return path;
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

QString CyberiadaSMEditorStateItem::name() const
{
    return QString(state->get_name().c_str());
}

StateRegion *CyberiadaSMEditorStateItem::getRegion()
{
    return region;
}

void CyberiadaSMEditorStateItem::updateRegion()
{
    if (SettingsManager::instance().getInspectorMode()) {
        if (state->has_region_geometry()) {
            Cyberiada::Rect r = state->get_region_geometry_rect();
            region->setRect(QRectF(- r.width / 2,
                                   - r.height / 2,
                                   r.width,
                                   r.height));
        } else {
            region->setRect(rect());
        }
        region->setPos(0, 0);
        return;
    }

    // draw region based on state title and actions
    qreal top_delta = title->boundingRect().height();
    qreal bottom_delta = 0;
    region->setTopLine(false);
    region->setBottomLine(false);

    if (entry) {
        top_delta += entry->boundingRect().height();
        region->setTopLine(true);
    }
    if (exit) {
        bottom_delta += exit->boundingRect().height();
        region->setBottomLine(true);
    }

    region->setRect(-width()/2, -(height() - top_delta - bottom_delta) / 2, width(), height() - top_delta - bottom_delta);
    region->setPos(0, (top_delta - bottom_delta)/2 );
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
    // qDebug() << "synk" << name() << boundingRect() << pos() - QPointF(x(), y());
    QRectF r1 = mapRectToParent(boundingRect());
    // qDebug() << "before" << r1 << name();
    setPos(QPointF(x(), y()));
    QRectF r2 = mapRectToParent(boundingRect());
    // qDebug() << "after" << r2 << name();
    initializeActions();
    if (title->toPlainText() != name()) {
        title->setPlainText(name());
    }
    if (state->is_composite_state()) {
        if (region == nullptr) {
            region = new StateRegion(this);
            region->setVisibleRegon(SettingsManager::instance().getInspectorMode());
        }
        updateRegion();
    }

    CyberiadaSMEditorAbstractItem* cParent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem());
    if (cParent == nullptr) {
        // try to find region parent
        cParent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem()->parentItem());
        MY_ASSERT(cParent);
    }

    if (cParent->getId() != element->get_parent()->get_id()) {
        QGraphicsItem* newcParent = (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getMap()).value(element->get_parent()->get_id());
        QPointF posInThis = mapFromParent(pos());
        QPointF newCoords = mapToItem(newcParent, posInThis);
        Cyberiada::Rect newRect = Cyberiada::Rect(newCoords.x(), newCoords.y(), width(), height());
        setParentItem(newcParent);
        model->updateGeometry(model->elementToIndex(element), newRect);
        prevItemUnderCursor = static_cast<CyberiadaSMEditorAbstractItem*>(newcParent);
    }
    CyberiadaSMEditorAbstractItem::syncFromModel();
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
            entry = action;
        } else if (type == Cyberiada::actionExit) {
            exit = action;
        }
        connect(action, &EditableTextItem::sizeChanged, this, &CyberiadaSMEditorStateItem::onTextItemSizeChanged);
        connect(action, &StateAction::actionDeleted, this, &CyberiadaSMEditorStateItem::onActionDeleted);
        connect(action, &StateAction::actionUpdated, this, &CyberiadaSMEditorStateItem::onActionChanged);
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

void CyberiadaSMEditorStateItem::updateSizeToFitChildren(CyberiadaSMEditorAbstractItem* child)
{
    prepareGeometryChange();

    QRectF rect = child->mapRectToParent(child->boundingRect());

    QRectF newRect = boundingRect().united(rect);

    qDebug() << "parent change" << name() << (newRect.width() - boundingRect().width()) / 2 << child->pos();
    qDebug() << newRect << "|" << boundingRect() << "|" << rect;

    if (newRect.width() - boundingRect().width() == 0 && newRect.height() - boundingRect().height() == 0) {
        return;
    }
    Cyberiada::Rect r = Cyberiada::Rect(x(),
                                        y(),
                                        newRect.width(),
                                        newRect.height());

    // Cyberiada::Rect r = Cyberiada::Rect(pos().x() + (newRect.width() - boundingRect().width()) / 2,
    //                                     pos().y() + (newRect.height() - boundingRect().height()) / 2,
    if(newRect.x() < boundingRect().x()) {
        // qDebug() << "1";
        r = Cyberiada::Rect(r.x - (newRect.width() - boundingRect().width()) / 2, r.y, r.width - (newRect.width() - boundingRect().width()) / 2, r.height);
        model->updateGeometry(model->elementToIndex(element), r);
        emit sizeChanged(CornerFlags::Left, + (newRect.width() - boundingRect().width()) / 2);
    }
    else if (newRect.width() - boundingRect().width() != 0){
        // qDebug() << "2";
        r = Cyberiada::Rect(r.x + (newRect.width() - boundingRect().width()) / 2, r.y, r.width, r.height);
        model->updateGeometry(model->elementToIndex(element), r);
        emit sizeChanged(CornerFlags::Right, (newRect.width() - boundingRect().width()) / 2);
    }

    if(newRect.y() < boundingRect().y()) {
        // qDebug() << "3";
        r = Cyberiada::Rect(r.x, r.y - (newRect.height() - boundingRect().height()) / 2, r.width, r.height);
        model->updateGeometry(model->elementToIndex(element), r);
        emit sizeChanged(CornerFlags::Top, - (newRect.height() - boundingRect().height()) / 2);
    }
    else if (newRect.height() - boundingRect().height() != 0) {
        // qDebug() << "4";
        r = Cyberiada::Rect(r.x, r.y + (newRect.height() - boundingRect().height()) / 2, r.width, r.height);
        model->updateGeometry(model->elementToIndex(element), r);
        emit sizeChanged(CornerFlags::Bottom, (newRect.height() - boundingRect().height()) / 2);
    }
}

QStringList CyberiadaSMEditorStateItem::getSameLevelStateNames() const
{
    QStringList names;
    auto parentElementCollection = dynamic_cast<Cyberiada::ElementCollection*>(element->get_parent());
    if (!parentElementCollection) {
        return names;
    }

    if (parentElementCollection->has_children()){
        const Cyberiada::ElementList& children = parentElementCollection->get_children();
        for (Cyberiada::ElementList::const_iterator i = children.begin(); i != children.end(); i++) {
            Cyberiada::Element* child = *i;
            Cyberiada::ElementType type = child->get_type();

            switch(type) {
            case Cyberiada::elementCompositeState: {
                names << QString(child->get_name().c_str());
                break;
            }
            case Cyberiada::elementSimpleState: {
                names << QString(child->get_name().c_str());
                break;
            }
            }
        }
    }
    return names;
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

void CyberiadaSMEditorStateItem::onActionChanged(StateAction* signalOwner)
{
    int i;
    for(i = 0; i < actions.size(); i++){
        if(actions.at(i) == signalOwner){
            break;
        }
    }
    model->updateAction(model->elementToIndex(element), i, QString(), QString(), signalOwner->getBehavior());
}

void CyberiadaSMEditorStateItem::slotInspectorModeChanged(bool on)
{
    if (state->is_composite_state()) {
        region->setVisibleRegon(on);
    }
    update();
}

void CyberiadaSMEditorStateItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget){
    Q_UNUSED(option)
    Q_UNUSED(widget)

    qreal titleHeight = title->boundingRect().height();

    QPen pen = QPen(Qt::black, 2, Qt::SolidLine);
    if (isSelected() || isHighlighted) {
        SettingsManager& sm = SettingsManager::instance();
        pen.setColor(sm.getSelectionColor());
        pen.setWidth(sm.getSelectionBorderWidth());
        QColor fillColor = sm.getSelectionColor();
        fillColor.setAlpha(50);
        painter->setBrush(QBrush(fillColor));
    }

    painter->setPen(pen);

    QPainterPath path;
    QRectF tmpRect = rect();
    path.addRoundedRect(tmpRect, ROUNDED_RECT_RADIUS, ROUNDED_RECT_RADIUS);
    painter->drawLine(QPointF(tmpRect.x(), tmpRect.y() + titleHeight), QPointF(tmpRect.right(), tmpRect.y() + titleHeight));
    painter->drawPath(path);

    if (SettingsManager::instance().getInspectorMode()) {
        painter->setBrush(Qt::red);
        painter->drawEllipse(QPointF(0, 0), 2, 2); // The center of the coordinate system
    }

}

void CyberiadaSMEditorStateItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    CyberiadaSMEditorAbstractItem::mousePressEvent(event);

    if (cornerFlags == 0) {
        creatingOfTrans = true;
    }
}

void CyberiadaSMEditorStateItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    // TODO create transition
    if (creatingOfTrans) {
        if (!trans) {
            CyberiadaSMEditorScene* cScene = dynamic_cast<CyberiadaSMEditorScene*>(scene());
            if (!cScene) return;
            trans = cScene->addTransition(this, this);
            trans->setSelected(true);
            trans->getDot(1)->setVisible(true);
            trans->getDot(1)->grabMouse();
        }
        return;
    }

    // if you want to update this, update StateTitle::mouseMoveEvent as well
    CyberiadaSMEditorAbstractItem::mouseMoveEvent(event);
    CyberiadaSMEditorAbstractItem* newParent = collectionUnderItem();

    if (prevItemUnderCursor == newParent) return;

    if (newParent == nullptr || parentItem() == newParent){
        prevItemUnderCursor->setHighlighted(false);
        prevItemUnderCursor = newParent;
        return;
    }

    prevItemUnderCursor->setHighlighted(false);
    prevItemUnderCursor = newParent;
    newParent->setHighlighted(true);
}

void CyberiadaSMEditorStateItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    // TODO create transition
    if (creatingOfTrans && trans) {
        creatingOfTrans = false;
        trans = nullptr;

        CyberiadaSMEditorAbstractItem::mouseReleaseEvent(event);
        return;
    }

    // if you want to update this, update StateTitle::mouseReleaseEvent as well
    CyberiadaSMEditorAbstractItem::mouseReleaseEvent(event);
    prevItemUnderCursor->setHighlighted(false);
    updateParent(prevItemUnderCursor);
}

void CyberiadaSMEditorStateItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;

    QAction *deleteAction = menu.addAction("Удалить");
    QAction *addTransitionAction = menu.addAction("Добавить переход");
    QAction *addEntryAction = menu.addAction("Добавить entry");
    QAction *addExitAction = menu.addAction("Добавить exit");
    QAction *addDoAction = menu.addAction("Добавить do");
    QAction *unlinkAction = menu.addAction("Отвязать");

    if(entry != nullptr) {
        addEntryAction->setEnabled(false);
    }
    if(exit != nullptr) {
        addExitAction->setEnabled(false);
    }

    QAction *selectedAction = menu.exec(event->screenPos());

    if (selectedAction == deleteAction) {
        CyberiadaSMEditorScene* cScene = dynamic_cast<CyberiadaSMEditorScene*>(scene());
        if (cScene) {
            cScene->deleteItemsRecursively(element);
        }
    } else if (selectedAction == addTransitionAction) {

    } else if (selectedAction == addEntryAction) {
        addAction(Cyberiada::ActionType::actionEntry);
    } else if (selectedAction == addExitAction) {
        addAction(Cyberiada::ActionType::actionExit);
    } else if (selectedAction == addDoAction) {
        // TODO
    } else if (selectedAction == unlinkAction) {
        updateParent(nullptr);
    }

    event->accept();
}

void CyberiadaSMEditorStateItem::updateParent(CyberiadaSMEditorAbstractItem *newParent)
{
    if (newParent == nullptr) {
        model->updateParent(model->elementToIndex(element), model->rootDocument()->get_parent_sm(element)->get_id());
        // TODO update geometry pos
    } else {
        model->updateParent(model->elementToIndex(element), newParent->getId());
    }
}

/* -----------------------------------------------------------------------------
 * State Title Item
 * ----------------------------------------------------------------------------- */

StateTitle::StateTitle(const QString &text, QGraphicsItem *parent):
    EditableTextItem(text, parent) {
    setTextAlignment(Qt::AlignCenter);
    setTextMargin(0);
}

void StateTitle::focusOutEvent(QFocusEvent *event)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    isEdit = false;

    QTextCursor cursor = textCursor();
    cursor.clearSelection();
    setTextCursor(cursor);

    // check the uniqueness
    CyberiadaSMEditorStateItem* state = dynamic_cast<CyberiadaSMEditorStateItem*>(parentItem());
    if (state == nullptr) { return; }
    QStringList names = state->getSameLevelStateNames();
    QString newName = toPlainText().trimmed();

    if (newName.isEmpty()) {
        QMessageBox::warning(nullptr, "Предупреждение", QString("Имя не может быть пустым!"));
        setPlainText(state->name());
        QGraphicsTextItem::focusOutEvent(event);
        return;
    }

    if (newName == state->name()) {
        QGraphicsTextItem::focusOutEvent(event);
        return;
    }

    if (names.contains(newName)) {
        QMessageBox::warning(nullptr, "Предупреждение",
                             QString("Сосстояние с именем \"%1\" уже существует на этом уровне иерархии.").arg(newName));
        setPlainText(state->name());
        QGraphicsTextItem::focusOutEvent(event);
    }

    QGraphicsTextItem::focusOutEvent(event);
    state->model->updateTitle(state->model->elementToIndex(state->element), newName);
    // emit editingFinished();
}

void StateTitle::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    ToolType currentTool = dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool();
    if(currentTool != ToolType::Select) {
        event->ignore();
        return;
    }

    CyberiadaSMEditorStateItem* state = nullptr;
    if (parentItem()) {
        state = dynamic_cast<CyberiadaSMEditorStateItem*>(parentItem());
    }
    if (state != nullptr && SettingsManager::instance().getInspectorMode()) {
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && !hasFocus()) {
        startPos = event->scenePos();
        isMoving = true;
        isLeftMouseButtonPressed = true;

        setCursor(QCursor(Qt::ClosedHandCursor));
        QGraphicsScene *scene = this->scene();
        if (scene) {
            scene->clearSelection();
        }
        if (parentItem()) {
            parentItem()->setSelected(true);
        }
        event->accept();
        return;
    }
    QGraphicsTextItem::mousePressEvent(event);
}

void StateTitle::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    ToolType currentTool = dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool();
    CyberiadaSMEditorStateItem* state = nullptr;

    if (parentItem()) {
        state = dynamic_cast<CyberiadaSMEditorStateItem*>(parentItem());
    }

    if(currentTool != ToolType::Select ||
            (state != nullptr && SettingsManager::instance().getInspectorMode())) {
        event->ignore();
        return;
    }

    if (isLeftMouseButtonPressed && !hasFocus()) {
        if (isMoving && parentItem()) {
            if (state == nullptr) { return; }
            state->setSelected(true);

            QPointF delta = event->scenePos() - startPos;
            Cyberiada::Rect r = Cyberiada::Rect(state->pos().x() + delta.x(),
                                                state->pos().y() + delta.y(),
                                                state->boundingRect().width(),
                                                state->boundingRect().height());
            state->model->updateGeometry(state->model->elementToIndex(state->element), r);
            startPos = event->scenePos();

            if (state->parentItem()) {
                CyberiadaSMEditorAbstractItem* parent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(state->parentItem());
                if(parent) {
                    parent->updateSizeToFitChildren(state);
                }
                StateRegion* stateArea = dynamic_cast<StateRegion*>(parentItem());
                if(stateArea) {
                    parent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(stateArea->parentItem());
                    if(parent) {
                        parent->updateSizeToFitChildren(state);
                    }
                }
            }

            CyberiadaSMEditorAbstractItem* newParent = state->collectionUnderItem();

            if (state->prevItemUnderCursor == newParent) return;

            if (newParent == nullptr || state->parentItem() == newParent){
                state->prevItemUnderCursor->setHighlighted(false);
                state->prevItemUnderCursor = newParent;
                return;
            }

            state->prevItemUnderCursor->setHighlighted(false);
            state->prevItemUnderCursor = newParent;
            newParent->setHighlighted(true);
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

    if (parentItem()) {
        CyberiadaSMEditorStateItem* state = dynamic_cast<CyberiadaSMEditorStateItem*>(parentItem());
        state->prevItemUnderCursor->setHighlighted(false);
        state->updateParent(state->prevItemUnderCursor);
    }

    if (event->button() == Qt::LeftButton && !hasFocus()) {
        isMoving = false;
        isLeftMouseButtonPressed = false;
        setCursor(QCursor(Qt::ArrowCursor));
        return;
    }

    QGraphicsTextItem::mouseReleaseEvent(event);
}

/* -----------------------------------------------------------------------------
 * State Region Item
 * ----------------------------------------------------------------------------- */

void StateRegion::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if(topLine) painter->drawLine(boundingRect().topLeft(), boundingRect().topRight());
    if(bottomLine) painter->drawLine(boundingRect().bottomLeft(), boundingRect().bottomRight());

    if (visible) {
        painter->setPen(Qt::blue);
        painter->drawRect(rect());

        painter->setBrush(Qt::blue);
        painter->drawEllipse(QPointF(0, 0), 2, 2); // Центр системы координат
    }
}


/* -----------------------------------------------------------------------------
 * State Action Item
 * ----------------------------------------------------------------------------- */

StateAction::StateAction(const Cyberiada::Action* action, QGraphicsItem *parent):
    EditableTextItem(parent),
    action(action) {
    setTextMargin(30);

    Cyberiada::ActionType type = action->get_type();
    switch(type) {
    // TODO "exit", "entry" and "/" are constants from cyberiadamlpp
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

QString StateAction::getBehavior()
{
    QString fullText = toPlainText();
    return fullText.mid(typeText.length());
}

void StateAction::keyPressEvent(QKeyEvent *event)
{
    if (!isEdit) return;

    QTextCursor cursor = textCursor();
    const int protectedLen = typeText.length();
    int textLen = document()->toPlainText().length();

    // Разрешаем копирование (Ctrl+C)
    if (event->matches(QKeySequence::Copy)) {
        QGraphicsTextItem::keyPressEvent(event);
        return;
    }

    // Блокируем ввод любых символов, если курсор в запретной зоне
    if (((cursor.position() <= protectedLen && textLen > protectedLen) ||
         (cursor.position() < protectedLen && textLen >= protectedLen)) && !event->text().isEmpty()) {
        event->ignore();
        return;
    }

    // Обработка выделения
    if (cursor.hasSelection()) {
        int selStart = cursor.selectionStart();
        int selEnd = cursor.selectionEnd();

        // При удалении или вводе — защищаем запретную часть
        if ((event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete || !event->text().isEmpty())) {
            // Если выделение задевает запретную зону
            if (selStart < protectedLen && selEnd > protectedLen) {
                cursor.setPosition(protectedLen);
                cursor.setPosition(selEnd, QTextCursor::KeepAnchor);
                setTextCursor(cursor);
            }
            // Если полностью в запретной зоне — блокируем
            else if (selEnd < protectedLen) {
                event->ignore();
                return;
            }
        }
    }

    // Блок перемещения в запретную зону
    if (cursor.position() < protectedLen &&
        (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Left)) {
        event->ignore();
        return;
    }

    QGraphicsTextItem::keyPressEvent(event);
}

void StateAction::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->type() == QEvent::GraphicsSceneMouseDoubleClick) {
        return;
    }

    EditableTextItem::mousePressEvent(event);
}

void StateAction::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }

    QTextCursor cursor = textCursor();
    if (cursor.position() < typeText.length()) {
        cursor.setPosition(typeText.length());
        setTextCursor(cursor);
    } else {
        QGraphicsTextItem::mouseDoubleClickEvent(event);
    }

    isEdit = true;
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setFocus();

    // setTextCursor(cursor);
    event->accept();
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

void StateAction::focusOutEvent(QFocusEvent *event)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    isEdit = false;
    QGraphicsTextItem::focusOutEvent(event);
    emit actionUpdated(this);
}

