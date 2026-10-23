/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The property view tests
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
#include "cyberiadasm_editor_state_item.h"
#include "cyberiadasm_properties_widget.h"
#include "settings_manager.h"
#include "cyberiada_constants.h"
#include "dialogs/multilinetextdialog.h"

// the editor creation is protected in the browser
class PropertiesProbe: public CyberiadaSMPropertiesWidget {
public:
	using CyberiadaSMPropertiesWidget::createEditor;
};

// the rows are found by their manager, so the test is independent of the
// locale; the sub-rows come in the manager order: X, Y, Width, Height
class TestProperties: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void test_rect_edit();
	void test_point_edit();
	void test_transition_edit();
	void test_label_rect();
	void test_size_limit();
	void test_label_point_disabled();
	void test_inspector_mode();
	void test_model_refresh();
	void test_name_edit();
	void test_meta_parameters();
	void test_multiline_dialog();
	void test_multiline_edit();
	void test_meta_description_multiline();
	void test_subject_edit();
	void test_removed_element();

private:
	void select(const char* id);
	template <class Manager> QList<QtProperty*> rows();
	template <class Manager> void collect(const QList<QtProperty*>& props, QList<QtProperty*>& found);
	int rowCount();
	void setDouble(QtProperty* row, double value);
	double getDouble(QtProperty* row);
	QGraphicsItem* item(const char* id);
	StateTitle* title(const char* id);
	// the rows of a name, in the view order
	QList<QtProperty*> named(const QString& name);
	CyberiadaSMModel* model;
	CyberiadaSMEditorScene* scene;
	PropertiesProbe* view;
};

void TestProperties::select(const char* id)
{
	view->slotElementSelected(model->elementToIndex(model->idToElement(id)));
}

template <class Manager>
void TestProperties::collect(const QList<QtProperty*>& props, QList<QtProperty*>& found)
{
	for (QList<QtProperty*>::const_iterator i = props.begin(); i != props.end(); i++) {
		if (dynamic_cast<Manager*>((*i)->propertyManager())) {
			found << *i;
		}
		collect<Manager>((*i)->subProperties(), found);
	}
}

template <class Manager>
QList<QtProperty*> TestProperties::rows()
{
	QList<QtProperty*> found;
	collect<Manager>(view->properties(), found);
	return found;
}

int TestProperties::rowCount()
{
	return rows<QtAbstractPropertyManager>().size();
}

// the spin box factory writes the sub-row through its manager
void TestProperties::setDouble(QtProperty* row, double value)
{
	QtDoublePropertyManager* m = dynamic_cast<QtDoublePropertyManager*>(row->propertyManager());
	QVERIFY(m);
	m->setValue(row, value);
}

double TestProperties::getDouble(QtProperty* row)
{
	return dynamic_cast<QtDoublePropertyManager*>(row->propertyManager())->value(row);
}

QList<QtProperty*> TestProperties::named(const QString& name)
{
	QList<QtProperty*> found;
	for (QtProperty* p : rows<QtAbstractPropertyManager>()) {
		if (p->propertyName() == name) found << p;
	}
	return found;
}

QGraphicsItem* TestProperties::item(const char* id)
{
	return scene->getMap().value(id);
}

StateTitle* TestProperties::title(const char* id)
{
	QList<QGraphicsItem*> children = item(id)->childItems();
	for (QList<QGraphicsItem*>::const_iterator i = children.begin(); i != children.end(); i++) {
		StateTitle* t = dynamic_cast<StateTitle*>(*i);
		if (t) return t;
	}
	return nullptr;
}

void TestProperties::initTestCase()
{
	model = new CyberiadaSMModel(this);
	scene = new CyberiadaSMEditorScene(model, this);
	view = new PropertiesProbe();
	view->setModel(model);
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	SettingsManager::instance().setInspectorMode(false);
}

