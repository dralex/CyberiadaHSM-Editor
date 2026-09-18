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
#include <QGraphicsView>
#include <cmath>
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_editor_transition_item.h"
#include "cyberiadasm_editor_state_item.h"
#include "cyberiadasm_editor_vertex_item.h"
#include "dotsignal.h"
#include "settings_manager.h"
#include "cyberiadasm_dump.h"
#include "editable_text_item.h"

class TestScene: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void cleanup();
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
	void test_ctrl_axis_move();
	void test_new_sm_single_item();
	void test_double_click_action();
	void test_action_layout();
	void test_border_resize();
	void test_container_resize_clamp();
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
	void test_ctrl_snap_endpoint();
	void test_double_click_label();
	void test_segment_drag_vertex();
	void test_remove_vertex();
	void test_default_name_unique();
	void test_reparent_simple();
	void test_retarget_id();
	void test_move_grows_parent();
	void test_batch_action_guard();
	void test_grow_skips_rectless_sm();
	void test_grow_cascades_to_ancestors();
	void test_grow_pushes_siblings();
	void test_reparent_into_descendant();
	void test_choice_edge_rule();
	void test_choice_tip_attach();
	void test_nested_state_grows_parent();
	void test_creation_tools();
	void test_comment_name();
	void test_transition_from_initial();
	void test_transition_boxes_tool();
	void test_label_move();
	void test_label_drag_tracks();
	void test_single_drag_retargets();
	void test_vertex_border_attach();
	void test_vertex_grows_parent();
	void test_command_sm_has_item();
	void test_frameless_border_persists();
	void test_promoted_composite_region_stable();
	void test_paste_state();
	void test_paste_transition();
	void test_sm_not_pasteable();
	void test_action_multiline();
	void test_name_only_state();
	void test_title_drag_through();
	void test_vertex_name();
	void test_label_rect();
	void test_meta_locked();
	void test_comment_relayout();
	void test_history();
	void test_submachine();
	void test_promote_title();
	void test_empty_action_deletes();
	// runs last: it reloads and modifies the shared document
	void test_directional_grow();

private:
	int countItems(int type);
	static bool onBorder(const QRectF& box, const QPointF& p);
	void deleteTransition(const Cyberiada::ID& source, const Cyberiada::ID& target);
	void mouse(QEvent::Type type, const QPointF& scenePos, Qt::MouseButtons buttons,
			   Qt::KeyboardModifiers mods = Qt::NoModifier);
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

// a left button gesture delivered through a given scene, as a view would
static void sceneMouse(CyberiadaSMEditorScene* sc, QEvent::Type type, const QPointF& scenePos,
					   Qt::MouseButtons buttons, Qt::KeyboardModifiers mods = Qt::NoModifier)
{
	QGraphicsSceneMouseEvent event(type);
	event.setScenePos(scenePos);
	event.setScreenPos(scenePos.toPoint());
	event.setButton(Qt::LeftButton);
	event.setButtons(buttons);
	event.setModifiers(mods);
	QApplication::sendEvent(sc, &event);
}

