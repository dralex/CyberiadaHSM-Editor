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
#include <QCoreApplication>
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
#include "cyberiadasm_editor_transition_item.h"
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
    title->setVisible(SettingsManager::instance().getShowText());
    connect(title, &EditableTextItem::sizeChanged, this, &CyberiadaSMEditorStateItem::onTextItemSizeChanged);

    initializeActions();

    if (state->is_composite_state()) {
        ensureRegion();
        updateRegion();
    }

    isHighlighted = false;

    initializeDots();
    setDotsPosition();
    hideDots();
    // a state draws a transition from any of its 8 border boxes
    enableTransitionSourceDots(QList<int>() << GrabberTop << GrabberBottom << GrabberLeft
                               << GrabberRight << GrabberTopLeft << GrabberTopRight
                               << GrabberBottomLeft << GrabberBottomRight);
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

// a simple state that gains a child becomes composite: its region must exist
// before the child item is added (a live reparent adds it from a row signal)
StateRegion *CyberiadaSMEditorStateItem::ensureRegion()
{
    if (state->is_composite_state() && region == nullptr) {
        region = new StateRegion(this);
        region->setVisibleRegon(SettingsManager::instance().getShowServiceObjects());
        // position the fresh region below the title/actions, as a
        // composite-from-load does: a state promoted to composite otherwise
        // leaves its region at (0,0), shifting the children on the next reload
        updateRegion();
    }
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
        // the document region has no place for the action separators
        region->setTopLine(false);
        region->setBottomLine(false);
        return;
    }

    // draw region based on state title and actions
    qreal top_delta = 0;
    qreal bottom_delta = 0;
    region->setTopLine(false);
    region->setBottomLine(false);

    if (SettingsManager::instance().getShowText()) {
        top_delta = title->boundingRect().height();
        if (entry) {
            top_delta += entry->boundingRect().height();
            region->setTopLine(true);
        }
        // the internal-transition block shares the top area with the entry, so
        // its height is reserved out of the child region too
        for (StateAction* a : internalActions) {
            top_delta += a->boundingRect().height();
            region->setTopLine(true);
        }
        if (exit) {
            bottom_delta += exit->boundingRect().height();
            region->setBottomLine(true);
        }
    }

    region->setRect(-width()/2, -(height() - top_delta - bottom_delta) / 2, width(), height() - top_delta - bottom_delta);
    region->setPos(0, (top_delta - bottom_delta)/2 );
    region_action_inset = top_delta + bottom_delta;
    m_topInset = top_delta;
    m_bottomInset = bottom_delta;
}

QRectF CyberiadaSMEditorStateItem::boundingRect() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    return rect();
}

QMarginsF CyberiadaSMEditorStateItem::contentInset() const
{
    // the children live in the region, inset below the title / entry / internal
    // blocks (top) and above the exit block (bottom); the region spans full width
    if (!state->is_composite_state()) return QMarginsF();
    return QMarginsF(0, m_topInset, 0, m_bottomInset);
}

qreal CyberiadaSMEditorStateItem::minSpanHeight() const
{
    // a composite keeps room for its action blocks even with no children
    if (!state->is_composite_state())
        return CyberiadaSMEditorAbstractItem::minSpanHeight();
    return std::max((qreal)ELEMENT_MIN_SIZE, (qreal)(region_action_inset + ELEMENT_MIN_SIZE));
}

qreal CyberiadaSMEditorStateItem::minimumWidth() const
{
    // a simple state has no children: keep the base floor
    if (!state->is_composite_state())
        return CyberiadaSMEditorAbstractItem::minimumWidth();
    // children live in the region, whose width equals the state width
    double hw, hh;
    model->childrenHalfExtent(static_cast<const Cyberiada::ElementCollection*>(element), hw, hh);
    return std::max((qreal)ELEMENT_MIN_SIZE, (qreal)(2.0 * hw));
}

qreal CyberiadaSMEditorStateItem::minimumHeight() const
{
    if (!state->is_composite_state())
        return CyberiadaSMEditorAbstractItem::minimumHeight();
    // the region (child area) sits below the title/entry/internal blocks and
    // above the exit block, so the state must be that much taller than the
    // content to keep the children inside the region
    double hw, hh;
    model->childrenHalfExtent(static_cast<const Cyberiada::ElementCollection*>(element), hw, hh);
    return std::max((qreal)ELEMENT_MIN_SIZE, (qreal)(2.0 * hh + region_action_inset));
}