void TestProperties::test_rect_edit()
{
	// a rect sub-row writes the model and the scene item at once
	select("node-0-0-1");
	QList<QtProperty*> rects = rows<QtRectFPropertyManager>();
	QCOMPARE(rects.size(), 1);
	QList<QtProperty*> sub = rects.first()->subProperties();
	QCOMPARE(sub.size(), 4);
	QVERIFY(view->createEditor(sub.at(0), view));

	const Cyberiada::State* state = static_cast<const Cyberiada::State*>(model->idToElement("node-0-0-1"));
	QCOMPARE(state->get_geometry_rect().x, -100.0f);
	setDouble(sub.at(0), -50);
	QCOMPARE(state->get_geometry_rect().x, -50.0f);
	QCOMPARE(item("node-0-0-1")->pos(), QPointF(-50, 25));

	setDouble(sub.at(2), 400);
	QCOMPARE(state->get_geometry_rect().width, 400.0f);
	QCOMPARE(item("node-0-0-1")->boundingRect().width(), 400.0);
	// the title is wrapped at the new width
	QVERIFY(title("node-0-0-1"));
	QCOMPARE(title("node-0-0-1")->textWidth(), 400.0);
	// the scene index knows the grown box: a point outside the old one hits it
	QPointF grown = item("node-0-0-1")->mapToScene(QPointF(-190, 0));
	QVERIFY(scene->items(grown).contains(item("node-0-0-1")));

	setDouble(sub.at(0), -100);
	setDouble(sub.at(2), 300);
	QCOMPARE(state->get_geometry_rect().x, -100.0f);
	QCOMPARE(state->get_geometry_rect().width, 300.0f);
}

void TestProperties::test_point_edit()
{
	// a point sub-row moves the vertex
	select("node-0-0-0");
	QList<QtProperty*> points = rows<QtPointFPropertyManager>();
	QCOMPARE(points.size(), 1);
	QList<QtProperty*> sub = points.first()->subProperties();
	QCOMPARE(sub.size(), 2);

	const Cyberiada::Vertex* v = static_cast<const Cyberiada::Vertex*>(model->idToElement("node-0-0-0"));
	QCOMPARE(v->get_geometry_point().y, -80.0f);
	setDouble(sub.at(1), -60);
	QCOMPARE(v->get_geometry_point().y, -60.0f);
	QCOMPARE(item("node-0-0-0")->pos(), QPointF(-100, -60));

	setDouble(sub.at(1), -80);
	QCOMPARE(v->get_geometry_point().y, -80.0f);
}

void TestProperties::test_transition_edit()
{
	// the endpoint and the polyline rows write the transition geometry
	select("edge-0");
	QList<QtProperty*> points = rows<QtPointFPropertyManager>();
	// the source point, the target point and three polyline points
	QCOMPARE(points.size(), 5);
	const Cyberiada::Transition* t = static_cast<const Cyberiada::Transition*>(model->idToElement("edge-0"));

	QCOMPARE(t->get_source_point().x, -150.0f);
	setDouble(points.at(0)->subProperties().at(0), -140);
	QCOMPARE(t->get_source_point().x, -140.0f);
	QCOMPARE(t->get_target_point().y, 75.0f);

	QCOMPARE(t->get_geometry_polyline().at(1).y, 100.0f);
	setDouble(points.at(3)->subProperties().at(1), 110);
	QCOMPARE(t->get_geometry_polyline().at(1).y, 110.0f);
	QCOMPARE(t->get_geometry_polyline().at(0).y, 0.0f);

	setDouble(points.at(0)->subProperties().at(0), -150);
	setDouble(points.at(3)->subProperties().at(1), 100);
	QCOMPARE(t->get_source_point().x, -150.0f);
	QCOMPARE(t->get_geometry_polyline().at(1).y, 100.0f);
}

void TestProperties::test_label_rect()
{
	// a transition carrying a label rect shows an editable Label Rect row
	QModelIndex idx = model->elementToIndex(model->idToElement("edge-0"));
	model->updateLabel(idx, Cyberiada::Rect(10, 20, 120, 40));
	const Cyberiada::Transition* t = static_cast<const Cyberiada::Transition*>(model->idToElement("edge-0"));
	QVERIFY(t->has_geometry_label_rect());

	select("edge-0");
	QList<QtProperty*> rects = rows<QtRectFPropertyManager>();
	QCOMPARE(rects.size(), 1);                    // the label rect (endpoints are points)
	QList<QtProperty*> sub = rects.first()->subProperties();
	QCOMPARE(sub.size(), 4);                      // x, y, width, height
	QCOMPARE(t->get_label_rect().width, 120.0f);
	setDouble(sub.at(2), 200);                    // edit the width
	QCOMPARE(t->get_label_rect().width, 200.0f);

	model->updateLabel(idx, Cyberiada::Point());  // reset to auto-placement
}