// a left button gesture delivered through the shared scene
void TestScene::mouse(QEvent::Type type, const QPointF& scenePos, Qt::MouseButtons buttons,
					  Qt::KeyboardModifiers mods)
{
	sceneMouse(scene, type, scenePos, buttons, mods);
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

void TestScene::cleanup()
{
	// the model and scene are shared across tests: drop any mouse grab a gesture
	// left behind so it cannot reach into the next test's events
	if (scene) {
		if (QGraphicsItem* g = scene->mouseGrabberItem()) g->ungrabMouse();
	}
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
	// the prefix may sit on its own line when it doesn't fit (#6); the behaviour is stable
	QCOMPARE(action->getBehavior(), QString("first()"));

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

void TestScene::test_container_resize_clamp()
{
	// the property-editor path (resizeToRect) must not shrink a composite below
	// its content, and its nested states keep their absolute positions
	CyberiadaSMEditorStateItem* comp =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0-0"));
	QVERIFY(comp);
	const Cyberiada::State* el =
		static_cast<const Cyberiada::State*>(model->idToElement("node-0-0"));
	QVERIFY(el->is_composite_state());

	QGraphicsItem* child = scene->getMap().value("node-0-0-1");
	QVERIFY(child);
	QPointF childBefore = child->scenePos();

	// ask for an absurdly small rect at the current centre
	Cyberiada::Rect before = el->get_geometry_rect();
	comp->resizeToRect(Cyberiada::Rect(before.x, before.y, 10, 10));
	Cyberiada::Rect after = el->get_geometry_rect();

	// clamped directionally to contain the content, not the requested 10x10
	QVERIFY(after.width  > 10);
	QVERIFY(after.height > 10);
	// the nested states hold their absolute positions across the clamp
	QCOMPARE(child->scenePos(), childBefore);
	// the child fits inside the clamped border
	QVERIFY(child->sceneBoundingRect().left()   >= comp->sceneBoundingRect().left()   - 0.5);
	QVERIFY(child->sceneBoundingRect().right()  <= comp->sceneBoundingRect().right()  + 0.5);
	QVERIFY(child->sceneBoundingRect().top()    >= comp->sceneBoundingRect().top()    - 0.5);
	QVERIFY(child->sceneBoundingRect().bottom() <= comp->sceneBoundingRect().bottom() + 0.5);
}

// find the state's own title text item (not an action block)
static StateTitle* stateTitle(CyberiadaSMEditorStateItem* s)
{
	for (QGraphicsItem* c : s->childItems()) {
		if (StateTitle* t = dynamic_cast<StateTitle*>(c)) return t;
	}
	return nullptr;
}

void TestScene::test_name_only_state()
{
	// a bare state (only a name) centres the name in the box; adding an action
	// moves it back to the top header
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();

	CyberiadaSMEditorStateItem* st =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0-0-1"));
	QVERIFY(st);
	QVERIFY(st->isNameOnly());

	StateTitle* title = stateTitle(st);
	QVERIFY(title);
	QRectF box = st->rect();
	// the title is vertically centred (its middle near the box middle), not at the top
	qreal titleMidY = title->pos().y() + title->boundingRect().height() / 2.0;
	QVERIFY(std::fabs(titleMidY - box.center().y()) < 2.0);
	QVERIFY(title->pos().y() > box.top() + 2.0);

	// add an entry action -> no longer name-only, the title returns to the top
	QVERIFY(model->newAction(model->elementToIndex(model->idToElement("node-0-0-1")),
							 Cyberiada::actionEntry, QString(), QString(), "foo();"));
	st = dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0-0-1"));
	QVERIFY(st);
	QVERIFY(!st->isNameOnly());
	title = stateTitle(st);
	QVERIFY(title);
	box = st->rect();
	QVERIFY(std::fabs(title->pos().y() - box.top()) < 2.0);
}

void TestScene::test_title_drag_through()
{
	// a bare state's name is centred over the box; a press on it must not be a drag
	// dead zone - it falls through to the state, which drags and grows its parent -
	// while a double click on it still edits the name. Driven through a real view so
	// the grab/hover pipeline matches the GUI.
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();

	// node-0-0-1 is a name-only simple state (centred title) inside composite node-0-0
	CyberiadaSMEditorStateItem* child =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0-0-1"));
	QGraphicsItem* parent = scene->getMap().value("node-0-0");
	QVERIFY(child && parent && child->isNameOnly());

	QGraphicsView view(scene);
	view.resize(1200, 800);
	view.show();
	QVERIFY(QTest::qWaitForWindowExposed(&view));
	view.setTransform(QTransform());
	view.centerOn(child);

	QPointF nameCenter = child->sceneBoundingRect().center();   // the centred name
	QWidget* vp = view.viewport();
	auto post = [&](QEvent::Type t, QPoint p, Qt::MouseButton b, Qt::MouseButtons bs) {
		QMouseEvent me(t, p, vp->mapToGlobal(p), b, bs, Qt::NoModifier);
		QApplication::sendEvent(vp, &me);
	};

	// press on the name -> the STATE grabs the drag (the title fell through)
	post(QEvent::MouseButtonPress, view.mapFromScene(nameCenter), Qt::LeftButton, Qt::LeftButton);
	QCOMPARE(dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene->mouseGrabberItem()),
	         static_cast<CyberiadaSMEditorAbstractItem*>(child));
	for (int i = 1; i <= 6; i++)
		post(QEvent::MouseMove, view.mapFromScene(nameCenter + QPointF(120.0 * i / 6.0, 0)),
		     Qt::NoButton, Qt::LeftButton);
	post(QEvent::MouseButtonRelease, view.mapFromScene(nameCenter + QPointF(120, 0)),
	     Qt::LeftButton, Qt::NoButton);
	// the drag moved the state (the name did not swallow the gesture); the parent
	// grow itself is covered by test_directional_grow / test_move_grows_parent
	QVERIFY(child->sceneBoundingRect().center().x() > nameCenter.x() + 1.0);
	(void)parent;

	// a double click on the name starts editing (focus goes to the title text item)
	QPoint dc = view.mapFromScene(child->sceneBoundingRect().center());
	post(QEvent::MouseButtonPress, dc, Qt::LeftButton, Qt::LeftButton);
	post(QEvent::MouseButtonRelease, dc, Qt::LeftButton, Qt::NoButton);
	QMouseEvent dce(QEvent::MouseButtonDblClick, dc, vp->mapToGlobal(dc),
	                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
	QApplication::sendEvent(vp, &dce);
	QVERIFY(dynamic_cast<QGraphicsTextItem*>(scene->focusItem()) != nullptr);
}

void TestScene::test_vertex_name()
{
	// a point object (initial/final/terminate) shows its name centred under the
	// point when set, and a double click edits it
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();

	// node-0-0-0 is an initial pseudostate
	CyberiadaSMEditorVertexItem* v =
		dynamic_cast<CyberiadaSMEditorVertexItem*>(scene->getMap().value("node-0-0-0"));
	QVERIFY(v);

	auto vertexName = [](CyberiadaSMEditorVertexItem* it) -> QGraphicsTextItem* {
		for (QGraphicsItem* c : it->childItems())
			if (QGraphicsTextItem* t = dynamic_cast<QGraphicsTextItem*>(c)) return t;
		return nullptr;
	};

	// initially unnamed: the label carries no text
	QGraphicsTextItem* label = vertexName(v);
	QVERIFY(label);
	QVERIFY(label->toPlainText().isEmpty());

	// naming it through the model shows the name, centred below the point
	QVERIFY(model->updateTitle(model->elementToIndex(model->idToElement("node-0-0-0")), "Begin"));
	label = vertexName(v);
	QVERIFY(label && label->isVisible());
	QCOMPARE(label->toPlainText(), QString("Begin"));
	QVERIFY(label->pos().y() >= VERTEX_POINT_RADIUS);                 // under the point
	QVERIFY(std::fabs(label->pos().x() + label->boundingRect().width() / 2.0) < 1.0);  // centred

	// a double click on the point starts editing the name (through a real view);
	// use a fresh final with no transition on it, so the point is unoccluded
	Cyberiada::ElementCollection* sm =
		dynamic_cast<Cyberiada::ElementCollection*>(model->indexToElement(model->firstSMIndex()));
	QVERIFY(sm);
	Cyberiada::Element* fin = model->newFinal(sm, Cyberiada::Point(1000, 0));
	QVERIFY(fin);
	CyberiadaSMEditorVertexItem* fv =
		dynamic_cast<CyberiadaSMEditorVertexItem*>(scene->getMap().value(fin->get_id()));
	QVERIFY(fv);

	QGraphicsView view(scene);
	view.resize(800, 600);
	view.show();
	QVERIFY(QTest::qWaitForWindowExposed(&view));
	view.setTransform(QTransform());
	view.centerOn(fv);
	QPoint dc = view.mapFromScene(fv->scenePos());   // the point itself, not the label below
	QWidget* vp = view.viewport();
	auto post = [&](QEvent::Type t, QPoint p, Qt::MouseButton b, Qt::MouseButtons bs) {
		QMouseEvent me(t, p, vp->mapToGlobal(p), b, bs, Qt::NoModifier);
		QApplication::sendEvent(vp, &me);
	};
	post(QEvent::MouseButtonPress, dc, Qt::LeftButton, Qt::LeftButton);
	post(QEvent::MouseButtonRelease, dc, Qt::LeftButton, Qt::NoButton);
	QMouseEvent dce(QEvent::MouseButtonDblClick, dc, vp->mapToGlobal(dc),
	                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
	QApplication::sendEvent(vp, &dce);
	QVERIFY(dynamic_cast<QGraphicsTextItem*>(scene->focusItem()) != nullptr);
}

void TestScene::test_label_rect()
{
	// an edge label is a box: it wraps to the stored width and grows downward; a
	// wider box re-wraps shorter; a point label migrates to a rect on demand
	QVERIFY(model->loadDocument("diagrams/label-geometry.graphml"));
	scene->loadScene();

	const Cyberiada::Transition* t =
		static_cast<const Cyberiada::Transition*>(model->idToElement("n0-n1"));
	QVERIFY(t && t->has_geometry_label_rect());
	QCOMPARE(t->get_label_rect().width, 60.0);

	CyberiadaSMEditorTransitionItem* tr =
		dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value("n0-n1"));
	QVERIFY(tr);
	EditableTextItem* label = nullptr;
	for (QGraphicsItem* c : tr->childItems())
		if ((label = dynamic_cast<EditableTextItem*>(c))) break;
	QVERIFY(label);
	// wraps to the stored 60 px width and grows downward (several lines)
	QCOMPARE(label->textWidth(), 60.0);
	QVERIFY(label->boundingRect().height() > 40.0);

	// widen the box -> the text re-wraps to fewer lines
	Cyberiada::Rect lr = t->get_label_rect();
	QVERIFY(model->updateLabel(model->elementToIndex(model->idToElement("n0-n1")),
							   Cyberiada::Rect(lr.x, lr.y, 300, 27)));
	QCOMPARE(label->textWidth(), 300.0);
	QVERIFY(label->boundingRect().height() < 40.0);

	// a point label migrates to a rect on demand (edit mode); a rect stays a rect
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	const Cyberiada::Transition* e2 =
		static_cast<const Cyberiada::Transition*>(model->idToElement("edge-2"));
	QVERIFY(e2 && e2->has_geometry_label_point());
	scene->migrateLabelsToRect();
	QVERIFY(e2->has_geometry_label_rect());
	QVERIFY(!e2->has_geometry_label_point());
}

void TestScene::test_comment_relayout()
{
	// changing a comment body re-lays-out its auto-sized box (P-26/P-27): the item
	// must not keep the stale size, or save/reopen disagree
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	Cyberiada::ElementCollection* sm =
		dynamic_cast<Cyberiada::ElementCollection*>(model->indexToElement(model->firstSMIndex()));
	QVERIFY(sm);
	// a comment created with no rect keeps an auto-managed box sized to its text
	Cyberiada::Element* c = model->newComment(sm, "short");
	QVERIFY(c && !c->has_geometry());
	QGraphicsItem* item = scene->getMap().value(c->get_id());
	QVERIFY(item);
	qreal h0 = item->boundingRect().height();

	// a taller (multi-line) body must grow the box
	QVERIFY(model->updateCommentBody(model->elementToIndex(c),
									 "line one\nline two\nline three\nline four"));
	QVERIFY(item->boundingRect().height() > h0 + 1.0);
}

void TestScene::test_meta_locked()
{
	// the CGML_META document-meta comment cannot be renamed or its body edited
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	const Cyberiada::Comment* meta = model->rootDocument()->get_meta_element();
	QVERIFY(meta);
	QModelIndex mi = model->elementToIndex(meta);
	QVERIFY(mi.isValid());
	QString name0 = QString(meta->get_name().c_str());
	QString body0 = QString(meta->get_body().c_str());

	QVERIFY(!model->updateTitle(mi, "Renamed"));
	QVERIFY(!model->updateCommentBody(mi, "hacked body"));
	QCOMPARE(QString(meta->get_name().c_str()), name0);
	QCOMPARE(QString(meta->get_body().c_str()), body0);
}

void TestScene::test_directional_grow()
{
	// moving a child past a parent's edge extends only that edge: the opposite edge
	// holds and the other children keep their absolute places (the parent centre
	// shifts and the siblings re-base). A clean inside layout is set up first, since
	// the fixture deliberately places some children outside their parents.
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();

	Cyberiada::ElementCollection* parent =
		dynamic_cast<Cyberiada::ElementCollection*>(model->idToElement("node-0"));
	Cyberiada::Element* child   = model->idToElement("node-0-0");
	Cyberiada::Element* sibling = model->idToElement("node-0-1");
	QVERIFY(parent && child && sibling);
	QCOMPARE(child->get_parent(), (Cyberiada::Element*)parent);
	QCOMPARE(sibling->get_parent(), (Cyberiada::Element*)parent);

	// a clean layout: both children well inside a 400x400 parent
	QVERIFY(model->updateGeometry(model->elementToIndex(parent), Cyberiada::Rect(0, 0, 400, 400)));
	QVERIFY(model->updateGeometry(model->elementToIndex(child),  Cyberiada::Rect(-100, 0, 100, 100)));
	QVERIFY(model->updateGeometry(model->elementToIndex(sibling), Cyberiada::Rect(100, 0, 100, 100)));

	Cyberiada::Rect p0 = parent->get_geometry_rect();
	Cyberiada::Rect s0 = static_cast<const Cyberiada::ElementCollection*>(sibling)->get_geometry_rect();
	double leftBefore  = p0.x - p0.width / 2.0;              // parent edges, absolute (parent frame is G)
	double rightBefore = p0.x + p0.width / 2.0;
	double sibAbsX0    = p0.x + s0.x;                        // sibling centre, absolute

	// push the child out past the right edge, then grow directionally
	QVERIFY(model->updateGeometry(model->elementToIndex(child), Cyberiada::Rect(250, 0, 100, 100)));
	double childAbsX0 = p0.x + 250.0;                        // the child's absolute centre after the move
	model->growToFitChildren(child, true);

	Cyberiada::Rect p1 = parent->get_geometry_rect();
	Cyberiada::Rect s1 = static_cast<const Cyberiada::ElementCollection*>(sibling)->get_geometry_rect();
	Cyberiada::Rect c1 = static_cast<const Cyberiada::ElementCollection*>(child)->get_geometry_rect();

	// the left edge held, only the right edge extended
	QVERIFY(std::fabs((p1.x - p1.width / 2.0) - leftBefore) < 0.5);
	QVERIFY(p1.x + p1.width / 2.0 > rightBefore + 0.5);
	// the sibling and the moved child kept their absolute places
	QVERIFY(std::fabs((p1.x + s1.x) - sibAbsX0) < 0.5);
	QVERIFY(std::fabs((p1.x + c1.x) - childAbsX0) < 0.5);
	// the child is now contained on the right
	QVERIFY(c1.x + c1.width / 2.0 <= p1.width / 2.0 + 0.5);
	// the move did not reparent it
	QCOMPARE(child->get_parent(), (Cyberiada::Element*)parent);
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
	QVERIFY(model->updateParent(model->elementToIndex(model->idToElement("node-0-1")), "node-0-0"));
	QGraphicsItem* item = scene->getMap().value("node-0-1");
	QVERIFY(item);
	// the item was rebuilt for the copied element
	QCOMPARE(dynamic_cast<CyberiadaSMEditorAbstractItem*>(item)->getElement(),
			 model->idToElement("node-0-1"));
	QVERIFY(item->parentItem());
	QCOMPARE(item->parentItem()->parentItem(), scene->getMap().value("node-0-0"));
	// the child keeps its size and the new parent grew to contain it: its box,
	// relative to the new parent's centre, lies within the parent's rect
	Cyberiada::Rect after = static_cast<const Cyberiada::State*>(
		model->idToElement("node-0-1"))->get_geometry_rect();
	Cyberiada::Rect parent = static_cast<const Cyberiada::State*>(
		model->idToElement("node-0-0"))->get_geometry_rect();
	QCOMPARE(after.width, before.width);
	QVERIFY(after.x - after.width / 2.0 >= -parent.width / 2.0 - 2.0);
	QVERIFY(after.x + after.width / 2.0 <= parent.width / 2.0 + 2.0);
	QVERIFY(after.y - after.height / 2.0 >= -parent.height / 2.0 - 2.0);
	QVERIFY(after.y + after.height / 2.0 <= parent.height / 2.0 + 2.0);
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
	// a top-level element dragged toward the edge grows the machine to make
	// room; the element stays inside the (grown) border
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);
	QModelIndex smi = model->firstSMIndex();
	Cyberiada::ElementCollection* smc =
		static_cast<Cyberiada::ElementCollection*>(model->indexToElement(smi));
	QVERIFY(model->updateGeometry(smi, Cyberiada::Rect(0, 0, 1600, 1200)));
	double borderW0 = smc->get_geometry_rect().width;

	Cyberiada::State* freeState = model->newState(smc, "Free", Cyberiada::Action(),
												  Cyberiada::Rect(600, 450, 100, 60));
	QVERIFY(freeState);
	QGraphicsItem* fi = scene->getMap().value(freeState->get_id());
	QVERIFY(fi);

	QPointF c = fi->sceneBoundingRect().center();
	scene->clearSelection();
	fi->setSelected(true);
	mouse(QEvent::GraphicsSceneMousePress, c, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, c + QPointF(2000, 1500), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, c + QPointF(2000, 1500), Qt::NoButton);

	// the border grew, and the element sits inside the grown border
	double borderW1 = smc->get_geometry_rect().width;
	QVERIFY(borderW1 > borderW0);
	QGraphicsItem* smItem = scene->getMap().value(smc->get_id());
	QRectF border = smItem->sceneBoundingRect();
	QRectF moved = fi->sceneBoundingRect();
	QVERIFY(moved.left() >= border.left() - 1.0 && moved.right() <= border.right() + 1.0);
	QVERIFY(moved.top() >= border.top() - 1.0 && moved.bottom() <= border.bottom() + 1.0);
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

void TestScene::test_ctrl_snap_endpoint()
{
	// with Ctrl held, a dragged endpoint locks to the adjacent point on one
	// axis, so its segment stays strictly horizontal or vertical
	QVERIFY(model->loadDocument("diagrams/polyline.graphml"));
	scene->loadScene();
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);

	CyberiadaSMEditorTransitionItem* tr =
		dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value("t0"));
	QVERIFY(tr);
	const Cyberiada::Transition* t =
		static_cast<const Cyberiada::Transition*>(model->idToElement("t0"));
	QVERIFY(t->has_polyline());
	int lastDot = int(t->get_geometry_polyline().size()) + 1;
	const Cyberiada::Point& back = t->get_geometry_polyline().back();
	QPointF adjacent = QPointF(back.x, back.y) + tr->sourceCenter();

	scene->clearSelection();
	tr->setSelected(true);
	DotSignal* dot = tr->getDot(lastDot);
	QVERIFY(dot);
	dot->setVisible(true);
	QPointF on = dot->scenePos();
	QVERIFY(scene->itemAt(on, QTransform()) == dot);

	// (700, 450) is free space below both states; the y delta to the adjacent
	// point (150) is the smaller, so Ctrl snaps y and the last segment is level
	QPointF freeTarget(700, 450);
	mouse(QEvent::GraphicsSceneMousePress, on, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, freeTarget, Qt::LeftButton, Qt::ControlModifier);
	QPainterPath pp = tr->path();
	QPointF end = pp.elementAt(pp.elementCount() - 1);
	QVERIFY(qAbs(end.y() - adjacent.y()) < 0.5);
	QCOMPARE(end.x(), freeTarget.x());
	mouse(QEvent::GraphicsSceneMouseRelease, freeTarget, Qt::NoButton);

	// the endpoint tracked, nothing was stored: a fresh scene has it back
	scene->loadScene();
	tr = dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value("t0"));
	QVERIFY(tr);
	scene->clearSelection();
	tr->setSelected(true);

	// without Ctrl the same drag does not snap
	dot = tr->getDot(lastDot);
	QVERIFY(dot);
	dot->setVisible(true);
	on = dot->scenePos();
	mouse(QEvent::GraphicsSceneMousePress, on, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, freeTarget, Qt::LeftButton);
	pp = tr->path();
	end = pp.elementAt(pp.elementCount() - 1);
	QVERIFY(qAbs(end.y() - adjacent.y()) > 0.5);
	mouse(QEvent::GraphicsSceneMouseRelease, freeTarget, Qt::NoButton);
}

