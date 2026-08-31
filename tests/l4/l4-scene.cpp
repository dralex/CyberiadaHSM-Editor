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
	void test_new_state();
	void test_new_transition();
	void test_new_comment();
	void test_new_choice();
	void test_reparent();
	void test_delete();

private:
	int countItems(int type);
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

QTEST_MAIN(TestScene)
#include "l4-scene.moc"
