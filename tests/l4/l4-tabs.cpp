/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process window test: the documents and the tab line (see docs/TESTING.md, L4)
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
#include <QTabBar>
#include <QMessageBox>
#include <QPushButton>
#include <QGraphicsItem>
#include "smeditor_window.h"
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_view.h"
#include "cyberiadasm_editor_scene.h"
#include "gesture_log.h"

// the EDIT-DOC laws at the widget level: the tab line, the re-targeting of the
// panels and the actions, the close prompts, the shared clipboard
class TestTabs: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void test_start_single();
	void test_new_opens_tab();
	void test_open_policy();
	void test_switch_retargets();
	void test_zoom_per_document();
	void test_tool_follows();
	void test_clipboard_shared();
	void test_close_prompt_discard();
	void test_close_last_leaves_untitled();
	void test_exit_activates_dirty();
	void test_log_session_rebinds();
	void test_open_files();

private:
	// answer the pending modal prompt with the button once it is up
	void answerPrompt(QMessageBox::StandardButton button);
	static int stateCount(CyberiadaSMModel* model);

	CyberiadaSMEditorWindow* window;
	QTemporaryDir logRoot;
};

void TestTabs::answerPrompt(QMessageBox::StandardButton button)
{
	QTimer::singleShot(50, [button]() {
		QMessageBox* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
		if (box) box->button(button)->click();
	});
}

int TestTabs::stateCount(CyberiadaSMModel* model)
{
	if (!model->rootDocument()) return 0;
	return int(model->rootDocument()->find_elements_by_type(Cyberiada::elementSimpleState).size());
}

void TestTabs::initTestCase()
{
	QVERIFY(logRoot.isValid());
	qputenv("CYBERIADA_SESSION_LOG_DIR", logRoot.path().toUtf8());
	window = new CyberiadaSMEditorWindow();
	window->resize(1200, 800);
	window->show();
	QVERIFY(QTest::qWaitForWindowExposed(window));
}

// EDIT-DOC-1: one untitled document, no tab line
void TestTabs::test_start_single()
{
	QCOMPARE(window->documentCount(), 1);
	QVERIFY(!window->tabBar()->isVisible());
	QVERIFY(window->windowTitle().contains(tr("untitled")));
	QVERIFY(window->currentDocument()->isUntitled());
}

// EDIT-DOC-2: New is a new tab without a prompt; the line appears
void TestTabs::test_new_opens_tab()
{
	CyberiadaSMModel* first = window->getModel();
	window->actionNew->trigger();
	QCOMPARE(window->documentCount(), 2);
	QVERIFY(window->tabBar()->isVisible());
	QVERIFY(window->getModel() != first);
	QVERIFY(window->windowTitle().contains(tr("untitled")));
	// closing the clean new tab needs no prompt and hides the line again
	window->actionClose->trigger();
	QCOMPARE(window->documentCount(), 1);
	QCOMPARE(window->getModel(), first);
	QVERIFY(!window->tabBar()->isVisible());
}

// EDIT-DOC-2: a clean untitled tab takes the file, an edited one gets a new
// tab, an open file activates its tab
void TestTabs::test_open_policy()
{
	QVERIFY(window->openFile("diagrams/geometry.graphml"));
	QCOMPARE(window->documentCount(), 1);
	QCOMPARE(window->currentDocument()->title(), QString("geometry.graphml"));
	CyberiadaSMModel* geometry = window->getModel();
	QVERIFY(geometry->updateTitle(geometry->elementToIndex(geometry->idToElement("node-0-1")), "Renamed"));
	QVERIFY(window->openFile("diagrams/hierarchy.graphml"));
	QCOMPARE(window->documentCount(), 2);
	QCOMPARE(window->currentDocument()->title(), QString("hierarchy.graphml"));
	QCOMPARE(window->tabBar()->currentIndex(), 1);
	QVERIFY(window->openFile("diagrams/geometry.graphml"));
	QCOMPARE(window->documentCount(), 2);
	QCOMPARE(window->getModel(), geometry);
	QCOMPARE(window->tabBar()->currentIndex(), 0);
	QCOMPARE(window->tabBar()->tabText(0), QString("geometry.graphml*"));
	QCOMPARE(window->tabBar()->tabText(1), QString("hierarchy.graphml"));
}

// EDIT-DOC-3: the tree, the undo actions and the title follow the active tab
void TestTabs::test_switch_retargets()
{
	CyberiadaSMModel* geometry = window->documentAt(0)->model();
	CyberiadaSMModel* hierarchy = window->documentAt(1)->model();
	QCOMPARE(window->SMView->model(), geometry);
	QVERIFY(window->actionUndo->isEnabled());
	QCOMPARE(window->actionUndo->text(), QString("Undo rename"));
	QVERIFY(window->isWindowModified());

	window->tabBar()->setCurrentIndex(1);
	QCOMPARE(window->getModel(), hierarchy);
	QCOMPARE(window->SMView->model(), hierarchy);
	QCOMPARE(window->SMView->rootIndex(), hierarchy->rootIndex());
	QVERIFY(!window->actionUndo->isEnabled());
	QVERIFY(!window->isWindowModified());
	QVERIFY(window->windowTitle().contains("hierarchy"));
	QCOMPARE(window->sceneView, window->documentAt(1)->view());

	window->tabBar()->setCurrentIndex(0);
	QCOMPARE(window->getModel(), geometry);
	QVERIFY(window->actionUndo->isEnabled());
	QVERIFY(window->isWindowModified());
	// the background document was not touched by the switches
	QCOMPARE(QString(geometry->idToElement("node-0-1")->get_name().c_str()), QString("Renamed"));
}