void TestScene::test_double_click_label()
{
	// a double click on the body edits the label, it does not add a vertex
	QVERIFY(model->loadDocument("diagrams/polyline.graphml"));
	scene->loadScene();
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);

	CyberiadaSMEditorTransitionItem* tr =
		dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value("t0"));
	QVERIFY(tr);
	const Cyberiada::Transition* t =
		static_cast<const Cyberiada::Transition*>(model->idToElement("t0"));
	size_t points = t->get_geometry_polyline().size();

	// the middle segment of t0 runs level at (100..500, 300) in scene space
	mouse(QEvent::GraphicsSceneMouseDoubleClick, QPointF(300, 300), Qt::LeftButton);

	EditableTextItem* label = nullptr;
	for (QGraphicsItem* child : tr->childItems()) {
		if ((label = dynamic_cast<EditableTextItem*>(child))) break;
	}
	QVERIFY(label);
	QVERIFY(label->hasFocus());
	QVERIFY(label->textInteractionFlags() & Qt::TextEditorInteraction);
	QCOMPARE(t->get_geometry_polyline().size(), points);

	// typing a trigger and committing on the focus out persists the label
	label->setPlainText("EV / act()");
	label->clearFocus();
	QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	QVERIFY(t->has_action());
	QCOMPARE(QString(t->get_action().get_trigger().c_str()), QString("EV"));
	QCOMPARE(QString(t->get_action().get_behavior().c_str()), QString("act()"));
}

void TestScene::test_segment_drag_vertex()
{
	// dragging a segment inserts a polyline vertex and drags it further
	QVERIFY(model->loadDocument("diagrams/polyline.graphml"));
	scene->loadScene();
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);

	CyberiadaSMEditorTransitionItem* tr =
		dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value("t0"));
	QVERIFY(tr);
	const Cyberiada::Transition* t =
		static_cast<const Cyberiada::Transition*>(model->idToElement("t0"));
	size_t before = t->get_geometry_polyline().size();

	// press the level middle segment, then drag off it: one vertex is added
	scene->clearSelection();
	tr->setSelected(true);
	mouse(QEvent::GraphicsSceneMousePress, QPointF(300, 300), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, QPointF(300, 360), Qt::LeftButton);
	QCOMPARE(t->get_geometry_polyline().size(), before + 1);

	// the new dot took the drag: a further move repositions that vertex
	mouse(QEvent::GraphicsSceneMouseMove, QPointF(300, 400), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, QPointF(300, 400), Qt::NoButton);
	QCOMPARE(t->get_geometry_polyline().size(), before + 1);
	const Cyberiada::Point& inserted = t->get_geometry_polyline().at(1);
	QPointF scenePt = QPointF(inserted.x, inserted.y) + tr->sourceCenter();
	QVERIFY(qAbs(scenePt.y() - 400.0) < 1.0);
}