bool CyberiadaSMEditorStateItem::isNameOnly() const
{
    return !state->has_actions() && !state->is_composite_state();
}

void CyberiadaSMEditorStateItem::setTextPosition()
{
    // TODO refactor
    QRectF oldRect = rect();
    QRectF titleRect = title->boundingRect();
    // a bare state (no actions, no children) centres the name in the box; any
    // action or nested state turns it into the top header, with the blocks below
    if (isNameOnly()) {
        title->setPos(oldRect.x() + (oldRect.width() - titleRect.width()) / 2,
                      oldRect.y() + (oldRect.height() - titleRect.height()) / 2);
        return;
    }
    title->setPos(oldRect.x() + (oldRect.width() - titleRect.width()) / 2 , oldRect.y());

    // the entry is pinned to the top-left of the region under the title and
    // the exit to its bottom-left, in the simple and the composite state alike
    qreal top = oldRect.y() + titleRect.height();
    if (entry != nullptr) {
        entry->setPos(oldRect.x() + 15, top);
        top += entry->boundingRect().height();
    }
    // the internal transitions stack below the entry
    for (StateAction* a : internalActions) {
        a->setPos(oldRect.x() + 15, top);
        top += a->boundingRect().height();
    }
    if (exit != nullptr) {
        exit->setPos(oldRect.x() + 15, oldRect.bottom() - exit->boundingRect().height());
    }
    // setPositionGrabbers();
}



void CyberiadaSMEditorStateItem::syncFromModel()
{
    // the bounding rect follows the model, so the size may change here
    prepareGeometryChange();
    setPos(QPointF(x(), y()));
    title->updateTextWidth();
    initializeActions();
    if (title->toPlainText() != name()) {
        title->setPlainText(name());
    }
    if (state->is_composite_state()) {
        ensureRegion();
        updateRegion();
    }
    // a reparent rebuilds the item through the model row signals
    CyberiadaSMEditorAbstractItem::syncFromModel();
}

