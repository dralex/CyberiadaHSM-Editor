/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Editor Scene
 *
 * Copyright (C) 2024 Alexey Fedoseev <aleksey@fedoseev.net>
 *
 * Based on the Qt Visual Graph Editor (QVGE)
 * Copyright (C) 2016-2021 Ars L. Masiuk's <ars.masiuk@gmail.com>
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

#include <QDebug>
#include <QPainter>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QPen>
#include <QCursor>
#include <QMessageBox>

#include <cmath>
#include <cstdio>
#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_editor_items.h"
#include "cyberiadasm_editor_sm_item.h"
#include "cyberiadasm_editor_state_item.h"
#include "cyberiadasm_editor_vertex_item.h"
#include "cyberiadasm_editor_transition_item.h"
#include "cyberiadasm_editor_comment_item.h"
#include "cyberiadasm_editor_choice_item.h"
#include "cyberiada_constants.h"
#include "smeditor_window.h"
#include "settings_manager.h"
#include "gesture_log.h"
#include "myassert.h"

static double DEFAULT_SCENE_X = -500;
static double DEFAULT_SCENE_Y = -500;
static double DEFAULT_SCENE_WIDTH = 1000;
static double DEFAULT_SCENE_HEIGHT = 1000;
static double DEFAULT_SCENE_DELTA = 0.2;
static double DEFAULT_SCENE_BORDER_MARGIN = 50;

// report a creation error: a modal box in the GUI, a stderr line in batch mode
// (a modal exec() would hang the headless batch run)
static void showError(const QString& title, const QString& text)
{
    if (qApp && qApp->property("batchMode").toBool()) {
        fprintf(stderr, "%s: %s\n", qPrintable(title), qPrintable(text));
        return;
    }
    QMessageBox::critical(NULL, title, text);
}

CyberiadaSMEditorScene::CyberiadaSMEditorScene(CyberiadaSMModel* _model, QObject *_parent):
    QGraphicsScene(_parent), model(_model), currentSM(NULL)
{
    // gridSize = 25;
    // gridEnabled = true;
    // gridSnap = true;
    gridPen = QPen(Qt::gray, 0, Qt::DotLine);
    connect(&SettingsManager::instance(), &SettingsManager::gridSettingsChanged, this, &CyberiadaSMEditorScene::slotGridSettingsChanged);
    connect(&SettingsManager::instance(), &SettingsManager::serviceObjectsChanged, this, &CyberiadaSMEditorScene::slotServiceObjectsChanged);

	setBackgroundBrush(Qt::white);
    connect(this, &QGraphicsScene::selectionChanged, this, &CyberiadaSMEditorScene::slotSelectionChanged);
    connect(model, &CyberiadaSMModel::dataChanged, this, &CyberiadaSMEditorScene::slotModelDataChanged);
    connect(model, &CyberiadaSMModel::rowsInserted, this, &CyberiadaSMEditorScene::slotRowsInserted);
    connect(model, &CyberiadaSMModel::rowsAboutToBeRemoved, this, &CyberiadaSMEditorScene::slotRowsAboutToBeRemoved);
    connect(model, &CyberiadaSMModel::modelAboutToBeReset, this, &CyberiadaSMEditorScene::slotModelAboutToBeReset);
    connect(model, &CyberiadaSMModel::modelReset, this, &CyberiadaSMEditorScene::slotModelReset);
    reset();
}

// a snapshot restore replaces every element: the items are rebuilt and the
// selection is brought back by id
void CyberiadaSMEditorScene::slotModelAboutToBeReset()
{
    selectedBeforeReset.clear();
    for (QGraphicsItem* item : selectedItems()) {
        CyberiadaSMEditorAbstractItem* cItem = dynamic_cast<CyberiadaSMEditorAbstractItem*>(item);
        if (cItem) selectedBeforeReset.append(cItem->getId());
    }
}

void CyberiadaSMEditorScene::slotModelReset()
{
    if (model->firstSMIndex().isValid()) {
        loadScene(false);
    } else {
        elementIdToItemMap.clear();
        clear();
        currentSM = NULL;
    }
    for (const Cyberiada::ID& id : selectedBeforeReset) {
        QGraphicsItem* item = elementIdToItemMap.value(id);
        if (item) item->setSelected(true);
    }
    selectedBeforeReset.clear();
}

CyberiadaSMEditorScene::~CyberiadaSMEditorScene()
{
    // the items are destroyed with the scene; stop reacting to their
    // deselection, which would read the already-freed item map
    disconnect(this, &QGraphicsScene::selectionChanged,
               this, &CyberiadaSMEditorScene::slotSelectionChanged);
}

void CyberiadaSMEditorScene::reset()
{
	clear();
	setSceneRect(DEFAULT_SCENE_X,
				 DEFAULT_SCENE_Y,
				 DEFAULT_SCENE_WIDTH,
				 DEFAULT_SCENE_HEIGHT);
	update();
}

void CyberiadaSMEditorScene::slotSelectionChanged() {
    if (selectedItems().size() > 0) {
        QGraphicsItem* currItem = nullptr;
        for (QGraphicsItem *item : selectedItems()) {
            if (auto cItem = dynamic_cast<CyberiadaSMEditorAbstractItem*>(item)) {
                currItem = cItem;
            }
        }
        if (currItem == nullptr) return;
        Cyberiada::ID item_id = elementIdToItemMap.key(currItem);
        const Cyberiada::Element* element = model->idToElement(QString::fromStdString(item_id));

        if(!element) return;
        MY_ASSERT(element);
		QModelIndex index = model->elementToIndex(element);
        // the scene may live without the editor window (batch, tests)
        CyberiadaSMEditorWindow* p = dynamic_cast<CyberiadaSMEditorWindow*>(parent());
        if (p) {
            p->SMView->select(index);
        }
	}
}