void TestScene::test_remove_vertex()
{
	// deleting an interior dot removes its polyline vertex; endpoints are kept
	QVERIFY(model->loadDocument("diagrams/polyline.graphml"));
	scene->loadScene();

	CyberiadaSMEditorTransitionItem* tr =
		dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value("t0"));
	QVERIFY(tr);
	const Cyberiada::Transition* t =
		static_cast<const Cyberiada::Transition*>(model->idToElement("t0"));
	size_t before = t->get_geometry_polyline().size();
	QVERIFY(before >= 1);

	DotSignal* vertex = tr->getDot(1);
	QVERIFY(vertex);
	vertex->deleteDot();
	QCOMPARE(t->get_geometry_polyline().size(), before - 1);

	// an endpoint dot has no vertex to remove
	size_t mid = t->get_geometry_polyline().size();
	tr->getDot(0)->deleteDot();
	QCOMPARE(t->get_geometry_polyline().size(), mid);
}

void TestScene::test_default_name_unique()
{
	// several states added at the top level get distinct names
	QVERIFY(model->loadDocument("diagrams/polyline.graphml"));
	scene->loadScene();
	CyberiadaSMEditorAbstractItem* smItem =
		dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene->getMap().value("G0"));
	QVERIFY(smItem);
	const Cyberiada::ElementCollection* sm =
		static_cast<const Cyberiada::ElementCollection*>(smItem->getElement());

	int before = countItems(CyberiadaSMEditorAbstractItem::StateItem);
	scene->clearSelection();
	scene->addSMItem(Cyberiada::elementSimpleState);
	scene->clearSelection();
	scene->addSMItem(Cyberiada::elementSimpleState);
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::StateItem), before + 2);

	// no two sibling states of the machine share a name
	QStringList names;
	Cyberiada::ConstElementList kids = sm->get_children();
	for (Cyberiada::ConstElementList::const_iterator i = kids.begin(); i != kids.end(); i++) {
		if ((*i)->get_type() != Cyberiada::elementSimpleState &&
			(*i)->get_type() != Cyberiada::elementCompositeState) continue;
		QString n = QString::fromStdString((*i)->get_name());
		QVERIFY2(!names.contains(n), qPrintable("duplicate state name: " + n));
		names.append(n);
	}
}

void TestScene::test_reparent_simple()
{
	// dragging a state into a simple state (making it composite) keeps the
	// dragged item visible, nested in the new region - not lost until reload
	QVERIFY(model->loadDocument("diagrams/polyline.graphml"));
	scene->loadScene();
	QVERIFY(scene->getMap().value("n0"));
	QVERIFY(scene->getMap().value("n1"));
	QVERIFY(!static_cast<const Cyberiada::State*>(model->idToElement("n0"))->is_composite_state());

	QVERIFY(model->updateParent(model->elementToIndex(model->idToElement("n1")), "n0"));

	// the dragged item survived (it is deleted then re-added under the region)
	QGraphicsItem* child = scene->getMap().value("n1");
	QVERIFY(child);
	QVERIFY(child->isVisible());
	QVERIFY(child->parentItem());
	// the child lives in n0's region, whose parent item is the n0 state
	QCOMPARE(child->parentItem()->parentItem(), scene->getMap().value("n0"));
	QVERIFY(static_cast<const Cyberiada::State*>(model->idToElement("n0"))->is_composite_state());
}

void TestScene::test_retarget_id()
{
	// a transition drawn as a self-loop, then retargeted, is renamed to the
	// endpoint convention source-target (P-4)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	Cyberiada::StateMachine* sm = dynamic_cast<Cyberiada::StateMachine*>(model->idToElement("G"));
	Cyberiada::Element* src = model->idToElement("node-0-0-1");
	Cyberiada::Element* tgt = model->idToElement("node-0-0-2");
	QVERIFY(sm && src && tgt);

	Cyberiada::Element* t = model->newTransition(sm, Cyberiada::transitionExternal, src, src,
												 Cyberiada::Action(Cyberiada::actionTransition));
	QVERIFY(t);
	QCOMPARE(QString::fromStdString(t->get_id()), QString("node-0-0-1-node-0-0-1"));

	QVERIFY(model->updateGeometry(model->elementToIndex(t),
								  Cyberiada::ID("node-0-0-1"), Cyberiada::ID("node-0-0-2")));
	QCOMPARE(QString::fromStdString(t->get_id()), QString("node-0-0-1-node-0-0-2"));
	QCOMPARE(model->idToElement("node-0-0-1-node-0-0-2"), t);
	QVERIFY(model->idToElement("node-0-0-1-node-0-0-1") == NULL);
}

void TestScene::test_move_grows_parent()
{
	// a programmatic move that puts a child past its parent grows the parent
	// so the child stays inside, as a drag does (P-7)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	Cyberiada::Element* child = model->idToElement("node-0-0-1");
	QVERIFY(child);
	Cyberiada::ElementCollection* parent =
		dynamic_cast<Cyberiada::ElementCollection*>(child->get_parent());
	QVERIFY(parent);

	// move the child well below the parent's lower edge, then grow
	QVERIFY(model->updateGeometry(model->elementToIndex(child),
								  Cyberiada::Rect(100, 150, 320, 180)));
	model->growToFitChildren(child);

	Cyberiada::Rect pr = parent->get_geometry_rect();
	Cyberiada::Rect cr = static_cast<const Cyberiada::ElementCollection*>(child)->get_geometry_rect();
	// the child (centre-relative) is contained in the grown parent
	QVERIFY(std::fabs(cr.x) + cr.width / 2.0 <= pr.width / 2.0 + 0.01);
	QVERIFY(std::fabs(cr.y) + cr.height / 2.0 <= pr.height / 2.0 + 0.01);
}

void TestScene::test_batch_action_guard()
{
	// in batch mode a double click must not open the modal action dialog
	// (it would hang with no user); it adds no action (P-5)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);
	CyberiadaSMEditorStateItem* state =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0-0-2"));
	QVERIFY(state);
	QCOMPARE(state->missingActionType(), int(Cyberiada::actionEntry));

	qApp->setProperty("batchMode", true);
	scene->clearSelection();
	state->setSelected(true);
	QPointF centre = state->sceneBoundingRect().center();
	// the guarded double click returns at once; without the guard exec() hangs
	mouse(QEvent::GraphicsSceneMouseDoubleClick, centre, Qt::LeftButton);
	qApp->setProperty("batchMode", QVariant());

	QCOMPARE(state->missingActionType(), int(Cyberiada::actionEntry));
}

void TestScene::test_grow_skips_rectless_sm()
{
	// a rect-less state machine must not be given a stored rect: growing it
	// would read its uninitialised geometry and persist garbage (P-9, P-14)
	QVERIFY(model->loadDocument("diagrams/polyline.graphml"));
	scene->loadScene();
	Cyberiada::Element* sm = model->idToElement("G0");
	Cyberiada::Element* n0 = model->idToElement("n0");   // a top-level state, child of G0
	QVERIFY(sm && n0);
	QVERIFY(!sm->has_geometry());                        // the SM starts rect-less

	QVERIFY(model->updateGeometry(model->elementToIndex(n0), Cyberiada::Rect(0, 0, 900, 700)));
	model->growToFitChildren(n0);

	// the SM was skipped: still rect-less, no garbage rect written
	QVERIFY(!sm->has_geometry());
}

void TestScene::test_grow_cascades_to_ancestors()
{
	// growing a nested child grows its parent AND the grandparent, so a
	// composite cannot be left outside its own parent (P-12)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	Cyberiada::Element* child = model->idToElement("node-0-0-1");
	QVERIFY(child);
	Cyberiada::ElementCollection* parent =
		dynamic_cast<Cyberiada::ElementCollection*>(child->get_parent());          // node-0-0
	Cyberiada::ElementCollection* grand =
		parent ? dynamic_cast<Cyberiada::ElementCollection*>(parent->get_parent()) : NULL;  // node-0
	QVERIFY(parent && grand);
	double gw0 = grand->get_geometry_rect().width;

	QVERIFY(model->updateGeometry(model->elementToIndex(child),
								  Cyberiada::Rect(-230, -40, 260, 130)));
	model->growToFitChildren(child);

	// the parent contains the child, and the grandparent grew to contain the parent
	Cyberiada::Rect pr = parent->get_geometry_rect();
	Cyberiada::Rect cr = static_cast<const Cyberiada::ElementCollection*>(child)->get_geometry_rect();
	QVERIFY(std::fabs(cr.x) + cr.width / 2.0 <= pr.width / 2.0 + 0.01);
	Cyberiada::Rect gr = grand->get_geometry_rect();
	QVERIFY(std::fabs(pr.x) + pr.width / 2.0 <= gr.width / 2.0 + 0.01);
	QVERIFY(gr.width > gw0);                              // the grandparent actually grew
	QVERIFY(!model->idToElement("G")->has_geometry());   // the rect-less SM stays rect-less
}