void CyberiadaSMEditorStateItem::initializeActions()
{
    // the rebuild runs from the model change signal, which may originate in
    // an old action's own event handler (focus out, context menu) - the item
    // must outlive the current call stack
    for (StateAction* action : actions) {
        action->disconnect(this);
        action->hide();
        if (action->scene()) action->scene()->removeItem(action);
        action->deleteLater();
    }
    actions.clear();
    internalActions.clear();
    entry = nullptr;
    exit = nullptr;

    int actionIndex = 0;
    for (std::vector<Cyberiada::Action>::const_iterator i = state->get_actions().begin(); i != state->get_actions().end(); i++) {
        StateAction* action = new StateAction(&(*i), this);
        action->setVisible(SettingsManager::instance().getShowText());
        Cyberiada::ActionType type = i->get_type();
        if (type == Cyberiada::actionEntry) {
            entry = action;
        } else if (type == Cyberiada::actionExit) {
            exit = action;
        } else {
            // an internal transition (trigger/guard/behaviour) shares the top
            // block with the entry action
            internalActions.push_back(action);
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
    // batch mode has no user to dismiss a modal dialog: a scripted double
    // click would otherwise hang in the nested event loop
    if (qApp && qApp->property("batchMode").toBool()) return;

    StateActionDialog dialog(type == Cyberiada::actionEntry ? "entry" : "exit");

    if (dialog.exec() == QDialog::Accepted) {
        model->newAction(model->elementToIndex(element), type,
                         QString(), QString(), dialog.getBehaviour());
    }
}

void CyberiadaSMEditorStateItem::addInternalTransition()
{
    // batch mode has no user to dismiss a modal dialog (see addAction)
    if (qApp && qApp->property("batchMode").toBool()) return;

    StateActionDialog dialog(StateActionDialog::Mode::Transition);

    if (dialog.exec() == QDialog::Accepted) {
        model->newAction(model->elementToIndex(element), Cyberiada::actionTransition,
                         dialog.getTrigger(), dialog.getGuard(), dialog.getBehaviour());
    }
}

void CyberiadaSMEditorStateItem::updateSizeToFitChildren(CyberiadaSMEditorAbstractItem* child)
{
    if (!child || !element->has_geometry()) return;

    prepareGeometryChange();

    // the child measured in this state's own frame; a child is parented to the
    // region, which sits below the title/entry inset, so map through it
    QRectF childRect = mapRectFromItem(child, child->boundingRect());

    // the inner box the child must stay within: the region (the area left for
    // nested states below the title / entry / exit blocks) or the state rect
    QRectF inner = region ? mapRectFromItem(region, region->rect()) : rect();

    // how far the child spills past each side of the inner box
    qreal overLeft   = inner.left()       - childRect.left();
    qreal overRight  = childRect.right()  - inner.right();
    qreal overTop    = inner.top()        - childRect.top();
    qreal overBottom = childRect.bottom() - inner.bottom();

    // grow only the crossed edge, keeping the opposite edge fixed: the centre
    // shifts by half the growth, and every child re-bases by that half to hold
    // its absolute place. The dragged child's model was committed from the cursor
    // just before this call, so re-basing it too lands it back on the cursor.
    const qreal eps = 0.01;
    Cyberiada::Rect r = state->get_geometry_rect();
    CornerFlags sideX = CornerFlags(0), sideY = CornerFlags(0);
    qreal dX = 0.0, dY = 0.0;

    if (overRight > eps)      { r.x += overRight / 2; r.width += overRight; sideX = CornerFlags::Right; dX = overRight / 2; }
    else if (overLeft > eps)  { r.x -= overLeft / 2;  r.width += overLeft;  sideX = CornerFlags::Left;  dX = overLeft / 2; }
    if (overBottom > eps)     { r.y += overBottom / 2; r.height += overBottom; sideY = CornerFlags::Bottom; dY = overBottom / 2; }
    else if (overTop > eps)   { r.y -= overTop / 2;   r.height += overTop;  sideY = CornerFlags::Top;   dY = overTop / 2; }

    if (!sideX && !sideY) return;

    model->updateGeometry(model->elementToIndex(element), r);
    if (sideX) emit sizeChanged(sideX, dX);
    if (sideY) emit sizeChanged(sideY, dY);

    // the region follows the state size; refresh it so the inner box is in step
    // for the next step of the same drag gesture
    if (region) updateRegion();
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
    if (i >= actions.size()) return;   // the owner is gone (matches onActionChanged)

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
    if (i >= actions.size()) return;   // the owner is gone

    QModelIndex idx = model->elementToIndex(element);
    if (signalOwner->isTransition()) {
        // the whole "EVENT [guard] / behaviour" label was edited; an empty event
        // leaves no valid internal transition, so it is dropped rather than kept
        QString trigger = signalOwner->getTrigger();
        if (trigger.trimmed().isEmpty()) {
            model->deleteAction(idx, i);
            return;
        }
        model->updateAction(idx, i, trigger, signalOwner->getGuard(), signalOwner->getBehavior());
        return;
    }

    // an entry/exit emptied of its behaviour is deleted rather than kept as a ghost
    QString behavior = signalOwner->getBehavior();
    if (behavior.trimmed().isEmpty()) {
        model->deleteAction(idx, i);
        return;
    }
    model->updateAction(idx, i, QString(), QString(), behavior);
}

void CyberiadaSMEditorStateItem::slotInspectorModeChanged(bool on)
{
    if (state->is_composite_state()) {
        // the inspected region follows the document, the edited one follows the text
        updateRegion();
    }
    update();
}

void CyberiadaSMEditorStateItem::slotServiceObjectsChanged(bool on)
{
    // the region border is a decoration: the region geometry does not change
    if (state->is_composite_state()) {
        region->setVisibleRegon(on);
    }
    update();
}

void CyberiadaSMEditorStateItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget){
    Q_UNUSED(option)
    Q_UNUSED(widget)

    QPen pen = QPen(Qt::black, 2, Qt::SolidLine);
    if (Cyberiada::element_has_color(element)) {
        pen.setColor(QColor(QString::fromStdString(Cyberiada::element_get_color(element))));
    }
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
    // the header separator is drawn only when the name sits in the top header;
    // a bare state shows the name centred with no separating line
    if (SettingsManager::instance().getShowText() && !isNameOnly()) {
        qreal titleHeight = title->boundingRect().height();
        painter->drawLine(QPointF(tmpRect.x(), tmpRect.y() + titleHeight), QPointF(tmpRect.right(), tmpRect.y() + titleHeight));
    }
    painter->drawPath(path);

    if (SettingsManager::instance().getShowServiceObjects()) {
        painter->setBrush(Qt::red);
        painter->drawEllipse(QPointF(0, 0), 2, 2); // The center of the coordinate system
    }

}

void CyberiadaSMEditorStateItem::startTransition()
{
    CyberiadaSMEditorScene* cScene = dynamic_cast<CyberiadaSMEditorScene*>(scene());
    if (!cScene) return;
    // a state can be its own target: seed a self-loop and let the drag retarget
    cScene->beginTransientTool(ToolType::Transition);
    CyberiadaSMEditorTransitionItem* trans = cScene->addTransition(this, this);
    if (!trans) return;
    trans->setSelected(true);
    trans->getDot(1)->setVisible(true);
    trans->getDot(1)->grabMouse();
}

void CyberiadaSMEditorStateItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (creatingOfTrans) {
        creatingOfTrans = false;
        startTransition();
        return;
    }

    // a drag of the body moves the state like a drag of its border zones
    bool bodyDrag = isEditable() && isLeftMouseButtonPressed && cornerFlags == 0;
    if (bodyDrag) {
        // place the state at the cursor in SCENE space (not via ItemIsMovable,
        // whose parent-anchored delta would drift when the container grows
        // directionally under the drag) and commit its model BEFORE the parent
        // grow, so the parent's re-base holds this state at the cursor too
        QPointF target = snapToGrid(event->scenePos() + grabOffset);
        // Ctrl locks the move to the dominant axis (strict horizontal/vertical)
        if (event->modifiers() & Qt::ControlModifier) {
            QPointF d = event->scenePos() + grabOffset - dragStartScenePos;
            if (qAbs(d.x()) >= qAbs(d.y())) target.setY(dragStartScenePos.y());
            else                            target.setX(dragStartScenePos.x());
        }
        setPos(parentItem() ? parentItem()->mapFromScene(target) : target);
        Cyberiada::Rect r(pos().x(), pos().y(), boundingRect().width(), boundingRect().height());
        model->updateGeometry(model->elementToIndex(element), r);
    }

    // if you want to update this, update StateTitle::mouseMoveEvent as well
    CyberiadaSMEditorAbstractItem::mouseMoveEvent(event);
    CyberiadaSMEditorAbstractItem* newParent = collectionUnderItem();

    if (prevItemUnderCursor == newParent) return;

    if (newParent == nullptr || parentItem() == newParent){
        if (prevItemUnderCursor) prevItemUnderCursor->setHighlighted(false);
        prevItemUnderCursor = newParent;
        return;
    }

    if (prevItemUnderCursor) prevItemUnderCursor->setHighlighted(false);
    prevItemUnderCursor = newParent;
    newParent->setHighlighted(true);
}

