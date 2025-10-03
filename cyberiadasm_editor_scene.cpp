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

	setBackgroundBrush(Qt::white);
    connect(this, &QGraphicsScene::selectionChanged, this, &CyberiadaSMEditorScene::slotSelectionChanged);
    connect(model, &CyberiadaSMModel::dataChanged, this, &CyberiadaSMEditorScene::slotModelDataChanged);
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
        CyberiadaSMEditorWindow* p = dynamic_cast<CyberiadaSMEditorWindow*>(parent());
        //p->SMView->setCurrentIndex(index);
		p->SMView->select(index);
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

void CyberiadaSMEditorScene::deleteItemsRecursively(Cyberiada::Element *element)
{
    Cyberiada::ElementType type = element->get_type();

    if(type == Cyberiada::elementCompositeState || type == Cyberiada::elementSM || type == Cyberiada::elementRoot) {
        while(element->has_children()){
            Cyberiada::ElementCollection* collection = static_cast<Cyberiada::ElementCollection*>(element);
            const Cyberiada::ElementList& children = collection->get_children();
            Cyberiada::Element* child = children.at(0);
            deleteItemsRecursively(child);
        }
    }

    // remove transitions
    QList<Cyberiada::ID> toRemove;
    for (auto it = elementIdToItemMap.begin(); it != elementIdToItemMap.end(); ++it) {
        QGraphicsItem* item = it.value();

        auto* transition = dynamic_cast<CyberiadaSMEditorTransitionItem*>(item);
        if (transition) {
            if(transition->sourceId() == element->get_id() || transition->targetId() == element->get_id()) {
                toRemove.append(it.key());
                QModelIndex i = transition->getIndex();
                delete transition;
                model->deleteElement(i);
            }
        }
    }
    for (const Cyberiada::ID& id : toRemove) {
        elementIdToItemMap.remove(id);
    }

    // remove element
    CyberiadaSMEditorAbstractItem* citem = dynamic_cast<CyberiadaSMEditorAbstractItem*>(elementIdToItemMap.value(element->get_id()));
    Cyberiada::ElementCollection* parent_element = dynamic_cast<Cyberiada::ElementCollection*>(element->get_parent());
    MY_ASSERT(parent_element);
    elementIdToItemMap.remove(element->get_id());
    QModelIndex i = citem->getIndex();
    delete citem;
    model->deleteElement(i);
}

void CyberiadaSMEditorScene::slotModelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Cyberiada::Element* element = model->indexToElement(topLeft);
    // поиск CyberiadaSMEditorAbstractItem по QMap QMap<Cyberiada::ID, QGraphicsItem*> elementIdToItemMap;
    // updateItemsRecursively(nullptr, static_cast<Cyberiada::ElementCollection*>(element));
    CyberiadaSMEditorAbstractItem* current_item = dynamic_cast<CyberiadaSMEditorAbstractItem*>(elementIdToItemMap.value(element->get_id()));
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

