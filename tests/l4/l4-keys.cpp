/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process tests of the keys of the window actions
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
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QLineEdit>
#include <QSettings>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QTimer>
#include <QToolBar>
#include "smeditor_window.h"
#include "code_editor.h"
#include "text_document.h"
#include "find_bar.h"
#include "shortcut_map.h"
#include "dialogs/preferences_dialog.h"
#include "dialogs/shortcuts_page.h"
#include "cyberiadasm_editor_view.h"
#include "settings_manager.h"
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_editor_state_item.h"
#include "cyberiadasm_editor_transition_item.h"
#include "editable_text_item.h"
#include "dotsignal.h"

// the EDIT-KEYS laws (docs/SHORTCUTS.md): the default keys, the stored changes, the
// keys a text takes, Go to Line; the keys go through the shortcuts of the window
class TestKeys: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void test_key_text();
	void test_defaults();
	void test_stored();
	void test_tool_keys();
	void test_text_takes_keys();
	void test_find_bar_esc();
	void test_go_to_line();
	void test_delete_point();
	void test_save_commits_edit();
	void test_escape_cancels_edit();
	void test_undo_keys_in_text();
	void test_page();
	void test_page_conflict();
	void test_page_reset();

private:
	static QKeySequence key(const char* text) { return QKeySequence(QString(text)); }
	static void activate(CyberiadaSMEditorWindow& window);
	static QList<QKeySequence> one(const char* text) { return QList<QKeySequence>() << key(text); }
	// the question of the page answered when it comes
	static void answer(QMessageBox::StandardButton button);
	static void type(QDialog& dialog, const char* text);
	static void click(QDialog& dialog, const char* button);
	// the first text of the kind on the scene, in edit mode with all of it selected
	template <class Text> static Text* edit(CyberiadaSMEditorWindow& window);
	QTemporaryDir settings;
};

void TestKeys::initTestCase()
{
	// the changed keys must not reach the profile the other cases share
	QVERIFY(settings.isValid());
	QSettings::setPath(QSettings::defaultFormat(), QSettings::UserScope, settings.path());
}

// a shortcut of the window acts in the active window only
void TestKeys::activate(CyberiadaSMEditorWindow& window)
{
	window.show();
	QApplication::setActiveWindow(&window);
	QVERIFY(QTest::qWaitForWindowActive(&window));
}

void TestKeys::test_key_text()
{
	QList<QKeySequence> keys = QList<QKeySequence>() << key("Ctrl+Shift+Z") << key("Ctrl+Y");
	QCOMPARE(shortcutKeysText(keys), QString("Ctrl+Shift+Z; Ctrl+Y"));
	QCOMPARE(shortcutKeysOf("Ctrl+Shift+Z; Ctrl+Y"), keys);
	QVERIFY(shortcutKeysOf(QString()).isEmpty());
	// a key written twice is one key
	QCOMPARE(shortcutKeysOf("F3; F3").size(), 1);
	QVERIFY(defaultShortcutKeys("actionNoSuch").isEmpty());
	QCOMPARE(defaultShortcutKeys("actionZoomIn"), QList<QKeySequence>() << key("Ctrl++") << key("Ctrl+="));
}

