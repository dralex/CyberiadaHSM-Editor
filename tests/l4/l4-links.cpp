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
#include "editable_text_item.h"
#include "settings_manager.h"
#include "subject_fragment.h"
#include "dotsignal.h"
#include <QAbstractTextDocumentLayout>
#include <QTextDocument>

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
	void test_link_handles_shown();
	void test_link_selects_comment();
	void test_link_retarget_by_handle();
	void test_link_delete();
	void test_link_rebased_on_resize();
	void test_link_rebased_on_model_resize();
	void test_fragment_underlined();
	void test_fragment_fallback();
	void test_fragment_wrapped();
	void test_fragment_reconciled();
	void test_comment_selection();
	void test_cleared_link_detached();

private:
	CyberiadaSMModel* model = nullptr;
	CyberiadaSMEditorScene* scene = nullptr;
	void load(const QString& path);
	CyberiadaSMEditorSubjectItem* link(const Cyberiada::ID& comment, int index = 0);
	// a press, two moves and a release under the given tool, as a view would send them
	void drag(ToolType tool, const QPointF& from, const QPointF& to);
	int subjects(const Cyberiada::ID& comment);
	QPointF centre(const Cyberiada::ID& id);
	// the fragment fixture on comment-transition: n0 gets entry/ motor.on() and
	// TIMEOUT [count > 3] / stop(); returns the text item drawing a subject's fragment
	void addFragmentActions();
	EditableTextItem* fragmentText(const Cyberiada::ID& comment, int index, int* from);
	// every underline segment lies on the glyphs of the fragment
	void checkUnderline(CyberiadaSMEditorSubjectItem* l, EditableTextItem* text, int from, int to);
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

void TestLinks::test_link_handles_shown()
{
	// the handles of a selected link show under the selection tool only (EDIT-EDGE-20)
	load("diagrams/comment-transition.graphml");
	CyberiadaSMEditorSubjectItem* l = link("n2");
	QVERIFY(l);
	scene->setCurrentTool(ToolType::Select);
	QVERIFY(!l->sourceDot()->isVisible() && !l->targetDot()->isVisible());
	l->setSelected(true);
	QVERIFY(l->sourceDot()->isVisible() && l->targetDot()->isVisible());
	QCOMPARE(l->sourceDot()->scenePos(), l->sourceEnd());
	QCOMPARE(l->targetDot()->scenePos(), l->targetEnd());
	scene->setCurrentTool(ToolType::Transition);
	QVERIFY(!l->sourceDot()->isVisible() && !l->targetDot()->isVisible());
	scene->setCurrentTool(ToolType::Select);
	QVERIFY(l->sourceDot()->isVisible());
}

void TestLinks::test_link_selects_comment()
{
	// a selected link reports its comment to the tree; the tree echo keeps the link
	load("diagrams/comment-transition.graphml");
	CyberiadaSMEditorSubjectItem* l = link("n2");
	QVERIFY(l);
	scene->clearSelection();
	QSignalSpy spy(scene, &CyberiadaSMEditorScene::elementSelected);
	l->setSelected(true);
	QVERIFY(spy.count() >= 1);
	QModelIndex reported = spy.last().at(0).value<QModelIndex>();
	QCOMPARE(model->indexToElement(reported), model->idToElement("n2"));
	scene->slotElementSelected(reported);
	QVERIFY(l->isSelected());
	QVERIFY(!scene->getMap().value("n2")->isSelected());
}

void TestLinks::test_link_retarget_by_handle()
{
	// the target handle dropped on a state retargets the subject there, the foot on
	// its border; the link item stays (EDIT-EDGE-20)
	load("diagrams/comment-transition.graphml");
	CyberiadaSMEditorSubjectItem* l = link("n2");
	QVERIFY(l);
	scene->setCurrentTool(ToolType::Select);
	l->setSelected(true);
	QPointF from = l->targetDot()->scenePos();
	QPointF to = centre("n0") + QPointF(10, 10);
	drag(ToolType::Select, from, to);
	const Cyberiada::Comment* note = static_cast<const Cyberiada::Comment*>(model->idToElement("n2"));
	QCOMPARE(note->get_subjects().size(), size_t(1));
	QCOMPARE(note->get_subjects().at(0).get_element()->get_id(), std::string("n0"));
	QVERIFY(note->get_subjects().at(0).has_geometry_target_point());
	QCOMPARE(link("n2"), l);
	QVERIFY(onBorder(scene->getMap().value("n0")->sceneBoundingRect(), l->targetEnd()));
	// the undo restores a snapshot: the elements are new
	model->undoStack()->undo();
	note = static_cast<const Cyberiada::Comment*>(model->idToElement("n2"));
	QVERIFY(note);
	QCOMPARE(note->get_subjects().at(0).get_element()->get_id(), std::string("t0"));
}