void CyberiadaSMEditorScene::slotElementSelected(const QModelIndex& index)
{
    if (index.isValid() && index != model->rootIndex() && index != model->documentIndex()) {
        Cyberiada::Element* element = model->indexToElement(index);
        const Cyberiada::ID element_id = element->get_id().c_str();
        MY_ASSERT(element);

        blockSignals(true);
        clearSelection();
        blockSignals(false);

        QGraphicsItem* item = elementIdToItemMap.value(element_id);
        if (item) {
            blockSignals(true);
            item->setSelected(true);
            blockSignals(false);
        }

        // new elements land in the machine the user is working in
        Cyberiada::StateMachine* sm = model->rootDocument()->get_parent_sm(element);
        if (sm) currentSM = sm;

        // Cyberiada::StateMachine* sm = model->rootDocument()->get_parent_sm(element);
        // if (sm != currentSM) {
        //     currentSM = sm;
        //     /*Cyberiada::Rect bound = currentSM->get_bound_rect();
        //     clear();
        //     if (bound.valid) {
        //         setSceneRect(-(bound.width / 2.0) * (1.0 + DEFAULT_SCENE_DELTA),
        //                      -(bound.height / 2.0) * (1.0 + DEFAULT_SCENE_DELTA),
        //                      bound.width * (1.0 + 2.0 * DEFAULT_SCENE_DELTA),
        //                      bound.height * (1.0 + 2.0 * DEFAULT_SCENE_DELTA));
        //         QRectF r = sceneRect();
        //         qDebug() << "Scene (" << r.left() << ", " << r.top() << ", " << r.right() << ", " << r.bottom() << ")";
        //     } else {
        //         //reconstructGeometry();
        //     }*/

        //     addItemsRecursively(NULL, currentSM);
        //     // TODO
        //     views().first()->fitInView(itemsBoundingRect(), Qt::KeepAspectRatio);
        //     update();
        // }
	}
}

void CyberiadaSMEditorScene::updateItemsRecursively(CyberiadaSMEditorAbstractItem* parent, Cyberiada::ElementCollection* collection)
{
    CyberiadaSMEditorAbstractItem* current_item = static_cast<CyberiadaSMEditorAbstractItem*>(elementIdToItemMap.value(collection->get_id()));
    current_item->syncFromModel();

    // updating children
    if (collection->has_children()) {
        const Cyberiada::ElementList& children = collection->get_children();
        for (Cyberiada::ElementList::const_iterator i = children.begin(); i != children.end(); i++) {
            Cyberiada::Element* child = *i;
            updateItemsRecursively(current_item, static_cast<Cyberiada::ElementCollection*>(child));
        }
    }
}

void CyberiadaSMEditorScene::removeItemsForElement(Cyberiada::Element* element)
{
    // children first: their items are deleted individually before the parent
    Cyberiada::ElementCollection* collection = dynamic_cast<Cyberiada::ElementCollection*>(element);
    if (collection && collection->has_children()) {
        const Cyberiada::ElementList& children = collection->get_children();
        for (Cyberiada::ElementList::const_iterator i = children.begin(); i != children.end(); i++) {
            removeItemsForElement(*i);
        }
    }
    QGraphicsItem* item = elementIdToItemMap.take(element->get_id());
    if (item) {
        releaseInputFor(item);
        delete item;
    }
}

void CyberiadaSMEditorScene::noteModified(const QModelIndex& index)
{
    const Cyberiada::Element* element = model->indexToElement(index);
    // walk up to the owning state machine and remember it for zoom-to-SM
    for (const Cyberiada::Element* e = element; e; e = e->get_parent()) {
        if (e->get_type() == Cyberiada::elementSM) {
            lastModifiedSM = e->get_id();
            return;
        }
    }
}

QRectF CyberiadaSMEditorScene::recentlyModifiedSMRect() const
{
    Cyberiada::ID id = lastModifiedSM;
    if (id.empty() && currentSM) id = currentSM->get_id();
    if (id.empty()) return QRectF();
    QGraphicsItem* item = elementIdToItemMap.value(id);
    if (!item) return QRectF();
    return item->sceneBoundingRect();
}

void CyberiadaSMEditorScene::releaseInputFor(QGraphicsItem* item)
{
    if (!item) return;
    QGraphicsItem* grabber = mouseGrabberItem();
    if (grabber && (grabber == item || item->isAncestorOf(grabber))) {
        grabber->ungrabMouse();
    }
    QGraphicsItem* focus = focusItem();
    if (focus && (focus == item || item->isAncestorOf(focus))) {
        focus->clearFocus();
    }
}

void CyberiadaSMEditorScene::slotRowsInserted(const QModelIndex& parent, int first, int last)
{
    QGraphicsItem* parent_item = graphicsParentFor(model->indexToElement(parent));
    if (!parent_item) {
        // the document root has no item, so a newly inserted top-level state
        // machine would get none: build any state machine still missing one,
        // as loadScene does (the root index gives no usable child element)
        std::vector<Cyberiada::StateMachine*> sms = model->rootDocument()->get_state_machines();
        for (std::vector<Cyberiada::StateMachine*>::iterator i = sms.begin(); i != sms.end(); i++) {
            if (elementIdToItemMap.value((*i)->get_id())) continue;
            addItemsRecursively(NULL, *i);
            QGraphicsItem* smItem = elementIdToItemMap.value((*i)->get_id());
            if (auto sm = dynamic_cast<CyberiadaSMEditorSMItem*>(smItem)) {
                connect(sm, &CyberiadaSMEditorAbstractItem::sizeChanged,
                        this, &CyberiadaSMEditorScene::slotSMSizeChanged);
            }
            if (!currentSM) currentSM = *i;
            // note the machine directly: the document-root parent index has no
            // usable element for noteModified() to resolve
            lastModifiedSM = (*i)->get_id();
        }
        update();
        return;
    }
    for (int row = first; row <= last; row++) {
        Cyberiada::Element* element = model->indexToElement(model->index(row, 0, parent));
        if (element) {
            addElementItem(element, parent_item);
        }
    }
    noteModified(parent);
    update();
}