void TestScene::test_grow_pushes_siblings()
{
	// a container that grows to fit its content must push its siblings aside
	// instead of overlapping them (NODE-6)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	// node-0-0 and node-0-1 are siblings inside node-0
	Cyberiada::ElementCollection* outer =
		dynamic_cast<Cyberiada::ElementCollection*>(model->idToElement("node-0-0"));
	Cyberiada::ElementCollection* sibling =
		dynamic_cast<Cyberiada::ElementCollection*>(model->idToElement("node-0-1"));
	Cyberiada::ElementCollection* grand =
		dynamic_cast<Cyberiada::ElementCollection*>(model->idToElement("node-0"));
	QVERIFY(outer && sibling && grand);
	double siblingX0 = sibling->get_geometry_rect().x;
	// push a child of the outer far to the right so the outer must grow rightward,
	// past its right sibling
	Cyberiada::Element* child = model->idToElement("node-0-0-2");
	QVERIFY(child);
	QVERIFY(model->updateGeometry(model->elementToIndex(child),
								  Cyberiada::Rect(500, 25, 150, 150)));
	model->growToFitChildren(child);

	Cyberiada::Rect outerR = outer->get_geometry_rect();
	Cyberiada::Rect sibR = sibling->get_geometry_rect();
	// the sibling was pushed to the right and no longer overlaps the grown outer
	QVERIFY(sibR.x > siblingX0);
	QVERIFY(outerR.x + outerR.width / 2.0 <= sibR.x - sibR.width / 2.0 + 0.5);
	// the grandparent grew to still contain the pushed sibling
	Cyberiada::Rect gr = grand->get_geometry_rect();
	QVERIFY(std::fabs(sibR.x) + sibR.width / 2.0 <= gr.width / 2.0 + 0.5);
}

void TestScene::test_reparent_into_descendant()
{
	// reparenting an element into its own descendant would free the target
	// subtree mid-move (use-after-free): it must be refused (P1)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	Cyberiada::Element* outer = model->idToElement("node-0");
	Cyberiada::Element* inner = model->idToElement("node-0-0-1");   // a descendant of node-0
	QVERIFY(outer && inner);
	const Cyberiada::Element* oldParent = outer->get_parent();
	QVERIFY(!model->updateParent(model->elementToIndex(outer), "node-0-0-1"));
	QCOMPARE(outer->get_parent(), oldParent);                       // unchanged, no crash
	// a normal reparent (into an unrelated composite) still works
	QVERIFY(model->updateParent(model->elementToIndex(model->idToElement("node-0-1")), "node-0-0"));
}

void TestScene::test_choice_edge_rule()
{
	// a choice's outgoing transition carries only a guard or a single 'else' (P2)
	QVERIFY(model->loadDocument("diagrams/choice.graphml"));
	scene->loadScene();
	Cyberiada::StateMachine* sm = dynamic_cast<Cyberiada::StateMachine*>(model->idToElement("G0"));
	Cyberiada::Element* choice = model->idToElement("n0");
	QVERIFY(sm && choice && choice->get_type() == Cyberiada::elementChoice);
	Cyberiada::State* a = model->newState(sm, std::string("A"), Cyberiada::Action(), Cyberiada::Rect(-100, 200, 100, 60));
	Cyberiada::State* b = model->newState(sm, std::string("B"), Cyberiada::Action(), Cyberiada::Rect(100, 200, 100, 60));
	QVERIFY(a && b);
	Cyberiada::Element* t1 = model->newTransition(sm, Cyberiada::transitionExternal, choice, a,
												  Cyberiada::Action(Cyberiada::actionTransition));
	Cyberiada::Element* t2 = model->newTransition(sm, Cyberiada::transitionExternal, choice, b,
												  Cyberiada::Action(Cyberiada::actionTransition));
	QVERIFY(t1 && t2);
	QModelIndex i1 = model->elementToIndex(t1);
	QModelIndex i2 = model->elementToIndex(t2);
	// a trigger or a behaviour on a choice edge is rejected
	QVERIFY(!model->updateAction(i1, 0, "EV", "", ""));
	QVERIFY(!model->updateAction(i1, 0, "", "", "act()"));
	// a guard is accepted
	QVERIFY(model->updateAction(i1, 0, "", "x > 0", ""));
	QCOMPARE(QString(static_cast<const Cyberiada::Transition*>(t1)->get_action().get_guard().c_str()),
			 QString("x > 0"));
	// one 'else' is accepted, a second is rejected
	QVERIFY(model->updateAction(i2, 0, "", "else", ""));
	QVERIFY(!model->updateAction(i1, 0, "", "else", ""));
}

void TestScene::test_choice_tip_attach()
{
	// incoming/outgoing transitions bind to the rhombus tips (the rect-edge
	// midpoints), whatever the choice rect proportions are
	QVERIFY(model->loadDocument("diagrams/choice.graphml"));
	scene->loadScene();
	Cyberiada::StateMachine* sm = dynamic_cast<Cyberiada::StateMachine*>(model->idToElement("G0"));
	Cyberiada::Element* choice = model->idToElement("n0");
	QVERIFY(sm && choice && choice->get_type() == Cyberiada::elementChoice);

	// a definite square rect: the tips are (0,-20),(20,0),(0,20),(-20,0)
	QVERIFY(model->updateGeometry(model->elementToIndex(choice), Cyberiada::Rect(0, 0, 40, 40)));
	Cyberiada::State* right = model->newState(sm, std::string("R"), Cyberiada::Action(), Cyberiada::Rect(300, 0, 120, 80));
	Cyberiada::State* top = model->newState(sm, std::string("T"), Cyberiada::Action(), Cyberiada::Rect(0, -300, 120, 80));
	QVERIFY(right && top);
	Cyberiada::Element* out = model->newTransition(sm, Cyberiada::transitionExternal, choice, right,
												   Cyberiada::Action(Cyberiada::actionTransition));
	Cyberiada::Element* in = model->newTransition(sm, Cyberiada::transitionExternal, top, choice,
												  Cyberiada::Action(Cyberiada::actionTransition));
	QVERIFY(out && in);

	CyberiadaSMEditorTransitionItem* outItem =
		dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value(out->get_id()));
	CyberiadaSMEditorTransitionItem* inItem =
		dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value(in->get_id()));
	QVERIFY(outItem && inItem);

	// the choice-side endpoint (offset from the choice centre) is the facing tip
	QPointF sp = outItem->sourcePoint();          // choice is the source, neighbour to the right
	QVERIFY(qAbs(sp.x() - 20.0) < 0.5 && qAbs(sp.y()) < 0.5);
	QPointF tp = inItem->targetPoint();           // choice is the target, neighbour above
	QVERIFY(qAbs(tp.x()) < 0.5 && qAbs(tp.y() + 20.0) < 0.5);

	// a non-square choice binds to its own edge midpoints, not the default size
	QVERIFY(model->updateGeometry(model->elementToIndex(choice), Cyberiada::Rect(0, 0, 60, 20)));
	sp = outItem->sourcePoint();
	QVERIFY(qAbs(sp.x() - 30.0) < 0.5 && qAbs(sp.y()) < 0.5);   // right tip at half-width
	tp = inItem->targetPoint();
	QVERIFY(qAbs(tp.x()) < 0.5 && qAbs(tp.y() + 10.0) < 0.5);   // top tip at half-height
}

void TestScene::test_nested_state_grows_parent()
{
	// adding a state inside a composite grows the composite to contain it (#7)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	Cyberiada::ElementCollection* comp =
		dynamic_cast<Cyberiada::ElementCollection*>(model->idToElement("node-0-0"));
	QVERIFY(comp && comp->has_geometry());
	QGraphicsItem* compItem = scene->getMap().value("node-0-0");
	QVERIFY(compItem);
	scene->clearSelection();
	compItem->setSelected(true);

	scene->addSMItem(Cyberiada::elementSimpleState);

	// the parent contains every rect child, the newly added one included
	Cyberiada::Rect pr = comp->get_geometry_rect();
	bool foundNew = false;
	const Cyberiada::ElementList& kids = comp->get_children();
	for (Cyberiada::ElementList::const_iterator i = kids.begin(); i != kids.end(); i++) {
		Cyberiada::ElementCollection* c = dynamic_cast<Cyberiada::ElementCollection*>(*i);
		if (!c || !c->has_geometry()) continue;
		Cyberiada::Rect cr = c->get_geometry_rect();
		QVERIFY(std::fabs(cr.x) + cr.width / 2.0 <= pr.width / 2.0 + 0.01);
		QVERIFY(std::fabs(cr.y) + cr.height / 2.0 <= pr.height / 2.0 + 0.01);
		if (QString(c->get_name().c_str()) == "New state") foundNew = true;
	}
	QVERIFY(foundNew);
}

// count the state children of a collection
static int countStates(const Cyberiada::ElementCollection* c)
{
	int n = 0;
	Cyberiada::ConstElementList kids = c->get_children();
	for (Cyberiada::ConstElementList::const_iterator i = kids.begin(); i != kids.end(); i++) {
		if ((*i)->get_type() == Cyberiada::elementSimpleState ||
			(*i)->get_type() == Cyberiada::elementCompositeState) n++;
	}
	return n;
}