void TestProperties::test_size_limit()
{
	// the width row refuses what the mouse resize refuses
	select("node-0-0-1");
	QList<QtProperty*> sub = rows<QtRectFPropertyManager>().first()->subProperties();
	const Cyberiada::State* state = static_cast<const Cyberiada::State*>(model->idToElement("node-0-0-1"));
	setDouble(sub.at(2), ELEMENT_MIN_SIZE / 2);
	QCOMPARE(getDouble(sub.at(2)), double(ELEMENT_MIN_SIZE));
	QCOMPARE(state->get_geometry_rect().width, float(ELEMENT_MIN_SIZE));
	setDouble(sub.at(2), 300);
	QCOMPARE(state->get_geometry_rect().width, 300.0f);
}

void TestProperties::test_label_point_disabled()
{
	// the label point is shown but has no editor: the library cannot set it
	select("edge-2");
	QList<QtProperty*> points = rows<QtPointFPropertyManager>();
	QCOMPARE(points.size(), 3);
	QtProperty* label = points.at(2);
	QVERIFY(!label->isEnabled());
	QVERIFY(!label->subProperties().at(0)->isEnabled());
	QVERIFY(!label->subProperties().at(1)->isEnabled());
	QVERIFY(points.at(0)->isEnabled());
	QVERIFY(points.at(0)->subProperties().at(0)->isEnabled());
}

void TestProperties::test_inspector_mode()
{
	// the inspected geometry is displayed, never edited
	select("node-0-0-1");
	QList<QtProperty*> sub = rows<QtRectFPropertyManager>().first()->subProperties();
	const Cyberiada::State* state = static_cast<const Cyberiada::State*>(model->idToElement("node-0-0-1"));

	SettingsManager::instance().setInspectorMode(true);
	QVERIFY(!view->createEditor(sub.at(0), view));
	setDouble(sub.at(0), 0);
	QCOMPARE(state->get_geometry_rect().x, -100.0f);

	select("node-0-0-0");
	QtProperty* point = rows<QtPointFPropertyManager>().first();
	QVERIFY(!view->createEditor(point->subProperties().at(0), view));

	SettingsManager::instance().setInspectorMode(false);
	QVERIFY(view->createEditor(point->subProperties().at(0), view));
	select("node-0-0-1");
	sub = rows<QtRectFPropertyManager>().first()->subProperties();
	QCOMPARE(getDouble(sub.at(0)), -100.0);
	QVERIFY(view->createEditor(sub.at(0), view));
}

void TestProperties::test_model_refresh()
{
	// a model change refreshes the rows in place, adding none
	select("edge-1");
	int count = rowCount();
	QList<QtProperty*> points = rows<QtPointFPropertyManager>();
	QCOMPARE(points.size(), 2);
	QtPointFPropertyManager* m = dynamic_cast<QtPointFPropertyManager*>(points.first()->propertyManager());
	QVERIFY(m);
	QCOMPARE(m->value(points.first()), QPointF(0, 0));

	QModelIndex index = model->elementToIndex(model->idToElement("edge-1"));
	QVERIFY(model->updateGeometry(index, Cyberiada::Point(5, 5), Cyberiada::Point(0, -75)));
	QCOMPARE(m->value(points.first()), QPointF(5, 5));
	QCOMPARE(rowCount(), count);

	QVERIFY(model->updateGeometry(index, Cyberiada::Point(0, 0), Cyberiada::Point(0, -75)));
	QCOMPARE(m->value(points.first()), QPointF(0, 0));
	QCOMPARE(rowCount(), count);
}