void CyberiadaSMEditorScene::slotRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last)
{
    // the indexes are still valid here, before the model frees the elements
    noteModified(parent);
    for (int row = first; row <= last; row++) {
        Cyberiada::Element* element = model->indexToElement(model->index(row, 0, parent));
        if (element) {
            removeItemsForElement(element);
        }
    }
    update();
}

void CyberiadaSMEditorScene::slotModelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    noteModified(topLeft);
    Cyberiada::Element* element = model->indexToElement(topLeft);
    if (!element) return;
    CyberiadaSMEditorAbstractItem* current_item = dynamic_cast<CyberiadaSMEditorAbstractItem*>(elementIdToItemMap.value(element->get_id()));
    if (current_item == nullptr) {
        // the element id may have changed: find its item and re-key the map
        for (QMap<Cyberiada::ID, QGraphicsItem*>::iterator i = elementIdToItemMap.begin();
             i != elementIdToItemMap.end(); i++) {
            CyberiadaSMEditorAbstractItem* candidate = dynamic_cast<CyberiadaSMEditorAbstractItem*>(i.value());
            if (candidate && candidate->getElement() == element) {
                current_item = candidate;
                elementIdToItemMap.erase(i);
                elementIdToItemMap.insert(element->get_id(), current_item);
                break;
            }
        }
    }
    if (current_item != nullptr) {
        current_item->syncFromModel();
    }
    update();
}

void CyberiadaSMEditorScene::slotSMSizeChanged(CyberiadaSMEditorAbstractItem::CornerFlags side, qreal d)
{
    // TODO
}

void CyberiadaSMEditorScene::slotGridSettingsChanged()
{
    update();
}

void CyberiadaSMEditorScene::slotServiceObjectsChanged()
{
    // the scene origin marker lives in the background
    update();
}

QGraphicsItem* CyberiadaSMEditorScene::graphicsParentFor(const Cyberiada::Element* parent)
{
    if (!parent) return NULL;
    QGraphicsItem* item = elementIdToItemMap.value(parent->get_id());
    if (item && parent->get_type() == Cyberiada::elementCompositeState) {
        // the children of a composite state live in its region; a state that
        // just turned composite (a live reparent) needs it created now
        return static_cast<CyberiadaSMEditorStateItem*>(item)->ensureRegion();
    }
    return item;
}

// an item built with a parent enters the scene with it, so adding it again
// would either warn or, when the parent is not attached yet, detach the item
void CyberiadaSMEditorScene::addSceneItem(QGraphicsItem* item)
{
    if (!item->parentItem()) {
        addItem(item);
    }
}

QGraphicsItem* CyberiadaSMEditorScene::addElementItem(Cyberiada::Element* child, QGraphicsItem* new_parent)
{
    QGraphicsItem* item = NULL;
    switch (child->get_type()) {
    case Cyberiada::elementCompositeState: {
        CyberiadaSMEditorStateItem* state = new CyberiadaSMEditorStateItem(this, model, child, new_parent);
        elementIdToItemMap.insert(child->get_id(), state);
        // the subtree is built inside an attached root, or Qt would detach it
        addSceneItem(state);
        addItemsRecursively(state->getRegion(), static_cast<Cyberiada::ElementCollection*>(child));
        return state;
    }
    case Cyberiada::elementSimpleState:
        item = new CyberiadaSMEditorStateItem(this, model, child, new_parent);
        break;
    case Cyberiada::elementInitial:
    case Cyberiada::elementFinal:
    case Cyberiada::elementTerminate:
        item = new CyberiadaSMEditorVertexItem(model, child, new_parent);
        break;
    case Cyberiada::elementChoice:
        item = new CyberiadaSMEditorChoiceItem(model, child, new_parent);
        break;
    case Cyberiada::elementComment:
        item = new CyberiadaSMEditorCommentItem(this, model, child, new_parent, elementIdToItemMap);
        break;
    case Cyberiada::elementFormalComment:
        // the geometry-less formal comments (the document meta) are not drawn
        if (child->has_geometry()) {
            item = new CyberiadaSMEditorCommentItem(this, model, child, new_parent, elementIdToItemMap);
        }
        break;
    case Cyberiada::elementTransition:
        item = new CyberiadaSMEditorTransitionItem(this, model, child, NULL, elementIdToItemMap);
        break;
    default:
        MY_ASSERT(false);
    }
    if (item) {
        elementIdToItemMap.insert(child->get_id(), item);
        addSceneItem(item);
    }
    return item;
}

void CyberiadaSMEditorScene::addItemsRecursively(QGraphicsItem* parent, Cyberiada::ElementCollection* collection)
{
    QGraphicsItem* new_parent = parent;

    if (collection->get_type() == Cyberiada::elementSM) {
        new_parent = new CyberiadaSMEditorSMItem(model, collection, parent);
        elementIdToItemMap.insert(collection->get_id(), new_parent);
        addSceneItem(new_parent);
        new_parent->setSelected(true);
    }

    if (collection->has_children()) {
        const Cyberiada::ElementList& children = collection->get_children();
        for (Cyberiada::ElementList::const_iterator i = children.begin(); i != children.end(); i++) {
            addElementItem(*i, new_parent);
        }
    }
}

// void CyberiadaSMEditorScene::setGridSize(int newSize)
// {
//     if (newSize > 0) {
// 		gridSize = newSize;
// 		update();
// 	}
// }

// void CyberiadaSMEditorScene::enableGrid(bool on)
// {
//     gridEnabled = on;
//     update();
// }

// void CyberiadaSMEditorScene::enableGridSnap(bool on)
// {
//     gridSnap = on;
// }

void CyberiadaSMEditorScene::setGridPen(const QPen &pen)
{
    gridPen = pen;
    update();
}

