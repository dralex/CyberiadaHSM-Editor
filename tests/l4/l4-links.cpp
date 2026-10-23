/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process comment link test: the links drawn, followed, drawn with the
 * transition tool and edited (see docs/TESTING.md, L4; EDIT-EDGE-6/17-20)
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
#include <QGraphicsSceneMouseEvent>
#include <cmath>
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_editor_transition_item.h"
#include "cyberiadasm_editor_subject_item.h"

class TestLinks: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void cleanup();
	void test_link_drawn();
	void test_link_follows_move();
	void test_link_rebuilt();
	void test_link_gone_with_target();
	void test_link_not_drawn_without_geometry();
	void test_link_draw_tool();
	void test_link_draw_to_transition();
	void test_link_draw_refused();
	void test_press_without_drag();

private:
	CyberiadaSMModel* model = nullptr;
	CyberiadaSMEditorScene* scene = nullptr;
	void load(const QString& path);
	CyberiadaSMEditorSubjectItem* link(const Cyberiada::ID& comment, int index = 0);
	// a press, two moves and a release under the given tool, as a view would send them
	void drag(ToolType tool, const QPointF& from, const QPointF& to);
	int subjects(const Cyberiada::ID& comment);
	QPointF centre(const Cyberiada::ID& id);
};

static void sceneMouse(CyberiadaSMEditorScene* sc, QEvent::Type type, const QPointF& scenePos,
					   Qt::MouseButtons buttons)
{
	QGraphicsSceneMouseEvent event(type);
	event.setScenePos(scenePos);
	event.setScreenPos(scenePos.toPoint());
	event.setButton(Qt::LeftButton);
	event.setButtons(buttons);
	QApplication::sendEvent(sc, &event);
}

// the distance of a point from a drawn path
static qreal offPath(const QPainterPath& path, const QPointF& p)
{
	qreal best = 1e9;
	for (const QPolygonF& part : path.toSubpathPolygons()) {
		for (int i = 0; i + 1 < part.size(); i++) {
			QPointF a = part[i], d = part[i + 1] - a;
			qreal len2 = QPointF::dotProduct(d, d);
			qreal t = len2 > 0 ? qBound(0.0, QPointF::dotProduct(p - a, d) / len2, 1.0) : 0.0;
			QPointF q = a + d * t;
			best = qMin(best, std::hypot(q.x() - p.x(), q.y() - p.y()));
		}
	}
	return best;
}

// on the border of a box, within a pixel
static bool onBorder(const QRectF& box, const QPointF& p)
{
	return box.adjusted(-1, -1, 1, 1).contains(p) && !box.adjusted(1, 1, -1, -1).contains(p);
}

void TestLinks::initTestCase()
{
	model = new CyberiadaSMModel(this);
	scene = new CyberiadaSMEditorScene(model, this);
}

void TestLinks::cleanup()
{
	if (scene) {
		if (QGraphicsItem* g = scene->mouseGrabberItem()) g->ungrabMouse();
	}
}

void TestLinks::load(const QString& path)
{
	QVERIFY(model->loadDocument(path));
	scene->loadScene();
}

CyberiadaSMEditorSubjectItem* TestLinks::link(const Cyberiada::ID& comment, int index)
{
	QList<CyberiadaSMEditorSubjectItem*> links = scene->subjectLinks(comment);
	return index < links.size() ? links.at(index) : nullptr;
}

void TestLinks::test_link_drawn()
{
	// the link to a transition: dashed from the comment border to the path, its
	// foot off the label, above the transition (EDIT-EDGE-6/17)
	load("diagrams/comment-transition.graphml");
	CyberiadaSMEditorSubjectItem* l = link("n2");
	QVERIFY(l);
	QCOMPARE(scene->subjectLinks("n2").size(), 1);
	CyberiadaSMEditorTransitionItem* t =
		dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value("t0"));
	QVERIFY(t);
	QVERIFY(onBorder(scene->getMap().value("n2")->sceneBoundingRect(), l->sourceEnd()));
	QVERIFY(offPath(t->mapToScene(t->path()), l->targetEnd()) < 0.5);
	QVERIFY(!t->labelSceneRect().adjusted(1, 1, -1, -1).contains(l->targetEnd()));
	QVERIFY(l->zValue() > t->zValue());
	// the links are not elements: the element map keeps its items
	QVERIFY(!scene->getMap().values().contains(l));
}