// a new loop whose target dot takes the current drag over
int CyberiadaSMEditorStateItem::missingActionType() const
{
    if (!entry) return Cyberiada::actionEntry;
    if (!exit) return Cyberiada::actionExit;
    return -1;
}

void CyberiadaSMEditorStateItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    // the texts edit themselves on a double click; the free space adds one
    if (event->button() != Qt::LeftButton || !isEditable()) {
        QGraphicsItem::mouseDoubleClickEvent(event);
        return;
    }
    event->accept();
    // a double click on the name edits it (the title lets presses fall through, so
    // the box routes the edit); elsewhere on the box it adds a missing action block
    if (title && title->isVisible() && title->sceneBoundingRect().contains(event->scenePos())) {
        title->startEditing();
        return;
    }
    int type = missingActionType();
    if (type >= 0) {
        addAction(Cyberiada::ActionType(type));
    }
}

void CyberiadaSMEditorStateItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (creatingOfTrans) {
        // a press without a move leaves nothing behind
        creatingOfTrans = false;
        event->accept();
        return;
    }

    if (!isEditable()) {
        CyberiadaSMEditorAbstractItem::mouseReleaseEvent(event);
        return;
    }

    // if you want to update this, update StateTitle::mouseReleaseEvent as well
    CyberiadaSMEditorAbstractItem::mouseReleaseEvent(event);
    if (prevItemUnderCursor) prevItemUnderCursor->setHighlighted(false);
    updateParent(prevItemUnderCursor);
}

void CyberiadaSMEditorStateItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (!isEditable()) { return; }

    QMenu menu;

    QAction *deleteAction = menu.addAction(tr("Delete"));
    QAction *addTransitionAction = menu.addAction(tr("Add internal transition"));
    QAction *addEntryAction = menu.addAction(tr("Add entry"));
    QAction *addExitAction = menu.addAction(tr("Add exit"));
    QAction *addDoAction = menu.addAction(tr("Add do"));
    QAction *unlinkAction = menu.addAction(tr("Unlink"));

    if(entry != nullptr) {
        addEntryAction->setEnabled(false);
    }
    if(exit != nullptr) {
        addExitAction->setEnabled(false);
    }

    QAction *selectedAction = menu.exec(event->screenPos());

    if (selectedAction == deleteAction) {
        model->deleteElement(model->elementToIndex(element));
    } else if (selectedAction == addTransitionAction) {
        addInternalTransition();
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
        // dropped on empty space: keep it in its own state machine
        Cyberiada::StateMachine* sm = model->rootDocument()->get_parent_sm(element);
        if (sm) model->updateParent(model->elementToIndex(element), sm->get_id());
    } else {
        model->updateParent(model->elementToIndex(element), newParent->getId());
    }
}