void CyberiadaSMEditorScene::loadScene(bool fit)
{
    elementIdToItemMap.clear();

    // clear() frees every item; release the grab and focus first so a pending
    // gesture is not delivered to a freed item
    if (QGraphicsItem* grabber = mouseGrabberItem()) grabber->ungrabMouse();
    if (QGraphicsItem* focus = focusItem()) focus->clearFocus();
    clear();

    MY_ASSERT(elementIdToItemMap.isEmpty());
    MY_ASSERT(items().isEmpty());

    // every state machine of the document is drawn, in the shared global
    // coordinate space (7.2.1); the first is the default active one
    std::vector<Cyberiada::StateMachine*> sms = model->rootDocument()->get_state_machines();
    currentSM = sms.empty() ? NULL : sms.front();
    for (std::vector<Cyberiada::StateMachine*>::iterator i = sms.begin(); i != sms.end(); i++) {
        addItemsRecursively(NULL, *i);
    }
    for (auto item : items()) {
        if (auto smItem = dynamic_cast<CyberiadaSMEditorSMItem*>(item)) {
            connect(smItem, &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorScene::slotSMSizeChanged);
        }
    }
    clearSelection();
    setSceneRect(diagramRect());
    if (fit && !views().isEmpty()) {
        views().first()->fitInView(sceneRect(), Qt::KeepAspectRatio);
    }
    update();
}

QRectF CyberiadaSMEditorScene::diagramRect() const
{
    qreal margin = DEFAULT_SCENE_BORDER_MARGIN;
    return visibleItemsBoundingRect().adjusted(-margin, -margin, margin, margin);
}

// itemsBoundingRect() ignores visibility - union the visible items only, so
// hidden elements (text in the no-text mode, dots) do not leak into the rect
QRectF CyberiadaSMEditorScene::visibleItemsBoundingRect() const
{
    QRectF bounds;
    for (QGraphicsItem* item : items()) {
        if (item->isVisible()) {
            bounds |= item->sceneBoundingRect();
        }
    }
    return bounds;
}

void CyberiadaSMEditorScene::setCurrentTool(ToolType tool) {
    currentTool = tool;
    transientTool = false;
    refreshToolDecorations();
}

void CyberiadaSMEditorScene::beginTransientTool(ToolType tool)
{
    currentTool = tool;
    transientTool = true;
    refreshToolDecorations();
    emit toolChanged(tool);
}

void CyberiadaSMEditorScene::refreshToolDecorations()
{
    // the transition source boxes show only under the transition tool: refresh
    // every item when the tool changes
    for (QGraphicsItem* item : items()) {
        if (CyberiadaSMEditorAbstractItem* ci = dynamic_cast<CyberiadaSMEditorAbstractItem*>(item)) {
            ci->refreshTransitionDots();
        }
    }
}

// the session log coordinates and the keyboard modifiers, in the batch script
// spelling (press/drag/release ... x y [ctrl|shift|alt])
static QString logPoint(const QPointF& p)
{
    return QString("%1 %2").arg(p.x()).arg(p.y());
}

static QString logMods(Qt::KeyboardModifiers mods)
{
    QString s;
    if (mods & Qt::ControlModifier) s += " ctrl";
    if (mods & Qt::ShiftModifier)   s += " shift";
    if (mods & Qt::AltModifier)     s += " alt";
    return s;
}

// the element creation tools: rect-drawing (SM, state) and click-placement
static bool isRectTool(ToolType t)
{
    return t == ToolType::NewSM || t == ToolType::NewState;
}

static bool isCreationTool(ToolType t)
{
    return isRectTool(t) ||
        t == ToolType::NewInitial || t == ToolType::NewFinal ||
        t == ToolType::NewChoice || t == ToolType::NewTerminate ||
        t == ToolType::NewComment || t == ToolType::NewFormalComment;
}

static Cyberiada::ElementType toolElementType(ToolType t)
{
    switch (t) {
    case ToolType::NewSM:           return Cyberiada::elementSM;
    case ToolType::NewState:        return Cyberiada::elementSimpleState;
    case ToolType::NewInitial:      return Cyberiada::elementInitial;
    case ToolType::NewFinal:        return Cyberiada::elementFinal;
    case ToolType::NewChoice:       return Cyberiada::elementChoice;
    case ToolType::NewTerminate:    return Cyberiada::elementTerminate;
    case ToolType::NewComment:      return Cyberiada::elementComment;
    case ToolType::NewFormalComment:return Cyberiada::elementFormalComment;
    default:                        return Cyberiada::elementRoot;
    }
}

// a mouse gesture is one undo step whatever it writes on the way
// a human-readable note of what a press lands on, recorded in the gesture log as
// a comment: the element under the cursor and which part - body, title, region or
// an action block. The part decides which item grabs the drag, so a replay that
// contradicts the GUI (e.g. a title grabbing a body drag) is visible in the log.
static QString pressTargetNote(QGraphicsScene* scene, const QPointF& scenePos)
{
    for (QGraphicsItem* gi : scene->items(scenePos)) {
        if (StateTitle* t = dynamic_cast<StateTitle*>(gi)) {
            CyberiadaSMEditorAbstractItem* st = dynamic_cast<CyberiadaSMEditorAbstractItem*>(t->parentItem());
            return (st ? QString::fromStdString(st->getElement()->get_id()) : QString("?")) + " title";
        }
        if (StateAction* a = dynamic_cast<StateAction*>(gi)) {
            CyberiadaSMEditorAbstractItem* st = dynamic_cast<CyberiadaSMEditorAbstractItem*>(a->parentItem());
            return (st ? QString::fromStdString(st->getElement()->get_id()) : QString("?")) + " action";
        }
        if (StateRegion* r = dynamic_cast<StateRegion*>(gi)) {
            CyberiadaSMEditorAbstractItem* st = dynamic_cast<CyberiadaSMEditorAbstractItem*>(r->parentItem());
            return (st ? QString::fromStdString(st->getElement()->get_id()) : QString("?")) + " region";
        }
        if (CyberiadaSMEditorAbstractItem* ci = dynamic_cast<CyberiadaSMEditorAbstractItem*>(gi)) {
            return QString::fromStdString(ci->getElement()->get_id()) + " body";
        }
    }
    return QString("empty");
}

void CyberiadaSMEditorScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    model->beginUndoStep(QString());
    if (event->button() == Qt::LeftButton) {
        // the model edits the gesture triggers are recorded as the gesture, not
        // twice as semantic verbs
        GestureLog::instance().enterGesture();
        // a comment (skipped on replay) recording what the press hit, for diagnosis
        GestureLog::instance().logGesture("# on " + pressTargetNote(this, event->scenePos()));
        GestureLog::instance().logGesture("press " + logPoint(event->scenePos()) +
                                          logMods(event->modifiers()));
        loggingPressed = true;
        loggingLastPoint = event->scenePos();
    }
    if (event->button() == Qt::LeftButton && isCreationTool(currentTool) &&
        handleCreationPress(event)) {
        event->accept();
        return;
    }
    QGraphicsScene::mousePressEvent(event);
}

void CyberiadaSMEditorScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (loggingPressed && (event->buttons() & Qt::LeftButton)) {
        QPointF p = event->scenePos();
        // decimate: a few pixels between the recorded points is enough to
        // reproduce the drag, the release carries the exact final point
        if ((p - loggingLastPoint).manhattanLength() >= 3) {
            GestureLog::instance().logGesture("drag " + logPoint(p));
            loggingLastPoint = p;
        }
    }
    if (transitionDrawLine) {
        transitionDrawLine->setLine(QLineF(transitionDrawLine->line().p1(), event->scenePos()));
        event->accept();
        return;
    }
    if (creating) {
        handleCreationMove(event);
        event->accept();
        return;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void CyberiadaSMEditorScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (transitionDrawLine && event->button() == Qt::LeftButton) {
        finishTransitionDraw(event->scenePos());   // creates the transition and reverts to Select
    } else if (creating && event->button() == Qt::LeftButton) {
        handleCreationRelease(event);   // creates the element and reverts to Select
    } else {
        QGraphicsScene::mouseReleaseEvent(event);
        if (transientTool) {
            setCurrentTool(ToolType::Select);
            emit toolChanged(ToolType::Select);
        }
    }
    if (event->button() == Qt::LeftButton && loggingPressed) {
        GestureLog::instance().logGesture("release " + logPoint(event->scenePos()));
        loggingPressed = false;
        GestureLog::instance().leaveGesture();
    }
    model->endUndoStep();
}

bool CyberiadaSMEditorScene::handleCreationPress(QGraphicsSceneMouseEvent* event)
{
    creating = true;
    creationStart = event->scenePos();
    if (isRectTool(currentTool)) {
        creationPreview = new QGraphicsRectItem(QRectF(creationStart, creationStart));
        QPen pen(Qt::DashLine);
        pen.setColor(Qt::gray);
        creationPreview->setPen(pen);
        creationPreview->setZValue(1e6);
        addItem(creationPreview);
    }
    return true;
}

void CyberiadaSMEditorScene::handleCreationMove(QGraphicsSceneMouseEvent* event)
{
    if (creationPreview) {
        creationPreview->setRect(QRectF(creationStart, event->scenePos()).normalized());
    }
}

void CyberiadaSMEditorScene::handleCreationRelease(QGraphicsSceneMouseEvent* event)
{
    ToolType tool = currentTool;
    QRectF rect(creationStart, event->scenePos());
    rect = rect.normalized();
    if (creationPreview) {
        removeItem(creationPreview);
        delete creationPreview;
        creationPreview = nullptr;
    }
    creating = false;
    createByTool(tool, rect);
    // a creation tool is one-shot: back to the selection tool
    setCurrentTool(ToolType::Select);
    emit toolChanged(ToolType::Select);
}

void CyberiadaSMEditorScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        GestureLog::instance().enterGesture();
        GestureLog::instance().logGesture("double-click " + logPoint(event->scenePos()) +
                                          logMods(event->modifiers()));
    }
    QGraphicsScene::mouseDoubleClickEvent(event);
    if (event->button() == Qt::LeftButton) {
        GestureLog::instance().leaveGesture();
    }
}

