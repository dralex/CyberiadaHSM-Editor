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

private:
	CyberiadaSMModel* model = nullptr;
	CyberiadaSMEditorScene* scene = nullptr;
	void load(const QString& path);
	CyberiadaSMEditorSubjectItem* link(const Cyberiada::ID& comment, int index = 0);
};

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

QTEST_MAIN(TestLinks)
#include "l4-links.moc"
