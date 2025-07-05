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

#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_editor_items.h"
#include "cyberiadasm_editor_sm_item.h"
#include "cyberiadasm_editor_state_item.h"
#include "cyberiadasm_editor_vertex_item.h"
#include "cyberiadasm_editor_transition_item.h"
#include "cyberiadasm_editor_comment_item.h"
#include "smeditor_window.h"
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
    gridSize = 25;
    gridEnabled = true;
    gridSnap = true;
	gridPen = QPen(Qt::gray, 0, Qt::DotLine);

	setBackgroundBrush(Qt::white);
    connect(this, &QGraphicsScene::selectionChanged, this, &CyberiadaSMEditorScene::onSelectionChanged);
    connect(model, &CyberiadaSMModel::dataChanged, this, &CyberiadaSMEditorScene::onModelDataChanged);
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

void CyberiadaSMEditorScene::onSelectionChanged() {
	if (selectedItems().size() > 0) {
        QGraphicsItem* item = selectedItems().first();
        Cyberiada::ID item_id = elementIdToItemMap.key(item);
        const Cyberiada::Element* element = model->idToElement(QString::fromStdString(item_id));

        if(!element) return; // not a smeditor base element
        // MY_ASSERT(element);
		QModelIndex index = model->elementToIndex(element);
		CyberiadaSMEditorWindow* p = dynamic_cast<CyberiadaSMEditorWindow*>(parent());
		//p->SMView->setCurrentIndex(index);
		p->SMView->select(index);
	}
}

void CyberiadaSMEditorScene::toggleTransitionText(bool visible)
{
    for (auto item : items()) {
        if (auto transition = dynamic_cast<CyberiadaSMEditorTransitionItem*>(item)) {
            transition->setActionVisibility(visible);
        }
    }
}

void CyberiadaSMEditorScene::toggleInspectorMode(bool on)
{
    inspectorModeEnabled = on;
    for (auto item : items()) {
        if (auto cyberiadaItem = dynamic_cast<CyberiadaSMEditorAbstractItem*>(item)) {
            cyberiadaItem->setInspectorMode(on);
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
    // нужно настроить QMap для поиска элемента
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

void CyberiadaSMEditorScene::onModelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Cyberiada::Element* element = model->indexToElement(topLeft);
    // поиск CyberiadaSMEditorAbstractItem по QMap QMap<Cyberiada::ID, QGraphicsItem*> elementIdToItemMap;
    updateItemsRecursively(nullptr, static_cast<Cyberiada::ElementCollection*>(element));
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
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(new_parent).c_str();
                addItemsRecursively(state->getRegion(), static_cast<Cyberiada::ElementCollection*>(child));
                addItem(state);
                break;
            }
            case Cyberiada::elementSimpleState: {
                CyberiadaSMEditorStateItem* state = new CyberiadaSMEditorStateItem(this, model, child, new_parent);
                elementIdToItemMap.insert(child->get_id(), state);
                addItem(state);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(new_parent).c_str();
				break;
            }
            case Cyberiada::elementInitial: {
                CyberiadaSMEditorVertexItem* initial = new CyberiadaSMEditorVertexItem(model, child, new_parent);
                elementIdToItemMap.insert(child->get_id(), initial);
                addItem(initial);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(new_parent).c_str();
                break;
            }
            case Cyberiada::elementFinal: {
                CyberiadaSMEditorVertexItem* final = new CyberiadaSMEditorVertexItem(model, child, new_parent);
                elementIdToItemMap.insert(child->get_id(), final);
                addItem(final);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(new_parent).c_str();
                break;
            }
            case Cyberiada::elementTerminate: {
                CyberiadaSMEditorVertexItem* terminate = new CyberiadaSMEditorVertexItem(model, child, new_parent);
                elementIdToItemMap.insert(child->get_id(), terminate);
                addItem(terminate);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(new_parent).c_str();
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
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(new_parent).c_str();
                break;
            }
            case Cyberiada::elementFormalComment: {
                if (!child->has_geometry()) break;
                CyberiadaSMEditorCommentItem* formalComment = new CyberiadaSMEditorCommentItem(this, model, child, new_parent, elementIdToItemMap);
                elementIdToItemMap.insert(child->get_id(), formalComment);
                addItem(formalComment);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(new_parent).c_str();
                break;
            }
            case Cyberiada::elementTransition: {
                CyberiadaSMEditorTransitionItem* transition = new CyberiadaSMEditorTransitionItem(this, model, child, NULL, elementIdToItemMap);
                elementIdToItemMap.insert(child->get_id(), transition);
                addItem(transition);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementIdToItemMap.key(new_parent).c_str();
                break;
            }
			default:
				MY_ASSERT(false);
			}
		}
    }
}