/* -----------------------------------------------------------------------------
 * State Title Item
 * ----------------------------------------------------------------------------- */

StateTitle::StateTitle(const QString &text, QGraphicsItem *parent):
    EditableTextItem(text, parent) {
    setFontRole(fontRoleStateTitle);
    setTextAlignment(Qt::AlignCenter);
    setTextMargin(0);
}

// a refused title: the warning dialog, or the diagnostics in batch mode
static void titleRefused(const QString& message)
{
    if (qApp && qApp->property("batchMode").toBool()) {
        fprintf(stderr, "%s\n", qPrintable(message));
        return;
    }
    QMessageBox::warning(nullptr, QObject::tr("Warning"), message);
}

void StateTitle::focusOutEvent(QFocusEvent *event)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    isEdit = false;

    QTextCursor cursor = textCursor();
    cursor.clearSelection();
    setTextCursor(cursor);

    // works for any titled item (a state or a state machine border)
    CyberiadaSMEditorAbstractItem* owner = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem());
    if (owner == nullptr) { QGraphicsTextItem::focusOutEvent(event); return; }
    QString current = QString(owner->getElement()->get_name().c_str());
    QString newName = toPlainText().trimmed();
    QGraphicsTextItem::focusOutEvent(event);

    if (newName == current) { return; }

    if (newName.isEmpty()) {
        // an empty name silently keeps the previous one, no warning box
        setPlainText(current);
        return;
    }

    // the model refuses a name taken on this level
    if (!owner->getModel()->updateTitle(owner->getIndex(), newName)) {
        titleRefused(tr("The name \"%1\" already exists at this hierarchy level.").arg(newName));
        setPlainText(current);
    }
}

// a state's name must not be a drag dead zone: when it is not being edited, a
// press over the title of a STATE falls through to the state, whose body drag then
// moves it and grows its parent (the same anywhere on the box, including the
// centred name of a bare state); the name is edited by a double click routed by
// the state. The SM has no body drag, so its title keeps its own handling.
static bool titleFallsThrough(QGraphicsItem* parent, bool editing)
{
    return !editing && dynamic_cast<CyberiadaSMEditorStateItem*>(parent) != nullptr;
}

void StateTitle::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (titleFallsThrough(parentItem(), hasFocus())) { event->ignore(); return; }

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
    if (titleFallsThrough(parentItem(), hasFocus())) { event->ignore(); return; }

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
    if (titleFallsThrough(parentItem(), hasFocus())) { event->ignore(); return; }

    CyberiadaSMEditorScene* sc = dynamic_cast<CyberiadaSMEditorScene*>(scene());
    if (sc && sc->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }

    if (event->button() == Qt::LeftButton && !hasFocus()) {
        // a move/reparent release. Finish our own drag state first, because the
        // reparent below can reset the model and delete this very item - nothing
        // may touch `this` after updateParent().
        isMoving = false;
        isLeftMouseButtonPressed = false;
        setCursor(QCursor(Qt::ArrowCursor));

        CyberiadaSMEditorStateItem* state = dynamic_cast<CyberiadaSMEditorStateItem*>(parentItem());
        if (state != nullptr) {
            if (state->prevItemUnderCursor) state->prevItemUnderCursor->setHighlighted(false);
            state->updateParent(state->prevItemUnderCursor);
        }
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

    if (visible && SettingsManager::instance().getShowServiceObjects()) {
        painter->setPen(Qt::blue);
        painter->drawRect(rect());

        painter->setBrush(Qt::blue);
        painter->drawEllipse(QPointF(0, 0), 2, 2); // coordinate system origin
    }
}


/* -----------------------------------------------------------------------------
 * State Action Item
 * ----------------------------------------------------------------------------- */

