/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process scene structure test (see docs/TESTING.md, L4)
 *
 * Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
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

#include <QtTest>
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_editor_transition_item.h"
#include "cyberiadasm_editor_state_item.h"
#include "dotsignal.h"
#include "settings_manager.h"

class TestScene: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void test_load_scene();
	void test_item_hierarchy();
	void test_item_geometry();
	void test_loop_polyline();
	void test_inspector_region();
	void test_service_objects();
	void test_selection();
	void test_title_sync();
	void test_action_edit();
	void test_body_drag();
	void test_double_click_action();
	void test_action_layout();
	void test_border_resize();
	void test_box_transition();
	void test_auto_attach();
	void test_new_element_place();
	void test_undo_gesture();
	void test_sm_border();
	void test_new_state();
	void test_new_transition();
	void test_new_comment();
	void test_new_choice();
	void test_reparent();
	void test_delete();
	void test_multi_sm();
	void test_new_sm_place();
	void test_sm_contains();
	void test_sm_extends();
	void test_sm_title_size();

private:
	int countItems(int type);
	static bool onBorder(const QRectF& box, const QPointF& p);
	void deleteTransition(const Cyberiada::ID& source, const Cyberiada::ID& target);
	void mouse(QEvent::Type type, const QPointF& scenePos, Qt::MouseButtons buttons);
	CyberiadaSMModel* model;
	CyberiadaSMEditorScene* scene;
};

int TestScene::countItems(int type)
{
	int count = 0;
	const QMap<Cyberiada::ID, QGraphicsItem*>& map = scene->getMap();
	for (QMap<Cyberiada::ID, QGraphicsItem*>::const_iterator i = map.begin();
		 i != map.end(); i++) {
		if ((*i)->type() == type) count++;
	}
	return count;
}

// a left button gesture delivered through the scene, as a view would
void TestScene::mouse(QEvent::Type type, const QPointF& scenePos, Qt::MouseButtons buttons)
{
	QGraphicsSceneMouseEvent event(type);
	event.setScenePos(scenePos);
	event.setScreenPos(scenePos.toPoint());
	event.setButton(Qt::LeftButton);
	event.setButtons(buttons);
	QApplication::sendEvent(scene, &event);
}

// on the boundary of a (rounded) box: inside it grown by a pixel, outside
// it shrunk by the corner radius
bool TestScene::onBorder(const QRectF& box, const QPointF& p)
{
	qreal r = ROUNDED_RECT_RADIUS + 1;
	return box.adjusted(-1, -1, 1, 1).contains(p) && !box.adjusted(r, r, -r, -r).contains(p);
}

void TestScene::deleteTransition(const Cyberiada::ID& source, const Cyberiada::ID& target)
{
	const QMap<Cyberiada::ID, QGraphicsItem*>& map = scene->getMap();
	for (QMap<Cyberiada::ID, QGraphicsItem*>::const_iterator i = map.begin(); i != map.end(); i++) {
		if ((*i)->type() != CyberiadaSMEditorAbstractItem::TransitionItem) continue;
		Cyberiada::Element* e = dynamic_cast<CyberiadaSMEditorAbstractItem*>(*i)->getElement();
		const Cyberiada::Transition* t = static_cast<const Cyberiada::Transition*>(e);
		if (t->source_element_id() == source && t->target_element_id() == target) {
			QVERIFY(model->deleteElement(model->elementToIndex(e)));
			return;
		}
	}
	QFAIL("no such transition");
}

void TestScene::initTestCase()
{
	model = new CyberiadaSMModel(this);
	scene = new CyberiadaSMEditorScene(model, this);
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
}

void TestScene::test_load_scene()
{
	// the item map mirrors the diagram structure
	const QMap<Cyberiada::ID, QGraphicsItem*>& map = scene->getMap();
	QCOMPARE(map.size(), 10);
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::SMItem), 1);
	// the state item class covers the simple and the composite states
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::StateItem), 5);
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::VertexItem), 1);
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::TransitionItem), 3);
}

void TestScene::test_item_hierarchy()
{
	// the items enter the scene through their parents, so a nested element
	// must sit in the region of its state and appear in the scene once
	const QMap<Cyberiada::ID, QGraphicsItem*>& map = scene->getMap();
	QGraphicsItem* sm = map.value("G");
	CyberiadaSMEditorStateItem* outer =
		dynamic_cast<CyberiadaSMEditorStateItem*>(map.value("node-0"));
	CyberiadaSMEditorStateItem* inner =
		dynamic_cast<CyberiadaSMEditorStateItem*>(map.value("node-0-0"));
	QVERIFY(sm && outer && inner);

	QVERIFY(!sm->parentItem());
	QCOMPARE(outer->parentItem(), sm);
	QCOMPARE(inner->parentItem(), static_cast<QGraphicsItem*>(outer->getRegion()));
	QCOMPARE(map.value("node-0-0-1")->parentItem(),
			 static_cast<QGraphicsItem*>(inner->getRegion()));
	// the transitions live at the top level
	QVERIFY(!map.value("edge-0")->parentItem());

	// every item belongs to this scene and is listed exactly once
	QList<QGraphicsItem*> items = scene->items();
	for (QMap<Cyberiada::ID, QGraphicsItem*>::const_iterator i = map.begin(); i != map.end(); i++) {
		QCOMPARE((*i)->scene(), static_cast<QGraphicsScene*>(scene));
		QCOMPARE(items.count(*i), 1);
	}
}