void TestScene::test_creation_tools()
{
	// the creation tools draw/place elements and revert to Select afterwards
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	Cyberiada::ElementCollection* sm =
		dynamic_cast<Cyberiada::ElementCollection*>(model->indexToElement(model->firstSMIndex()));
	QVERIFY(sm && !sm->has_geometry());

	// the SM tool over a border-less machine adopts it (gives it a border)
	QGraphicsItem* smItem = scene->getMap().value(sm->get_id());
	QVERIFY(smItem);
	QPointF centre = smItem->sceneBoundingRect().center();
	int smsBefore = int(model->rootDocument()->get_state_machines().size());
	scene->setCurrentTool(ToolType::NewSM);
	mouse(QEvent::GraphicsSceneMousePress,   centre + QPointF(-60, -60), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove,    centre + QPointF(260, 200), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, centre + QPointF(260, 200), Qt::NoButton);
	QVERIFY(sm->has_geometry());
	QCOMPARE(int(model->rootDocument()->get_state_machines().size()), smsBefore);
	QCOMPARE(int(scene->getCurrentTool()), int(ToolType::Select));   // one-shot

	// the state tool draws a state inside the machine
	int statesBefore = countStates(sm);
	QRectF smRect = scene->getMap().value(sm->get_id())->sceneBoundingRect();
	scene->setCurrentTool(ToolType::NewState);
	mouse(QEvent::GraphicsSceneMousePress,   smRect.center() + QPointF(-100, -50), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove,    smRect.center() + QPointF(100, 50), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, smRect.center() + QPointF(100, 50), Qt::NoButton);
	QCOMPARE(countStates(sm), statesBefore + 1);
	QCOMPARE(int(scene->getCurrentTool()), int(ToolType::Select));

	// a click-placement tool drops an element at the point
	int initialsBefore = int(sm->find_elements_by_type(Cyberiada::elementInitial).size());
	scene->setCurrentTool(ToolType::NewInitial);
	QPointF p = scene->getMap().value(sm->get_id())->sceneBoundingRect().center();
	mouse(QEvent::GraphicsSceneMousePress,   p, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, p, Qt::NoButton);
	QCOMPARE(int(sm->find_elements_by_type(Cyberiada::elementInitial).size()), initialsBefore + 1);
	QCOMPARE(int(scene->getCurrentTool()), int(ToolType::Select));
}

void TestScene::test_comment_name()
{
	// a comment name is shown as a bold title at the top; empty means no title
	QVERIFY(model->loadDocument("diagrams/comment-transition.graphml"));
	scene->loadScene();
	Cyberiada::Element* c = model->idToElement("n2");
	QVERIFY(c && c->get_type() == Cyberiada::elementComment);
	QGraphicsItem* item = scene->getMap().value(c->get_id());
	QVERIFY(item);

	QVERIFY(model->updateTitle(model->elementToIndex(c), "Note"));
	bool found = false;
	std::vector<EditableTextItem*> texts = textItemsOf(item);
	for (size_t i = 0; i < texts.size(); i++) {
		if (texts[i]->getFontRole() == fontRoleStateTitle) {
			QCOMPARE(texts[i]->toPlainText(), QString("Note"));
			QVERIFY(texts[i]->isVisible());
			QVERIFY(texts[i]->font().bold());
			found = true;
		}
	}
	QVERIFY(found);

	// clearing the name hides the title again
	QVERIFY(model->updateTitle(model->elementToIndex(c), ""));
	texts = textItemsOf(item);
	for (size_t i = 0; i < texts.size(); i++) {
		if (texts[i]->getFontRole() == fontRoleStateTitle) QVERIFY(!texts[i]->isVisible());
	}
}

static const Cyberiada::Transition* findTransition(CyberiadaSMModel* model,
												   const Cyberiada::ID& src, const Cyberiada::ID& tgt)
{
	std::vector<Cyberiada::StateMachine*> sms = model->rootDocument()->get_state_machines();
	for (size_t i = 0; i < sms.size(); i++) {
		std::vector<Cyberiada::Transition*> trs = sms[i]->get_transitions();
		for (size_t j = 0; j < trs.size(); j++) {
			if (trs[j]->source_element_id() == src && trs[j]->target_element_id() == tgt)
				return trs[j];
		}
	}
	return nullptr;
}

static int transitionsFrom(CyberiadaSMModel* model, const Cyberiada::ID& src)
{
	int n = 0;
	std::vector<Cyberiada::StateMachine*> sms = model->rootDocument()->get_state_machines();
	for (size_t i = 0; i < sms.size(); i++) {
		std::vector<Cyberiada::Transition*> trs = sms[i]->get_transitions();
		for (size_t j = 0; j < trs.size(); j++) {
			if (trs[j]->source_element_id() == src) n++;
		}
	}
	return n;
}

void TestScene::test_transition_from_initial()
{
	// a transition can be drawn from an initial pseudostate by a body drag under
	// the transition tool (previously only states could begin one)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	QGraphicsItem* initItem = scene->getMap().value("node-0-0-0");   // an initial vertex
	QGraphicsItem* stateItem = scene->getMap().value("node-0-0-1");  // a sibling state
	QVERIFY(initItem && stateItem);
	QPointF from = initItem->sceneBoundingRect().center();
	QPointF to = stateItem->sceneBoundingRect().center();

	int before = transitionsFrom(model, "node-0-0-0");
	scene->setCurrentTool(ToolType::Transition);
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);
	mouse(QEvent::GraphicsSceneMousePress, from, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, (from + to) / 2, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, to, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, to, Qt::NoButton);

	QCOMPARE(transitionsFrom(model, "node-0-0-0"), before + 1);
	// the one-shot tool returned to select
	QCOMPARE(int(scene->getCurrentTool()), int(ToolType::Select));
}

void TestScene::test_transition_boxes_tool()
{
	// the source boxes show on a selected source item only under the transition
	// tool; a count of visible DotSignal children reflects it
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	QGraphicsItem* stateItem = scene->getMap().value("node-0-0-1");
	QVERIFY(stateItem);
	scene->clearSelection();
	stateItem->setSelected(true);

	scene->setCurrentTool(ToolType::Select);
	int underSelect = 0;
	for (QGraphicsItem* c : stateItem->childItems())
		if (dynamic_cast<DotSignal*>(c) && c->isVisible()) underSelect++;
	QCOMPARE(underSelect, 0);   // no source boxes under the select tool

	scene->setCurrentTool(ToolType::Transition);
	int underTransition = 0;
	for (QGraphicsItem* c : stateItem->childItems())
		if (dynamic_cast<DotSignal*>(c) && c->isVisible()) underTransition++;
	QCOMPARE(underTransition, 8);   // a state shows all 8 source boxes
}

void TestScene::test_label_move()
{
	// a transition label position persists and resets (P3)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	CyberiadaSMEditorTransitionItem* tr =
		dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value("edge-0"));
	const Cyberiada::Transition* t =
		static_cast<const Cyberiada::Transition*>(model->idToElement("edge-0"));
	QVERIFY(tr && t);
	QModelIndex idx = model->elementToIndex(model->idToElement("edge-0"));

	QVERIFY(model->updateLabel(idx, Cyberiada::Point(37, -21)));
	QVERIFY(t->has_geometry_label_point());
	QCOMPARE(t->get_label_point().x, 37.0f);
	QCOMPARE(t->get_label_point().y, -21.0f);
	tr->updateActionPosition();                     // honours the stored point, no crash

	// an invalid point clears the stored label (auto-placement resumes)
	QVERIFY(model->updateLabel(idx, Cyberiada::Point()));
	QVERIFY(!t->has_geometry_label_point());
}

void TestScene::test_label_drag_tracks()
{
	// a label dragged by the mouse tracks the cursor from where it is shown; a
	// reflow triggered mid-drag must not snap it back to the auto midpoint (#5).
	// A private scene keeps the shared scene's grab/edit state out of the gesture.
	CyberiadaSMModel m(nullptr);
	CyberiadaSMEditorScene s(&m, nullptr);
	QVERIFY(m.loadDocument("diagrams/geometry.graphml"));
	s.loadScene();
	// let the queued font signal settle so the label has its real metrics (an
	// unlaid-out label has a degenerate hit area and unstable event routing)
	QCoreApplication::processEvents();
	// edge-2 carries an action, so its label has a real (stable) hit area
	CyberiadaSMEditorTransitionItem* tr =
		dynamic_cast<CyberiadaSMEditorTransitionItem*>(s.getMap().value("edge-2"));
	QVERIFY(tr);
	TransitionAction* label = nullptr;
	for (QGraphicsItem* c : tr->childItems())
		if ((label = dynamic_cast<TransitionAction*>(c))) break;
	QVERIFY(label);
	label->setVisible(true);
	QVERIFY(!label->toPlainText().isEmpty());

	// grab the label so the gesture routes to it regardless of the (lazily laid
	// out, font-dependent) hit area; measure pos(), which text layout can't shift
	s.setCurrentTool(ToolType::Select);
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(&s, &activate);
	label->grabMouse();
	QPointF anchor = label->scenePos();
	QPointF delta(25, 18);
	// press then a warm-up move: the move anchors dragLast (a late-activating
	// grab can miss the press), so what follows tracks the cursor exactly
	sceneMouse(&s, QEvent::GraphicsSceneMousePress, anchor, Qt::LeftButton);
	sceneMouse(&s, QEvent::GraphicsSceneMouseMove, anchor, Qt::LeftButton);
	QVERIFY2(label->isDragging(), "the label did not receive the drag");
	QPointF startPos = label->pos();
	tr->updateActionPosition();   // a mid-drag reflow must not snap the label back
	sceneMouse(&s, QEvent::GraphicsSceneMouseMove, anchor + delta, Qt::LeftButton);
	sceneMouse(&s, QEvent::GraphicsSceneMouseRelease, anchor + delta, Qt::NoButton);

	QPointF moved = label->pos() - startPos;
	QVERIFY2(QLineF(moved, delta).length() < 3.0, "label did not track the cursor");
}