void TestProperties::test_name_edit()
{
	// the name row renames the element and the canvas title; a refused name
	// stays in the row until the row is left
	select("node-0-0-1");
	QList<QtProperty*> strings = rows<QtStringPropertyManager>();
	QtStringPropertyManager* m = dynamic_cast<QtStringPropertyManager*>(strings.first()->propertyManager());
	QVERIFY(m);
	QtProperty* name = nullptr;
	for (QList<QtProperty*>::const_iterator i = strings.begin(); i != strings.end(); i++) {
		if (m->value(*i) == "node 0-0-1") name = *i;
	}
	QVERIFY(name);
	const Cyberiada::Element* element = model->idToElement("node-0-0-1");

	m->setValue(name, "Renamed");
	QCOMPARE(QString(element->get_name().c_str()), QString("Renamed"));
	QCOMPARE(title("node-0-0-1")->toPlainText(), QString("Renamed"));

	m->setValue(name, "node 0-0-2");
	QCOMPARE(QString(element->get_name().c_str()), QString("Renamed"));
	QCOMPARE(m->value(name), QString("node 0-0-2"));
	view->setCurrentItem(view->items(name).first());
	view->setCurrentItem(view->items(strings.first()).first());
	QCOMPARE(m->value(name), QString("Renamed"));

	m->setValue(name, "node 0-0-1");
	QCOMPARE(QString(element->get_name().c_str()), QString("node 0-0-1"));
}

void TestProperties::test_meta_parameters()
{
	const Cyberiada::LocalDocument* doc = model->rootDocument();
	QModelIndex docIndex = model->documentIndex();

	// a free-form parameter is added, then editable through its row
	QVERIFY(model->updateMetainformation(docIndex, "author", "Jane"));
	QCOMPARE(QString(doc->meta().get_string("author").c_str()), QString("Jane"));

	view->slotElementSelected(docIndex);
	QList<QtProperty*> strings = rows<QtStringPropertyManager>();
	QtProperty* author = nullptr;
	for (QList<QtProperty*>::const_iterator i = strings.begin(); i != strings.end(); i++) {
		if ((*i)->propertyName() == "author") author = *i;
	}
	QVERIFY(author);
	QtStringPropertyManager* m = dynamic_cast<QtStringPropertyManager*>(author->propertyManager());
	QVERIFY(m);
	m->setValue(author, "Doe");
	QCOMPARE(QString(doc->meta().get_string("author").c_str()), QString("Doe"));

	// a free-form parameter is removable; a fixed one is not
	QVERIFY(model->removeMetainformation(docIndex, "author"));
	QVERIFY(doc->meta().get_string("author").empty());
	QVERIFY(!model->removeMetainformation(docIndex, CYBERIADA_META_STANDARD_VERSION));
	QCOMPARE(QString(doc->meta().standard_version.c_str()), QString("1.0"));
}

// the multiline dialog keeps the newlines of the edited text (EDIT-TEXT-10)
void TestProperties::test_multiline_dialog()
{
	MultilineTextDialog dialog(QStringLiteral("Edit"), QStringLiteral("Text:"),
	                           QStringLiteral("a\nb\nc"));
	QCOMPARE(dialog.text(), QStringLiteral("a\nb\nc"));
}