void TestScene::test_item_geometry()
{
	// a known state item sits at the model geometry (parent-local Qt coords)
	QGraphicsItem* item = scene->getMap().value("node-0-0-1");
	QVERIFY(item);
	QCOMPARE(item->pos(), QPointF(-100.0, 25.0));
}

void TestScene::test_loop_polyline()
{
	// the loop follows its polyline instead of the default arc
	CyberiadaSMEditorTransitionItem* loop =
		dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value("edge-0"));
	QVERIFY(loop);
	const Cyberiada::Transition* t =
		static_cast<const Cyberiada::Transition*>(model->idToElement("edge-0"));
	QVERIFY(t->has_polyline());
	QRectF routed = loop->boundingRect();
	const Cyberiada::Polyline& pl = t->get_geometry_polyline();
	for (Cyberiada::Polyline::const_iterator i = pl.begin(); i != pl.end(); i++) {
		QVERIFY(routed.contains(loop->sourceCenter() + QPointF(i->x, i->y)));
	}
	// without the points the loop falls back to the arc between the endpoints
	QPointF farthest = loop->sourceCenter() + QPointF(pl.front().x, pl.front().y);
	QVERIFY(model->updateGeometry(model->elementToIndex(model->idToElement("edge-0")),
								  Cyberiada::Polyline()));
	QVERIFY(!loop->boundingRect().contains(farthest));
}

void TestScene::test_inspector_region()
{
	// the inspected region follows the document, the edited one is laid out
	// around the state title and activities
	CyberiadaSMEditorStateItem* state =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0"));
	QVERIFY(state);
	QVERIFY(state->getRegion());

	QVERIFY(!SettingsManager::instance().getInspectorMode());
	QVERIFY(state->getRegion()->pos() != QPointF(0, 0));

	SettingsManager::instance().setInspectorMode(true);
	QCOMPARE(state->getRegion()->pos(), QPointF(0, 0));
	QCOMPARE(state->getRegion()->rect(), state->rect());

	SettingsManager::instance().setInspectorMode(false);
	QVERIFY(state->getRegion()->pos() != QPointF(0, 0));
}

void TestScene::test_service_objects()
{
	// the service objects are a decoration: showing them must not move the
	// region, which is the graphics parent of every nested element
	CyberiadaSMEditorStateItem* state =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0"));
	QVERIFY(state);
	QVERIFY(state->getRegion());

	// the setting is persisted, so the test states its own starting point
	SettingsManager::instance().setShowServiceObjects(false);
	QPointF pos = state->getRegion()->pos();
	QRectF rect = state->getRegion()->rect();

	SettingsManager::instance().setShowServiceObjects(true);
	QCOMPARE(state->getRegion()->pos(), pos);
	QCOMPARE(state->getRegion()->rect(), rect);

	SettingsManager::instance().setShowServiceObjects(false);
	QCOMPARE(state->getRegion()->pos(), pos);
	QCOMPARE(state->getRegion()->rect(), rect);

	// while the inspector mode still moves it, whatever the decoration
	SettingsManager::instance().setShowServiceObjects(true);
	SettingsManager::instance().setInspectorMode(true);
	QCOMPARE(state->getRegion()->pos(), QPointF(0, 0));
	QVERIFY(state->getRegion()->pos() != pos);

	SettingsManager::instance().setInspectorMode(false);
	SettingsManager::instance().setShowServiceObjects(false);
	QCOMPARE(state->getRegion()->pos(), pos);
}

void TestScene::test_selection()
{
	QGraphicsItem* first = scene->getMap().value("node-0-0-1");
	QGraphicsItem* second = scene->getMap().value("node-0-1");
	QVERIFY(first && second);
	scene->slotElementSelected(model->elementToIndex(model->idToElement("node-0-0-1")));
	QVERIFY(first->isSelected());
	scene->slotElementSelected(model->elementToIndex(model->idToElement("node-0-1")));
	QVERIFY(second->isSelected());
	QVERIFY(!first->isSelected());
}

void TestScene::test_title_sync()
{
	// the dataChanged path is the safe model->scene sync; the item survives
	// the update (reparent/delete through a connected scene stay untested -
	// the sync is re-entrant, see the batch driver)
	QVERIFY(model->updateTitle(model->elementToIndex(model->idToElement("node-0-0-1")),
							   "Synced"));
	QGraphicsItem* item = scene->getMap().value("node-0-0-1");
	QVERIFY(item);
	QVERIFY(scene->items().contains(item));
}

// the mutations below run through the connected scene: the row signals
// build and tear down the items