StateAction::StateAction(const Cyberiada::Action* action, QGraphicsItem *parent):
    EditableTextItem(parent) {
    setFontRole(fontRoleStateAction);
    setTextMargin(30);

    QString behaviour = QString(action->get_behavior().c_str());
    Cyberiada::ActionType type = action->get_type();
    if (type == Cyberiada::ActionType::actionTransition) {
        // an internal transition reads like an edge label: EVENT [guard] / behaviour;
        // the whole label is editable, so nothing is protected (typeText stays empty)
        transition = true;
        QString text = QString(action->get_trigger().c_str());
        QString guard = QString(action->get_guard().c_str());
        if (!guard.isEmpty()) text += " [" + guard + "]";
        if (!behaviour.isEmpty()) text += " / " + behaviour;
        setPlainText(text);
        return;
    }
    // TODO "exit", "entry" and "/" are constants from cyberiadamlpp
    switch(type) {
    case Cyberiada::ActionType::actionEntry:
        typeText = QString("entry");
        break;
    case Cyberiada::ActionType::actionExit:
        typeText = QString("exit");
        break;
    default:
        typeText = QString("");
    }
    if (!typeText.isEmpty()) {
        // the behaviour always starts on its own line under the keyword, so the
        // "entry/" (or "exit/") header reads the same whatever the behaviour length
        typeText += QString("/\n");
    }

    setPlainText(typeText + behaviour);
}

QString StateAction::getBehavior()
{
    if (transition) {
        QString trigger, guard, behaviour;
        TransitionAction::parseLabel(toPlainText(), trigger, guard, behaviour);
        return behaviour;
    }
    return toPlainText().mid(typeText.length());
}

QString StateAction::getTrigger()
{
    QString trigger, guard, behaviour;
    TransitionAction::parseLabel(toPlainText(), trigger, guard, behaviour);
    return trigger;
}

QString StateAction::getGuard()
{
    QString trigger, guard, behaviour;
    TransitionAction::parseLabel(toPlainText(), trigger, guard, behaviour);
    return guard;
}

void StateAction::keyPressEvent(QKeyEvent *event)
{
    if (!isEdit) return;

    QTextCursor cursor = textCursor();
    const int protectedLen = typeText.length();
    int textLen = document()->toPlainText().length();

    // allow copy (Ctrl+C)
    if (event->matches(QKeySequence::Copy)) {
        QGraphicsTextItem::keyPressEvent(event);
        return;
    }

    // block any input while the cursor is in the protected zone
    if (((cursor.position() <= protectedLen && textLen > protectedLen) ||
         (cursor.position() < protectedLen && textLen >= protectedLen)) && !event->text().isEmpty()) {
        event->ignore();
        return;
    }

    // handle the selection
    if (cursor.hasSelection()) {
        int selStart = cursor.selectionStart();
        int selEnd = cursor.selectionEnd();

        // on delete or input, protect the reserved prefix
        if ((event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete || !event->text().isEmpty())) {
            // the selection touches the protected zone
            if (selStart < protectedLen && selEnd > protectedLen) {
                cursor.setPosition(protectedLen);
                cursor.setPosition(selEnd, QTextCursor::KeepAnchor);
                setTextCursor(cursor);
            }
            // fully inside the protected zone: block
            else if (selEnd < protectedLen) {
                event->ignore();
                return;
            }
        }
    }

    // block moving into the protected zone
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

void StateAction::beginTextEditing()
{
    // keep the caret out of the protected "entry/"/"exit/" prefix
    QTextCursor cursor = textCursor();
    if (cursor.position() < typeText.length()) {
        cursor.setPosition(typeText.length());
        setTextCursor(cursor);
    }
    startEditing();   // the interaction flags, focus and isEdit, in one place
}

void StateAction::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select ||
        SettingsManager::instance().getInspectorMode()) {
        event->ignore();
        return;
    }

    // a click past the prefix places the caret at that word first
    if (textCursor().position() >= typeText.length()) {
        QGraphicsTextItem::mouseDoubleClickEvent(event);
    }
    beginTextEditing();
    event->accept();
}

void StateAction::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
    if (SettingsManager::instance().getInspectorMode()) { return; }

    QMenu menu;

    QAction *deleteAction = menu.addAction(tr("Delete"));
    QAction *editAction = menu.addAction(tr("Edit text"));

    QAction *selectedAction = menu.exec(event->screenPos());

    if (selectedAction == deleteAction) {
        emit actionDeleted(this);
    } else if (selectedAction == editAction) {
        // enter editing exactly as a double-click does, only under the Select tool
        CyberiadaSMEditorScene* sc = dynamic_cast<CyberiadaSMEditorScene*>(scene());
        if (sc && sc->getCurrentTool() == ToolType::Select) {
            beginTextEditing();
        }
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

