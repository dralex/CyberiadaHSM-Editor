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

class TestWindow: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void test_undo_actions();
	void test_modified_state();
	void test_creation_tools_arm();

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

QTEST_MAIN(TestWindow)
#include "l4-window.moc"
