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
#include <QCursor>
#include <QMessageBox>

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
#include "myassert.h"

static double DEFAULT_SCENE_X = -500;
static double DEFAULT_SCENE_Y = -500;
static double DEFAULT_SCENE_WIDTH = 1000;
static double DEFAULT_SCENE_HEIGHT = 1000;
static double DEFAULT_SCENE_DELTA = 0.2;
static double DEFAULT_SCENE_BORDER_MARGIN = 50;

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
    reset();
}

CyberiadaSMEditorScene::~CyberiadaSMEditorScene()
{
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
        delete item;
    }
}

void CyberiadaSMEditorScene::slotRowsInserted(const QModelIndex& parent, int first, int last)
{
    // only the displayed state machine has items: a parent without an item
    // (the document, another state machine) gets nothing
    QGraphicsItem* parent_item = graphicsParentFor(model->indexToElement(parent));
    if (!parent_item) return;
    for (int row = first; row <= last; row++) {
        Cyberiada::Element* element = model->indexToElement(model->index(row, 0, parent));
        if (element) {
            addElementItem(element, parent_item);
        }
    }
    update();
}

void CyberiadaSMEditorScene::slotRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last)
{
    // the indexes are still valid here, before the model frees the elements
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
        // the children of a composite state live in its region
        return static_cast<CyberiadaSMEditorStateItem*>(item)->getRegion();
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

void CyberiadaSMEditorScene::loadScene()
{
    elementIdToItemMap.clear();

    clear();

    MY_ASSERT(elementIdToItemMap.isEmpty());
    MY_ASSERT(items().isEmpty());

    Cyberiada::StateMachine* sm = static_cast<Cyberiada::StateMachine*>(model->indexToElement(model->firstSMIndex()));
    currentSM = sm;
    addItemsRecursively(NULL, sm);
    for (auto item : items()) {
        if (auto smItem = dynamic_cast<CyberiadaSMEditorSMItem*>(item)) {
            connect(smItem, &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorScene::slotSMSizeChanged);
            break;
        }
    }
    // qreal margin = std::max(itemsBoundingRect().width(), itemsBoundingRect().height()) * DEFAULT_SCENE_BORDER_MARGIN_PERCENT;
    qreal margin = DEFAULT_SCENE_BORDER_MARGIN;
    // itemsBoundingRect() ignores visibility - union the visible items only,
    // so hidden elements (text in the no-text mode, dots) do not leak into
    // the scene rect
    QRectF bounds;
    for (QGraphicsItem* item : items()) {
        if (item->isVisible()) {
            bounds |= item->sceneBoundingRect();
        }
    }
    setSceneRect(bounds.adjusted(-margin, -margin, margin, margin));
    if (!views().isEmpty()) {
        views().first()->fitInView(sceneRect(), Qt::KeepAspectRatio);
    }
    update();
}

void CyberiadaSMEditorScene::setCurrentTool(ToolType tool) {
    currentTool = tool;
    transientTool = false;
}

void CyberiadaSMEditorScene::beginTransientTool(ToolType tool)
{
    currentTool = tool;
    transientTool = true;
    emit toolChanged(tool);
}

void CyberiadaSMEditorScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsScene::mouseReleaseEvent(event);
    if (transientTool) {
        setCurrentTool(ToolType::Select);
        emit toolChanged(ToolType::Select);
    }
}

void CyberiadaSMEditorScene::addSMItem(Cyberiada::ElementType type)
{
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

    if (type == Cyberiada::elementSM) {
        // the document has no item, so the new state machine item is built here
        try {
            Cyberiada::Element* element = model->newStateMachine("New State Machine",
                                                                 Cyberiada::Rect(sceneRect().center().x(),
                                                                                 sceneRect().center().y(), 200, 100));
            if (!element) return;
            currentSM = static_cast<Cyberiada::StateMachine*>(element);
            CyberiadaSMEditorSMItem* sm = new CyberiadaSMEditorSMItem(model, element, NULL);
            elementIdToItemMap.insert(element->get_id(), sm);
            addItem(sm);
            sm->setSelected(true);
        } catch (const Cyberiada::ParametersException& e) {
            QMessageBox::critical(NULL, tr("Create new state machine"),
                                  tr("Parameters error:\n") + QString(e.str().c_str()));
        }
        return;
    }

    if (parentColl == NULL) return;

    Cyberiada::Element* element = NULL;
    try {
        switch(type) {
        case Cyberiada::elementCompositeState:
        case Cyberiada::elementSimpleState:
            element = model->newState(parentColl, "New state", Cyberiada::Action(),
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
        QMessageBox::critical(NULL, tr("Create new element"),
                              tr("Parameters error:\n") + QString(e.str().c_str()));
        return;
    }
    if (!element) return;

    // the item itself is built by the model row signals
    QGraphicsItem* item = elementIdToItemMap.value(element->get_id());
    if (item) {
        item->setSelected(true);
    }
}

// the first slot of the parent (the origin, then to the right, then the
// next row) that overlaps no sibling: new elements do not pile up
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
        QMessageBox::critical(NULL, tr("Create new transition"),
                              tr("Parameters error:\n") + QString(e.str().c_str()));
        // error = true;
        return nullptr;
    }
}

void CyberiadaSMEditorScene::drawBackground(QPainter* painter, const QRectF &)
{
    SettingsManager& sm = SettingsManager::instance();

	painter->setPen(QPen(Qt::darkGray, 2, Qt::SolidLine));
	painter->setBrush(backgroundBrush());
	painter->drawRect(sceneRect());

    if (sm.getShowServiceObjects()) {
        painter->setBrush(Qt::green);
        painter->drawEllipse(QPointF(0, 0), 5, 5); // the center of the coordinate system
    }
    painter->setBrush(Qt::NoBrush);

    if (sm.getGridSpacing() <= 0 || !sm.getShowGrid()) {
		return ;
	}

	painter->setPen(gridPen);

	QRectF rect = sceneRect();

    int gridSize = sm.getGridSpacing();

	double left = int(rect.left()) - (int(rect.left()) % gridSize);
	double top = int(rect.top()) - (int(rect.top()) % gridSize);

	QVarLengthArray<QLineF, 100> lines;

	for (double x = left; x < rect.right(); x += gridSize)
		lines.append(QLineF(x, rect.top(), x, rect.bottom()));
	for (double y = top; y < rect.bottom(); y += gridSize)
		lines.append(QLineF(rect.left(), y, rect.right(), y));

	painter->drawLines(lines.data(), lines.size());
}