// a behaviour and a comment body are multiline rows: their editor is the
// multiline button, and a multiline value reaches the model (EDIT-TEXT-10)
void TestProperties::test_multiline_edit()
{
	// a state behaviour
	QVERIFY(model->loadDocument("diagrams/multiline-actions.graphml"));
	scene->loadScene();
	select("parent");
	QList<QtProperty*> strings = rows<QtStringPropertyManager>();
	QtProperty* behaviour = nullptr;
	for (QList<QtProperty*>::const_iterator i = strings.begin(); i != strings.end(); i++) {
		if ((*i)->propertyName() == tr("Behavior", "Property name")) { behaviour = *i; break; }
	}
	QVERIFY(behaviour);
	// the row carries the multiline editor, not a line edit
	QVERIFY(dynamic_cast<MultilineEditButton*>(view->createEditor(behaviour, view)));

	QtStringPropertyManager* bm = dynamic_cast<QtStringPropertyManager*>(behaviour->propertyManager());
	QVERIFY(bm);
	const Cyberiada::State* state = static_cast<const Cyberiada::State*>(model->idToElement("parent"));
	// the row shows the model value, newlines kept
	QCOMPARE(bm->value(behaviour), QString(state->get_actions().front().get_behavior().c_str()));

	// an edit through the row reaches the model
	bm->setValue(behaviour, "one();\ntwo();\nthree();");
	QCOMPARE(QString(state->get_actions().front().get_behavior().c_str()),
	         QString("one();\ntwo();\nthree();"));

	// a model change (a scene edit) refreshes the row in place
	QVERIFY(model->updateAction(model->elementToIndex(model->idToElement("parent")), 0,
	                            QString(), QString(), "alpha();\nbeta();"));
	QCOMPARE(bm->value(behaviour), QString(state->get_actions().front().get_behavior().c_str()));
	QVERIFY(bm->value(behaviour).contains('\n'));

	// a comment body
	QVERIFY(model->loadDocument("diagrams/components.graphml"));
	scene->loadScene();
	select("nComment");
	strings = rows<QtStringPropertyManager>();
	QtProperty* body = nullptr;
	for (QList<QtProperty*>::const_iterator i = strings.begin(); i != strings.end(); i++) {
		if ((*i)->propertyName() == tr("Body", "Property name")) { body = *i; break; }
	}
	QVERIFY(body);
	QVERIFY(dynamic_cast<MultilineEditButton*>(view->createEditor(body, view)));

	QtStringPropertyManager* cm = dynamic_cast<QtStringPropertyManager*>(body->propertyManager());
	QVERIFY(cm);
	const Cyberiada::Comment* comment = static_cast<const Cyberiada::Comment*>(model->idToElement("nComment"));
	// the row shows the model body
	QCOMPARE(cm->value(body), QString(comment->get_body().c_str()));

	cm->setValue(body, "first\nsecond");
	QCOMPARE(QString(comment->get_body().c_str()), QString("first\nsecond"));

	// a model change refreshes the row
	QVERIFY(model->updateCommentBody(model->elementToIndex(model->idToElement("nComment")), "edited\nbody"));
	QCOMPARE(cm->value(body), QString(comment->get_body().c_str()));
}

// the document description meta row carries the multiline dialog button, while a
// plain parameter keeps the line edit, and a multiline value round-trips (EDIT-TEXT-10)
void TestProperties::test_meta_description_multiline()
{
	SettingsManager::instance().setInspectorMode(false);
	QVERIFY(model->loadDocument("diagrams/geometry.graphml"));
	scene->loadScene();
	const Cyberiada::LocalDocument* doc = model->rootDocument();
	QModelIndex docIndex = model->documentIndex();

	QVERIFY(model->updateMetainformation(docIndex, METAINFORMATION_KEY_DESCRIPTION, "one line"));
	QVERIFY(model->updateMetainformation(docIndex, "author", "Jane"));

	view->slotElementSelected(docIndex);
	QList<QtProperty*> strings = rows<QtStringPropertyManager>();
	QtProperty* description = nullptr;
	QtProperty* author = nullptr;
	for (QList<QtProperty*>::const_iterator i = strings.begin(); i != strings.end(); i++) {
		if ((*i)->propertyName() == METAINFORMATION_KEY_DESCRIPTION) description = *i;
		if ((*i)->propertyName() == "author") author = *i;
	}
	QVERIFY(description);
	QVERIFY(author);

	// the description row opens the dialog; a plain parameter stays a line edit
	QVERIFY(dynamic_cast<MultilineEditButton*>(view->createEditor(description, view)));
	QVERIFY(!dynamic_cast<MultilineEditButton*>(view->createEditor(author, view)));

	QtStringPropertyManager* m = dynamic_cast<QtStringPropertyManager*>(description->propertyManager());
	QVERIFY(m);
	QCOMPARE(m->value(description), QString("one line"));

	// a multiline value set through the row reaches the model, newlines kept
	m->setValue(description, "line one\nline two");
	QCOMPARE(QString(doc->meta().get_string(METAINFORMATION_KEY_DESCRIPTION).c_str()),
	         QString("line one\nline two"));

	// a model change refreshes the row in place
	QVERIFY(model->updateMetainformation(docIndex, METAINFORMATION_KEY_DESCRIPTION, "alpha\nbeta"));
	QCOMPARE(m->value(description), QString("alpha\nbeta"));
}