void TestLinks::test_link_delete()
{
	// deleting a link removes its subject, the comment stays; undo brings it back
	load("diagrams/comment-transition.graphml");
	CyberiadaSMEditorSubjectItem* l = link("n2");
	QVERIFY(l);
	QVERIFY(l->deleteSubject());
	QCOMPARE(subjects("n2"), 0);
	QCOMPARE(scene->subjectLinks("n2").size(), 0);
	QVERIFY(scene->getMap().value("n2"));
	QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	model->undoStack()->undo();
	QCOMPARE(subjects("n2"), 1);
	QCOMPARE(scene->subjectLinks("n2").size(), 1);
}

void TestLinks::test_link_rebased_on_resize()
{
	// a stored source point keeps its place when the comment grows to the right, and
	// stays on the border (EDIT-EDGE-19)
	load("diagrams/comment-transition.graphml");
	CyberiadaSMEditorSubjectItem* l = link("n2");
	QVERIFY(l);
	QPointF before = l->sourceEnd();
	QRectF box = scene->getMap().value("n2")->sceneBoundingRect();
	QPointF at(box.right() - 3, box.center().y() + 20);
	scene->clearSelection();
	drag(ToolType::Select, at, at + QPointF(40, 0));
	QVERIFY(scene->getMap().value("n2")->sceneBoundingRect().width() > box.width());
	l = link("n2");
	QVERIFY(l);
	QVERIFY(QLineF(l->sourceEnd(), before).length() < 0.5);
	QVERIFY(onBorder(scene->getMap().value("n2")->sceneBoundingRect(), l->sourceEnd()));
}

void TestLinks::test_link_rebased_on_model_resize()
{
	// a stored source point stays on the comment border when the comment is resized
	// THROUGH THE MODEL (the move verb), not only by an interactive drag (EDIT-EDGE-19)
	load("diagrams/comment-transition.graphml");
	CyberiadaSMEditorSubjectItem* l = link("n2");
	QVERIFY(l);
	Cyberiada::Element* note = model->idToElement("n2");
	Cyberiada::Rect r = static_cast<Cyberiada::Comment*>(note)->get_geometry_rect();
	// grow the comment taller: the top-border source point would otherwise fall inside
	QVERIFY(model->updateGeometry(model->elementToIndex(note),
	        Cyberiada::Rect(r.x, r.y, r.width, r.height + 120)));
	l = link("n2");
	QVERIFY(l);
	QVERIFY(onBorder(scene->getMap().value("n2")->sceneBoundingRect(), l->sourceEnd()));
}

QTEST_MAIN(TestLinks)

void TestLinks::addFragmentActions()
{
	QModelIndex si = model->elementToIndex(model->idToElement("n0"));
	QVERIFY(model->newAction(si, Cyberiada::actionEntry, QString(), QString(), "motor.on()"));
	QVERIFY(model->newAction(si, Cyberiada::actionTransition, "TIMEOUT", "count > 3", "stop()"));
}

EditableTextItem* TestLinks::fragmentText(const Cyberiada::ID& comment, int index, int* from)
{
	const Cyberiada::Comment* c = static_cast<const Cyberiada::Comment*>(model->idToElement(QString::fromStdString(comment)));
	if (!c || size_t(index) >= c->get_subjects().size()) return nullptr;
	const Cyberiada::CommentSubject& s = c->get_subjects().at(size_t(index));
	SubjectFragment::Place place = SubjectFragment::locate(s.get_element(), s.get_type(),
														   QString::fromStdString(s.get_fragment()));
	CyberiadaSMEditorAbstractItem* owner =
		dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene->getMap().value(s.get_element()->get_id()));
	int start = -1;
	EditableTextItem* text = place.found() && owner ? owner->fieldText(place.field, &start) : nullptr;
	*from = start + place.offset;
	return text;
}

