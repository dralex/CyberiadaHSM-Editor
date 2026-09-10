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
	void test_new_sm_content();

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

void TestWindow::test_new_sm_content()
{
	// a frameless machine that already has content is bordered in place by the
	// New State Machine action; only a further use adds a separate machine
	QVERIFY(window->openDocument("diagrams/geometry.graphml"));
	int before = int(model->rootDocument()->get_state_machines().size());
	QModelIndex smi = model->firstSMIndex();
	QVERIFY(!model->indexToElement(smi)->has_geometry());

	window->actionNewStateMachine->trigger();
	QVERIFY(model->indexToElement(smi)->has_geometry());
	QCOMPARE(int(model->rootDocument()->get_state_machines().size()), before);

	window->actionNewStateMachine->trigger();
	QCOMPARE(int(model->rootDocument()->get_state_machines().size()), before + 1);
}

QTEST_MAIN(TestWindow)
#include "l4-window.moc"