void CyberiadaSMEditorScene::setGridSize(int newSize)
{
    if (newSize > 0) {
		gridSize = newSize;
		update();
	}
}

void CyberiadaSMEditorScene::enableGrid(bool on)
{
    gridEnabled = on;
    update();
}

void CyberiadaSMEditorScene::enableGridSnap(bool on)
{
    gridSnap = on;
}

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
    addItemsRecursively(NULL, sm);
    // qreal margin = std::max(itemsBoundingRect().width(), itemsBoundingRect().height()) * DEFAULT_SCENE_BORDER_MARGIN_PERCENT;
    qreal margin = DEFAULT_SCENE_BORDER_MARGIN;
    setSceneRect(itemsBoundingRect().adjusted(-margin, -margin, margin, margin));
    views().first()->fitInView(itemsBoundingRect(), Qt::KeepAspectRatio);
    update();
}

void CyberiadaSMEditorScene::setCurrentTool(ToolType tool) {
    currentTool = tool;
}

void CyberiadaSMEditorScene::removeSMItem(Cyberiada::Element* element)
{
    // TODO add model method for deleting from model
    /*Cyberiada::ElementType type = element->get_type();
    if(type == Cyberiada::elementCompositeState) {
        if(element->has_children()) {
            Cyberiada::ElementCollection* collection = static_cast<Cyberiada::ElementCollection*>(element);
            const Cyberiada::ElementList& children = collection->get_children();
            for (Cyberiada::ElementList::const_iterator i = children.begin(); i != children.end(); i++) {
                Cyberiada::Element* child = *i;
                removeSMItem(child);
            }
        }
    }

    QList<Cyberiada::ID> toRemove;
    for (auto it = elementIdToItemMap.begin(); it != elementIdToItemMap.end(); ++it) {
        QGraphicsItem* item = it.value();

        // Пытаемся привести к TransitionItem
        auto* transition = dynamic_cast<CyberiadaSMEditorTransitionItem*>(item);
        if (transition) {
            if(transition->sourceId() == element->get_id()) {
                // elementIdToItemMap.remove(it.key());
                toRemove.append(it.key());
                removeItem(transition);
                continue;
            }
            if(transition->sourceId() == element->get_id()) {
                // elementIdToItemMap.remove(it.key());
                toRemove.append(it.key());
                removeItem(transition);
            }
        }
    }
    for (const Cyberiada::ID& id : toRemove) {
        elementIdToItemMap.remove(id);
    }*/
}

void CyberiadaSMEditorScene::drawBackground(QPainter* painter, const QRectF &)
{
	painter->setPen(QPen(Qt::darkGray, 2, Qt::SolidLine));
	painter->setBrush(backgroundBrush());
	painter->drawRect(sceneRect());

	if (gridSize <= 0 || !gridEnabled) {
		return ;
	}

	painter->setPen(gridPen);

	QRectF rect = sceneRect();

	double left = int(rect.left()) - (int(rect.left()) % gridSize);
	double top = int(rect.top()) - (int(rect.top()) % gridSize);

	QVarLengthArray<QLineF, 100> lines;

	for (double x = left; x < rect.right(); x += gridSize)
		lines.append(QLineF(x, rect.top(), x, rect.bottom()));
	for (double y = top; y < rect.bottom(); y += gridSize)
		lines.append(QLineF(rect.left(), y, rect.right(), y));

	painter->drawLines(lines.data(), lines.size());

    if (inspectorModeEnabled) {
        painter->setBrush(Qt::red);
        painter->drawEllipse(QPointF(0, 0), 2, 2); // the center of the coordinate system
    }
}