void TestScene::test_action_edit()
{
	// the commit of an action edit rebuilds the action items from the very
	// event handler of the edited one: the old item must outlive the call
	CyberiadaSMEditorStateItem* state =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0-1"));
	QVERIFY(state);
	QModelIndex index = model->elementToIndex(model->idToElement("node-0-1"));
	QVERIFY(model->newAction(index, Cyberiada::actionEntry, QString(), QString(), "first()"));
	StateAction* action = nullptr;
	for (QGraphicsItem* child : state->childItems()) {
		if ((action = dynamic_cast<StateAction*>(child))) break;
	}
	QVERIFY(action);
	QCOMPARE(action->toPlainText(), QString("entry / first()"));

	// the in-scene edit path: focus, retype, commit on the focus out
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);
	action->setTextInteractionFlags(Qt::TextEditorInteraction);
	action->setFocus();
	QVERIFY(action->hasFocus());
	action->setPlainText("entry / second()");
	QPointer<StateAction> old(action);
	action->clearFocus();
	// the commit rebuilt the actions, but the emitting item must still be
	// alive here - it is destroyed only after its handlers left the stack
	QVERIFY(!old.isNull());
	QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	QVERIFY(old.isNull());
	const Cyberiada::State* st =
		static_cast<const Cyberiada::State*>(model->idToElement("node-0-1"));
	QCOMPARE(int(st->get_actions().size()), 1);
	QCOMPARE(QString(st->get_actions()[0].get_behavior().c_str()), QString("second()"));

	// a multiline behaviour is shown under the keyword, as in the document
	action = nullptr;
	for (QGraphicsItem* child : state->childItems()) {
		if ((action = dynamic_cast<StateAction*>(child))) break;
	}
	QVERIFY(action);
	action->setTextInteractionFlags(Qt::TextEditorInteraction);
	action->setFocus();
	action->setPlainText("entry / a();\nb();");
	action->clearFocus();
	QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	QCOMPARE(QString(st->get_actions()[0].get_behavior().c_str()), QString("a();\nb();"));
	action = nullptr;
	for (QGraphicsItem* child : state->childItems()) {
		if ((action = dynamic_cast<StateAction*>(child))) break;
	}
	QVERIFY(action);
	QCOMPARE(action->toPlainText(), QString("entry/\na();\nb();"));
}

void TestScene::test_body_drag()
{
	// the select tool moves a state by its body; only the transition tool
	// draws a transition from it
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);
	CyberiadaSMEditorStateItem* state =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0-1"));
	QVERIFY(state);
	const Cyberiada::State* element =
		static_cast<const Cyberiada::State*>(model->idToElement("node-0-1"));
	Cyberiada::Rect before = element->get_geometry_rect();
	int transitions = countItems(CyberiadaSMEditorAbstractItem::TransitionItem);
	// the free body area: the title and the actions sit at the top and the left
	QPointF centre = state->sceneBoundingRect().bottomRight() - QPointF(20, 20);

	scene->clearSelection();
	mouse(QEvent::GraphicsSceneMousePress, centre, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, centre + QPointF(30, 20), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, centre + QPointF(30, 20), Qt::NoButton);
	Cyberiada::Rect moved = element->get_geometry_rect();
	QCOMPARE(moved.x, before.x + 30);
	QCOMPARE(moved.y, before.y + 20);
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::TransitionItem), transitions);

	scene->setCurrentTool(ToolType::Transition);
	centre = state->sceneBoundingRect().bottomRight() - QPointF(20, 20);
	mouse(QEvent::GraphicsSceneMousePress, centre, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, centre + QPointF(30, 20), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, centre + QPointF(30, 20), Qt::NoButton);
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::TransitionItem), transitions + 1);
	QCOMPARE(element->get_geometry_rect().x, moved.x);
	scene->setCurrentTool(ToolType::Select);

	// the gesture left nothing armed: the next body drag moves again
	mouse(QEvent::GraphicsSceneMousePress, centre, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, centre + QPointF(-30, -20), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, centre + QPointF(-30, -20), Qt::NoButton);
	QCOMPARE(element->get_geometry_rect().x, before.x);
	QCOMPARE(element->get_geometry_rect().y, before.y);
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::TransitionItem), transitions + 1);

	// the drawn loop goes away, the later tests count the diagram's own
	const QMap<Cyberiada::ID, QGraphicsItem*>& map = scene->getMap();
	for (QMap<Cyberiada::ID, QGraphicsItem*>::const_iterator i = map.begin(); i != map.end(); i++) {
		if ((*i)->type() != CyberiadaSMEditorAbstractItem::TransitionItem) continue;
		Cyberiada::Element* e = dynamic_cast<CyberiadaSMEditorAbstractItem*>(*i)->getElement();
		const Cyberiada::Transition* t = static_cast<const Cyberiada::Transition*>(e);
		if (t->source_element_id() == "node-0-1" && t->target_element_id() == "node-0-1") {
			QVERIFY(model->deleteElement(model->elementToIndex(e)));
			break;
		}
	}
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::TransitionItem), transitions);
}