// EDIT-KEYS-1/8: the default keys, the keyless and the unnamed entries, no key twice
void TestKeys::test_defaults()
{
	CyberiadaSMEditorWindow window;
	ShortcutMap* map = window.shortcuts();
	QCOMPARE(window.actionSave->shortcuts(), QList<QKeySequence>() << key("Ctrl+S"));
	QCOMPARE(window.actionDeleteElement->shortcuts(), QList<QKeySequence>() << key("Del"));
	QVERIFY(window.actionRedo->shortcuts().contains(key("Ctrl+Shift+Z")));
	QVERIFY(window.actionRedo->shortcuts().contains(key("Ctrl+Y")));
	QVERIFY(window.actionFind->shortcuts().contains(key("Ctrl+F")));
	QCOMPARE(window.actionFindNext->shortcuts().first(), key("F3"));
	QCOMPARE(window.actionGoToLine->shortcuts(), QList<QKeySequence>() << key("Alt+G"));
	QCOMPARE(window.actionSelectTool->shortcuts(), QList<QKeySequence>() << key("Esc"));
	QVERIFY(!window.actionNextDocument->shortcuts().isEmpty());
	QCOMPARE(map->keys("actionSessionStep"), QList<QKeySequence>() << key("F10"));
	QCOMPARE(map->path("actionSessionStep"), QString("Plugins > Session"));
	QCOMPARE(map->path("actionNewState"), QString("Edit > Tools"));
	QCOMPARE(map->path("actionSelectTool"), QString("Edit > Tools"));
	QCOMPARE(map->label("actionOpen"), QString("Open..."));
	// registered without a key
	const char* keyless[] = { "actionAbout", "actionNewTerminate", "actionModeFA", "actionReadOnly",
	                          "actionStructureTree", "actionPropertiesPane", "actionOutputPanel",
	                          "actionPluginManager", "actionSessionRestart", "actionZoomToSM" };
	for (size_t i = 0; i < sizeof(keyless) / sizeof(keyless[0]); i++) {
		QVERIFY2(map->contains(keyless[i]), keyless[i]);
		QVERIFY2(map->keys(keyless[i]).isEmpty(), keyless[i]);
		QVERIFY2(!map->isChanged(keyless[i]), keyless[i]);
	}
	for (const QString& id : map->ids()) {
		QVERIFY2(id.startsWith("action"), qPrintable(id));
		QCOMPARE(map->action(id)->shortcuts(), map->keys(id));
		for (const QKeySequence& k : map->keys(id)) {
			QVERIFY2(map->holder(k, id).isEmpty(),
			         qPrintable(id + " shares " + k.toString() + " with " + map->holder(k, id)));
		}
	}
	QCOMPARE(map->holder(key("Ctrl+S")), QString("actionSave"));
	QVERIFY(map->holder(key("Ctrl+Alt+F12")).isEmpty());
}

// EDIT-KEYS-4: only the changes are stored, a second window has them, a reset drops them
void TestKeys::test_stored()
{
	const QString grid = "shortcuts/actionGridVisibility";
	const QString about = "shortcuts/actionAbout";
	{
		CyberiadaSMEditorWindow window;
		ShortcutMap* map = window.shortcuts();
		QSignalSpy changed(map, &ShortcutMap::changed);
		QVERIFY(QSettings().allKeys().filter("shortcuts/").isEmpty());
		map->setKeys("actionAbout", QList<QKeySequence>() << key("F1") << key("Shift+F1"));
		map->setKeys("actionGridVisibility", QList<QKeySequence>());
		QCOMPARE(changed.count(), 2);
		// the same keys again are no change
		map->setKeys("actionGridVisibility", QList<QKeySequence>());
		QCOMPARE(changed.count(), 2);
		QVERIFY(map->isChanged("actionAbout"));
		QCOMPARE(window.actionAbout->shortcuts().size(), 2);
		QVERIFY(window.actionGridVisibility->shortcuts().isEmpty());
		QCOMPARE(QSettings().value(about).toString(), QString("F1; Shift+F1"));
		QVERIFY(QSettings().contains(grid));
		QCOMPARE(QSettings().value(grid).toString(), QString());
		QCOMPARE(QSettings().allKeys().filter("shortcuts/").size(), 2);
	}
	{
		CyberiadaSMEditorWindow window;
		ShortcutMap* map = window.shortcuts();
		QCOMPARE(window.actionAbout->shortcuts(), QList<QKeySequence>() << key("F1") << key("Shift+F1"));
		QVERIFY(window.actionGridVisibility->shortcuts().isEmpty());
		QVERIFY(map->isChanged("actionGridVisibility"));
		// an action not changed keeps its default keys
		QCOMPARE(window.actionSave->shortcuts(), QList<QKeySequence>() << key("Ctrl+S"));
		map->reset("actionGridVisibility");
		QCOMPARE(window.actionGridVisibility->shortcuts(), QList<QKeySequence>() << key("Ctrl+G"));
		QVERIFY(!QSettings().contains(grid));
		// the default keys written back are no change either
		map->setKeys("actionAbout", QList<QKeySequence>());
		QVERIFY(!QSettings().contains(about));
		map->setKeys("actionAbout", QList<QKeySequence>() << key("F1"));
		map->resetAll();
		QVERIFY(window.actionAbout->shortcuts().isEmpty());
		QVERIFY(QSettings().allKeys().filter("shortcuts/").isEmpty());
	}
}