void CyberiadaSMEditorScene::addSMItem(Cyberiada::ElementType type)
{
    if (type == Cyberiada::elementSM) {
        // a state machine has no parent and is placed on its own in free space;
        // it must NOT fall through the child-container logic below, which would
        // create a second, border-less machine. The window (slotNewSM) already
        // adopts an existing border-less machine before reaching here, so this
        // path always means a genuinely new machine.
        try {
            // room for a 2x2 grid of default states (200x100) with a separator
            // and a border around them
            const qreal gap = 40;
            QSizeF smSize(2 * 200 + 3 * gap, 2 * 100 + 3 * gap);   // 520 x 320
            QPointF c = freeStateMachinePlace(smSize);
            Cyberiada::Element* element = model->newStateMachine("New State Machine",
                                                                 Cyberiada::Rect(c.x(), c.y(),
                                                                                 smSize.width(), smSize.height()));
            if (!element) return;
            currentSM = static_cast<Cyberiada::StateMachine*>(element);
            CyberiadaSMEditorSMItem* sm = new CyberiadaSMEditorSMItem(model, element, NULL);
            elementIdToItemMap.insert(element->get_id(), sm);
            addItem(sm);
            sm->setSelected(true);
        } catch (const Cyberiada::ParametersException& e) {
            showError(tr("Create new state machine"),
                                  tr("Parameters error:\n") + QString(e.str().c_str()));
        }
        return;
    }

    CyberiadaSMEditorAbstractItem* parentCItem = nullptr;
    Cyberiada::ElementCollection* parentColl = nullptr;
    if (selectedItems().size() > 0) {
        QGraphicsItem* item = selectedItems().first();
        if (item) {
            CyberiadaSMEditorAbstractItem* cItem = dynamic_cast<CyberiadaSMEditorAbstractItem*>(item);
            if (cItem) {
                if (cItem->type() == CyberiadaSMEditorAbstractItem::SMItem ||
                    cItem->type() == CyberiadaSMEditorAbstractItem::StateItem ||
                    cItem->type() == CyberiadaSMEditorAbstractItem::CompositeStateItem) {
                    parentCItem = cItem;
                    parentColl = static_cast<Cyberiada::ElementCollection*>(cItem->getElement());
                }
            }
        }
    }
    if (parentCItem == nullptr) {
        if (currentSM == nullptr) {
            Cyberiada::Element* element = model->newStateMachine("New State Machine");
            if (!element) return;
            currentSM = static_cast<Cyberiada::StateMachine*>(element);
            CyberiadaSMEditorSMItem* sm = new CyberiadaSMEditorSMItem(model, element, nullptr);
            parentCItem = sm;
            parentColl = static_cast<Cyberiada::ElementCollection*>(element);
            elementIdToItemMap.insert(element->get_id(), sm);
            addItem(sm);
        } else {
            for (auto item : items()) {
                if (auto smItem = dynamic_cast<CyberiadaSMEditorSMItem*>(item)) {
                    parentCItem = smItem;
                    parentColl = static_cast<Cyberiada::ElementCollection*>(smItem->getElement());
                    break;
                }
            }
        }
    }

    Cyberiada::LocalDocument* d = model->rootDocument();
    if (d == NULL) {
        qWarning() << "the document is not loaded";
        // TODO create doc
    }

    QPointF center;
    if (parentCItem) {
        center = freePlace(parentColl, QSizeF(200, 100));
    } else {
        center = sceneRect().center();
    }

    if (parentColl == NULL) return;

    Cyberiada::Element* element = NULL;
    try {
        switch(type) {
        case Cyberiada::elementCompositeState:
        case Cyberiada::elementSimpleState:
            element = model->newState(parentColl, model->uniqueStateName(parentColl, "New state"),
                                      Cyberiada::Action(),
                                      Cyberiada::Rect(center.x(), center.y(), 200, 100));
            break;
        case Cyberiada::elementInitial:
            element = model->newInitial(parentColl, Cyberiada::Point(center.x(), center.y()));
            break;
        case Cyberiada::elementFinal:
            element = model->newFinal(parentColl, Cyberiada::Point(center.x(), center.y()));
            break;
        case Cyberiada::elementTerminate:
            element = model->newTerminate(parentColl, Cyberiada::Point(center.x(), center.y()));
            break;
        case Cyberiada::elementChoice:
            element = model->newChoice(parentColl, Cyberiada::Rect(center.x(), center.y(),
                                                                   CHOICE_DEFAULT_SIZE, CHOICE_DEFAULT_SIZE));
            break;
        case Cyberiada::elementComment:
            element = model->newComment(parentColl, "New comment",
                                        Cyberiada::Rect(center.x(), center.y(), 200, 100));
            break;
        case Cyberiada::elementFormalComment:
            element = model->newFormalComment(parentColl, "New formal comment",
                                              Cyberiada::Rect(center.x(), center.y(), 200, 100));
            break;
        default:
            // the transitions are created by the transition tool
            return;
        }
    } catch (const Cyberiada::ParametersException& e) {
        showError(tr("Create new element"),
                              tr("Parameters error:\n") + QString(e.str().c_str()));
        return;
    }
    if (!element) return;

    // the item itself is built by the model row signals
    QGraphicsItem* item = elementIdToItemMap.value(element->get_id());
    if (item) {
        // a new object dropped in a bordered machine stays inside it: the
        // border extends to contain it (unlike a drag, which is clamped)
        CyberiadaSMEditorSMItem* smItem = dynamic_cast<CyberiadaSMEditorSMItem*>(parentCItem);
        if (smItem && smItem->getElement()->has_geometry()) {
            extendStateMachineForChild(smItem, item);
        } else if (parentCItem &&
                   dynamic_cast<Cyberiada::ElementCollection*>(parentCItem->getElement()) &&
                   parentCItem->getElement()->has_geometry()) {
            // a composite state parent: grow it (and its ancestors) about the
            // centre to contain the new child, keeping the existing children put
            model->growToFitChildren(element, false);
            // a parent with entry/exit action blocks needs extra height, since
            // its child region is inset by those blocks
            CyberiadaSMEditorStateItem* st = dynamic_cast<CyberiadaSMEditorStateItem*>(parentCItem);
            if (st && st->actionInset() > 0.5) {
                Cyberiada::Rect r =
                    static_cast<Cyberiada::ElementCollection*>(parentCItem->getElement())->get_geometry_rect();
                model->updateGeometry(parentCItem->getIndex(),
                                      Cyberiada::Rect(r.x, r.y, r.width, r.height + st->actionInset()));
                model->growToFitChildren(element, false);   // re-cascade the extra height up to the SM
            }
        }
        item->setSelected(true);
    }
}