void TestScene::test_double_click_action()
{
	// a double click on the free space adds the entry, then the exit
	CyberiadaSMEditorStateItem* state =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0-0-2"));
	QVERIFY(state);
	QModelIndex index = model->elementToIndex(model->idToElement("node-0-0-2"));
	QCOMPARE(state->missingActionType(), int(Cyberiada::actionEntry));
	QVERIFY(model->newAction(index, Cyberiada::actionEntry, QString(), QString(), "in()"));
	QCOMPARE(state->missingActionType(), int(Cyberiada::actionExit));
	QVERIFY(model->newAction(index, Cyberiada::actionExit, QString(), QString(), "out()"));
	QCOMPARE(state->missingActionType(), -1);
	QVERIFY(model->deleteAction(index, 1));
	QVERIFY(model->deleteAction(index, 0));
	QCOMPARE(state->missingActionType(), int(Cyberiada::actionEntry));
}

void TestScene::test_action_layout()
{
	// the entry sits under the title at the left, the exit at the bottom left
	CyberiadaSMEditorStateItem* state =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0-0-2"));
	QVERIFY(state);
	QModelIndex index = model->elementToIndex(model->idToElement("node-0-0-2"));
	QVERIFY(model->newAction(index, Cyberiada::actionEntry, QString(), QString(), "in()"));
	QVERIFY(model->newAction(index, Cyberiada::actionExit, QString(), QString(), "out()"));
	StateTitle* title = nullptr;
	StateAction* entry = nullptr;
	StateAction* exit = nullptr;
	for (QGraphicsItem* child : state->childItems()) {
		if (StateTitle* t = dynamic_cast<StateTitle*>(child)) title = t;
		if (StateAction* a = dynamic_cast<StateAction*>(child)) {
			if (a->toPlainText().startsWith("entry")) entry = a;
			if (a->toPlainText().startsWith("exit")) exit = a;
		}
	}
	QVERIFY(title && entry && exit);
	QRectF rect = state->rect();
	QCOMPARE(entry->pos(), QPointF(rect.x() + 15, rect.y() + title->boundingRect().height()));
	QCOMPARE(exit->pos(), QPointF(rect.x() + 15, rect.bottom() - exit->boundingRect().height()));
	QVERIFY(model->deleteAction(index, 1));
	QVERIFY(model->deleteAction(index, 0));
}

void TestScene::test_border_resize()
{
	// the bottom border resizes even where the exit block covers it, and a
	// press decides the zone without a prior hover
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);
	CyberiadaSMEditorStateItem* state =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0-0-2"));
	QVERIFY(state);
	QModelIndex index = model->elementToIndex(model->idToElement("node-0-0-2"));
	QVERIFY(model->newAction(index, Cyberiada::actionExit, QString(), QString(), "out()"));
	const Cyberiada::State* element =
		static_cast<const Cyberiada::State*>(model->idToElement("node-0-0-2"));
	Cyberiada::Rect before = element->get_geometry_rect();
	QRectF box = state->sceneBoundingRect();
	QPointF onExit(box.center().x() - 20, box.bottom() - 3);
	QVERIFY(dynamic_cast<StateAction*>(scene->itemAt(onExit, QTransform())));

	scene->clearSelection();
	mouse(QEvent::GraphicsSceneMousePress, onExit, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, onExit + QPointF(0, 40), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, onExit + QPointF(0, 40), Qt::NoButton);
	// the border follows the pointer, which started 3 px inside
	Cyberiada::Rect after = element->get_geometry_rect();
	QCOMPARE(after.height, before.height + 37);
	QCOMPARE(after.width, before.width);

	// the right border still resizes the width
	box = state->sceneBoundingRect();
	QPointF onRight(box.right() - 3, box.center().y());
	mouse(QEvent::GraphicsSceneMousePress, onRight, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, onRight + QPointF(30, 0), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, onRight + QPointF(30, 0), Qt::NoButton);
	QCOMPARE(element->get_geometry_rect().width, before.width + 27);
	QCOMPARE(element->get_geometry_rect().height, after.height);
	QVERIFY(model->deleteAction(index, 0));
}

void TestScene::test_box_transition()
{
	// a drag from a border box draws a transition under a transient
	// transition tool; the release brings the select tool back
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);
	CyberiadaSMEditorStateItem* state =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0-1"));
	QVERIFY(state);
	int transitions = countItems(CyberiadaSMEditorAbstractItem::TransitionItem);
	scene->clearSelection();
	state->setSelected(true);
	DotSignal* box = nullptr;
	for (QGraphicsItem* child : state->childItems()) {
		DotSignal* dot = dynamic_cast<DotSignal*>(child);
		if (dot && dot->pos() == QPointF(0, state->rect().bottom())) box = dot;
	}
	QVERIFY(box);
	box->setVisible(true);
	QPointF on = box->scenePos();
	QVERIFY(scene->itemAt(on, QTransform()) == box);

	mouse(QEvent::GraphicsSceneMousePress, on, Qt::LeftButton);
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::TransitionItem), transitions);
	mouse(QEvent::GraphicsSceneMouseMove, on + QPointF(0, 30), Qt::LeftButton);
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::TransitionItem), transitions + 1);
	QVERIFY(scene->getCurrentTool() == ToolType::Transition);
	mouse(QEvent::GraphicsSceneMouseRelease, on + QPointF(0, 30), Qt::NoButton);
	QVERIFY(scene->getCurrentTool() == ToolType::Select);

	const QMap<Cyberiada::ID, QGraphicsItem*>& map = scene->getMap();
	for (QMap<Cyberiada::ID, QGraphicsItem*>::const_iterator i = map.begin(); i != map.end(); i++) {
		if ((*i)->type() != CyberiadaSMEditorAbstractItem::TransitionItem) continue;
		Cyberiada::Element* e = dynamic_cast<CyberiadaSMEditorAbstractItem*>(*i)->getElement();
		const Cyberiada::Transition* t = static_cast<const Cyberiada::Transition*>(e);
		if (t->source_element_id() == "node-0-1" && t->target_element_id() == "node-0-1") {
			QVERIFY(model->deleteElement(model->elementToIndex(e)));
			break;
		}
	}
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::TransitionItem), transitions);
}