void TestScene::test_single_drag_retargets()
{
	// a transition drawn with a single drag from a state to another state binds
	// to that state on release, not left as the seeded self-loop (P-22..P-25)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	QGraphicsItem* fromItem = scene->getMap().value("node-0-1");
	QGraphicsItem* toItem = scene->getMap().value("node-0-0-2");
	QVERIFY(fromItem && toItem);
	QPointF from = fromItem->sceneBoundingRect().center();
	QPointF to = toItem->sceneBoundingRect().center();

	scene->setCurrentTool(ToolType::Transition);
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);
	mouse(QEvent::GraphicsSceneMousePress, from, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, to, Qt::LeftButton);   // one effective drag
	mouse(QEvent::GraphicsSceneMouseRelease, to, Qt::NoButton);

	const Cyberiada::Transition* ext = findTransition(model, "node-0-1", "node-0-0-2");
	QVERIFY2(ext, "single drag did not bind the target on release");
	QCOMPARE(int(ext->get_transition_type()), int(Cyberiada::transitionExternal));
	QVERIFY2(!findTransition(model, "node-0-1", "node-0-1"), "a stray self-loop was left");
}

void TestScene::test_vertex_border_attach()
{
	// a transition endpoint on a pseudostate attaches on the circle border, at
	// the vertex radius from its centre, not at the centre (#2)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	QGraphicsItem* fromItem = scene->getMap().value("node-0-0-2");
	QGraphicsItem* initItem = scene->getMap().value("node-0-0-0");   // an initial vertex
	QVERIFY(fromItem && initItem);
	QPointF from = fromItem->sceneBoundingRect().center();
	QPointF to = initItem->sceneBoundingRect().center();

	scene->setCurrentTool(ToolType::Transition);
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);
	mouse(QEvent::GraphicsSceneMousePress, from, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, to, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, to, Qt::NoButton);

	const Cyberiada::Transition* t = findTransition(model, "node-0-0-2", "node-0-0-0");
	QVERIFY2(t, "the transition did not bind to the initial");
	// the endpoint attaches on the circle border at display time (whether or not
	// the point is written back), so measure the item's shown target point
	CyberiadaSMEditorTransitionItem* item =
		dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value(t->get_id()));
	QVERIFY(item);
	QPointF tp = item->targetPoint();
	qreal r = std::hypot(tp.x(), tp.y());
	QVERIFY2(qAbs(r - VERTEX_POINT_RADIUS) < 0.5,
			 qPrintable(QString("target attaches at %1, expected %2").arg(r).arg(VERTEX_POINT_RADIUS)));
}

void TestScene::test_vertex_grows_parent()
{
	// dragging an initial pseudostate to the parent edge extends the parent to
	// keep containing it, as a state child does (#3)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	Cyberiada::ElementCollection* comp =
		dynamic_cast<Cyberiada::ElementCollection*>(model->idToElement("node-0-0"));
	const Cyberiada::Vertex* init =
		dynamic_cast<const Cyberiada::Vertex*>(model->idToElement("node-0-0-0"));
	QGraphicsItem* initItem = scene->getMap().value("node-0-0-0");
	QVERIFY(comp && init && initItem);

	scene->setCurrentTool(ToolType::Select);
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);
	scene->clearSelection();
	initItem->setSelected(true);

	QPointF start = initItem->sceneBoundingRect().center();
	QPointF far = start + QPointF(-400, -300);   // out past the parent's edge
	mouse(QEvent::GraphicsSceneMousePress, start, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, far, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, far, Qt::NoButton);

	Cyberiada::Point vp = init->get_geometry_point();
	Cyberiada::Rect pr = comp->get_geometry_rect();
	QVERIFY2(std::fabs(double(vp.x)) + VERTEX_POINT_RADIUS <= pr.width / 2.0 + 0.5,
			 "parent did not grow to contain the moved vertex (x)");
	QVERIFY2(std::fabs(double(vp.y)) + VERTEX_POINT_RADIUS <= pr.height / 2.0 + 0.5,
			 "parent did not grow to contain the moved vertex (y)");
}

void TestScene::test_command_sm_has_item()
{
	// a state machine added to the document (as the new-sm command does) gets a
	// scene item, not only on the next reopen (P-15/P-16)
	CyberiadaSMModel m(nullptr);
	CyberiadaSMEditorScene s(&m, nullptr);
	QVERIFY(m.loadDocument("diagrams/geometry.graphml"));
	s.loadScene();
	auto countSM = [&]() {
		int n = 0;
		for (auto i = s.getMap().begin(); i != s.getMap().end(); i++)
			if ((*i)->type() == CyberiadaSMEditorAbstractItem::SMItem) n++;
		return n;
	};
	int before = countSM();
	Cyberiada::StateMachine* sm =
		m.newStateMachine("SM added by command", Cyberiada::Rect(178, 285, 300, 80));
	QVERIFY(sm);
	QVERIFY2(s.getMap().value(sm->get_id()) != nullptr,
			 "the added state machine got no scene item");
	QCOMPARE(countSM(), before + 1);
}

void TestScene::test_frameless_border_persists()
{
	// a border added to a geometry-less document (format none) upgrades it to a
	// format that serialises the geometry, so the border saves and undoes (P-21)
	CyberiadaSMModel m(nullptr);
	Cyberiada::StateMachine* sm = m.newStateMachine("SM", Cyberiada::Rect());  // frameless
	QVERIFY(sm);
	QVERIFY(!sm->has_geometry());
	QCOMPARE(int(m.rootDocument()->get_geometry_format()), int(Cyberiada::geometryFormatNone));

	QVERIFY(m.updateGeometry(m.elementToIndex(sm), Cyberiada::Rect(100, 100, 200, 100)));
	QVERIFY(sm->has_geometry());
	QVERIFY2(m.rootDocument()->get_geometry_format() != Cyberiada::geometryFormatNone,
			 "the document kept format none, so the border would not be saved");
}

void TestScene::test_promoted_composite_region_stable()
{
	// a simple state promoted to composite by gaining a child must place its
	// region below the title just as a reload does, so a child (and the
	// transitions attached to it) keep their scene position across a reload (P-18)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	Cyberiada::ElementCollection* simple =
		dynamic_cast<Cyberiada::ElementCollection*>(model->idToElement("node-0-1"));  // a simple state
	QVERIFY(simple && !simple->has_children());

	Cyberiada::State* child = model->newState(simple, "child", Cyberiada::Action(),
											  Cyberiada::Rect(0, 0, 40, 30));
	QVERIFY(child);
	QGraphicsItem* childItem = scene->getMap().value(child->get_id());
	QVERIFY(childItem);
	QPointF promoted = childItem->scenePos();

	scene->loadScene();   // rebuild from the model, as reopen/redo-all does
	childItem = scene->getMap().value(child->get_id());
	QVERIFY(childItem);
	QPointF reloaded = childItem->scenePos();

	QVERIFY2(QLineF(promoted, reloaded).length() < 0.5,
			 qPrintable(QString("child moved on reload: %1,%2 -> %3,%4")
						.arg(promoted.x()).arg(promoted.y()).arg(reloaded.x()).arg(reloaded.y())));
}

void TestScene::test_paste_state()
{
	// pasting a state produces a sibling with a fresh id and a unique name, in
	// one undo step (#9)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	Cyberiada::Element* src = model->idToElement("node-0-1");   // a simple state
	Cyberiada::ElementCollection* parent =
		dynamic_cast<Cyberiada::ElementCollection*>(src->get_parent());
	QVERIFY(src && parent);
	int before = model->undoStack()->count();

	Cyberiada::Element* pasted = model->pasteElement(parent, src);
	QVERIFY(pasted);
	QVERIFY(pasted->get_id() != src->get_id());                 // fresh id
	QVERIFY(pasted->get_name() != src->get_name());             // unique name
	QCOMPARE(pasted->get_parent(), src->get_parent());          // same hierarchy level
	QCOMPARE(model->undoStack()->count(), before + 1);          // one undo step
}

void TestScene::test_paste_transition()
{
	// a pasted transition keeps its source and target but gets a fresh id and
	// slightly shifted points (#9)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	Cyberiada::Transition* src =
		static_cast<Cyberiada::Transition*>(model->idToElement("edge-2"));  // has stored points
	QVERIFY(src && src->has_geometry_source_point());
	Cyberiada::ElementCollection* parent =
		dynamic_cast<Cyberiada::ElementCollection*>(src->get_parent());
	QVERIFY(parent);
	Cyberiada::ID s = src->source_element_id(), t = src->target_element_id();
	Cyberiada::Point sp = src->get_source_point();

	const Cyberiada::Transition* pasted =
		dynamic_cast<const Cyberiada::Transition*>(model->pasteElement(parent, src));
	QVERIFY(pasted);
	QVERIFY(pasted->get_id() != src->get_id());                 // fresh id
	QCOMPARE(pasted->source_element_id(), s);                   // same source
	QCOMPARE(pasted->target_element_id(), t);                   // same target
	QVERIFY(pasted->has_geometry_source_point());
	QVERIFY(pasted->get_source_point().x != sp.x || pasted->get_source_point().y != sp.y);  // shifted
}

void TestScene::test_sm_not_pasteable()
{
	// a State Machine can never be pasted (#9)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	Cyberiada::Element* sm = model->indexToElement(model->firstSMIndex());
	QVERIFY(sm && sm->get_type() == Cyberiada::elementSM);
	Cyberiada::ElementCollection* root =
		dynamic_cast<Cyberiada::ElementCollection*>(sm->get_parent());
	QCOMPARE(model->pasteElement(root, sm), (Cyberiada::Element*)nullptr);
}