void CyberiadaSMEditorScene::createByTool(ToolType tool, const QRectF& sceneRect)
{
    Cyberiada::ElementType et = toolElementType(tool);
    bool tiny = sceneRect.width() < 10 || sceneRect.height() < 10;

    if (tool == ToolType::NewSM) {
        QRectF r = tiny ? QRectF(creationStart.x() - 260, creationStart.y() - 160, 520, 320) : sceneRect;
        // adopt a border-less machine (objects but no drawn border) if one is
        // the only machine, extending it to cover the drawn rect and its content
        std::vector<Cyberiada::StateMachine*> sms;
        if (model->rootDocument()) sms = model->rootDocument()->get_state_machines();
        Cyberiada::StateMachine* borderless = nullptr;
        bool anyBordered = false;
        for (std::vector<Cyberiada::StateMachine*>::iterator i = sms.begin(); i != sms.end(); i++) {
            if ((*i)->has_geometry()) anyBordered = true;
            else if (!borderless) borderless = *i;
        }
        if (borderless && !anyBordered) {
            QRectF total = r;
            Cyberiada::Rect content = borderless->get_bound_rect(*model->rootDocument());
            if (content.valid) {
                total = total.united(QRectF(content.x - content.width / 2, content.y - content.height / 2,
                                            content.width, content.height));
            }
            model->updateGeometry(model->elementToIndex(borderless),
                                  Cyberiada::Rect(total.center().x(), total.center().y(),
                                                  total.width(), total.height()));
            return;
        }
        Cyberiada::Element* element = model->newStateMachine("New State Machine",
            Cyberiada::Rect(r.center().x(), r.center().y(), r.width(), r.height()));
        if (!element) return;
        currentSM = static_cast<Cyberiada::StateMachine*>(element);
        CyberiadaSMEditorSMItem* smi = new CyberiadaSMEditorSMItem(model, element, NULL);
        elementIdToItemMap.insert(element->get_id(), smi);
        addItem(smi);
        smi->setSelected(true);
        return;
    }

    // a child element: find the collection under the start point
    CyberiadaSMEditorAbstractItem* parentCItem = nullptr;
    Cyberiada::ElementCollection* parentColl = nullptr;
    for (QGraphicsItem* gi : items(creationStart)) {
        CyberiadaSMEditorAbstractItem* ci = dynamic_cast<CyberiadaSMEditorAbstractItem*>(gi);
        if (ci && (ci->type() == CyberiadaSMEditorAbstractItem::SMItem ||
                   ci->type() == CyberiadaSMEditorAbstractItem::StateItem ||
                   ci->type() == CyberiadaSMEditorAbstractItem::CompositeStateItem)) {
            parentCItem = ci;
            parentColl = static_cast<Cyberiada::ElementCollection*>(ci->getElement());
            break;
        }
    }
    if (!parentColl) {
        if (!currentSM) { addSMItem(et); return; }   // empty canvas: auto-place
        parentColl = static_cast<Cyberiada::ElementCollection*>(currentSM);
        parentCItem = dynamic_cast<CyberiadaSMEditorAbstractItem*>(elementIdToItemMap.value(currentSM->get_id()));
    }

    QGraphicsItem* gp = graphicsParentFor(parentColl);
    QPointF c = gp ? gp->mapFromScene(sceneRect.center()) : sceneRect.center();
    double w = tiny ? 200 : sceneRect.width();
    double h = tiny ? 100 : sceneRect.height();

    Cyberiada::Element* element = NULL;
    try {
        switch (et) {
        case Cyberiada::elementSimpleState:
            element = model->newState(parentColl, model->uniqueStateName(parentColl, "New state"),
                                      Cyberiada::Action(), Cyberiada::Rect(c.x(), c.y(), w, h));
            break;
        case Cyberiada::elementChoice:
            element = model->newChoice(parentColl, Cyberiada::Rect(c.x(), c.y(),
                                                                   CHOICE_DEFAULT_SIZE, CHOICE_DEFAULT_SIZE));
            break;
        case Cyberiada::elementComment:
            element = model->newComment(parentColl, "New comment", Cyberiada::Rect(c.x(), c.y(), 200, 100));
            break;
        case Cyberiada::elementFormalComment:
            element = model->newFormalComment(parentColl, "New formal comment", Cyberiada::Rect(c.x(), c.y(), 200, 100));
            break;
        case Cyberiada::elementInitial:
            element = model->newInitial(parentColl, Cyberiada::Point(c.x(), c.y()));
            break;
        case Cyberiada::elementFinal:
            element = model->newFinal(parentColl, Cyberiada::Point(c.x(), c.y()));
            break;
        case Cyberiada::elementTerminate:
            element = model->newTerminate(parentColl, Cyberiada::Point(c.x(), c.y()));
            break;
        default:
            return;
        }
    } catch (const Cyberiada::ParametersException& e) {
        showError(tr("Create new element"),
                              tr("Parameters error:\n") + QString(e.str().c_str()));
        return;
    }
    if (!element) return;

    QGraphicsItem* item = elementIdToItemMap.value(element->get_id());
    if (item) {
        CyberiadaSMEditorSMItem* smItem = dynamic_cast<CyberiadaSMEditorSMItem*>(parentCItem);
        if (smItem && smItem->getElement()->has_geometry()) {
            extendStateMachineForChild(smItem, item);
        } else if (parentCItem &&
                   dynamic_cast<Cyberiada::ElementCollection*>(parentCItem->getElement()) &&
                   parentCItem->getElement()->has_geometry()) {
            model->growToFitChildren(element, false);
        }
        item->setSelected(true);
    }
}

void CyberiadaSMEditorScene::extendStateMachineForChild(CyberiadaSMEditorAbstractItem* smItem,
                                                        QGraphicsItem* child)
{
    Cyberiada::ElementCollection* sm =
        static_cast<Cyberiada::ElementCollection*>(smItem->getElement());
    Cyberiada::Rect border = sm->get_geometry_rect();
    // the child position is relative to the machine centre; it fits when the
    // border half-size covers the child offset plus its half-size
    QRectF cb = child->boundingRect();
    QPointF cp = child->pos();
    const double pad = 10.0;
    double needW = 2.0 * (std::fabs(cp.x()) + cb.width() / 2.0 + pad);
    double needH = 2.0 * (std::fabs(cp.y()) + cb.height() / 2.0 + pad);
    double newW = std::max((double)border.width, needW);
    double newH = std::max((double)border.height, needH);
    if (newW > border.width + 0.5 || newH > border.height + 0.5) {
        // the centre is kept, so the existing children do not move
        model->updateGeometry(smItem->getIndex(), Cyberiada::Rect(border.x, border.y, newW, newH));
    }
}

// the first slot of the parent (the origin, then to the right, then the
// next row) that overlaps no sibling: new elements do not pile up
QPointF CyberiadaSMEditorScene::freeStateMachinePlace(const QSizeF& size)
{
    QList<QRectF> taken;
    for (QGraphicsItem* item : items()) {
        if (item->isVisible() && dynamic_cast<CyberiadaSMEditorSMItem*>(item)) {
            taken.append(item->sceneBoundingRect());
        }
    }
    const qreal gap = 40;
    for (int row = 0; row < 20; row++) {
        for (int col = 0; col < 20; col++) {
            QPointF centre(col * (size.width() + gap), row * (size.height() + gap));
            QRectF slot(centre.x() - size.width() / 2, centre.y() - size.height() / 2,
                        size.width(), size.height());
            bool free = true;
            for (const QRectF& r : taken) {
                if (r.intersects(slot)) { free = false; break; }
            }
            if (free) return centre;
        }
    }
    return QPointF(0, 0);
}