void TestScene::test_auto_attach()
{
	// an end without a stored point is attached to the node border: the
	// drawn transition keeps its source unstored yet runs border to border
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);
	CyberiadaSMEditorStateItem* from =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0-1"));
	CyberiadaSMEditorStateItem* to =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0-0-2"));
	QVERIFY(from && to);
	int transitions = countItems(CyberiadaSMEditorAbstractItem::TransitionItem);
	scene->clearSelection();
	from->setSelected(true);
	DotSignal* box = nullptr;
	for (QGraphicsItem* child : from->childItems()) {
		DotSignal* dot = dynamic_cast<DotSignal*>(child);
		if (dot && dot->pos() == QPointF(0, from->rect().bottom())) box = dot;
	}
	QVERIFY(box);
	box->setVisible(true);
	QPointF on = box->scenePos();
	// off the centre: the drag's own intersection then differs from the
	// display attachment, so the target point really gets stored
	QPointF over = to->sceneBoundingRect().center() + QPointF(20, 10);
	mouse(QEvent::GraphicsSceneMousePress, on, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, on + QPointF(0, 30), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, over, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, over, Qt::NoButton);
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::TransitionItem), transitions + 1);
	deleteTransition("node-0-1", "node-0-0-2");

	// the same gesture with the first move still inside the source state
	// (the loop phase of the drag) must not pin the source to the centre
	from->setSelected(true);
	box->setVisible(true);
	mouse(QEvent::GraphicsSceneMousePress, on, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, on - QPointF(0, 20), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, over, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, over, Qt::NoButton);
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::TransitionItem), transitions + 1);

	CyberiadaSMEditorTransitionItem* drawn = nullptr;
	const QMap<Cyberiada::ID, QGraphicsItem*>& map = scene->getMap();
	for (QMap<Cyberiada::ID, QGraphicsItem*>::const_iterator i = map.begin(); i != map.end(); i++) {
		CyberiadaSMEditorTransitionItem* t = dynamic_cast<CyberiadaSMEditorTransitionItem*>(*i);
		if (t && t->sourceId() == "node-0-1" && t->targetId() == "node-0-0-2") drawn = t;
	}
	QVERIFY(drawn);
	const Cyberiada::Transition* element =
		static_cast<const Cyberiada::Transition*>(drawn->getElement());
	QVERIFY(!element->has_geometry_source_point());
	QVERIFY(element->has_geometry_target_point());
	QPointF sp = drawn->sourcePoint() + drawn->sourceCenter();
	QVERIFY(onBorder(from->sceneBoundingRect(), sp));
	QVERIFY(onBorder(to->sceneBoundingRect(), drawn->targetPoint() + drawn->targetCenter()));
	// on the border facing the target, not the one behind
	QVERIFY(QLineF(sp, to->sceneBoundingRect().center()).length() <
			QLineF(from->sceneBoundingRect().center(), to->sceneBoundingRect().center()).length());
	deleteTransition("node-0-1", "node-0-0-2");

	// a transition without any stored point attaches at both borders that
	// face each other
	CyberiadaSMEditorTransitionItem* plain = scene->addTransition(from, to);
	QVERIFY(plain);
	sp = plain->sourcePoint() + plain->sourceCenter();
	QPointF tp = plain->targetPoint() + plain->targetCenter();
	QVERIFY(onBorder(from->sceneBoundingRect(), sp));
	QVERIFY(onBorder(to->sceneBoundingRect(), tp));
	qreal centres = QLineF(from->sceneBoundingRect().center(), to->sceneBoundingRect().center()).length();
	QVERIFY(QLineF(sp, to->sceneBoundingRect().center()).length() < centres);
	QVERIFY(QLineF(tp, from->sceneBoundingRect().center()).length() < centres);
	deleteTransition("node-0-1", "node-0-0-2");
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::TransitionItem), transitions);
}