void TestScene::test_action_multiline()
{
	// a state action's behaviour drops to its own line when the single line would
	// not fit the state width, and stays on one line when it fits (#6)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	CyberiadaSMEditorStateItem* state =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0-1"));
	QVERIFY(state);
	QModelIndex idx = model->elementToIndex(model->idToElement("node-0-1"));

	auto entryText = [&]() -> QString {
		for (QGraphicsItem* c : state->childItems())
			if (StateAction* a = dynamic_cast<StateAction*>(c))
				if (a->toPlainText().startsWith("entry")) return a->toPlainText();
		return QString();
	};

	QVERIFY(model->newAction(idx, Cyberiada::actionEntry, QString(), QString(), "f()"));
	QVERIFY2(!entryText().contains('\n'), "a short action should stay on one line");
	QVERIFY(model->deleteAction(idx, 0));

	QVERIFY(model->newAction(idx, Cyberiada::actionEntry, QString(), QString(),
							 "a_very_long_behaviour_that_cannot_fit()"));
	QString t = entryText();
	QVERIFY2(t.startsWith("entry/\n"), "a long action should break after entry/");
	QVERIFY2(!t.mid(QString("entry/\n").length()).contains('\n'), "the behaviour keeps its own line");
}

void TestScene::test_history()
{
	// a history pseudostate is a point vertex; creating one inside a simple state
	// promotes that state to a composite state (it gains a child)
	Cyberiada::ElementCollection* sm =
		dynamic_cast<Cyberiada::ElementCollection*>(model->indexToElement(model->firstSMIndex()));
	QVERIFY(sm);
	Cyberiada::State* st = model->newState(sm, "History host", Cyberiada::Action(),
										   Cyberiada::Rect(1200, 0, 200, 150));
	QVERIFY(st);
	QCOMPARE(st->get_type(), Cyberiada::elementSimpleState);

	Cyberiada::Element* sh = model->newShallowHistory(st, Cyberiada::Point(0, 0));
	QVERIFY(sh);
	QCOMPARE(sh->get_type(), Cyberiada::elementShallowHistory);
	// the host is now composite
	QCOMPARE(st->get_type(), Cyberiada::elementCompositeState);

	Cyberiada::Element* dh = model->newDeepHistory(st, Cyberiada::Point(40, 0));
	QVERIFY(dh);
	QCOMPARE(dh->get_type(), Cyberiada::elementDeepHistory);

	// both are drawn by the shared point-vertex item
	QVERIFY(dynamic_cast<CyberiadaSMEditorVertexItem*>(scene->getMap().value(sh->get_id())));
	QVERIFY(dynamic_cast<CyberiadaSMEditorVertexItem*>(scene->getMap().value(dh->get_id())));
}

void TestScene::test_submachine()
{
	// a submachine state references another machine and holds entry/exit points
	Cyberiada::ElementCollection* sm =
		dynamic_cast<Cyberiada::ElementCollection*>(model->indexToElement(model->firstSMIndex()));
	QVERIFY(sm);
	Cyberiada::SubmachineState* sub = model->newSubmachineState(sm, "OtherSM",
															   Cyberiada::Rect(1500, 0, 200, 120));
	QVERIFY(sub);
	QCOMPARE(sub->get_type(), Cyberiada::elementSubmachineState);
	QVERIFY(sub->is_submachine_state());
	QCOMPARE(QString::fromStdString(sub->get_submachine_reference()), QString("OtherSM"));

	Cyberiada::Element* en = model->newEntryPoint(sub, Cyberiada::Point(0, 0));
	Cyberiada::Element* ex = model->newExitPoint(sub, Cyberiada::Point(200, 0));
	QVERIFY(en && ex);
	QCOMPARE(en->get_type(), Cyberiada::elementEntryPoint);
	QCOMPARE(ex->get_type(), Cyberiada::elementExitPoint);
	// the submachine keeps its type after gaining children
	QCOMPARE(sub->get_type(), Cyberiada::elementSubmachineState);

	// the box is a state item; the points are point-vertex items
	QVERIFY(dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value(sub->get_id())));
	QVERIFY(dynamic_cast<CyberiadaSMEditorVertexItem*>(scene->getMap().value(en->get_id())));
	QVERIFY(dynamic_cast<CyberiadaSMEditorVertexItem*>(scene->getMap().value(ex->get_id())));

	// the reference is editable through the model
	QVERIFY(model->updateSubmachineReference(model->elementToIndex(sub), "Changed"));
	QCOMPARE(QString::fromStdString(sub->get_submachine_reference()), QString("Changed"));
}

void TestScene::test_new_sm_single_item()
{
	// creating a state machine must build exactly one SM graphics item; a stale
	// second item (counted over all scene items, not the id map) would keep the
	// old title after a rename
	int before = countItems(CyberiadaSMEditorAbstractItem::SMItem);
	scene->addSMItem(Cyberiada::elementSM);
	QCOMPARE(countItems(CyberiadaSMEditorAbstractItem::SMItem), before + 1);
}

void TestScene::test_ctrl_axis_move()
{
	// Ctrl locks a body move to the dominant axis (strict horizontal/vertical)
	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);
	scene->setCurrentTool(ToolType::Select);
	CyberiadaSMEditorStateItem* state =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value("node-0-1"));
	QVERIFY(state);
	const Cyberiada::State* element =
		static_cast<const Cyberiada::State*>(model->idToElement("node-0-1"));
	Cyberiada::Rect before = element->get_geometry_rect();
	QPointF centre = state->sceneBoundingRect().bottomRight() - QPointF(20, 20);

	scene->clearSelection();
	mouse(QEvent::GraphicsSceneMousePress, centre, Qt::LeftButton);
	// x delta dominates and Ctrl is held -> only x moves, y is locked
	mouse(QEvent::GraphicsSceneMouseMove, centre + QPointF(40, 15), Qt::LeftButton, Qt::ControlModifier);
	mouse(QEvent::GraphicsSceneMouseRelease, centre + QPointF(40, 15), Qt::NoButton, Qt::ControlModifier);
	QCOMPARE(element->get_geometry_rect().x, before.x + 40);
	QCOMPARE(element->get_geometry_rect().y, before.y);

	// restore
	QPointF c2 = state->sceneBoundingRect().bottomRight() - QPointF(20, 20);
	mouse(QEvent::GraphicsSceneMousePress, c2, Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseMove, c2 + QPointF(-40, 0), Qt::LeftButton);
	mouse(QEvent::GraphicsSceneMouseRelease, c2 + QPointF(-40, 0), Qt::NoButton);
	QCOMPARE(element->get_geometry_rect().x, before.x);
}

void TestScene::test_promote_title()
{
	// adding a child promotes a simple state to composite; its title must move
	// from the centre to the top header
	Cyberiada::ElementCollection* sm =
		dynamic_cast<Cyberiada::ElementCollection*>(model->indexToElement(model->firstSMIndex()));
	QVERIFY(sm);
	Cyberiada::State* st = model->newState(sm, "PromoteMe", Cyberiada::Action(),
										   Cyberiada::Rect(1900, 0, 200, 150));
	QVERIFY(st && st->get_type() == Cyberiada::elementSimpleState);
	CyberiadaSMEditorStateItem* item =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value(st->get_id()));
	QVERIFY(item);
	StateTitle* title = stateTitle(item);
	QVERIFY(title);
	double centeredY = title->pos().y();

	model->newInitial(st, Cyberiada::Point(0, 0));            // gains a child
	QCOMPARE(st->get_type(), Cyberiada::elementCompositeState);
	StateTitle* after = stateTitle(item);                    // the same item, re-laid-out
	QVERIFY(after);
	QVERIFY(after->pos().y() < centeredY - 1.0);             // moved up to the header
}

void TestScene::test_empty_action_deletes()
{
	// clearing an action's text removes the action instead of leaving a ghost
	Cyberiada::ElementCollection* sm =
		dynamic_cast<Cyberiada::ElementCollection*>(model->indexToElement(model->firstSMIndex()));
	QVERIFY(sm);
	Cyberiada::State* fresh = model->newState(sm, "EmptyActionHost", Cyberiada::Action(),
											  Cyberiada::Rect(2200, 0, 160, 110));
	QVERIFY(fresh);
	QModelIndex index = model->elementToIndex(fresh);
	QVERIFY(model->newAction(index, Cyberiada::actionEntry, QString(), QString(), "gone()"));
	QCOMPARE(int(fresh->get_actions().size()), 1);

	CyberiadaSMEditorStateItem* item =
		dynamic_cast<CyberiadaSMEditorStateItem*>(scene->getMap().value(fresh->get_id()));
	QVERIFY(item);
	StateAction* action = nullptr;
	for (QGraphicsItem* child : item->childItems())
		if ((action = dynamic_cast<StateAction*>(child))) break;
	QVERIFY(action);

	QEvent activate(QEvent::WindowActivate);
	QApplication::sendEvent(scene, &activate);
	action->setTextInteractionFlags(Qt::TextEditorInteraction);
	action->setFocus();
	action->setPlainText("");            // clear the behaviour
	action->clearFocus();                // commit -> empty -> delete
	QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	QCOMPARE(int(fresh->get_actions().size()), 0);
}

QTEST_MAIN(TestScene)
#include "l4-scene.moc"