void TestProperties::test_subject_edit()
{
	// the subject rows of a comment: the target and the type are edited in place, each
	// subject row follows its own subject, the link geometry is shown, not edited
	QVERIFY(model->loadDocument("diagrams/comment-transition.graphml"));
	scene->loadScene();
	Cyberiada::Element* note = model->idToElement("n2");
	QVERIFY(model->newCommentSubject(model->elementToIndex(note), model->idToElement("n1"),
									 Cyberiada::commentSubjectElement, QString(),
									 Cyberiada::Point(100, 0), Cyberiada::Point(-100, 0)));
	select("n2");
	QList<QtProperty*> targets = named("Subject Target");
	QCOMPARE(targets.size(), 2);
	QtEnumPropertyManager* m = dynamic_cast<QtEnumPropertyManager*>(targets.at(1)->propertyManager());
	QVERIFY(m);
	QStringList names = m->enumNames(targets.at(1));
	QCOMPARE(names.at(m->value(targets.at(0))), QString("[t0]"));
	QCOMPARE(names.at(m->value(targets.at(1))), QString("Second state"));

	// the second subject retargeted to the first state, the first one untouched
	m->setValue(targets.at(1), names.indexOf("First state"));
	const Cyberiada::Comment* c = static_cast<const Cyberiada::Comment*>(model->idToElement("n2"));
	QCOMPARE(c->get_subjects().at(1).get_element()->get_id(), std::string("n0"));
	QCOMPARE(c->get_subjects().at(0).get_element()->get_id(), std::string("t0"));
	QCoreApplication::sendPostedEvents();
	targets = named("Subject Target");
	QCOMPARE(names.at(m->value(targets.at(1))), QString("First state"));

	// the type set to a name fragment starts with the whole name
	QList<QtProperty*> types = named("Subject Type");
	QCOMPARE(types.size(), 2);
	m->setValue(types.at(1), int(Cyberiada::commentSubjectName));
	QCOMPARE(int(c->get_subjects().at(1).get_type()), int(Cyberiada::commentSubjectName));
	QCOMPARE(c->get_subjects().at(1).get_fragment(), std::string("First state"));
	QCoreApplication::sendPostedEvents();
	QCOMPARE(named("Fragment").size(), 1);

	// a refused target (the comment itself) leaves the subject as it was
	targets = named("Subject Target");
	int self = m->enumNames(targets.at(0)).indexOf("[n2]");
	if (self >= 0) {
		m->setValue(targets.at(0), self);
		QCOMPARE(c->get_subjects().at(0).get_element()->get_id(), std::string("t0"));
		QCoreApplication::sendPostedEvents();
	}

	// the link geometry rows are disabled
	for (QtProperty* g : named("Geometry")) {
		bool under_subject = false;
		for (QtProperty* s : named("Subject")) {
			if (s->subProperties().contains(g)) under_subject = true;
		}
		if (under_subject) QVERIFY(!g->isEnabled());
	}
}

void TestProperties::test_removed_element()
{
	// the element shown goes with its row: the view empties and a rebuild queued
	// earlier (a reshaped subject list) finds nothing to read
	QVERIFY(model->loadDocument("diagrams/comment-transition.graphml"));
	scene->loadScene();
	select("n2");
	QVERIFY(model->newCommentSubject(model->elementToIndex(model->idToElement("n2")), model->idToElement("n1"),
									 Cyberiada::commentSubjectElement, QString()));
	select("n1");
	QVERIFY(rowCount() > 0);
	QVERIFY(model->deleteElement(model->elementToIndex(model->idToElement("n1"))));
	QCOMPARE(rowCount(), 0);
	QCoreApplication::sendPostedEvents();
	QCOMPARE(rowCount(), 0);
}

QTEST_MAIN(TestProperties)
#include "l4-properties.moc"