// EDIT-DOC-3: the zoom is a property of the document view
void TestTabs::test_zoom_per_document()
{
	window->sceneView->setScale(2.0);
	QCOMPARE(window->sceneView->currentScale(), 2.0);
	window->tabBar()->setCurrentIndex(1);
	QVERIFY(window->sceneView->currentScale() != 2.0);
	window->tabBar()->setCurrentIndex(0);
	QCOMPARE(window->sceneView->currentScale(), 2.0);
}

// EDIT-DOC-3: the armed tool reaches the newly active scene
void TestTabs::test_tool_follows()
{
	window->actionNewState->trigger();
	QCOMPARE(window->getScene()->getCurrentTool(), ToolType::NewState);
	window->tabBar()->setCurrentIndex(1);
	QCOMPARE(window->getScene()->getCurrentTool(), ToolType::NewState);
	QVERIFY(window->actionNewState->isChecked());
	window->actionSelectTool->trigger();
	QCOMPARE(window->getScene()->getCurrentTool(), ToolType::Select);
	window->tabBar()->setCurrentIndex(0);
	QCOMPARE(window->getScene()->getCurrentTool(), ToolType::Select);
}

// EDIT-DOC-5: a state copied in one document is pasted into the other
void TestTabs::test_clipboard_shared()
{
	CyberiadaSMModel* geometry = window->documentAt(0)->model();
	CyberiadaSMModel* hierarchy = window->documentAt(1)->model();
	// the load selected the machine through the tree: one selected item only
	window->getScene()->clearSelection();
	QGraphicsItem* state = window->getScene()->getMap().value("node-0-1");
	QVERIFY(state);
	state->setSelected(true);
	QCoreApplication::processEvents();   // the selection reaches the actions
	QVERIFY(window->actionCopy->isEnabled());
	window->actionCopy->trigger();
	QVERIFY(window->hasClipboard());
	int before = stateCount(hierarchy);
	window->tabBar()->setCurrentIndex(1);
	QVERIFY(window->actionPaste->isEnabled());
	window->actionPaste->trigger();
	QCOMPARE(stateCount(hierarchy), before + 1);
	QVERIFY(window->isWindowModified());
	QCOMPARE(window->tabBar()->tabText(1), QString("hierarchy.graphml*"));
}

// EDIT-DOC-4: the prompt is about the closing document; Discard drops it
void TestTabs::test_close_prompt_discard()
{
	QCOMPARE(window->documentCount(), 2);
	QCOMPARE(window->currentDocument()->title(), QString("hierarchy.graphml"));
	answerPrompt(QMessageBox::Cancel);
	window->actionClose->trigger();
	QCOMPARE(window->documentCount(), 2);
	answerPrompt(QMessageBox::Discard);
	window->actionClose->trigger();
	QCOMPARE(window->documentCount(), 1);
	QCOMPARE(window->currentDocument()->title(), QString("geometry.graphml"));
	QVERIFY(!window->tabBar()->isVisible());
}

// EDIT-DOC-4: the last tab gives way to a fresh untitled document
void TestTabs::test_close_last_leaves_untitled()
{
	QVERIFY(window->closeDocument(window->currentDocument(), true));
	QCOMPARE(window->documentCount(), 1);
	QVERIFY(window->currentDocument()->isUntitled());
	QVERIFY(window->windowTitle().contains(tr("untitled")));
	QVERIFY(!window->isWindowModified());
	QVERIFY(!window->actionUndo->isEnabled());
}

// EDIT-DOC-4: Exit activates each modified document for its prompt; Cancel keeps the editor
void TestTabs::test_exit_activates_dirty()
{
	QVERIFY(window->openFile("diagrams/geometry.graphml"));
	CyberiadaSMModel* geometry = window->getModel();
	window->actionNew->trigger();
	QCOMPARE(window->tabBar()->currentIndex(), 1);
	// the background document gets dirty
	QVERIFY(geometry->updateTitle(geometry->elementToIndex(geometry->idToElement("node-0-1")), "Dirty"));
	QVERIFY(!window->isWindowModified());
	answerPrompt(QMessageBox::Cancel);
	QVERIFY(!window->close());
	QCOMPARE(window->tabBar()->currentIndex(), 0);
	QVERIFY(window->isWindowModified());
	geometry->undoStack()->setClean();
}

// EDIT-DOC-8: a tab switch while logging starts a new session on the new document
void TestTabs::test_log_session_rebinds()
{
	GestureLog::instance().startSession(window->getModel());
	QVERIFY(GestureLog::instance().isActive());
	QString first = GestureLog::instance().sessionDir();
	window->tabBar()->setCurrentIndex(1);
	QVERIFY(GestureLog::instance().isActive());
	QVERIFY(GestureLog::instance().sessionDir() != first);
	GestureLog::instance().endSession();
}

// EDIT-DOC-7: the command-line files open as tabs, the last one active
void TestTabs::test_open_files()
{
	CyberiadaSMEditorWindow fresh;
	QString error;
	QVERIFY(fresh.openFiles(QStringList() << "diagrams/geometry.graphml" << "diagrams/hierarchy.graphml", &error));
	QCOMPARE(fresh.documentCount(), 2);
	QCOMPARE(fresh.currentDocument()->title(), QString("hierarchy.graphml"));
	// a missing file is reported and skipped, the rest stay open
	QVERIFY(!fresh.openFiles(QStringList() << "diagrams/missing.graphml", &error));
	QVERIFY(error.contains("missing.graphml"));
	QCOMPARE(fresh.documentCount(), 2);
}

QTEST_MAIN(TestTabs)
#include "l4-tabs.moc"