void TestLinks::test_link_follows_move()
{
	// a stored source point moves with the comment, the foot stays on the moved
	// transition (EDIT-EDGE-19)
	load("diagrams/comment-transition.graphml");
	CyberiadaSMEditorSubjectItem* l = link("n2");
	QVERIFY(l);
	QPointF from = l->sourceEnd();
	Cyberiada::Element* note = model->idToElement("n2");
	Cyberiada::Rect r = static_cast<Cyberiada::Comment*>(note)->get_geometry_rect();
	QVERIFY(model->updateGeometry(model->elementToIndex(note), Cyberiada::Rect(r.x + 40, r.y + 30, r.width, r.height)));
	l = link("n2");
	QVERIFY(l);
	QCOMPARE(l->sourceEnd(), from + QPointF(40, 30));

	Cyberiada::Element* second = model->idToElement("n1");
	Cyberiada::Rect s = static_cast<Cyberiada::State*>(second)->get_geometry_rect();
	QVERIFY(model->updateGeometry(model->elementToIndex(second), Cyberiada::Rect(s.x, s.y + 120, s.width, s.height)));
	CyberiadaSMEditorTransitionItem* t =
		dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value("t0"));
	l = link("n2");
	QVERIFY(t && l);
	QVERIFY(offPath(t->mapToScene(t->path()), l->targetEnd()) < 0.5);
}

void TestLinks::test_link_rebuilt()
{
	// a new subject is drawn at once, a removed one goes, undo brings it back
	load("diagrams/comment-transition.graphml");
	Cyberiada::Element* note = model->idToElement("n2");
	QVERIFY(model->newCommentSubject(model->elementToIndex(note), model->idToElement("n1"),
									 Cyberiada::commentSubjectElement, QString()));
	QCOMPARE(scene->subjectLinks("n2").size(), 2);
	CyberiadaSMEditorSubjectItem* l = link("n2", 1);
	QVERIFY(l);
	QVERIFY(onBorder(scene->getMap().value("n1")->sceneBoundingRect(), l->targetEnd()));
	QVERIFY(onBorder(scene->getMap().value("n2")->sceneBoundingRect(), l->sourceEnd()));
	QVERIFY(model->deleteCommentSubject(model->elementToIndex(note), 1));
	QCOMPARE(scene->subjectLinks("n2").size(), 1);
	model->undoStack()->undo();
	QCOMPARE(scene->subjectLinks("n2").size(), 2);
}

void TestLinks::test_link_gone_with_target()
{
	// deleting the commented transition strips the subject and its link; the
	// comment stays (EDIT-STRUCT-9)
	load("diagrams/comment-transition.graphml");
	QVERIFY(model->deleteElement(model->elementToIndex(model->idToElement("t0"))));
	QVERIFY(scene->getMap().value("n2"));
	QCOMPARE(scene->subjectLinks("n2").size(), 0);
	for (QGraphicsItem* item : scene->items()) {
		QVERIFY(!dynamic_cast<CyberiadaSMEditorSubjectItem*>(item));
	}
}

void TestLinks::test_link_not_drawn_without_geometry()
{
	// a comment without geometry is not drawn, nor are its links (EDIT-EDGE-19)
	load("diagrams/hierarchy.graphml");
	Cyberiada::ElementCollection* sm = static_cast<Cyberiada::ElementCollection*>(model->idToElement("G0"));
	QVERIFY(sm);
	Cyberiada::Comment* note = model->newComment(sm, "no geometry");
	QVERIFY(note && !note->has_geometry());
	QVERIFY(model->newCommentSubject(model->elementToIndex(note), model->idToElement("n0"),
									 Cyberiada::commentSubjectElement, QString()));
	QCOMPARE(scene->subjectLinks(note->get_id()).size(), 0);
}

void TestLinks::drag(ToolType tool, const QPointF& from, const QPointF& to)
{
	QEvent activate(QEvent::WindowActivate);
	scene->setCurrentTool(tool);
	QApplication::sendEvent(scene, &activate);
	sceneMouse(scene, QEvent::GraphicsSceneMousePress, from, Qt::LeftButton);
	sceneMouse(scene, QEvent::GraphicsSceneMouseMove, (from + to) / 2, Qt::LeftButton);
	sceneMouse(scene, QEvent::GraphicsSceneMouseMove, to, Qt::LeftButton);
	sceneMouse(scene, QEvent::GraphicsSceneMouseRelease, to, Qt::NoButton);
}

int TestLinks::subjects(const Cyberiada::ID& comment)
{
	const Cyberiada::Element* e = model->idToElement(QString::fromStdString(comment));
	return e ? int(static_cast<const Cyberiada::Comment*>(e)->get_subjects().size()) : -1;
}

QPointF TestLinks::centre(const Cyberiada::ID& id)
{
	QGraphicsItem* item = scene->getMap().value(id);
	return item ? item->sceneBoundingRect().center() : QPointF();
}

