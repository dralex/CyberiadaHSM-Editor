/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process model contract test (see docs/TESTING.md, L4)
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
#include <QAbstractItemModelTester>
#include "cyberiadasm_model.h"

class TestModel: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void test_load();
	void test_reload();
	void test_update_title();
	void test_actions();
	void test_new_elements();
	void test_update_id();
	void test_reparent();
	void test_subjects();
	void test_subject_on_transition();
	void test_delete();

private:
	QModelIndex indexOf(const char* id);
	CyberiadaSMModel* model;
	QAbstractItemModelTester* tester;
};

QModelIndex TestModel::indexOf(const char* id)
{
	return model->elementToIndex(model->idToElement(id));
}

void TestModel::initTestCase()
{
	model = new CyberiadaSMModel(this);
	// the tester checks the model invariants and the signal legality
	// through every reset and mutation below
	tester = new QAbstractItemModelTester(
		model, QAbstractItemModelTester::FailureReportingMode::QtTest, this);
}

void TestModel::test_load()
{
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	QVERIFY(model->rootIndex().isValid());
	QVERIFY(model->documentIndex().isValid());
	QVERIFY(model->firstSMIndex().isValid());
	// the state machine: 2 top children (node-0 + 3 edges attached to it)
	QCOMPARE(model->rowCount(model->firstSMIndex()), 5);
	QModelIndex state = indexOf("node-0-0-1");
	QVERIFY(state.isValid());
	QVERIFY(model->isStateIndex(state));
	QVERIFY(!model->data(state, Qt::DisplayRole).toString().isEmpty());
}

void TestModel::test_reload()
{
	// a second load resets the model cleanly
	QVERIFY(model->loadDocument("diagrams/hierarchy.graphml"));
	QVERIFY(model->idToElement("n0::n1::n0"));
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	QVERIFY(model->idToElement("node-0-0-1"));
}

void TestModel::test_update_title()
{
	QModelIndex state = indexOf("node-0-0-1");
	QSignalSpy spy(model, &CyberiadaSMModel::dataChanged);
	QVERIFY(model->updateTitle(state, "Renamed"));
	QCOMPARE(spy.count(), 1);
	QCOMPARE(model->data(state, Qt::DisplayRole).toString(), QString("Renamed"));
}

void TestModel::test_actions()
{
	QModelIndex state = indexOf("node-0-0-2");
	QSignalSpy spy(model, &CyberiadaSMModel::dataChanged);
	QVERIFY(model->newAction(state, Cyberiada::actionEntry, "", "", "init()"));
	QVERIFY(model->updateAction(state, 0, "", "ready", "start()"));
	QVERIFY(model->deleteAction(state, 0));
	QCOMPARE(spy.count(), 3);
	const Cyberiada::State* s =
		static_cast<const Cyberiada::State*>(model->idToElement("node-0-0-2"));
	QVERIFY(!s->has_actions());
}

void TestModel::test_new_elements()
{
	Cyberiada::ElementCollection* parent = static_cast<Cyberiada::ElementCollection*>(
		model->idToElement("node-0"));
	QVERIFY(parent);
	QSignalSpy spy(model, &CyberiadaSMModel::rowsInserted);
	Cyberiada::State* s = model->newState(parent, "Fresh");
	QVERIFY(s);
	QCOMPARE(spy.count(), 1);
	QModelIndex idx = model->elementToIndex(s);
	QVERIFY(idx.isValid());
	QCOMPARE(model->indexToElement(idx), s);
}

void TestModel::test_update_id()
{
	// the transition endpoints must follow the renamed state id
	QModelIndex state = indexOf("node-0-0-1");
	QSignalSpy spy(model, &CyberiadaSMModel::dataChanged);
	QVERIFY(model->updateID(state, "central"));
	QVERIFY(spy.count() >= 1);
	QVERIFY(model->idToElement("central"));
	QVERIFY(!model->idToElement("node-0-0-1"));
	const Cyberiada::Transition* t =
		static_cast<const Cyberiada::Transition*>(model->idToElement("edge-0"));
	QVERIFY(t);
	QCOMPARE(t->source_element_id(), Cyberiada::ID("central"));
	QCOMPARE(t->target_element_id(), Cyberiada::ID("central"));
	QVERIFY(model->updateID(indexOf("central"), "node-0-0-1"));
}

void TestModel::test_reparent()
{
	QVERIFY(model->updateParent(indexOf("node-0-1"), "node-0-0"));
	const Cyberiada::Element* moved = model->idToElement("node-0-1");
	QVERIFY(moved);
	QCOMPARE(moved->get_parent()->get_id(), Cyberiada::ID("node-0-0"));
}

void TestModel::test_subjects()
{
	Cyberiada::ElementCollection* parent = static_cast<Cyberiada::ElementCollection*>(
		model->idToElement("node-0"));
	Cyberiada::Comment* c = model->newComment(parent, "A note");
	QVERIFY(c);
	QModelIndex comment = model->elementToIndex(c);
	QSignalSpy spy(model, &CyberiadaSMModel::dataChanged);
	QVERIFY(model->newCommentSubject(comment, model->idToElement("node-0-0-2"),
									 Cyberiada::commentSubjectElement, QString()));
	QCOMPARE(c->get_subjects().size(), (size_t)1);
	QVERIFY(model->deleteCommentSubject(comment, 0));
	QVERIFY(!c->has_subjects());
	QCOMPARE(spy.count(), 2);
}

void TestModel::test_subject_on_transition()
{
	Cyberiada::StateMachine* sm = static_cast<Cyberiada::StateMachine*>(
		model->indexToElement(model->firstSMIndex()));
	QVERIFY(sm);
	Cyberiada::Comment* c = model->newComment(sm, "A note on the transition");
	QVERIFY(c);
	// the transitions stay the last children, so the comment goes before them
	QCOMPARE(sm->get_children().back()->get_type(), Cyberiada::elementTransition);
	QVERIFY(model->elementToIndex(c).row() < model->rowCount(model->firstSMIndex()) - 1);
	Cyberiada::Element* t = model->idToElement("edge-2");
	QVERIFY(t);
	QVERIFY(model->newCommentSubject(model->elementToIndex(c), t,
									 Cyberiada::commentSubjectElement, QString()));
	QCOMPARE(c->get_subjects().size(), (size_t)1);
	QCOMPARE(c->get_subjects().front().get_element()->get_id(), Cyberiada::ID("edge-2"));
	// deleting the transition strips the subject pointing at it
	QVERIFY(model->deleteElement(model->elementToIndex(t)));
	QVERIFY(!c->has_subjects());
}

void TestModel::test_delete()
{
	// a comment subject pointing at the doomed state is stripped and the
	// attached transitions are removed with the state
	Cyberiada::Comment* c = static_cast<Cyberiada::Comment*>(
		model->newComment(static_cast<Cyberiada::ElementCollection*>(
							  model->idToElement("node-0")), "Doomed note"));
	QVERIFY(model->newCommentSubject(model->elementToIndex(c),
									 model->idToElement("node-0-0-1"),
									 Cyberiada::commentSubjectElement, QString()));
	QSignalSpy spy(model, &CyberiadaSMModel::rowsRemoved);
	QVERIFY(model->deleteElement(indexOf("node-0-0-1")));
	QVERIFY(spy.count() >= 1);
	QVERIFY(!model->idToElement("node-0-0-1"));
	QVERIFY(!model->idToElement("edge-0"));
	QVERIFY(!model->idToElement("edge-1"));
	QVERIFY(!c->has_subjects());
}

QTEST_MAIN(TestModel)
#include "l4-model.moc"