void TestScene::test_new_element_place()
{
	// two states added in a row do not land on each other
	QGraphicsItem* sm = scene->getMap().value("G");
	QVERIFY(sm);
	QList<Cyberiada::ID> before = scene->getMap().keys();
	scene->clearSelection();
	sm->setSelected(true);
	scene->addSMItem(Cyberiada::elementSimpleState);
	scene->clearSelection();
	sm->setSelected(true);
	scene->addSMItem(Cyberiada::elementSimpleState);
	QList<QGraphicsItem*> fresh;
	for (const Cyberiada::ID& id : scene->getMap().keys()) {
		if (!before.contains(id)) fresh.append(scene->getMap().value(id));
	}
	QCOMPARE(fresh.size(), 2);
	QVERIFY(!fresh[0]->sceneBoundingRect().intersects(fresh[1]->sceneBoundingRect()));
	for (QGraphicsItem* item : fresh) {
		Cyberiada::Element* e = dynamic_cast<CyberiadaSMEditorAbstractItem*>(item)->getElement();
		QVERIFY(model->deleteElement(model->elementToIndex(e)));
	}
}

void TestScene::test_undo_gesture()
{
	// a whole drag is one undo step; the undo rebuilds the scene from the
	// restored document and keeps the moved state selected
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);
	CyberiadaSMEditorStateItem* state =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0-1"));
	QVERIFY(state);
	QUndoStack* stack = model->undoStack();
	int steps = stack->count();
	Cyberiada::Rect before = static_cast<const Cyberiada::State*>(
		model->idToElement("node-0-1"))->get_geometry_rect();
	QPointF at = state->sceneBoundingRect().bottomRight() - QPointF(20, 20);
	scene->clearSelection();
	mouse(QEvent::GraphicsSceneMousePress, at, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, at + QPointF(10, 5), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, at + QPointF(20, 10), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, at + QPointF(30, 20), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, at + QPointF(30, 20), Qt::NoButton);
	QCOMPARE(stack->count(), steps + 1);
	QCOMPARE(static_cast<const Cyberiada::State*>(
		model->idToElement("node-0-1"))->get_geometry_rect().x, before.x + 30);

	stack->undo();
	QCOMPARE(static_cast<const Cyberiada::State*>(
		model->idToElement("node-0-1"))->get_geometry_rect().x, before.x);
	QGraphicsItem* rebuilt = scene->getMap().value("node-0-1");
	QVERIFY(rebuilt);
	QVERIFY(rebuilt != state);
	QVERIFY(rebuilt->isSelected());
	QVERIFY(scene->items().contains(rebuilt));
	stack->redo();
	QCOMPARE(static_cast<const Cyberiada::State*>(
		model->idToElement("node-0-1"))->get_geometry_rect().x, before.x + 30);
	stack->undo();
}

void TestScene::test_sm_border()
{
	// a rect-less state machine gains a drawn border and loses it again;
	// setting the border on an SM that already has content must not crash
	// (the resize handles are created lazily) nor move the content
	QModelIndex sm = model->firstSMIndex();
	QVERIFY(sm.isValid());
	Cyberiada::Element* smElem = model->indexToElement(sm);
	if (smElem->has_geometry()) QVERIFY(model->updateGeometry(sm, Cyberiada::Rect()));
	QVERIFY(!smElem->has_geometry());

	Cyberiada::Rect content = smElem->get_bound_rect(*model->rootDocument());
	QVERIFY(content.valid);
	QVERIFY(model->updateGeometry(sm, content));
	QVERIFY(smElem->has_geometry());
	QGraphicsItem* item = scene->getMap().value(smElem->get_id());
	QVERIFY(item);
	QVERIFY(item->type() == CyberiadaSMEditorAbstractItem::SMItem);
	QVERIFY(!item->boundingRect().isEmpty());
	QVERIFY(scene->items().contains(item));

	// the border has an editable title and no transition handles
	StateTitle* smTitle = nullptr;
	DotSignal* smDot = nullptr;
	for (QGraphicsItem* child : item->childItems()) {
		if (StateTitle* t = dynamic_cast<StateTitle*>(child)) smTitle = t;
		if (DotSignal* d = dynamic_cast<DotSignal*>(child)) smDot = d;
	}
	QVERIFY(smTitle);
	QVERIFY(smDot == nullptr);

	// editing the title commits the state machine name
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);
	smTitle->setTextInteractionFlags(Qt::TextEditorInteraction);
	smTitle->setFocus();
	smTitle->setPlainText("Renamed SM");
	smTitle->clearFocus();
	QCoreApplication::processEvents();
	QCOMPARE(QString(smElem->get_name().c_str()), QString("Renamed SM"));

	// clearing the border removes it, back to frameless
	QVERIFY(model->updateGeometry(sm, Cyberiada::Rect()));
	QVERIFY(!smElem->has_geometry());
}

void TestScene::test_new_state()
{
	Cyberiada::ElementCollection* parent = static_cast<Cyberiada::ElementCollection*>(
		model->idToElement("node-0"));
	Cyberiada::State* s = model->newState(parent, "Fresh");
	QVERIFY(s);
	QGraphicsItem* item = scene->getMap().value(s->get_id());
	QVERIFY(item);
	// the children of a composite state live in its region
	QVERIFY(item->parentItem());
	QCOMPARE(item->parentItem()->parentItem(), scene->getMap().value("node-0"));
}

void TestScene::test_new_transition()
{
	Cyberiada::StateMachine* sm = static_cast<Cyberiada::StateMachine*>(model->idToElement("G"));
	Cyberiada::Transition* t = model->newTransition(sm, Cyberiada::transitionExternal,
													model->idToElement("node-0-0-2"),
													model->idToElement("node-0-1"),
													Cyberiada::Action());
	QVERIFY(t);
	QVERIFY(scene->getMap().value(t->get_id()));
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::TransitionItem), 4);
}

