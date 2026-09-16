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
#include <QScrollBar>
#include <QMenu>
#include "smeditor_window.h"
#include "settings_manager.h"
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_view.h"
#include "cyberiadasm_editor_scene.h"

class TestWindow: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void test_undo_actions();
	void test_modified_state();
	void test_creation_tools_arm();
	void test_view_roundtrip();
	void test_pan_tool();
	void test_recent_files();
	void test_menu_refactor();
	void test_edit_action_gating();

private:
	CyberiadaSMEditorWindow* window;
	CyberiadaSMModel* model;
};

void TestWindow::initTestCase()
{
	window = new CyberiadaSMEditorWindow();
	model = window->getModel();
	// the viewport restore is deferred until the window is shown with a real size
	window->resize(1200, 800);
	window->show();
	QVERIFY(QTest::qWaitForWindowExposed(window));
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
	// the restore is deferred to after the layout settles: spin the loop for it
	QTest::qWait(20);
	QVERIFY(qAbs(window->sceneView->currentScale() - 1.5) < 0.01);   // applied to the view
}

void TestWindow::test_pan_tool()
{
	// the pan tool scrolls the viewport, even over a selected item's handles
	QVERIFY(window->openDocument("diagrams/geometry.graphml"));
	QTest::qWait(20);
	// zoom in so the scene overflows the viewport (the scrollbars gain a range)
	window->sceneView->setScale(3.0);
	QApplication::processEvents();
	// select an item so its handles are on the canvas under the drag start
	window->getScene()->clearSelection();
	if (QGraphicsItem* g = window->getScene()->getMap().value("node-0")) g->setSelected(true);

	int h0 = window->sceneView->horizontalScrollBar()->value();
	int v0 = window->sceneView->verticalScrollBar()->value();
	window->getScene()->setCurrentTool(ToolType::Pan);
	window->sceneView->setCurrentTool(ToolType::Pan);

	QWidget* vp = window->sceneView->viewport();
	QPoint c(vp->width() / 2, vp->height() / 2);
	auto post = [&](QEvent::Type t, QPoint p, Qt::MouseButton b, Qt::MouseButtons bs) {
		QMouseEvent me(t, p, vp->mapToGlobal(p), b, bs, Qt::NoModifier);
		QApplication::sendEvent(vp, &me);
	};
	post(QEvent::MouseButtonPress, c, Qt::LeftButton, Qt::LeftButton);
	post(QEvent::MouseMove, c - QPoint(60, 40), Qt::NoButton, Qt::LeftButton);
	post(QEvent::MouseButtonRelease, c - QPoint(60, 40), Qt::LeftButton, Qt::NoButton);
	// dragging up-left scrolls the content, so both scrollbar values increase
	QVERIFY(window->sceneView->horizontalScrollBar()->value() > h0);
	QVERIFY(window->sceneView->verticalScrollBar()->value() > v0);
}

void TestWindow::test_recent_files()
{
	// opening a document records it at the front of the recent list, and the
	// File > Open Recent submenu is rebuilt to match
	SettingsManager::instance().clearRecentFiles();
	QVERIFY(window->openDocument("diagrams/geometry.graphml"));
	QStringList recent = SettingsManager::instance().getRecentFiles();
	QVERIFY(!recent.isEmpty());
	QVERIFY(recent.first().endsWith("geometry.graphml"));
	// re-opening the same file keeps a single, front-most entry (dedup)
	QVERIFY(window->openDocument("diagrams/geometry.graphml"));
	QCOMPARE(SettingsManager::instance().getRecentFiles().count(recent.first()), 1);
	// the submenu carries a per-file action plus the "Clear list" entry
	QMenu* rm = nullptr;
	for (QMenu* m : window->findChildren<QMenu*>())
		if (m->title() == QString("Open Recent")) rm = m;
	QVERIFY(rm);
	QVERIFY(rm->actions().size() >= 2);
	// clearing empties the list and disables the submenu
	SettingsManager::instance().clearRecentFiles();
	QVERIFY(SettingsManager::instance().getRecentFiles().isEmpty());
	QVERIFY(!rm->isEnabled());
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

	// #4/#7/#9: cut/copy/paste/delete sit on the main toolbar right after undo/redo
	int iRedo = mt.indexOf(window->actionRedo);
	int iCut = mt.indexOf(window->actionCut);
	QVERIFY(iRedo >= 0 && iCut > iRedo);
	QVERIFY(mt.contains(window->actionCopy) && mt.contains(window->actionPaste) &&
			mt.contains(window->actionDeleteElement));

	// #7: delete has left the tool palette
	QVERIFY(!window->elementToolBar->actions().contains(window->actionDeleteElement));

	// the Edit menu hosts clipboard, delete, the Tools submenu and the inspector
	QList<QAction*> em = window->menuEdit->actions();
	QVERIFY(em.contains(window->actionCut) && em.contains(window->actionPaste) &&
			em.contains(window->actionDeleteElement) && em.contains(window->actionInspectorMode));
	QVERIFY(em.contains(window->menuTools->menuAction()));
	QVERIFY(window->menuTools->actions().contains(window->actionNewState));
}

void TestWindow::test_edit_action_gating()
{
	// #2: cut/copy/paste/delete track the selection (an SM is not copyable but its
	// border can be deleted); paste stays off until something is copied
	QVERIFY(window->openDocument("diagrams/geometry.graphml"));
	CyberiadaSMEditorScene* scene = window->getScene();
	scene->clearSelection();
	QCoreApplication::processEvents();
	QVERIFY(!window->actionCopy->isEnabled());
	QVERIFY(!window->actionCut->isEnabled());
	QVERIFY(!window->actionDeleteElement->isEnabled());
	QVERIFY(!window->actionPaste->isEnabled());

	// a simple state: copy/cut/delete enabled
	QGraphicsItem* st = scene->getMap().value("node-0-1");
	QVERIFY(st);
	st->setSelected(true);
	QCoreApplication::processEvents();
	QVERIFY(window->actionCopy->isEnabled());
	QVERIFY(window->actionCut->isEnabled());
	QVERIFY(window->actionDeleteElement->isEnabled());

	// the state machine: not copyable, but delete (border clear) is allowed
	scene->clearSelection();
	Cyberiada::Element* sm = window->getModel()->indexToElement(window->getModel()->firstSMIndex());
	QVERIFY(sm && sm->get_type() == Cyberiada::elementSM);
	QGraphicsItem* smItem = scene->getMap().value(sm->get_id());
	QVERIFY(smItem);
	smItem->setSelected(true);
	QCoreApplication::processEvents();
	QVERIFY(!window->actionCopy->isEnabled());
	QVERIFY(window->actionDeleteElement->isEnabled());
}

QTEST_MAIN(TestWindow)
#include "l4-window.moc"