void TestLinks::checkUnderline(CyberiadaSMEditorSubjectItem* l, EditableTextItem* text, int from, int to)
{
	QVector<QLineF> lines = l->underline();
	QVERIFY(!lines.isEmpty());
	QRectF box = text->sceneBoundingRect().adjusted(-1, -1, 1, 1);
	qreal ascent = QFontMetricsF(text->font()).ascent();
	for (const QLineF& line : lines) {
		QVERIFY(box.contains(line.p1()) && box.contains(line.p2()));
		QCOMPARE(line.p1().y(), line.p2().y());
		// the glyphs over the segment ends and middle belong to the fragment
		for (qreal t : { 0.1, 0.5, 0.9 }) {
			QPointF p = text->mapFromScene(line.pointAt(t) - QPointF(0, ascent / 2));
			int pos = text->document()->documentLayout()->hitTest(p, Qt::FuzzyHit);
			QVERIFY2(pos >= from && pos <= to, qPrintable(QString("%1 not in [%2, %3]").arg(pos).arg(from).arg(to)));
		}
	}
	QCOMPARE(l->targetEnd(), lines.first().center());
}

void TestLinks::test_fragment_underlined()
{
	// a fragment of a behaviour, a guard, a name and a transition label is
	// underlined in its text and the link ends at the middle (EDIT-EDGE-21)
	load("diagrams/comment-transition.graphml");
	addFragmentActions();
	QModelIndex ni = model->elementToIndex(model->idToElement("n2"));
	QVERIFY(model->newCommentSubject(ni, model->idToElement("n0"), Cyberiada::commentSubjectData, "stop()"));
	QVERIFY(model->newCommentSubject(ni, model->idToElement("n0"), Cyberiada::commentSubjectData, "count"));
	QVERIFY(model->newCommentSubject(ni, model->idToElement("n1"), Cyberiada::commentSubjectName, "Second"));
	QVERIFY(model->newCommentSubject(ni, model->idToElement("t0"), Cyberiada::commentSubjectData, "go()"));
	const char* fragments[] = { "stop()", "count", "Second", "go()" };
	for (int i = 1; i <= 4; i++) {
		CyberiadaSMEditorSubjectItem* l = link("n2", i);
		QVERIFY(l);
		int from = -1;
		EditableTextItem* text = fragmentText("n2", i, &from);
		QVERIFY(text);
		QCOMPARE(text->toPlainText().mid(from, int(strlen(fragments[i - 1]))), QString(fragments[i - 1]));
		checkUnderline(l, text, from, from + int(strlen(fragments[i - 1])));
		QCOMPARE(l->underline().size(), 1);
	}
	// an element link has no underline
	QVERIFY(link("n2", 0)->underline().isEmpty());
}

void TestLinks::test_fragment_fallback()
{
	// a text being edited or hidden leaves the end on the border (EDIT-EDGE-21)
	load("diagrams/comment-transition.graphml");
	addFragmentActions();
	QModelIndex ni = model->elementToIndex(model->idToElement("n2"));
	QVERIFY(model->newCommentSubject(ni, model->idToElement("n0"), Cyberiada::commentSubjectData, "stop()"));
	QVERIFY(model->newCommentSubject(ni, model->idToElement("t0"), Cyberiada::commentSubjectData, "go()"));
	int from = -1;
	EditableTextItem* text = fragmentText("n2", 1, &from);
	QVERIFY(text && !link("n2", 1)->underline().isEmpty());
	text->startEditing();
	QVERIFY(link("n2", 1)->underline().isEmpty());
	QVERIFY(onBorder(scene->getMap().value("n0")->sceneBoundingRect(), link("n2", 1)->targetEnd()));
	text->clearFocus();
	QVERIFY(!link("n2", 1)->underline().isEmpty());
	// the transition text toggled off: the end goes back onto the path
	SettingsManager::instance().setShowTransitionText(false);
	QCoreApplication::sendPostedEvents();
	CyberiadaSMEditorSubjectItem* l = link("n2", 2);
	QVERIFY(l && l->underline().isEmpty());
	CyberiadaSMEditorTransitionItem* t = dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value("t0"));
	QVERIFY(offPath(t->mapToScene(t->path()), l->targetEnd()) < 0.5);
	SettingsManager::instance().setShowTransitionText(true);
	QCoreApplication::sendPostedEvents();
	QVERIFY(!link("n2", 2)->underline().isEmpty());
	// no text drawn at all
	SettingsManager::instance().setShowText(false);
	scene->loadScene();
	QVERIFY(link("n2", 1)->underline().isEmpty());
	QVERIFY(onBorder(scene->getMap().value("n0")->sceneBoundingRect(), link("n2", 1)->targetEnd()));
	SettingsManager::instance().setShowText(true);
	scene->loadScene();
	QVERIFY(!link("n2", 1)->underline().isEmpty());
}