void TestLinks::test_link_draw_tool()
{
	// the transition tool dragged from a comment to a state adds an element subject
	// with both ends stored on the borders; one-shot, undoable (EDIT-TOOL-15, EDGE-18)
	load("diagrams/comment-transition.graphml");
	QCOMPARE(subjects("n2"), 1);
	drag(ToolType::Transition, centre("n2"), centre("n1"));
	QCOMPARE(subjects("n2"), 2);
	QCOMPARE(int(scene->getCurrentTool()), int(ToolType::Select));
	const Cyberiada::Comment* note = static_cast<const Cyberiada::Comment*>(model->idToElement("n2"));
	const Cyberiada::CommentSubject& s = note->get_subjects().at(1);
	QCOMPARE(s.get_element()->get_id(), std::string("n1"));
	QVERIFY(s.has_geometry_source_point() && s.has_geometry_target_point());
	CyberiadaSMEditorSubjectItem* l = link("n2", 1);
	QVERIFY(l);
	QVERIFY(onBorder(scene->getMap().value("n2")->sceneBoundingRect(), l->sourceEnd()));
	QVERIFY(onBorder(scene->getMap().value("n1")->sceneBoundingRect(), l->targetEnd()));
	model->undoStack()->undo();
	QCOMPARE(subjects("n2"), 1);
	QCOMPARE(scene->subjectLinks("n2").size(), 1);
}

void TestLinks::test_link_draw_to_transition()
{
	// a release on the transition line links to the transition: the foot stays where
	// the line was hit, stored from the centre of the transition source
	load("diagrams/comment-transition.graphml");
	CyberiadaSMEditorTransitionItem* t =
		dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value("t0"));
	QVERIFY(t);
	QPainterPath path = t->mapToScene(t->path());
	QPointF hit = path.pointAtPercent(0.05);
	drag(ToolType::Transition, centre("n2"), hit);
	QCOMPARE(subjects("n2"), 2);
	const Cyberiada::Comment* note = static_cast<const Cyberiada::Comment*>(model->idToElement("n2"));
	const Cyberiada::CommentSubject& s = note->get_subjects().at(1);
	QCOMPARE(s.get_element()->get_id(), std::string("t0"));
	QVERIFY(s.has_geometry_target_point());
	QPointF stored = QPointF(s.get_geometry_target_point().x, s.get_geometry_target_point().y) + centre("n0");
	QVERIFY(QLineF(stored, hit).length() < 1.0);
	CyberiadaSMEditorSubjectItem* l = link("n2", 1);
	QVERIFY(l && offPath(path, l->targetEnd()) < 0.5);
}

void TestLinks::test_link_draw_refused()
{
	// the comment itself, the empty canvas and an element of another machine are no
	// subjects: nothing is created and the tool still reverts (EDIT-STRUCT-12)
	load("diagrams/comment-transition.graphml");
	drag(ToolType::Transition, centre("n2"), centre("n2") + QPointF(10, 10));
	QCOMPARE(subjects("n2"), 1);
	QCOMPARE(int(scene->getCurrentTool()), int(ToolType::Select));
	drag(ToolType::Transition, centre("n2"), centre("n2") + QPointF(0, 400));
	QCOMPARE(subjects("n2"), 1);
	QCOMPARE(int(scene->getCurrentTool()), int(ToolType::Select));

	load("diagrams/two-sm-bordered.graphml");
	Cyberiada::ElementCollection* m = static_cast<Cyberiada::ElementCollection*>(model->idToElement("M"));
	QVERIFY(m);
	Cyberiada::Comment* note = model->newComment(m, "a note", Cyberiada::Rect(0, 100, 100, 40));
	QVERIFY(note && scene->getMap().value(note->get_id()));
	drag(ToolType::Transition, centre(note->get_id()), centre("wS"));
	QCOMPARE(subjects(note->get_id()), 0);
	QCOMPARE(int(scene->getCurrentTool()), int(ToolType::Select));
	// within its own machine the same gesture links
	drag(ToolType::Transition, centre(note->get_id()), centre("mS"));
	QCOMPARE(subjects(note->get_id()), 1);
}

void TestLinks::test_press_without_drag()
{
	// a transition-tool click on a comment arms nothing for a later drag: a select
	// drag onto a state draws no link
	load("diagrams/comment-transition.graphml");
	QEvent activate(QEvent::WindowActivate);
	scene->setCurrentTool(ToolType::Transition);
	QApplication::sendEvent(scene, &activate);
	// an empty corner of the note: no text over it, off the border zones
	QRectF box = scene->getMap().value("n2")->sceneBoundingRect();
	QPointF c = box.bottomRight() - QPointF(20, 15);
	sceneMouse(scene, QEvent::GraphicsSceneMousePress, c, Qt::LeftButton);
	sceneMouse(scene, QEvent::GraphicsSceneMouseRelease, c, Qt::NoButton);
	scene->setCurrentTool(ToolType::Select);
	drag(ToolType::Select, c, centre("n1"));
	QCOMPARE(subjects("n2"), 1);
}

QTEST_MAIN(TestLinks)
#include "l4-links.moc"