// EDIT-KEYS-1/6: a letter picks its tool on the canvas, with the toolbar hidden too
void TestKeys::test_tool_keys()
{
	CyberiadaSMEditorWindow window;
	activate(window);
	window.sceneView->setFocus();
	QVERIFY(window.actionSelectTool->isChecked());
	QTest::keyClick(window.sceneView, Qt::Key_S);
	QVERIFY(window.actionNewState->isChecked());
	QTest::keyClick(window.sceneView, Qt::Key_C, Qt::ShiftModifier);
	QVERIFY(window.actionNewFormalComment->isChecked());
	QTest::keyClick(window.sceneView, Qt::Key_Escape);
	QVERIFY(window.actionSelectTool->isChecked());
	window.elementToolBar->hide();
	QTest::keyClick(window.sceneView, Qt::Key_H);
	QVERIFY(window.actionPan->isChecked());
	QTest::keyClick(window.sceneView, Qt::Key_Escape);
	QVERIFY(window.actionSelectTool->isChecked());
	// a changed key acts at once, the old one no more
	window.shortcuts()->setKeys("actionNewState", QList<QKeySequence>() << key("N"));
	QTest::keyClick(window.sceneView, Qt::Key_S);
	QVERIFY(window.actionSelectTool->isChecked());
	QTest::keyClick(window.sceneView, Qt::Key_N);
	QVERIFY(window.actionNewState->isChecked());
	window.shortcuts()->reset("actionNewState");
}

// EDIT-KEYS-5: the typing keys go to the text edited
void TestKeys::test_text_takes_keys()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("texts/blink.cpp"));
	activate(window);
	CodeEditor* editor = window.currentText()->editor();
	editor->setFocus();
	editor->setTextCursor(QTextCursor(editor->document()));
	const QString source = window.currentText()->text();
	QTest::keyClick(editor, 's');
	QTest::keyClick(editor, 'C', Qt::ShiftModifier);
	QCOMPARE(window.currentText()->text(), "sC" + source);
	QTest::keyClick(editor, Qt::Key_Backspace);
	QTest::keyClick(editor, Qt::Key_Left);
	QTest::keyClick(editor, Qt::Key_Delete);
	QCOMPARE(window.currentText()->text(), source);
}

// EDIT-KEYS-11: Esc closes the find bar and does not reach the Select tool
void TestKeys::test_find_bar_esc()
{
	CyberiadaSMEditorWindow window;
	activate(window);
	window.sceneView->setFocus();
	QTest::keyClick(window.sceneView, Qt::Key_S);
	QVERIFY(window.actionNewState->isChecked());
	QTest::keyClick(window.sceneView, Qt::Key_F, Qt::ControlModifier);
	FindBar* bar = window.findBar();
	QVERIFY(!bar->isHidden());
	QVERIFY(bar->field()->hasFocus());
	// a letter of a tool is typed into the field
	QTest::keyClick(bar->field(), Qt::Key_T);
	QCOMPARE(bar->field()->text(), QString("t"));
	QVERIFY(window.actionNewState->isChecked());
	QTest::keyClick(bar->field(), Qt::Key_Escape);
	QVERIFY(bar->isHidden());
	QVERIFY(window.actionNewState->isChecked());
}

