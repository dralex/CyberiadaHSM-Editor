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
#include <QInputDialog>
#include <QLineEdit>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include "smeditor_window.h"
#include "code_editor.h"
#include "text_document.h"
#include "find_bar.h"
#include "shortcut_map.h"
#include "cyberiadasm_editor_view.h"
#include "settings_manager.h"

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

private:
	static QKeySequence key(const char* text) { return QKeySequence(QString(text)); }
	static void activate(CyberiadaSMEditorWindow& window);
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

QTEST_MAIN(TestKeys)
#include "l4-keys.moc"