void CyberiadaSMEditorScene::addItemsRecursively(QGraphicsItem* parent, Cyberiada::ElementCollection* collection)
{
	Cyberiada::ElementType parent_type = collection->get_type();
    QGraphicsItem* new_parent = parent;
    qDebug() << "parent=null" <<  (new_parent == NULL);

    if (parent_type == Cyberiada::elementSM) {
        new_parent = new CyberiadaSMEditorSMItem(model, collection, parent);
        elementIdToItemMap.insert(collection->get_id(), new_parent);
        addItem(new_parent);
        new_parent->setSelected(true);
    }

    qDebug() << "PARENT: " << collection->get_id().c_str();
    if (collection->has_children()) {
		const Cyberiada::ElementList& children = collection->get_children();
		for (Cyberiada::ElementList::const_iterator i = children.begin(); i != children.end(); i++) {
			Cyberiada::Element* child = *i;
            Cyberiada::ElementType type = child->get_type();

			switch(type) {
            case Cyberiada::elementCompositeState: {
                CyberiadaSMEditorStateItem* state = new CyberiadaSMEditorStateItem(this, model, child, new_parent);
                elementIdToItemMap.insert(child->get_id(), state);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(new_parent).c_str() << model->elementToIndex(child);
                addItemsRecursively(state->getRegion(), static_cast<Cyberiada::ElementCollection*>(child));
                addItem(state);
                break;
            }
            case Cyberiada::elementSimpleState: {
                CyberiadaSMEditorStateItem* state = new CyberiadaSMEditorStateItem(this, model, child, new_parent);
                elementIdToItemMap.insert(child->get_id(), state);
                addItem(state);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(new_parent).c_str() << model->elementToIndex(child);
				break;
            }
            case Cyberiada::elementInitial: {
                CyberiadaSMEditorVertexItem* initial = new CyberiadaSMEditorVertexItem(model, child, new_parent);
                elementIdToItemMap.insert(child->get_id(), initial);
                addItem(initial);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(new_parent).c_str() << model->elementToIndex(child);
                break;
            }
            case Cyberiada::elementFinal: {
                CyberiadaSMEditorVertexItem* final = new CyberiadaSMEditorVertexItem(model, child, new_parent);
                elementIdToItemMap.insert(child->get_id(), final);
                addItem(final);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(new_parent).c_str() << model->elementToIndex(child);
                break;
            }
            case Cyberiada::elementTerminate: {
                CyberiadaSMEditorVertexItem* terminate = new CyberiadaSMEditorVertexItem(model, child, new_parent);
                elementIdToItemMap.insert(child->get_id(), terminate);
                addItem(terminate);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(new_parent).c_str() << model->elementToIndex(child);
                break;
            }
			case Cyberiada::elementChoice:
                // new CyberiadaSMEditorChoiceItem(model, child, new_parent);
				break;
            case Cyberiada::elementComment: {
                if (!child->has_geometry()) break;
                CyberiadaSMEditorCommentItem* comment = new CyberiadaSMEditorCommentItem(this, model, child, new_parent, elementIdToItemMap);
                elementIdToItemMap.insert(child->get_id(), comment);
                addItem(comment);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(new_parent).c_str() << model->elementToIndex(child);
                break;
            }
            case Cyberiada::elementFormalComment: {
                if (!child->has_geometry()) break;
                CyberiadaSMEditorCommentItem* formalComment = new CyberiadaSMEditorCommentItem(this, model, child, new_parent, elementIdToItemMap);
                elementIdToItemMap.insert(child->get_id(), formalComment);
                addItem(formalComment);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(new_parent).c_str() << model->elementToIndex(child);
                break;
            }
            case Cyberiada::elementTransition: {
                CyberiadaSMEditorTransitionItem* transition = new CyberiadaSMEditorTransitionItem(this, model, child, NULL, elementIdToItemMap);
                elementIdToItemMap.insert(child->get_id(), transition);
                addItem(transition);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(new_parent).c_str() << model->elementToIndex(child);
                break;
            }
			default:
				MY_ASSERT(false);
			}
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
    setSceneRect(itemsBoundingRect().adjusted(-margin, -margin, margin, margin));
    qDebug() << "new scene rect" << sceneRect();
    // views().first()->fitInView(itemsBoundingRect(), Qt::KeepAspectRatio);
    views().first()->fitInView(sceneRect(), Qt::KeepAspectRatio);
    update();
}

void CyberiadaSMEditorScene::setCurrentTool(ToolType tool) {
    currentTool = tool;
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
            currentSM = static_cast<Cyberiada::StateMachine*>(element);
            CyberiadaSMEditorSMItem* sm = new CyberiadaSMEditorSMItem(model, element, nullptr);
            parentCItem = sm;
            parentColl = static_cast<Cyberiada::ElementCollection*>(element);
            elementIdToItemMap.insert(element->get_id(), sm);
            addItem(sm);
            qDebug() << "add item" << element->get_id().c_str() << "type" << type;
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
        qDebug() << "LocalDocument NULL";
        // TODO create doc
    }

    QPointF center;
    if (parentCItem) {
        center = QPointF(0, 0);
    } else {
        center = sceneRect().center();
    }

    switch(type) {
    case Cyberiada::elementSM: {
        try {
            Cyberiada::Element* element = model->newStateMachine("New State Machine", Cyberiada::Rect(sceneRect().center().x(), sceneRect().center().y(), 200, 100));
            currentSM = static_cast<Cyberiada::StateMachine*>(element);
            CyberiadaSMEditorSMItem* sm = new CyberiadaSMEditorSMItem(model, element, nullptr);
            elementIdToItemMap.insert(element->get_id(), sm);
            addItem(sm);
            qDebug() << "add item" << element->get_id().c_str() << "type" << type;
            sm->setSelected(true);
            break;
        } catch (const Cyberiada::ParametersException& e){
            QMessageBox::critical(NULL, tr("Create new state"),
                                  tr("Parameters error:\n") + QString(e.str().c_str()));
            // error = true;
        }
        break;
    }
    case Cyberiada::elementCompositeState:
    case Cyberiada::elementSimpleState: {
        try {
            Cyberiada::Element* element = model->newState(parentColl, "New state", Cyberiada::Action(),
                                                          Cyberiada::Rect(center.x(), center.y(), 200, 100));
            CyberiadaSMEditorStateItem* state = new CyberiadaSMEditorStateItem(this, model, element, parentCItem);
            elementIdToItemMap.insert(element->get_id(), state);
            addItem(state);
            qDebug() << "add item" << element->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(parentCItem).c_str();
            state->setSelected(true);
            break;
        } catch (const Cyberiada::ParametersException& e){
            QMessageBox::critical(NULL, tr("Create new state"),
                                  tr("Parameters error:\n") + QString(e.str().c_str()));
            // error = true;
        }
        break;

        // if (error) {

        // }
    }
    case Cyberiada::elementInitial: {
        try {
            Cyberiada::Element* element = model->newInitial(parentColl, Cyberiada::Point(center.x(), center.y()));
            CyberiadaSMEditorVertexItem* initial = new CyberiadaSMEditorVertexItem(model, element, parentCItem);
            elementIdToItemMap.insert(element->get_id(), initial);
            addItem(initial);
            qDebug() << "add item" << element->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(parentCItem).c_str();
            initial->setSelected(true);
            break;
        } catch (const Cyberiada::ParametersException& e){
            QMessageBox::critical(NULL, tr("Create new initial"),
                                  tr("Parameters error:\n") + QString(e.str().c_str()));
            // error = true;
        }
        break;
    }
    case Cyberiada::elementFinal: {
        try {
            Cyberiada::Element* element = model->newFinal(parentColl, Cyberiada::Point(center.x(), center.y()));
            CyberiadaSMEditorVertexItem* final = new CyberiadaSMEditorVertexItem(model, element, parentCItem);
            elementIdToItemMap.insert(element->get_id(), final);
            addItem(final);
            qDebug() << "add item" << element->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(parentCItem).c_str();
            final->setSelected(true);
            break;
        } catch (const Cyberiada::ParametersException& e){
            QMessageBox::critical(NULL, tr("Create new final"),
                                  tr("Parameters error:\n") + QString(e.str().c_str()));
            // error = true;
        }
        break;
    }
    case Cyberiada::elementTerminate: {
        try {
            Cyberiada::Element* element = model->newTerminate(parentColl, Cyberiada::Point(center.x(), center.y()));
            CyberiadaSMEditorVertexItem* terminate = new CyberiadaSMEditorVertexItem(model, element, parentCItem);
            elementIdToItemMap.insert(element->get_id(), terminate);
            addItem(terminate);
            qDebug() << "add item" << element->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(parentCItem).c_str();
            terminate->setSelected(true);
            break;
        } catch (const Cyberiada::ParametersException& e){
            QMessageBox::critical(NULL, tr("Create new terminate"),
                                  tr("Parameters error:\n") + QString(e.str().c_str()));
            // error = true;
        }
        break;
    }
    case Cyberiada::elementChoice:
        // new CyberiadaSMEditorChoiceItem(model, element, parentCItem);
        break;
    case Cyberiada::elementComment: {
        try {
            Cyberiada::Element* element = model->newComment(parentColl, "New comment", Cyberiada::Rect(center.x(), center.y(), 200, 100));
            CyberiadaSMEditorCommentItem* comment = new CyberiadaSMEditorCommentItem(this, model, element, parentCItem, elementIdToItemMap);
            elementIdToItemMap.insert(element->get_id(), comment);
            addItem(comment);
            qDebug() << "add item" << element->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(parentCItem).c_str();
            comment->setSelected(true);
            break;
        } catch (const Cyberiada::ParametersException& e){
            QMessageBox::critical(NULL, tr("Create new comment"),
                                  tr("Parameters error:\n") + QString(e.str().c_str()));
            // error = true;
        }
        break;
    }
    case Cyberiada::elementFormalComment: {
        try {
            Cyberiada::Element* element = model->newFormalComment(parentColl, "New formal comment", Cyberiada::Rect(center.x(), center.y(), 200, 100));
            CyberiadaSMEditorCommentItem* formalComment = new CyberiadaSMEditorCommentItem(this, model, element, parentCItem, elementIdToItemMap);
            elementIdToItemMap.insert(element->get_id(), formalComment);
            addItem(formalComment);
            qDebug() << "add item" << element->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(parentCItem).c_str();
            formalComment->setSelected(true);
            break;
        } catch (const Cyberiada::ParametersException& e){
            QMessageBox::critical(NULL, tr("Create new formal comment"),
                                  tr("Parameters error:\n") + QString(e.str().c_str()));
            // error = true;
        }
        break;
    }
    case Cyberiada::elementTransition: {
        try {
            // Cyberiada::Element* element = d->new_transition(currentSM, "New state", Cyberiada::Point(center.x(), center.y()));
            // CyberiadaSMEditorTransitionItem* transition = new CyberiadaSMEditorTransitionItem(this, model, element, settings, NULL, elementIdToItemMap);
            // elementIdToItemMap.insert(element->get_id(), transition);
            // addItem(transition);
            // qDebug() << "add item" << element->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(parentCItem).c_str();
            break;
        } catch (const Cyberiada::ParametersException& e){
            QMessageBox::critical(NULL, tr("Create new transition"),
                                  tr("Parameters error:\n") + QString(e.str().c_str()));
            // error = true;
        }
        break;

    }
    default:
        MY_ASSERT(false);
    }
}

void CyberiadaSMEditorScene::addTransitionFromTempopary(TemporaryTransition* ttrans, bool valid)
{
    if (!valid) {
        delete ttrans;
        return;
    }

    try {
        Cyberiada::Element* element = model->newTransition(currentSM, Cyberiada::transitionExternal,
                                                        ttrans->getSourceElement(), ttrans->getTargetElement(),
                                                        Cyberiada::Action(Cyberiada::actionTransition), Cyberiada::Polyline(),
                                                        ttrans->getSourcePoint(), ttrans->getTargetPoint());
        delete ttrans;
        CyberiadaSMEditorTransitionItem* transition =
            new CyberiadaSMEditorTransitionItem(this, model, element, NULL, elementIdToItemMap);
        elementIdToItemMap.insert(element->get_id(), transition);
        addItem(transition);
        transition->setSelected(true);
    } catch (const Cyberiada::ParametersException& e){
        delete ttrans;
        QMessageBox::critical(NULL, tr("Create new transition"),
                              tr("Parameters error:\n") + QString(e.str().c_str()));
        // error = true;
    }
}

TemporaryTransition *CyberiadaSMEditorScene::addTemporaryTransition(CyberiadaSMEditorAbstractItem* source, QPointF targetPoint)
{
    TemporaryTransition* ttrans = new TemporaryTransition(this, model, NULL, elementIdToItemMap, source, targetPoint);
    addItem(ttrans);
    connect(ttrans, &TemporaryTransition::signalReady, this, &CyberiadaSMEditorScene::addTransitionFromTempopary);
    return ttrans;
}

void CyberiadaSMEditorScene::drawBackground(QPainter* painter, const QRectF &)
{
    SettingsManager& sm = SettingsManager::instance();

	painter->setPen(QPen(Qt::darkGray, 2, Qt::SolidLine));
	painter->setBrush(backgroundBrush());
	painter->drawRect(sceneRect());

    if (sm.getInspectorMode()) {
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