// EDIT-KEYS-7: the line asked, the cursor at its start; text tabs showing a source only
void TestKeys::test_go_to_line()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(!window.actionGoToLine->isEnabled());
	QVERIFY(window.menuEdit->actions().contains(window.actionGoToLine));
	QVERIFY(window.openFile("texts/blink.cpp"));
	activate(window);
	TextDocument* text = window.currentText();
	QVERIFY(window.actionGoToLine->isEnabled());
	QVERIFY(text->lineCount() > 5);
	QCOMPARE(text->currentLine(), 1);
	QVERIFY(text->goToLine(3));
	QCOMPARE(text->currentLine(), 3);
	QCOMPARE(text->editor()->textCursor().positionInBlock(), 0);
	QVERIFY(!text->goToLine(text->lineCount() + 1));
	QVERIFY(!text->goToLine(0));
	QCOMPARE(text->currentLine(), 3);

	// the dialog offers the current line within the lines of the text
	int offered = 0, last = 0;
	QTimer::singleShot(50, [&]() {
		QInputDialog* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
		if (!dialog) return;
		offered = dialog->intValue();
		last = dialog->intMaximum();
		dialog->setIntValue(5);
		dialog->accept();
	});
	text->editor()->setFocus();
	QTest::keyClick(text->editor(), Qt::Key_G, Qt::AltModifier);
	QCOMPARE(offered, 3);
	QCOMPARE(last, text->lineCount());
	QCOMPARE(text->currentLine(), 5);

	// a rendered page has no lines
	QVERIFY(window.openFile("texts/notes.md"));
	QVERIFY(window.actionGoToLine->isEnabled());
	window.currentText()->setReadOnlyFlag(true);
	QVERIFY(window.currentText()->showsRendered());
	QVERIFY(!window.actionGoToLine->isEnabled());
}

void TestKeys::answer(QMessageBox::StandardButton button)
{
	QTimer::singleShot(50, [button]() {
		QMessageBox* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
		if (box) box->button(button)->click();
	});
}

void TestKeys::type(QDialog& dialog, const char* text)
{
	dialog.findChild<QKeySequenceEdit*>("shortcutKeyEdit")->setKeySequence(key(text));
}

void TestKeys::click(QDialog& dialog, const char* button)
{
	QPushButton* b = dialog.findChild<QPushButton*>(button);
	QVERIFY2(b && b->isEnabled(), button);
	b->click();
}

template <class Text> Text* TestKeys::edit(CyberiadaSMEditorWindow& window)
{
	window.sceneView->setFocus();
	for (QGraphicsItem* item : window.getScene()->items()) {
		Text* text = dynamic_cast<Text*>(item);
		if (!text || !text->isVisible() || text->toPlainText().isEmpty()) continue;
		text->startEditing();
		QTextCursor cursor = text->textCursor();
		cursor.movePosition(QTextCursor::End);
		text->setTextCursor(cursor);
		return text;
	}
	return nullptr;
}

// EDIT-KEYS-9: Del on a focused point removes the point, the transition stays
void TestKeys::test_delete_point()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("diagrams/polyline.graphml"));
	activate(window);
	CyberiadaSMEditorScene* scene = window.getScene();
	CyberiadaSMEditorTransitionItem* tr =
		dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value("t0"));
	QVERIFY(tr);
	const Cyberiada::Transition* t =
		static_cast<const Cyberiada::Transition*>(window.getModel()->idToElement("t0"));
	size_t before = t->get_geometry_polyline().size();
	QVERIFY(before >= 1);
	scene->clearSelection();
	tr->setSelected(true);
	DotSignal* vertex = tr->getDot(1);
	QVERIFY(vertex);
	vertex->setVisible(true);
	window.sceneView->setFocus();
	vertex->setFocus();
	QCOMPARE(scene->focusItem(), static_cast<QGraphicsItem*>(vertex));
	QVERIFY(window.actionDeleteElement->isEnabled());
	QTest::keyClick(window.sceneView, Qt::Key_Delete);
	QVERIFY(window.getModel()->idToElement("t0"));
	t = static_cast<const Cyberiada::Transition*>(window.getModel()->idToElement("t0"));
	QCOMPARE(t->get_geometry_polyline().size(), before - 1);
	// with no point focused Del removes the selected transition
	scene->setFocusItem(nullptr);
	tr = dynamic_cast<CyberiadaSMEditorTransitionItem*>(scene->getMap().value("t0"));
	scene->clearSelection();
	tr->setSelected(true);
	QTest::keyClick(window.sceneView, Qt::Key_Delete);
	QVERIFY(!window.getModel()->idToElement("t0"));
}