void TestScene::test_new_comment()
{
	Cyberiada::ElementCollection* parent = static_cast<Cyberiada::ElementCollection*>(
		model->idToElement("node-0"));
	// an informal comment without geometry gets a default sized item
	Cyberiada::Comment* bare = model->newComment(parent, "Bare");
	QVERIFY(bare);
	QGraphicsItem* bare_item = scene->getMap().value(bare->get_id());
	QVERIFY(bare_item);
	QCOMPARE(bare_item->type(), int(CyberiadaSMEditorAbstractItem::CommentItem));
	// the size of a geometry-less comment follows its text mode
	QVERIFY(!bare_item->boundingRect().isEmpty());
	Cyberiada::Comment* placed = model->newComment(parent, "Placed",
												   Cyberiada::Rect(10.0, 10.0, 80.0, 30.0));
	QVERIFY(placed);
	QVERIFY(scene->getMap().value(placed->get_id()));
	// the geometry-less document meta stays out of the scene
	QVERIFY(!scene->getMap().contains(model->idToElement("nMeta")->get_id()));
}

void TestScene::test_new_choice()
{
	Cyberiada::ElementCollection* parent = static_cast<Cyberiada::ElementCollection*>(
		model->idToElement("node-0"));
	Cyberiada::ChoicePseudostate* choice =
		model->newChoice(parent, Cyberiada::Rect(30.0, 40.0, 60.0, 50.0));
	QVERIFY(choice);
	QGraphicsItem* item = scene->getMap().value(choice->get_id());
	QVERIFY(item);
	QCOMPARE(item->type(), int(CyberiadaSMEditorAbstractItem::ChoiceItem));
	QCOMPARE(item->pos(), QPointF(30.0, 40.0));
	QCOMPARE(item->boundingRect(), QRectF(-30.0, -25.0, 60.0, 50.0));

	// the item follows the rect update
	QVERIFY(model->updateGeometry(model->elementToIndex(choice),
								  Cyberiada::Rect(70.0, 80.0, 60.0, 50.0)));
	QCOMPARE(item->pos(), QPointF(70.0, 80.0));

	// a choice without geometry gets a default sized diamond
	Cyberiada::ChoicePseudostate* bare = model->newChoice(parent);
	QVERIFY(bare);
	QGraphicsItem* bare_item = scene->getMap().value(bare->get_id());
	QVERIFY(bare_item);
	QCOMPARE(bare_item->boundingRect(), QRectF(-20.0, -20.0, 40.0, 40.0));
}

void TestScene::test_reparent()
{
	QVERIFY(scene->getMap().value("node-0-1"));
	Cyberiada::Rect before = static_cast<const Cyberiada::State*>(
		model->idToElement("node-0-1"))->get_geometry_rect();
	Cyberiada::Rect step = static_cast<const Cyberiada::State*>(
		model->idToElement("node-0-0"))->get_geometry_rect();
	QVERIFY(model->updateParent(model->elementToIndex(model->idToElement("node-0-1")), "node-0-0"));
	QGraphicsItem* item = scene->getMap().value("node-0-1");
	QVERIFY(item);
	// the item was rebuilt for the copied element
	QCOMPARE(dynamic_cast<CyberiadaSMEditorAbstractItem*>(item)->getElement(),
			 model->idToElement("node-0-1"));
	QVERIFY(item->parentItem());
	QCOMPARE(item->parentItem()->parentItem(), scene->getMap().value("node-0-0"));
	// the model keeps the absolute position across the reparent: the rect
	// is re-expressed relative to the new parent (one level deeper)
	Cyberiada::Rect after = static_cast<const Cyberiada::State*>(
		model->idToElement("node-0-1"))->get_geometry_rect();
	QCOMPARE(after.x, before.x - step.x);
	QCOMPARE(after.y, before.y - step.y);
	QCOMPARE(after.width, before.width);
}

void TestScene::test_delete()
{
	int transitions = countItems(CyberiadaSMEditorAbstractItem::TransitionItem);
	QVERIFY(scene->getMap().value("edge-0"));
	QVERIFY(model->deleteElement(model->elementToIndex(model->idToElement("node-0-0-1"))));
	QVERIFY(!scene->getMap().contains("node-0-0-1"));
	// the attached transitions went with the state
	QVERIFY(!scene->getMap().contains("edge-0"));
	QVERIFY(!scene->getMap().contains("edge-1"));
	QVERIFY(countItems(CyberiadaSMEditorAbstractItem::TransitionItem) < transitions);
	QVERIFY(!scene->items().isEmpty());
}

void TestScene::test_multi_sm()
{
	// every state machine of the document is drawn, not only the first
	QVERIFY(model->loadDocument("diagrams/two-sms.graphml"));
	scene->loadScene();
	QVERIFY(scene->getMap().value("G0"));
	QVERIFY(scene->getMap().value("G1"));
}

