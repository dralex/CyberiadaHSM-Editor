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

#include <sstream>

#include <QtTest>
#include <functional>
#include <QAbstractItemModelTester>
#include "cyberiadasm_model.h"
#include "settings_manager.h"

class TestModel: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void test_load();
	void test_reload();
	void test_update_title();
	void test_title_rules();
	void test_actions();
	void test_new_elements();
	void test_update_id();
	void test_reparent();
	void test_subjects();
	void test_subject_on_transition();
	void test_read_only();
	void test_delete();
	void test_geometry_declaration();
	void test_undo_redo();
	void test_move_subjects();
	void test_reparent_free_slot();
	void test_paste_free_slot();
	void test_paste_grows_packed_parent();
	void test_paste_internal_transition();
	void test_snap_to_grid();
	void test_undo_reset();
	void test_reconstruct_geometry();

private:
	static bool rectsOverlap(const Cyberiada::Rect& a, const Cyberiada::Rect& b);
	QModelIndex indexOf(const char* id);
	QString documentDump();
	CyberiadaSMModel* model;
	QAbstractItemModelTester* tester;
};

QModelIndex TestModel::indexOf(const char* id)
{
	return model->elementToIndex(model->idToElement(id));
}

QString TestModel::documentDump()
{
	std::ostringstream os;
	os << *static_cast<const Cyberiada::Element*>(model->rootDocument());
	return QString(os.str().c_str());
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

void TestModel::test_title_rules()
{
	// the states of one level are told apart by name: an empty or a taken
	// name is refused without a signal; the vertices carry no name
	QModelIndex state = indexOf("node-0-0-1");
	QSignalSpy spy(model, &CyberiadaSMModel::dataChanged);
	QVERIFY(!model->updateTitle(state, ""));
	QVERIFY(!model->updateTitle(state, "  "));
	QVERIFY(!model->updateTitle(state, "node 0-0-2"));
	QCOMPARE(spy.count(), 0);
	QVERIFY(model->updateTitle(state, "NODE 0-0-2"));
	QVERIFY(model->updateTitle(state, "Renamed"));
	QCOMPARE(spy.count(), 2);
	QVERIFY(model->updateTitle(indexOf("node-0-0-0"), ""));
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

	// a multiline behaviour is kept, but blank lines cannot be stored: they
	// separate the action blocks in the document text format
	QVERIFY(model->newAction(state, Cyberiada::actionEntry, "", "", "a();\n\nb();"));
	QCOMPARE(QString(s->get_actions()[0].get_behavior().c_str()), QString("a();\nb();"));
	QVERIFY(model->updateAction(state, 0, "", "", "\n  x();\n\n\ny();  "));
	QCOMPARE(QString(s->get_actions()[0].get_behavior().c_str()), QString("x();\ny();"));
	QVERIFY(model->deleteAction(state, 0));
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

void TestModel::test_read_only()
{
	// the inspected document refuses every mutation, so an editing handler
	// missing the mode check still cannot damage the file
	QModelIndex state = indexOf("node-0");
	QVERIFY(state.isValid());
	QVERIFY(model->flags(state) & Qt::ItemIsEditable);

	SettingsManager::instance().setInspectorMode(true);
	QVERIFY(model->readOnly());
	QString before = documentDump();

	QVERIFY(!model->updateTitle(state, "Renamed"));
	QVERIFY(!model->updateID(state, "node-renamed"));
	QVERIFY(!model->updateGeometry(state, Cyberiada::Rect(0, 0, 10, 10)));
	QVERIFY(!model->updateCommentBody(state, "text"));
	QVERIFY(!model->newAction(state, Cyberiada::actionEntry, "entry", "", "act();"));
	QVERIFY(!model->deleteAction(state, 0));
	QVERIFY(!model->deleteElement(state));
	QVERIFY(!model->newState(static_cast<Cyberiada::ElementCollection*>(
								 model->idToElement("node-0")), "Fresh"));
	QVERIFY(!model->newComment(static_cast<Cyberiada::ElementCollection*>(
								   model->idToElement("node-0")), "A note"));
	QVERIFY(!model->newStateMachine("Another"));
	QVERIFY(!model->setData(state, "Renamed", Qt::EditRole));

	// the tree is locked as well: nothing is renamed or dragged in place
	Qt::ItemFlags f = model->flags(state);
	QVERIFY(!(f & Qt::ItemIsEditable));
	QVERIFY(!(f & Qt::ItemIsDragEnabled));
	QVERIFY(!(f & Qt::ItemIsDropEnabled));
	QVERIFY(f & Qt::ItemIsSelectable);

	QCOMPARE(documentDump(), before);

	SettingsManager::instance().setInspectorMode(false);
	QVERIFY(!model->readOnly());
	QString title = QString(model->idToElement("node-0")->get_name().c_str());
	QVERIFY(model->updateTitle(state, "Renamed"));
	QVERIFY(model->updateTitle(state, title));
	QCOMPARE(documentDump(), before);
	QVERIFY(model->flags(state) & Qt::ItemIsEditable);
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

void TestModel::test_geometry_declaration()
{
	// the written document declares the geometry the editor writes: full,
	// or none when the geometry is skipped; the parameter is edited as well
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	QString path = dir.filePath("declared.graphml");
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	const Cyberiada::DocumentMetainformation& meta = model->rootDocument()->meta();
	QCOMPARE(meta.get_geometry(), Cyberiada::geometryDeclarationAbsent);

	model->saveAsDocument(path, Cyberiada::formatCyberiada10, true);
	QCOMPARE(meta.get_geometry(), Cyberiada::geometryDeclarationFull);
	model->saveAsDocument(path, Cyberiada::formatCyberiada10, true, true, false, false, false);
	QCOMPARE(meta.get_geometry(), Cyberiada::geometryDeclarationNone);

	// the declaration survives the file
	CyberiadaSMModel written(this);
	QVERIFY(written.loadDocument(path));
	QCOMPARE(written.rootDocument()->meta().get_geometry(), Cyberiada::geometryDeclarationNone);

	QModelIndex doc = model->documentIndex();
	QVERIFY(model->updateMetainformation(doc, CYBERIADA_META_GEOMETRY, CYBERIADA_META_GEOM_SHORT));
	QCOMPARE(meta.get_geometry(), Cyberiada::geometryDeclarationShort);
	QVERIFY(!model->updateMetainformation(doc, CYBERIADA_META_GEOMETRY, "wide"));
	QVERIFY(model->updateMetainformation(doc, CYBERIADA_META_GEOMETRY, ""));
	QCOMPARE(meta.get_geometry(), Cyberiada::geometryDeclarationAbsent);
	QVERIFY(meta.get_string(CYBERIADA_META_GEOMETRY).empty());
}

void TestModel::test_undo_redo()
{
	// every mutation is one undo step that brings the exact document back;
	// the elements are re-resolved by id, the restore replaces them all
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	QUndoStack* stack = model->undoStack();
	QCOMPARE(stack->count(), 0);
	QVERIFY(stack->isClean());

	QString d0 = documentDump();
	QVERIFY(model->updateTitle(indexOf("node-0-1"), "Undone"));
	QString d1 = documentDump();
	QVERIFY(d1 != d0);
	QCOMPARE(stack->count(), 1);
	QVERIFY(!stack->isClean());
	stack->undo();
	QCOMPARE(documentDump(), d0);
	// the file identity survives the restore
	QCOMPARE(QString(model->rootDocument()->get_file_path().c_str()), QString("diagrams/geometry.graphml"));
	QCOMPARE(int(model->rootDocument()->get_file_format()), int(Cyberiada::formatCyberiada10));
	stack->redo();
	QCOMPARE(documentDump(), d1);

	// a refused mutation pushes nothing
	QVERIFY(!model->updateTitle(indexOf("node-0-1"), ""));
	QCOMPARE(stack->count(), 1);

	// a bracketed gesture is one step whatever it writes
	model->beginUndoStep("gesture");
	QVERIFY(model->updateGeometry(indexOf("node-0-1"), Cyberiada::Rect(390, 15, 150, 150)));
	QVERIFY(model->updateGeometry(indexOf("node-0-1"), Cyberiada::Rect(400, 25, 150, 150)));
	QVERIFY(model->updateGeometry(indexOf("node-0-1"), Cyberiada::Rect(410, 35, 160, 160)));
	model->endUndoStep();
	QCOMPARE(stack->count(), 2);
	stack->undo();
	QCOMPARE(documentDump(), d1);
	stack->redo();

	// the structural mutations: each restores exactly
	struct Case { const char* name; std::function<bool()> run; };
	QList<Case> cases = {
		{"delete a state with its transitions", [&]() { return model->deleteElement(indexOf("node-0-0-1")); }},
		{"reparent", [&]() { return model->updateParent(indexOf("node-0-1"), "node-0-0"); }},
		{"new state", [&]() { return model->newState(static_cast<Cyberiada::ElementCollection*>(
			model->idToElement("node-0")), "Fresh", Cyberiada::Action(), Cyberiada::Rect(0, 0, 100, 50)) != NULL; }},
		{"new action", [&]() { return model->newAction(indexOf("node-0-0-2"), Cyberiada::actionEntry, "", "", "in()"); }},
		{"update action", [&]() { return model->updateAction(indexOf("node-0-0-2"), 0, "", "", "out()"); }},
		{"delete action", [&]() { return model->deleteAction(indexOf("node-0-0-2"), 0); }},
		{"id", [&]() { return model->updateID(indexOf("node-0-1"), "renamed-id"); }},
		{"metainformation", [&]() { return model->updateMetainformation(model->documentIndex(), "name", "undone"); }},
	};
	for (const Case& c : cases) {
		QString before = documentDump();
		int steps = stack->count();
		QVERIFY2(c.run(), c.name);
		QString after = documentDump();
		QVERIFY2(after != before, c.name);
		QVERIFY2(stack->count() > steps, c.name);
		while (stack->count() > steps && stack->canUndo() && stack->index() > steps) stack->undo();
		QVERIFY2(documentDump() == before, c.name);
		while (stack->canRedo()) stack->redo();
		QString redone = documentDump();
		if (redone != after) {
			int at = 0;
			while (at < redone.size() && at < after.size() && redone[at] == after[at]) at++;
			qWarning() << c.name << "differs at" << at
					   << "\n  after:" << after.mid(qMax(0, at - 60), 160)
					   << "\n  redone:" << redone.mid(qMax(0, at - 60), 160);
		}
		QVERIFY2(redone == after, c.name);
	}

	// the clean state follows the save
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	model->saveAsDocument(dir.filePath("undo.graphml"), Cyberiada::formatCyberiada10);
	QVERIFY(stack->isClean());
	QVERIFY(model->updateTitle(indexOf("node-0-0-2"), "Dirty"));
	QVERIFY(!stack->isClean());
	stack->undo();
	QVERIFY(stack->isClean());
}

void TestModel::test_reconstruct_geometry()
{
	// reconstruction strips and rebuilds the whole geometry as one atomic undo
	// step; the structure (ids, kinds, nesting) is untouched
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	QUndoStack* stack = model->undoStack();
	QCOMPARE(stack->count(), 0);

	QString before = documentDump();
	QVERIFY(model->reconstructGeometry());
	QString after = documentDump();
	QVERIFY(after != before);                 // the geometry was rebuilt
	QCOMPARE(stack->count(), 1);              // exactly one undo step
	QVERIFY(indexOf("node-0-0-0").isValid()); // the elements survive, resolvable by id
	QVERIFY(indexOf("node-0-0-1").isValid());

	stack->undo();
	QCOMPARE(documentDump(), before);         // atomic restore of the old layout
	stack->redo();
	QCOMPARE(documentDump(), after);
}

void TestModel::test_move_subjects()
{
	// a comment outside a subtree points into it; reparenting copies the
	// subtree and frees the original, and the subject follows the copy
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	Cyberiada::ElementCollection* sm = static_cast<Cyberiada::ElementCollection*>(
		model->idToElement("G"));
	Cyberiada::Comment* note = model->newComment(sm, "Into the subtree", Cyberiada::Rect(0, 0, 100, 40));
	QVERIFY(note);
	QString note_id = QString(note->get_id().c_str());
	Cyberiada::Element* inner = model->idToElement("node-0-0-1");
	QVERIFY(model->newCommentSubject(model->elementToIndex(note), inner,
									 Cyberiada::commentSubjectElement, QString()));
	QVERIFY(model->updateParent(indexOf("node-0-0"), "node-0-1"));
	Cyberiada::Element* moved = model->idToElement("node-0-0-1");
	QVERIFY(moved && moved != inner);
	const Cyberiada::Comment* c = static_cast<const Cyberiada::Comment*>(model->idToElement(note_id));
	QCOMPARE(int(c->get_subjects().size()), 1);
	QCOMPARE(c->get_subjects()[0].get_element(), static_cast<const Cyberiada::Element*>(moved));
	QVERIFY(documentDump().contains("to: 'node-0-0-1'"));
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	model->saveAsDocument(dir.filePath("moved.graphml"), Cyberiada::formatCyberiada10);
}

bool TestModel::rectsOverlap(const Cyberiada::Rect& a, const Cyberiada::Rect& b)
{
	const double tol = 0.5;   // edge-touching is not an overlap
	return a.x - a.width / 2 < b.x + b.width / 2 - tol &&
		   b.x - b.width / 2 < a.x + a.width / 2 - tol &&
		   a.y - a.height / 2 < b.y + b.height / 2 - tol &&
		   b.y - b.height / 2 < a.y + a.height / 2 - tol;
}

void TestModel::test_reparent_free_slot()
{
	// a child reparented onto a spot already taken by a sibling is relocated to a
	// free slot instead of overlapping it (EDIT-NODE-6)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	Cyberiada::ElementCollection* sm = static_cast<Cyberiada::ElementCollection*>(
		model->idToElement("G"));
	QVERIFY(sm);
	Cyberiada::State* p = model->newState(sm, "PR", Cyberiada::Action(), Cyberiada::Rect(0, 0, 600, 400));
	Cyberiada::State* a = model->newState(p, "AR", Cyberiada::Action(), Cyberiada::Rect(0, 0, 120, 80));
	QVERIFY(p && a);
	Cyberiada::ID p_id = p->get_id(), a_id = a->get_id();
	// a sibling of P sharing P's frame origin: its absolute place coincides with A,
	// so a plain reparent would drop it onto A
	Cyberiada::State* b = model->newState(sm, "BR", Cyberiada::Action(), Cyberiada::Rect(0, 0, 120, 80));
	QVERIFY(b);
	Cyberiada::ID b_id = b->get_id();
	QVERIFY(model->updateParent(indexOf(b_id.c_str()), p_id));
	Cyberiada::Element* moved = model->idToElement(b_id.c_str());
	QVERIFY(moved);
	QCOMPARE(moved->get_parent()->get_id(), p_id);
	Cyberiada::Rect ar = static_cast<const Cyberiada::ElementCollection*>(
		model->idToElement(a_id.c_str()))->get_geometry_rect();
	Cyberiada::Rect br = static_cast<const Cyberiada::ElementCollection*>(moved)->get_geometry_rect();
	QVERIFY(!rectsOverlap(ar, br));
}

void TestModel::test_paste_free_slot()
{
	// pasting into a populated parent: the copy clears the source (own-width offset)
	// and any other child it would land on (EDIT-NODE-6)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	Cyberiada::ElementCollection* sm = static_cast<Cyberiada::ElementCollection*>(
		model->idToElement("G"));
	QVERIFY(sm);
	Cyberiada::State* p = model->newState(sm, "PP", Cyberiada::Action(), Cyberiada::Rect(0, 0, 600, 400));
	Cyberiada::State* a = model->newState(p, "AP", Cyberiada::Action(), Cyberiada::Rect(-100, 0, 120, 80));
	// B sits where A's paste offset (own width + 20) would land the copy
	Cyberiada::State* b = model->newState(p, "BP", Cyberiada::Action(), Cyberiada::Rect(60, 0, 120, 80));
	QVERIFY(p && a && b);
	Cyberiada::Rect ar = a->get_geometry_rect(), br = b->get_geometry_rect();
	Cyberiada::Element* copy = model->pasteElement(p, a);
	QVERIFY(copy);
	Cyberiada::Rect cr = static_cast<const Cyberiada::ElementCollection*>(copy)->get_geometry_rect();
	QVERIFY(!rectsOverlap(cr, ar));
	QVERIFY(!rectsOverlap(cr, br));
}

void TestModel::test_paste_grows_packed_parent()
{
	// a parent packed by its child grows to re-contain a pasted copy pushed outside
	// its border (EDIT-NODE-2), and the two children stay clear (EDIT-NODE-6)
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	Cyberiada::ElementCollection* sm = static_cast<Cyberiada::ElementCollection*>(
		model->idToElement("G"));
	QVERIFY(sm);
	Cyberiada::State* p = model->newState(sm, "PG", Cyberiada::Action(), Cyberiada::Rect(0, 0, 140, 100));
	Cyberiada::State* a = model->newState(p, "AG", Cyberiada::Action(), Cyberiada::Rect(0, 0, 120, 80));
	QVERIFY(p && a);
	double before = p->get_geometry_rect().width;
	Cyberiada::Element* copy = model->pasteElement(p, a);
	QVERIFY(copy);
	QVERIFY(p->get_geometry_rect().width > before);   // grew to hold the copy
	Cyberiada::Rect cr = static_cast<const Cyberiada::ElementCollection*>(copy)->get_geometry_rect();
	QVERIFY(!rectsOverlap(cr, a->get_geometry_rect()));
}

static int countTransitions(Cyberiada::ElementCollection* sm)
{
	int n = 0;
	const Cyberiada::ElementList& kids = sm->get_children();
	for (Cyberiada::ElementList::const_iterator i = kids.begin(); i != kids.end(); i++) {
		if ((*i)->get_type() == Cyberiada::elementTransition) n++;
	}
	return n;
}

void TestModel::test_paste_internal_transition()
{
	// pasting a composite that owns an internal transition (the parent to its own
	// descendant) reproduces that transition on the copy, wired to the copied ids
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	Cyberiada::StateMachine* sm = static_cast<Cyberiada::StateMachine*>(model->idToElement("G"));
	QVERIFY(sm);
	Cyberiada::State* p = model->newState(sm, "PI", Cyberiada::Action(), Cyberiada::Rect(0, 0, 300, 200));
	Cyberiada::State* c = model->newState(p, "CI", Cyberiada::Action(), Cyberiada::Rect(0, 0, 120, 80));
	QVERIFY(p && c);
	Cyberiada::Transition* t = model->newTransition(sm, Cyberiada::transitionExternal, p, c, Cyberiada::Action());
	QVERIFY(t);

	int before = countTransitions(sm);
	Cyberiada::Element* copy = model->pasteElement(sm, p);
	QVERIFY(copy);
	QCOMPARE(countTransitions(sm), before + 1);   // the copy carries its own internal transition

	// the reproduced transition connects the copied subtree, not the original p/c
	Cyberiada::ElementCollection* copyCol = static_cast<Cyberiada::ElementCollection*>(copy);
	Cyberiada::ID copyId = copy->get_id();
	bool found = false;
	const Cyberiada::ElementList& kids = sm->get_children();
	for (Cyberiada::ElementList::const_iterator i = kids.begin(); i != kids.end(); i++) {
		if ((*i)->get_type() != Cyberiada::elementTransition) continue;
		Cyberiada::Transition* tr = static_cast<Cyberiada::Transition*>(*i);
		if (tr->source_element_id() != copyId) continue;
		Cyberiada::Element* tgt = model->idToElement(tr->target_element_id().c_str());
		QVERIFY(tgt && tgt->get_parent() == copyCol);   // the copied child, inside the copy
		QVERIFY(tr->target_element_id() != c->get_id());
		found = true;
	}
	QVERIFY(found);
}

void TestModel::test_snap_to_grid()
{
	SettingsManager& sm = SettingsManager::instance();
	bool oldSnap = sm.getSnapMode();
	double oldGrid = sm.getGridSpacing();
	sm.setGridSpacing(25);

	sm.setSnapMode(false);
	QCOMPARE(snapToGrid(QPointF(12, 63)), QPointF(12, 63));   // off: unchanged
	sm.setSnapMode(true);
	QCOMPARE(snapToGrid(QPointF(12, 63)), QPointF(0, 75));    // 12->0, 63->75
	QCOMPARE(snapToGrid(QPointF(-12, -63)), QPointF(0, -75)); // rounds toward the nearest line

	sm.setSnapMode(oldSnap);
	sm.setGridSpacing(oldGrid);
}

void TestModel::test_undo_reset()
{
	// a scope left open by a lost release must not swallow the next edit
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	QUndoStack* stack = model->undoStack();
	QModelIndex idx = model->elementToIndex(model->idToElement("node-0-1"));
	int base = stack->count();

	model->beginUndoStep("leaked");                 // a press whose release was lost
	QVERIFY(model->updateTitle(idx, "X"));          // nests, no step pushed yet
	QCOMPARE(stack->count(), base);
	model->resetUndoGesture();                      // the next gesture drops the leak

	QVERIFY(model->updateTitle(idx, "Y"));          // its own scope -> its own step
	QCOMPARE(stack->count(), base + 1);
}

QTEST_MAIN(TestModel)
#include "l4-model.moc"
