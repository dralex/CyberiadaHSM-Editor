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

#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_editor_items.h"
#include "cyberiadasm_editor_sm_item.h"
#include "cyberiadasm_editor_state_item.h"
#include "cyberiadasm_editor_vertex_item.h"
#include "cyberiadasm_editor_transition_item.h"
#include "cyberiadasm_editor_comment_item.h"

#include "myassert.h"

static double DEFAULT_SCENE_X = -700;
static double DEFAULT_SCENE_Y = -700;
static double DEFAULT_SCENE_WIDTH = 3000;
static double DEFAULT_SCENE_HEIGHT = 3000;
static double DEFAULT_SCENE_DELTA = 0.2;

CyberiadaSMEditorScene::CyberiadaSMEditorScene(CyberiadaSMModel* _model, QObject *parent): 
	QGraphicsScene(parent), model(_model), currentSM(NULL)
{
    gridSize = 25;
    gridEnabled = true;
    gridSnap = true;
	gridPen = QPen(Qt::gray, 0, Qt::DotLine);
	setBackgroundBrush(Qt::white);

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

void CyberiadaSMEditorScene::slotElementSelected(const QModelIndex& index)
{
	if (index.isValid() && index != model->rootIndex() && index != model->documentIndex()) {
/*		Cyberiada::Element* element = model->indexToElement(index);
		MY_ASSERT(element);
		Cyberiada::StateMachine* sm = model->rootDocument()->get_parent_sm(element);
		if (sm != currentSM) {
			currentSM = sm;
			Cyberiada::Rect bound = currentSM->get_bound_rect();
			clear();
			if (bound.valid) {
				setSceneRect(-(bound.width / 2.0) * (1.0 + DEFAULT_SCENE_DELTA),
							 -(bound.height / 2.0) * (1.0 + DEFAULT_SCENE_DELTA),
							 bound.width * (1.0 + 2.0 * DEFAULT_SCENE_DELTA),
							 bound.height * (1.0 + 2.0 * DEFAULT_SCENE_DELTA));
				QRectF r = sceneRect();
				qDebug() << "Scene (" << r.left() << ", " << r.top() << ", " << r.right() << ", " << r.bottom() << ")";
			} else {
				//reconstructGeometry();
			}
			addItemsRecursively(NULL, currentSM);
			update();
			}*/
	}
}

void CyberiadaSMEditorScene::addItemsRecursively(QGraphicsItem* parent, Cyberiada::ElementCollection* collection)
{
	Cyberiada::ElementType parent_type = collection->get_type();
    QGraphicsItem* new_parent = parent;
    qDebug() << "parent=null" <<  (new_parent == NULL);

    if (parent_type == Cyberiada::elementSM) {
        new_parent = new CyberiadaSMEditorSMItem(model, collection, parent);
        elementItem.insert(collection->get_id(), new_parent);
        addItem(new_parent);
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
                elementItem.insert(child->get_id(), state);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementItem.key(new_parent).c_str();
                addItemsRecursively(state->getArea(), static_cast<Cyberiada::ElementCollection*>(child));
                addItem(state);
                break;
            }
            case Cyberiada::elementSimpleState: {
                CyberiadaSMEditorStateItem* state = new CyberiadaSMEditorStateItem(this, model, child, new_parent);
                elementItem.insert(child->get_id(), state);
                addItem(state);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementItem.key(new_parent).c_str();
				break;
            }
            case Cyberiada::elementInitial: {
                CyberiadaSMEditorVertexItem* initial = new CyberiadaSMEditorVertexItem(model, child, new_parent);
                elementItem.insert(child->get_id(), initial);
                addItem(initial);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementItem.key(new_parent).c_str();
                break;
            }
            case Cyberiada::elementFinal: {
                CyberiadaSMEditorVertexItem* final = new CyberiadaSMEditorVertexItem(model, child, new_parent);
                elementItem.insert(child->get_id(), final);
                addItem(final);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementItem.key(new_parent).c_str();
                break;
            }
            case Cyberiada::elementTerminate: {
                CyberiadaSMEditorVertexItem* final = new CyberiadaSMEditorVertexItem(model, child, new_parent);
                elementItem.insert(child->get_id(), final);
                addItem(final);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementItem.key(new_parent).c_str();
                break;
            }
			case Cyberiada::elementChoice:
                // new CyberiadaSMEditorChoiceItem(model, child, new_parent);
				break;
            case Cyberiada::elementComment: {
                CyberiadaSMEditorCommentItem* formalComment = new CyberiadaSMEditorCommentItem(this, model, child, new_parent, &elementItem);
                elementItem.insert(child->get_id(), formalComment);
                addItem(formalComment);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementItem.key(new_parent).c_str();
                break;
            }
            case Cyberiada::elementFormalComment: {
                CyberiadaSMEditorCommentItem* formalComment = new CyberiadaSMEditorCommentItem(this, model, child, new_parent, &elementItem);
                elementItem.insert(child->get_id(), formalComment);
                addItem(formalComment);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementItem.key(new_parent).c_str();
                break;
            }
            case Cyberiada::elementTransition: {
                CyberiadaSMEditorTransitionItem* transition = new CyberiadaSMEditorTransitionItem(this, model, child, NULL, &elementItem);
                elementItem.insert(child->get_id(), transition);
                addItem(transition);
                qDebug() << "add item" << child->get_id().c_str() << "type" << type << "parent" << elementItem.key(new_parent).c_str();
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

void CyberiadaSMEditorScene::updateScene()
{
    elementItem.clear();

    clear();

    Cyberiada::StateMachine* sm = static_cast<Cyberiada::StateMachine*>(model->indexToElement(model->firstSMIndex()));
    addItemsRecursively(NULL, sm);
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

    painter->setBrush(Qt::red);
    painter->drawEllipse(QPointF(0, 0), 2, 2); // Центр системы координат
}