void TestLinks::test_fragment_wrapped()
{
	// a fragment wrapped over the lines of its text is underlined on each
	load("diagrams/comment-transition.graphml");
	QString longText = "alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu";
	QModelIndex si = model->elementToIndex(model->idToElement("n0"));
	QVERIFY(model->newAction(si, Cyberiada::actionEntry, QString(), QString(), longText));
	QModelIndex ni = model->elementToIndex(model->idToElement("n2"));
	QVERIFY(model->newCommentSubject(ni, model->idToElement("n0"), Cyberiada::commentSubjectData, longText));
	CyberiadaSMEditorSubjectItem* l = link("n2", 1);
	int from = -1;
	EditableTextItem* text = fragmentText("n2", 1, &from);
	QVERIFY(l && text);
	QVERIFY(l->underline().size() >= 2);
	checkUnderline(l, text, from, from + longText.length());
}

void TestLinks::test_fragment_reconciled()
{
	// a rename dropping a name fragment draws an element link with its box; the
	// undo brings the underline back (EDIT-STRUCT-13)
	load("diagrams/comment-transition.graphml");
	QModelIndex ni = model->elementToIndex(model->idToElement("n2"));
	QVERIFY(model->newCommentSubject(ni, model->idToElement("n1"), Cyberiada::commentSubjectName, "Second"));
	QVERIFY(!link("n2", 1)->underline().isEmpty());
	QVERIFY(model->updateTitle(model->elementToIndex(model->idToElement("n1")), "Other state"));
	CyberiadaSMEditorSubjectItem* l = link("n2", 1);
	QVERIFY(l && !l->subject().has_fragment());
	QVERIFY(l->underline().isEmpty());
	QVERIFY(onBorder(scene->getMap().value("n1")->sceneBoundingRect(), l->targetEnd()));
	model->undoStack()->undo();
	l = link("n2", 1);
	QVERIFY(l && l->subject().has_fragment());
	QVERIFY(!l->underline().isEmpty());
}

#include "l4-links.moc"