// EDIT-KEYS-10: Save during a canvas edit saves the text typed
void TestKeys::test_save_commits_edit()
{
	QTemporaryDir dir;
	QString path = dir.filePath("edit.graphml");
	QVERIFY(QFile::copy("diagrams/hierarchy.graphml", path));
	QVERIFY(QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner));
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile(path));
	activate(window);
	// Save is there for a clean document while a text is edited (EDIT-IO-7)
	QVERIFY(!window.actionSave->isEnabled());
	StateTitle* title = edit<StateTitle>(window);
	QVERIFY(title);
	QVERIFY(window.actionSave->isEnabled());
	QTest::keyClicks(window.sceneView, "Typed");
	QVERIFY(title->isEditing());
	QVERIFY(title->toPlainText().endsWith("Typed"));
	QTest::keyClick(window.sceneView, Qt::Key_S, Qt::ControlModifier);
	QFile saved(path);
	QVERIFY(saved.open(QIODevice::ReadOnly));
	QVERIFY(saved.readAll().contains("Typed"));
	QVERIFY(window.currentDocument()->isClean());
	QVERIFY(!window.actionSave->isEnabled());

	// an edit left by Esc, or one that changed nothing, enables nothing
	title = edit<StateTitle>(window);
	QVERIFY(window.actionSave->isEnabled());
	QTest::keyClick(window.sceneView, Qt::Key_Escape);
	QVERIFY(!window.actionSave->isEnabled());
	title = edit<StateTitle>(window);
	QDateTime written = QFileInfo(path).lastModified();
	QTest::keyClick(window.sceneView, Qt::Key_S, Qt::ControlModifier);
	QVERIFY(!title->isEditing());
	QCOMPARE(QFileInfo(path).lastModified(), written);
	QVERIFY(!window.actionSave->isEnabled());
}

// EDIT-KEYS-5/11: a tool letter is typed into the text edited; Esc abandons the edit
void TestKeys::test_escape_cancels_edit()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("diagrams/hierarchy.graphml"));
	activate(window);
	QUndoStack* undo = window.currentDocument()->undoStack();
	int steps = undo->count();

	StateTitle* title = edit<StateTitle>(window);
	QVERIFY(title);
	const QString name = title->toPlainText();
	QTest::keyClick(window.sceneView, 's');
	QTest::keyClick(window.sceneView, 'C', Qt::ShiftModifier);
	QCOMPARE(title->toPlainText(), name + "sC");
	QVERIFY(window.actionSelectTool->isChecked());
	QTest::keyClick(window.sceneView, Qt::Key_Escape);
	QVERIFY(!title->isEditing());
	QCOMPARE(title->toPlainText(), name);
	QCOMPARE(undo->count(), steps);
	QVERIFY(window.currentDocument()->isClean());

	// an action and a transition label alike
	QVERIFY(window.openFile("diagrams/internal-transition.graphml"));
	undo = window.currentDocument()->undoStack();
	steps = undo->count();
	StateAction* action = edit<StateAction>(window);
	QVERIFY(action);
	const QString code = action->toPlainText();
	QTest::keyClicks(window.sceneView, "x");
	QVERIFY(action->toPlainText() != code);
	QTest::keyClick(window.sceneView, Qt::Key_Escape);
	QVERIFY(!action->isEditing());
	QCOMPARE(action->toPlainText(), code);
	QVERIFY(window.openFile("diagrams/lift.graphml"));
	undo = window.currentDocument()->undoStack();
	steps = undo->count();
	TransitionAction* label = edit<TransitionAction>(window);
	QVERIFY(label);
	const QString event = label->toPlainText();
	QTest::keyClicks(window.sceneView, "x");
	QTest::keyClick(window.sceneView, Qt::Key_Escape);
	QVERIFY(!label->isEditing());
	QCOMPARE(label->toPlainText(), event);
	QCOMPARE(undo->count(), steps);

	// Esc with no edit is the Select tool again
	QTest::keyClick(window.sceneView, Qt::Key_T);
	QVERIFY(window.actionNewTransition->isChecked());
	QTest::keyClick(window.sceneView, Qt::Key_Escape);
	QVERIFY(window.actionSelectTool->isChecked());
}

