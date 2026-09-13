/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process window test: the undo actions and the modified state (see docs/TESTING.md, L4)
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
#include "smeditor_window.h"
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_view.h"

class TestWindow: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void test_undo_actions();
	void test_modified_state();
	void test_creation_tools_arm();
	void test_view_roundtrip();
	void test_menu_refactor();

private:
	CyberiadaSMEditorWindow* window;
	CyberiadaSMModel* model;
};

void TestWindow::initTestCase()
{
	window = new CyberiadaSMEditorWindow();
	model = window->getModel();
	QVERIFY(window->openDocument("diagrams/geometry.graphml"));
}

void TestWindow::test_undo_actions()
{
	// the actions follow the stack: enabled state, text, and effect
	QVERIFY(!window->actionUndo->isEnabled());
	QVERIFY(!window->actionRedo->isEnabled());
	QModelIndex index = model->elementToIndex(model->idToElement("node-0-1"));
	QVERIFY(model->updateTitle(index, "Undone"));
	QVERIFY(window->actionUndo->isEnabled());
	QCOMPARE(window->actionUndo->text(), QString("Undo rename"));
	window->actionUndo->trigger();
	QCOMPARE(QString(model->idToElement("node-0-1")->get_name().c_str()), QString("node 0-1"));
	QVERIFY(!window->actionUndo->isEnabled());
	QVERIFY(window->actionRedo->isEnabled());
	QCOMPARE(window->actionRedo->text(), QString("Redo rename"));
	window->actionRedo->trigger();
	QCOMPARE(QString(model->idToElement("node-0-1")->get_name().c_str()), QString("Undone"));
	// the tree follows the restored document
	QCOMPARE(window->SMView->rootIndex(), model->rootIndex());
}

void TestWindow::test_modified_state()
{
	// the title marker follows the clean state of the stack
	QVERIFY(window->isWindowModified());
	QVERIFY(window->windowTitle().contains("[*]"));
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	model->saveAsDocument(dir.filePath("window.graphml"), Cyberiada::formatCyberiada10);
	QVERIFY(!window->isWindowModified());
	QVERIFY(model->updateTitle(model->elementToIndex(model->idToElement("node-0-1")), "Dirty"));
	QVERIFY(window->isWindowModified());
	model->undoStack()->undo();
	QVERIFY(!window->isWindowModified());
	// a clean document closes without a prompt
	QVERIFY(window->close());
}

void TestWindow::test_creation_tools_arm()
{
	// the element creation actions are modal tools now: triggering one arms the
	// matching tool on the scene instead of creating immediately (the drawing
	// and placement themselves are covered by l4-scene)
	QVERIFY(window->openDocument("diagrams/geometry.graphml"));
	window->actionNewState->trigger();
	QCOMPARE(int(window->getScene()->getCurrentTool()), int(ToolType::NewState));
	window->actionNewStateMachine->trigger();
	QCOMPARE(int(window->getScene()->getCurrentTool()), int(ToolType::NewSM));
	window->actionNewChoise->trigger();
	QCOMPARE(int(window->getScene()->getCurrentTool()), int(ToolType::NewChoice));
	window->actionNewTransition->trigger();
	QCOMPARE(int(window->getScene()->getCurrentTool()), int(ToolType::Transition));
	window->actionSelectTool->trigger();
	QCOMPARE(int(window->getScene()->getCurrentTool()), int(ToolType::Select));
}

void TestWindow::test_view_roundtrip()
{
	// the editor view (scale) is saved in the document and restored on reopen
	QVERIFY(window->openDocument("diagrams/geometry.graphml"));
	window->sceneView->setScale(1.5);
	QString saved = model->editorView();
	QVERIFY(saved.isEmpty());                       // not written until a save
	model->setEditorView(window->sceneView->viewState());
	QVERIFY(model->editorView().startsWith("1.5"));

	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	QString path = dir.filePath("view.graphml");
	model->saveAsDocument(path, Cyberiada::formatCyberiada10, true);

	QVERIFY(window->openDocument(path));
	QVERIFY(model->editorView().startsWith("1.5"));  // round-tripped through the file
	QVERIFY(qAbs(window->sceneView->currentScale() - 1.5) < 0.01);   // applied to the view
}

void TestWindow::test_menu_refactor()
{
	// #8: the file actions lead the main toolbar, before undo
	QList<QAction*> mt = window->mainToolBar->actions();
	int iNew = mt.indexOf(window->actionNew);
	int iUndo = mt.indexOf(window->actionUndo);
	QVERIFY(iNew >= 0 && iUndo >= 0 && iNew < iUndo);
	QVERIFY(mt.contains(window->actionOpen) && mt.contains(window->actionSave) &&
			mt.contains(window->actionExport));

	// #7/#9: the clipboard toolbar holds cut/copy/paste/delete
	QList<QAction*> et = window->editToolBar->actions();
	QVERIFY(et.contains(window->actionCut) && et.contains(window->actionCopy) &&
			et.contains(window->actionPaste) && et.contains(window->actionDeleteElement));

	// #7: delete has left the tool palette
	QVERIFY(!window->elementToolBar->actions().contains(window->actionDeleteElement));

	// the Edit menu hosts clipboard, delete, the Tools submenu and the inspector
	QList<QAction*> em = window->menuEdit->actions();
	QVERIFY(em.contains(window->actionCut) && em.contains(window->actionPaste) &&
			em.contains(window->actionDeleteElement) && em.contains(window->actionInspectorMode));
	QVERIFY(em.contains(window->menuTools->menuAction()));
	QVERIFY(window->menuTools->actions().contains(window->actionNewState));
}

QTEST_MAIN(TestWindow)
#include "l4-window.moc"