QPointF CyberiadaSMEditorScene::freePlace(const Cyberiada::ElementCollection* parent, const QSizeF& size)
{
    QGraphicsItem* graphicsParent = graphicsParentFor(parent);
    if (!graphicsParent) return QPointF(0, 0);
    QList<QRectF> taken;
    if (parent->has_children()) {
        const Cyberiada::ConstElementList& children = parent->get_children();
        for (Cyberiada::ConstElementList::const_iterator i = children.begin(); i != children.end(); i++) {
            QGraphicsItem* item = elementIdToItemMap.value((*i)->get_id());
            if (item && (*i)->get_type() != Cyberiada::elementTransition) {
                taken.append(item->sceneBoundingRect());
            }
        }
    }
    const qreal gap = 20;
    for (int row = 0; row < 10; row++) {
        for (int col = 0; col < 10; col++) {
            QPointF centre(col * (size.width() + gap), row * (size.height() + gap));
            QRectF slot = graphicsParent->mapRectToScene(
                QRectF(centre.x() - size.width() / 2, centre.y() - size.height() / 2,
                       size.width(), size.height()));
            bool free = true;
            for (const QRectF& rect : taken) {
                if (rect.intersects(slot)) { free = false; break; }
            }
            if (free) return centre;
        }
    }
    return QPointF(0, 0);
}

CyberiadaSMEditorTransitionItem* CyberiadaSMEditorScene::addTransition(CyberiadaSMEditorAbstractItem *source,
                                           CyberiadaSMEditorAbstractItem *target)
{
    try {
        Cyberiada::Element* element = model->newTransition(currentSM, Cyberiada::transitionExternal,
                                                        source->getElement(), target->getElement(),
                                                        Cyberiada::Action(Cyberiada::actionTransition));
        if (!element) return NULL;
        CyberiadaSMEditorTransitionItem* transition = static_cast<CyberiadaSMEditorTransitionItem*>(
            elementIdToItemMap.value(element->get_id()));
        if (transition) {
            transition->setSelected(true);
        }
        return transition;
    } catch (const Cyberiada::ParametersException& e){
        showError(tr("Create new transition"),
                              tr("Parameters error:\n") + QString(e.str().c_str()));
        // error = true;
        return nullptr;
    }
}

void CyberiadaSMEditorScene::beginTransitionDraw(CyberiadaSMEditorAbstractItem* source)
{
    if (!source) return;
    beginTransientTool(ToolType::Transition);   // one-shot: back to select on release
    transitionSource = source;
    QPointF c = source->sceneBoundingRect().center();
    transitionDrawLine = new QGraphicsLineItem(QLineF(c, c));
    QPen pen(Qt::black, 1, Qt::DashLine);
    transitionDrawLine->setPen(pen);
    transitionDrawLine->setZValue(1e6);
    addItem(transitionDrawLine);
}

void CyberiadaSMEditorScene::finishTransitionDraw(const QPointF& scenePos)
{
    CyberiadaSMEditorAbstractItem* src = transitionSource;
    if (transitionDrawLine) {
        removeItem(transitionDrawLine);
        delete transitionDrawLine;
        transitionDrawLine = nullptr;
    }
    transitionSource = nullptr;

    // a valid target under the release point
    CyberiadaSMEditorAbstractItem* target = nullptr;
    for (QGraphicsItem* gi : items(scenePos)) {
        CyberiadaSMEditorAbstractItem* ci = dynamic_cast<CyberiadaSMEditorAbstractItem*>(gi);
        if (ci && (ci->type() == CyberiadaSMEditorAbstractItem::StateItem ||
                   ci->type() == CyberiadaSMEditorAbstractItem::CompositeStateItem ||
                   ci->type() == CyberiadaSMEditorAbstractItem::VertexItem ||
                   ci->type() == CyberiadaSMEditorAbstractItem::ChoiceItem)) {
            target = ci;
            break;
        }
    }
    if (src && target && currentSM) {
        try {
            // an invalid pair (e.g. into an initial) is silently dropped, no dialog
            Cyberiada::Element* element = model->newTransition(
                currentSM, Cyberiada::transitionExternal, src->getElement(), target->getElement(),
                Cyberiada::Action(Cyberiada::actionTransition));
            if (element) {
                QGraphicsItem* item = elementIdToItemMap.value(element->get_id());
                if (item) item->setSelected(true);
            }
        } catch (const Cyberiada::Exception&) {
            // not a valid source/target pair
        }
    }
    // the transition tool is one-shot
    setCurrentTool(ToolType::Select);
    emit toolChanged(ToolType::Select);
}

void CyberiadaSMEditorScene::drawBackground(QPainter* painter, const QRectF& exposed)
{
    SettingsManager& sm = SettingsManager::instance();

    // no frame: the diagram has no cosmetic boundary; a real boundary is the
    // optional state machine border element instead

    if (sm.getShowServiceObjects()) {
        painter->setBrush(Qt::green);
        painter->drawEllipse(QPointF(0, 0), 5, 5); // the center of the coordinate system
    }
    painter->setBrush(Qt::NoBrush);

    if (sm.getGridSpacing() <= 0 || !sm.getShowGrid()) {
		return ;
	}

	painter->setPen(gridPen);

	// the grid fills the visible area, not a bounded scene rectangle
	QRectF rect = exposed;

    int gridSize = sm.getGridSpacing();

	double left = std::floor(rect.left() / gridSize) * gridSize;
	double top = std::floor(rect.top() / gridSize) * gridSize;

	QVarLengthArray<QLineF, 256> lines;

	for (double x = left; x < rect.right(); x += gridSize)
		lines.append(QLineF(x, rect.top(), x, rect.bottom()));
	for (double y = top; y < rect.bottom(); y += gridSize)
		lines.append(QLineF(rect.left(), y, rect.right(), y));

	painter->drawLines(lines.data(), lines.size());
}