// EDIT-KEYS-12: the keys of Undo and Redo, whichever they are, drive the window's
void TestKeys::test_undo_keys_in_text()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("texts/blink.cpp"));
	activate(window);
	TextDocument* text = window.currentText();
	CodeEditor* editor = text->editor();
	const QString source = text->text();
	editor->setFocus();
	editor->setTextCursor(QTextCursor(editor->document()));
	QTest::keyClick(editor, 'a');
	QCOMPARE(text->text(), "a" + source);
	QTest::keyClick(editor, Qt::Key_Z, Qt::ControlModifier);
	QCOMPARE(text->text(), source);
	QTest::keyClick(editor, Qt::Key_Y, Qt::ControlModifier);
	QCOMPARE(text->text(), "a" + source);
	QVERIFY(text->undoStack()->canUndo());

	// other keys: the old ones do nothing, the new ones drive the same stack
	window.shortcuts()->setKeys("actionUndo", one("Ctrl+U"));
	window.shortcuts()->setKeys("actionRedo", one("Ctrl+Shift+Backspace"));
	QTest::keyClick(editor, Qt::Key_Z, Qt::ControlModifier);
	QCOMPARE(text->text(), "a" + source);
	QTest::keyClick(editor, Qt::Key_U, Qt::ControlModifier);
	QCOMPARE(text->text(), source);
	QVERIFY(!text->undoStack()->canUndo());
	QTest::keyClick(editor, Qt::Key_Y, Qt::ControlModifier);
	QCOMPARE(text->text(), source);
	QTest::keyClick(editor, Qt::Key_Backspace, Qt::ControlModifier | Qt::ShiftModifier);
	QCOMPARE(text->text(), "a" + source);
	window.shortcuts()->resetAll();
}

// EDIT-KEYS-2: the rows by the menus, the filter, Assign, Add and Clear on a copy,
// Apply and Cancel
void TestKeys::test_page()
{
	CyberiadaSMEditorWindow window;
	ShortcutMap* map = window.shortcuts();
	// no page without the keys of a window
	QVERIFY(!PreferencesDialog().shortcutsPage());
	{
		PreferencesDialog dialog(&window, map);
		ShortcutsPage* page = dialog.shortcutsPage();
		QVERIFY(page);
		QTreeWidget* tree = dialog.findChild<QTreeWidget*>("shortcutTree");
		QVERIFY(tree);
		int rows = 0;
		QStringList groups;
		for (int g = 0; g < tree->topLevelItemCount(); g++) {
			groups << tree->topLevelItem(g)->text(0);
			rows += tree->topLevelItem(g)->childCount();
		}
		QCOMPARE(rows, map->ids().size());
		QVERIFY(groups.contains("File"));
		QVERIFY(groups.contains("Edit > Tools"));
		QVERIFY(groups.contains("Plugins > Session"));
		QVERIFY(page->select("actionSave"));
		QTreeWidgetItem* save = tree->currentItem();
		QCOMPARE(save->text(0), QString("Save..."));
		QCOMPARE(save->text(1), key("Ctrl+S").toString(QKeySequence::NativeText));
		QVERIFY(!save->font(1).bold());
		QVERIFY(!page->select("actionNoSuch"));

		// the filter by the action, the menu and the key
		QLineEdit* filter = dialog.findChild<QLineEdit*>("shortcutFilter");
		filter->setText("zoom");
		QVERIFY(save->isHidden());
		QVERIFY(save->parent()->isHidden());
		page->select("actionZoomIn");
		QVERIFY(!tree->currentItem()->isHidden());
		filter->setText("session");
		page->select("actionSessionRestart");
		QVERIFY(!tree->currentItem()->isHidden());
		filter->setText("ctrl+s");
		QVERIFY(!save->isHidden());
		filter->clear();
		QVERIFY(!save->parent()->isHidden());

		// the keys of the copy change, the window keeps its own until Apply
		QVERIFY(page->select("actionAbout"));
		QTreeWidgetItem* about = tree->currentItem();
		QVERIFY(!dialog.findChild<QPushButton*>("shortcutClear")->isEnabled());
		QVERIFY(!dialog.findChild<QPushButton*>("shortcutReset")->isEnabled());
		type(dialog, "F1");
		click(dialog, "shortcutAssign");
		type(dialog, "Shift+F1");
		click(dialog, "shortcutAdd");
		QCOMPARE(page->keys("actionAbout"), QList<QKeySequence>() << key("F1") << key("Shift+F1"));
		QVERIFY(about->font(1).bold());
		QVERIFY(about->text(1).contains("; "));
		type(dialog, "F2");
		click(dialog, "shortcutAssign");
		QCOMPARE(page->keys("actionAbout"), one("F2"));
		QVERIFY(window.actionAbout->shortcuts().isEmpty());
		dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
		QCOMPARE(window.actionAbout->shortcuts(), one("F2"));
		QCOMPARE(QSettings().value("shortcuts/actionAbout").toString(), QString("F2"));

		click(dialog, "shortcutClear");
		QVERIFY(page->keys("actionAbout").isEmpty());
		QVERIFY(!about->font(1).bold());
		// Cancel: the cleared key stays in the window
	}
	QCOMPARE(window.actionAbout->shortcuts(), one("F2"));
	{
		PreferencesDialog dialog(&window, map);
		QCOMPARE(dialog.shortcutsPage()->keys("actionAbout"), one("F2"));
		dialog.shortcutsPage()->select("actionAbout");
		click(dialog, "shortcutReset");
		dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
	}
	QVERIFY(window.actionAbout->shortcuts().isEmpty());
	QVERIFY(QSettings().allKeys().filter("shortcuts/").isEmpty());
}