void TestScene::test_new_sm_place()
{
	// each added state machine gets a border that overlaps no other machine
	scene->addSMItem(Cyberiada::elementSM);
	scene->addSMItem(Cyberiada::elementSM);
	QList<QRectF> borders;
	const QMap<Cyberiada::ID, QGraphicsItem*>& map = scene->getMap();
	for (QMap<Cyberiada::ID, QGraphicsItem*>::const_iterator i = map.begin(); i != map.end(); i++) {
		if ((*i)->type() == CyberiadaSMEditorAbstractItem::SMItem && !(*i)->sceneBoundingRect().isEmpty()) {
			borders.append((*i)->sceneBoundingRect());
		}
	}
	QVERIFY(borders.size() >= 2);
	for (int a = 0; a < borders.size(); a++)
		for (int b = a + 1; b < borders.size(); b++)
			QVERIFY(!borders[a].intersects(borders[b]));
}

void TestScene::test_sm_contains()
{
	// a top-level element is clamped inside a bordered machine, which does
	// not grow to follow it (the user resizes the machine to make room)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);
	QModelIndex smi = model->firstSMIndex();
	Cyberiada::ElementCollection* smc =
		static_cast<Cyberiada::ElementCollection*>(model->indexToElement(smi));
	QVERIFY(model->updateGeometry(smi, Cyberiada::Rect(0, 0, 1600, 1200)));
	double borderW = smc->get_geometry_rect().width;

	Cyberiada::State* freeState = model->newState(smc, "Free", Cyberiada::Action(),
												  Cyberiada::Rect(600, 450, 100, 60));
	QVERIFY(freeState);
	QGraphicsItem* fi = scene->getMap().value(freeState->get_id());
	QGraphicsItem* smItem = scene->getMap().value(smc->get_id());
	QVERIFY(fi && smItem);

	QPointF c = fi->sceneBoundingRect().center();
	scene->clearSelection();
	fi->setSelected(true);
	mouse(QEvent::GraphicsSceneMousePress, c, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, c + QPointF(4000, 3000), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, c + QPointF(4000, 3000), Qt::NoButton);

	// the element stayed inside the border, and the border did not grow
	QRectF border = smItem->sceneBoundingRect();
	QRectF moved = fi->sceneBoundingRect();
	QVERIFY(moved.right() <= border.right() + 1.0);
	QVERIFY(moved.bottom() <= border.bottom() + 1.0);
	QCOMPARE(smc->get_geometry_rect().width, borderW);
}

void TestScene::test_sm_extends()
{
	// a new object added into a bordered machine goes inside it, and the
	// border extends to contain it
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	scene->addSMItem(Cyberiada::elementSM);           // a fresh empty bordered machine
	Cyberiada::StateMachine* sm = model->rootDocument()->get_state_machines().back();
	QVERIFY(sm && sm->has_geometry());
	Cyberiada::Rect before = sm->get_geometry_rect();

	for (int k = 0; k < 3; k++) {
		scene->clearSelection();
		QGraphicsItem* smItem = scene->getMap().value(sm->get_id());
		QVERIFY(smItem);
		smItem->setSelected(true);
		scene->addSMItem(Cyberiada::elementSimpleState);
	}

	Cyberiada::Rect after = sm->get_geometry_rect();
	QVERIFY(after.width >= before.width);
	QVERIFY(after.height >= before.height);

	// every child element sits within the (grown) border
	QGraphicsItem* smItem = scene->getMap().value(sm->get_id());
	QRectF border = smItem->sceneBoundingRect();
	const Cyberiada::ElementList& kids = sm->get_children();
	int states = 0;
	for (Cyberiada::ElementList::const_iterator i = kids.begin(); i != kids.end(); i++) {
		if ((*i)->get_type() != Cyberiada::elementSimpleState) continue;
		states++;
		QGraphicsItem* ci = scene->getMap().value((*i)->get_id());
		QVERIFY(ci);
		QRectF r = ci->sceneBoundingRect();
		QVERIFY(r.left() >= border.left() - 1.0 && r.right() <= border.right() + 1.0);
		QVERIFY(r.top() >= border.top() - 1.0 && r.bottom() <= border.bottom() + 1.0);
	}
	QCOMPARE(states, 3);
	// the extension was actually needed (three 200-wide states spread wider
	// than the default border)
	QVERIFY(after.width > before.width);
}

void TestScene::test_sm_title_size()
{
	// a title longer than the border widens the machine to fit its header
	QVERIFY(model->loadDocument("diagrams/two-sms.graphml"));
	scene->loadScene();
	QModelIndex g0 = model->elementToIndex(model->idToElement("G0"));
	QVERIFY(model->updateGeometry(g0, Cyberiada::Rect(0, 0, 100, 40)));
	double before = static_cast<const Cyberiada::ElementCollection*>(
		model->idToElement("G0"))->get_geometry_rect().width;
	QVERIFY(model->updateTitle(g0, "AVeryLongStateMachineTitleHere"));
	double after = static_cast<const Cyberiada::ElementCollection*>(
		model->idToElement("G0"))->get_geometry_rect().width;
	QVERIFY(after > before);
}

QTEST_MAIN(TestScene)
#include "l4-scene.moc"
