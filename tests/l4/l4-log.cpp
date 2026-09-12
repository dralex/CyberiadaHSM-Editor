/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process session logger test: the recorded script, the gesture
 * suppression and the crash marker (see docs/TESTING.md, L4)
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
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QGraphicsSceneMouseEvent>
#include <QApplication>
#include <sstream>

#include "cyberiadasm_editor_scene.h"

#include "smeditor_window.h"
#include "cyberiadasm_model.h"
#include "gesture_log.h"
#include "batch_script.h"
#include "cyberiadasm_dump.h"

class TestLog: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void test_session_from_scratch();
	void test_gesture_recording();
	void test_gesture_suppression();
	void test_crash_marker();

private:
	QString readScript(const QString& dir) const;
	QTemporaryDir logRoot;
};

QString TestLog::readScript(const QString& dir) const
{
	QFile f(QDir(dir).filePath("session.script"));
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
	return QTextStream(&f).readAll();
}

void TestLog::initTestCase()
{
	QVERIFY(logRoot.isValid());
	qputenv("CYBERIADA_SESSION_LOG_DIR", logRoot.path().toUtf8());
}

// a from-scratch session records new-sm first and replays end-to-end
void TestLog::test_session_from_scratch()
{
	CyberiadaSMEditorWindow win;
	CyberiadaSMModel* model = win.getModel();

	GestureLog::instance().startSession(model);
	QVERIFY(GestureLog::instance().isActive());
	QString dir = GestureLog::instance().sessionDir();

	// a fresh launch has no document: the snapshot is empty and new-sm
	// bootstraps the machine
	Cyberiada::StateMachine* sm = model->newStateMachine("Main", Cyberiada::Rect(0, 0, 520, 320));
	QVERIFY(sm);
	Cyberiada::State* a = model->newState(sm, "First", Cyberiada::Action(), Cyberiada::Rect(-100, -50, 200, 100));
	Cyberiada::State* b = model->newState(sm, "Second", Cyberiada::Action(), Cyberiada::Rect(120, -50, 200, 100));
	QVERIFY(a && b);
	Cyberiada::Transition* t = model->newTransition(sm, Cyberiada::transitionExternal, a, b, Cyberiada::Action());
	QVERIFY(t);
	QVERIFY(model->updateTitle(model->elementToIndex(a), "Renamed"));
	QVERIFY(model->updateLabel(model->elementToIndex(t), Cyberiada::Point(30, -40)));

	GestureLog::instance().endSession();
	QVERIFY(!GestureLog::instance().isActive());

	QString script = readScript(dir);
	// the header, the empty-document marker and the start/exit lines
	QVERIFY(script.contains("# Cyberiada State Machine Editor "));
	QVERIFY(script.contains("# document (empty)"));
	QVERIFY(script.contains("\n# start "));
	QVERIFY(script.contains("\n# exit "));
	// the from-scratch machine and the edits, in order
	QVERIFY(script.contains("new-sm 0 0 520 320 Main"));
	QVERIFY(script.contains("\nnew-state G0 -100 -50 200 100 First"));
	QVERIFY(script.contains("\nnew-transition G0 n0 n1"));
	QVERIFY(script.contains("\nrename n0 Renamed"));
	QVERIFY(script.contains("\nlabel n0-n1 30 -40"));
	// the from-scratch session has no start snapshot
	QVERIFY(!QFile::exists(QDir(dir).filePath("start.graphml")));

	// replay the recorded script on a fresh, empty window and compare the
	// documents: the session reproduces exactly
	CyberiadaSMEditorWindow replay;
	QString error;
	QVERIFY2(runEditScript(&replay, QDir(dir).filePath("session.script"), &error),
			 qPrintable(error));

	std::ostringstream original, replayed;
	dumpDocument(model, original);
	dumpDocument(replay.getModel(), replayed);
	QCOMPARE(QString::fromStdString(replayed.str()), QString::fromStdString(original.str()));
}

// a left-button gesture reaches the scene as press/drag/release; the moves are
// decimated to a few pixels
static void sendMouse(CyberiadaSMEditorScene* scene, QEvent::Type type, const QPointF& pos,
					  Qt::MouseButtons buttons)
{
	QGraphicsSceneMouseEvent event(type);
	event.setScenePos(pos);
	event.setButton(Qt::LeftButton);
	event.setButtons(buttons);
	QApplication::sendEvent(scene, &event);
}

void TestLog::test_gesture_recording()
{
	CyberiadaSMEditorWindow win;
	CyberiadaSMEditorScene* scene = win.getScene();

	GestureLog::instance().startSession(win.getModel());
	QString dir = GestureLog::instance().sessionDir();

	sendMouse(scene, QEvent::GraphicsSceneMousePress, QPointF(10, 10), Qt::LeftButton);
	sendMouse(scene, QEvent::GraphicsSceneMouseMove, QPointF(40, 40), Qt::LeftButton);
	sendMouse(scene, QEvent::GraphicsSceneMouseRelease, QPointF(40, 40), Qt::NoButton);

	GestureLog::instance().endSession();

	QString script = readScript(dir);
	QVERIFY(script.contains("\npress 10 10"));
	QVERIFY(script.contains("\ndrag 40 40"));
	QVERIFY(script.contains("\nrelease 40 40"));
}

// a model edit during a mouse gesture is recorded as the gesture, not twice as
// a semantic verb; outside a gesture it is logged
void TestLog::test_gesture_suppression()
{
	CyberiadaSMEditorWindow win;
	CyberiadaSMModel* model = win.getModel();
	Cyberiada::StateMachine* sm = model->newStateMachine("SM");
	Cyberiada::State* s = model->newState(sm, "S");

	GestureLog::instance().startSession(model);
	QString dir = GestureLog::instance().sessionDir();

	GestureLog::instance().enterGesture();
	QVERIFY(model->updateTitle(model->elementToIndex(s), "Suppressed"));
	GestureLog::instance().leaveGesture();
	QVERIFY(model->updateTitle(model->elementToIndex(s), "Logged"));

	GestureLog::instance().endSession();

	QString script = readScript(dir);
	QVERIFY(!script.contains("Suppressed"));
	QVERIFY(script.contains("rename n0 Logged"));
}

// a crash leaves the start line without a matching exit line
void TestLog::test_crash_marker()
{
	CyberiadaSMEditorWindow win;
	CyberiadaSMModel* model = win.getModel();

	GestureLog::instance().startSession(model);
	QString dir = GestureLog::instance().sessionDir();
	model->newStateMachine("SM");
	// deliberately do not call endSession: the process "crashed"

	QString script = readScript(dir);
	QVERIFY(script.contains("\n# start "));
	QVERIFY(!script.contains("# exit "));

	// leave the singleton clean for any later case
	GestureLog::instance().endSession();
}

QTEST_MAIN(TestLog)
#include "l4-log.moc"