// EDIT-KEYS-3: the holder of a key is named and gives it on a confirmation
void TestKeys::test_page_conflict()
{
	CyberiadaSMEditorWindow window;
	ShortcutMap* map = window.shortcuts();
	PreferencesDialog dialog(&window, map);
	ShortcutsPage* page = dialog.shortcutsPage();
	QLabel* holder = dialog.findChild<QLabel*>("shortcutHolder");
	page->select("actionAbout");
	type(dialog, "F2");
	QVERIFY(holder->text().isEmpty());
	type(dialog, "Ctrl+S");
	QVERIFY(holder->text().contains("File > Save..."));

	answer(QMessageBox::No);
	click(dialog, "shortcutAssign");
	QVERIFY(page->keys("actionAbout").isEmpty());
	QCOMPARE(page->keys("actionSave"), one("Ctrl+S"));

	answer(QMessageBox::Yes);
	click(dialog, "shortcutAssign");
	QCOMPARE(page->keys("actionAbout"), one("Ctrl+S"));
	QVERIFY(page->keys("actionSave").isEmpty());
	QVERIFY(holder->text().isEmpty());

	// one of two keys taken, the other stays
	type(dialog, "Ctrl+Y");
	answer(QMessageBox::Yes);
	click(dialog, "shortcutAdd");
	QCOMPARE(page->keys("actionAbout"), QList<QKeySequence>() << key("Ctrl+S") << key("Ctrl+Y"));
	QCOMPARE(page->keys("actionRedo"), one("Ctrl+Shift+Z"));

	dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
	QVERIFY(window.actionSave->shortcuts().isEmpty());
	QCOMPARE(window.actionAbout->shortcuts().size(), 2);
	for (const QString& id : map->ids()) {
		for (const QKeySequence& k : map->keys(id)) QVERIFY(map->holder(k, id).isEmpty());
	}

	// the reset of a row takes its default key back
	page->select("actionSave");
	click(dialog, "shortcutReset");
	QCOMPARE(page->keys("actionSave"), one("Ctrl+S"));
	QCOMPARE(page->keys("actionAbout"), one("Ctrl+Y"));
	click(dialog, "shortcutResetAll");
	QVERIFY(page->keys("actionAbout").isEmpty());
	QCOMPARE(page->keys("actionRedo").size(), 2);
	dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
	QVERIFY(QSettings().allKeys().filter("shortcuts/").isEmpty());
}

// EDIT-KEYS-4: Reset of the Preferences drops the changed keys
void TestKeys::test_page_reset()
{
	CyberiadaSMEditorWindow window;
	window.shortcuts()->setKeys("actionAbout", one("F1"));
	PreferencesDialog dialog(&window, window.shortcuts());
	QCOMPARE(dialog.shortcutsPage()->keys("actionAbout"), one("F1"));
	dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Reset)->click();
	QVERIFY(dialog.shortcutsPage()->keys("actionAbout").isEmpty());
	QVERIFY(window.actionAbout->shortcuts().isEmpty());
	QVERIFY(QSettings().allKeys().filter("shortcuts/").isEmpty());
}

QTEST_MAIN(TestKeys)
#include "l4-keys.moc"