void TestLinks::test_comment_selection()
{
	// a selection in one field of a text being edited links a new comment beside
	// the element, or an existing one, in one undo step; a selection across the
	// fields is refused (EDIT-TOOL-17)
	load("diagrams/comment-transition.graphml");
	addFragmentActions();
	SubjectFragment::Field internal;
	internal.kind = SubjectFragment::fieldBehaviour;
	internal.action = 1;
	CyberiadaSMEditorAbstractItem* state = dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene->getMap().value("n0"));
	int start = -1;
	EditableTextItem* text = state->fieldText(internal, &start);
	QVERIFY(text);
	QCOMPARE(text->toPlainText(), QString("TIMEOUT [count > 3] / stop()"));
	text->startEditing();
	QTextCursor c = text->textCursor();
	c.setPosition(0);
	c.setPosition(text->toPlainText().length(), QTextCursor::KeepAnchor);
	text->setTextCursor(c);
	Cyberiada::Element* target = nullptr;
	Cyberiada::CommentSubjectType type;
	QString fragment;
	QVERIFY(!scene->selectionFragment(text, &target, &type, &fragment));
	c.setPosition(start);
	c.setPosition(start + 6, QTextCursor::KeepAnchor);
	text->setTextCursor(c);
	QVERIFY(scene->selectionFragment(text, &target, &type, &fragment));
	QCOMPARE(fragment, QString("stop()"));
	QCOMPARE(type, Cyberiada::commentSubjectData);
	QVERIFY(scene->selectionComments(text).size() == 1);   // the note n2

	int steps = model->undoStack()->count();
	int elements = int(scene->getMap().size());
	QVERIFY(scene->commentSelection(text, Cyberiada::ID()));
	QCOMPARE(model->undoStack()->count(), steps + 1);
	QCOMPARE(int(scene->getMap().size()), elements + 1);
	// the new comment sits beside the state, clear of it, and is linked
	Cyberiada::Comment* fresh = nullptr;
	for (Cyberiada::Element* e : static_cast<Cyberiada::ElementCollection*>(model->idToElement("G0"))->get_children()) {
		if (Cyberiada::element_is_comment(e) && e->get_id() != "n2" && e->get_id() != "nMeta") fresh = static_cast<Cyberiada::Comment*>(e);
	}
	QVERIFY(fresh && fresh->has_subjects());
	QCOMPARE(QString::fromStdString(fresh->get_subjects().at(0).get_fragment()), QString("stop()"));
	QRectF stateBox = scene->getMap().value("n0")->sceneBoundingRect();
	QRectF freshBox = scene->getMap().value(fresh->get_id())->sceneBoundingRect();
	QVERIFY(!stateBox.intersects(freshBox));
	QVERIFY(!link(fresh->get_id(), 0)->underline().isEmpty());
	// a second new comment takes another free slot, clear of the first
	state = dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene->getMap().value("n0"));
	text = state->fieldText(internal, &start);
	text->startEditing();
	c = text->textCursor();
	c.setPosition(start);
	c.setPosition(start + 6, QTextCursor::KeepAnchor);
	text->setTextCursor(c);
	Cyberiada::ID first = fresh->get_id();
	QVERIFY(scene->commentSelection(text, Cyberiada::ID()));
	QCOMPARE(int(scene->getMap().size()), elements + 2);
	for (QGraphicsItem* item : scene->getMap().values()) {
		CyberiadaSMEditorAbstractItem* other = dynamic_cast<CyberiadaSMEditorAbstractItem*>(item);
		if (!other || other->getId() == first || !Cyberiada::element_is_comment(other->getElement()) ||
			!other->getElement()->has_geometry()) continue;
		QVERIFY(!other->sceneBoundingRect().intersects(scene->getMap().value(first)->sceneBoundingRect()));
	}
	model->undoStack()->undo();
	// the undo takes the comment and its link away together
	model->undoStack()->undo();
	QCOMPARE(int(scene->getMap().size()), elements);

	// an existing comment of the machine; the edit has ended
	state = dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene->getMap().value("n0"));
	SubjectFragment::Field name;
	text = state->fieldText(name, &start);
	QVERIFY(text);
	text->startEditing();
	c = text->textCursor();
	c.setPosition(0);
	c.setPosition(5, QTextCursor::KeepAnchor);
	text->setTextCursor(c);
	QVERIFY(scene->commentSelection(text, "n2"));
	QCOMPARE(subjects("n2"), 2);
	const Cyberiada::Comment* note = static_cast<const Cyberiada::Comment*>(model->idToElement("n2"));
	QCOMPARE(note->get_subjects().at(1).get_type(), Cyberiada::commentSubjectName);
	QCOMPARE(QString::fromStdString(note->get_subjects().at(1).get_fragment()), QString("First"));
	QVERIFY(scene->focusItem() == nullptr || !dynamic_cast<EditableTextItem*>(scene->focusItem()) ||
			!dynamic_cast<EditableTextItem*>(scene->focusItem())->isEditing());

	// a comment's own body is not offered to itself
	CyberiadaSMEditorAbstractItem* noteItem = dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene->getMap().value("n2"));
	SubjectFragment::Field body;
	body.kind = SubjectFragment::fieldBody;
	text = noteItem->fieldText(body, &start);
	QVERIFY(text);
	text->startEditing();
	QVERIFY(scene->selectionComments(text).isEmpty());
	text->clearFocus();
}

void TestLinks::test_cleared_link_detached()
{
	// a link cleared before its comment is freed leaves its ends at once: a move of
	// the target before the deferred deletion reads nothing of the freed comment
	load("diagrams/comment-transition.graphml");
	QModelIndex ni = model->elementToIndex(model->idToElement("n2"));
	QVERIFY(model->newCommentSubject(ni, model->idToElement("n1"), Cyberiada::commentSubjectElement, QString()));
	QPointer<CyberiadaSMEditorSubjectItem> old = link("n2", 1);
	QVERIFY(old && old->hasSubject());
	QVERIFY(model->deleteElement(ni));
	QVERIFY(old);                      // the deletion is deferred
	QVERIFY(!old->hasSubject());
	QVERIFY(!old->scene());
	Cyberiada::Element* second = model->idToElement("n1");
	Cyberiada::Rect r = static_cast<Cyberiada::State*>(second)->get_geometry_rect();
	QVERIFY(model->updateGeometry(model->elementToIndex(second), Cyberiada::Rect(r.x + 30, r.y, r.width, r.height)));
	QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

