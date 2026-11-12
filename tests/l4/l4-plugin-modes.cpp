/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process test of the plugins in the modes (see docs/TESTING.md, L4)
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
#include <QApplication>
#include <QMenu>
#include <QPlainTextEdit>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTreeWidget>
#include <QUndoStack>

#include "smeditor_window.h"
#include "code_editor.h"
#include "code_highlighter.h"
#include "diff_document.h"
#include "plugin_dock.h"
#include "plugin_registry.h"
#include "plugins_menu.h"
#include "settings_manager.h"
#include "text_document.h"

#define RUN_WAIT_MS 10000

class TestPluginModes: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void cleanup();
	void test_mode_gate();
	void test_text_replace();
	void test_text_line();
	void test_diff_refresh();
	void test_languages();
	void test_file_links();
};

static PluginResult waitFor(CyberiadaSMEditorWindow& win, PluginRunner* run)
{
	PluginResult result;
	bool done = false;
	QMetaObject::Connection c = QObject::connect(&win, &CyberiadaSMEditorWindow::pluginRunFinished,
		[&result, &done, run](PluginRunner* finished) {
			if (finished != run) return;
			result = finished->result();
			done = true;
		});
	QElapsedTimer clock;
	clock.start();
	while (!done && clock.elapsed() < RUN_WAIT_MS) QTest::qWait(20);
	QObject::disconnect(c);
	return result;
}

static QStringList menuTexts(QMenu* menu)
{
	QStringList texts;
	if (!menu) return texts;
	for (QAction* a : menu->actions()) texts.append(a->text());
	return texts;
}

void TestPluginModes::initTestCase()
{
	PluginRegistry::instance().setSearchPath(QStringList() << PLUGIN_TEST_DIR "/main"
	                                         << PLUGIN_TEST_DIR "/modes" << PLUGIN_TEST_DIR "/lang");
	PluginRegistry::instance().scan();
	QSettings().remove("plugins");
}

void TestPluginModes::cleanup()
{
	QSettings().remove("plugins");
	PluginRegistry::instance().scan();
}

// EDIT-PLUG-56: an action works in the modes it lists, the reason names them
void TestPluginModes::test_mode_gate()
{
	CyberiadaSMEditorWindow win;
	QVERIFY(win.openFile("texts/notes.md"));
	QAction* text = win.pluginsMenu()->action("modes", "text");
	QAction* plain = win.pluginsMenu()->action("modes", "default");
	QVERIFY(text && plain);
	QVERIFY(text->isEnabled());
	QVERIFY(!plain->isEnabled());
	QCOMPARE(plain->toolTip(), QString("the action works in: HSM diagram, FA diagram"));
	QVERIFY(win.openFile("diagrams/hierarchy.graphml"));
	QVERIFY(!text->isEnabled());
	QCOMPARE(text->toolTip(), QString("the action works in: Plain text, Markdown"));
	QVERIFY(plain->isEnabled());
	// an unknown result and a mode of no editor yet
	QVERIFY(!win.pluginsMenu()->action("modes", "next")->isEnabled());
	QVERIFY(!win.pluginsMenu()->action("modes", "future")->isEnabled());
	QVERIFY(win.pluginsMenu()->action("modes", "mixed")->isEnabled());
}

// EDIT-PLUG-57: the text is locked during the run, replaced as one step, undone
void TestPluginModes::test_text_replace()
{
	CyberiadaSMEditorWindow win;
	QVERIFY(win.openFile("texts/notes.md"));
	TextDocument* doc = win.currentText();
	QVERIFY(doc);
	QString before = doc->text();
	PluginRunner* run = win.runPlugin("modes", "text", PluginValues());
	QVERIFY(run);
	QVERIFY(!doc->isEditable());
	QVERIFY(doc->editor()->isReadOnly());
	PluginResult r = waitFor(win, run);
	QCOMPARE(r.state, PluginResult::stateDone);
	QVERIFY(r.changed);
	QVERIFY(doc->isEditable());
	QVERIFY(!doc->editor()->isReadOnly());
	QVERIFY(doc->text().contains("four states"));
	QCOMPARE(doc->undoStack()->count(), 1);
	QCOMPARE(doc->undoStack()->undoText(), QString("Plugin: Text"));
	QVERIFY(!doc->isClean());
	doc->undoStack()->undo();
	QCOMPARE(doc->text(), before);
	QVERIFY(doc->isClean());
	// the same text pushes no step
	r = win.runPluginAndWait("modes", "same", PluginValues());
	QCOMPARE(r.state, PluginResult::stateDone);
	QVERIFY(!r.changed);
	QCOMPARE(doc->undoStack()->index(), 0);
}

// a finding of a text tab goes to its line
void TestPluginModes::test_text_line()
{
	CyberiadaSMEditorWindow win;
	QVERIFY(win.openFile("texts/notes.md"));
	TextDocument* doc = win.currentText();
	PluginResult r = win.runPluginAndWait("modes", "lint", PluginValues());
	QCOMPARE(r.state, PluginResult::stateDone);
	QCOMPARE(r.diagnostics.size(), 1);
	QVERIFY(!r.diagnostics.first().linked);
	QVERIFY(win.openFile("diagrams/hierarchy.graphml"));
	QTreeWidget* list = win.pluginDock()->diagnostics();
	QCOMPARE(list->topLevelItemCount(), 1);
	emit list->itemActivated(list->topLevelItem(0), 0);
	QCOMPARE(win.currentTab(), static_cast<EditorDocument*>(doc));
	QCOMPARE(doc->editor()->textCursor().blockNumber(), 2);
}

// EDIT-PLUG-58: the output is the base, Refresh runs the action again
void TestPluginModes::test_diff_refresh()
{
	CyberiadaSMEditorWindow win;
	QVERIFY(win.openFile("diagrams/geometry.graphml"));
	CyberiadaSMEditorDocument* source = win.currentDocument();
	CyberiadaSMModel* m = source->model();
	int runs = 0;
	connect(&win, &CyberiadaSMEditorWindow::pluginRunStarted, this, [&runs]() { runs++; });
	PluginValues values;
	values.insert("revision", "HEAD");
	PluginResult r = win.runPluginAndWait("modes", "base", values);
	QCOMPARE(r.state, PluginResult::stateDone);
	DiffDocument* diff = win.currentDiff();
	QVERIFY(diff);
	QCOMPARE(r.tab, win.documentCount() - 1);
	QCOMPARE(diff->title(), QString("geometry.graphml ↔ HEAD"));
	QCOMPARE(diff->base().kind, DiffOrigin::originPlugin);
	QCOMPARE(diff->changed(), 1);
	// the source is untouched
	QVERIFY(source->isClean());
	QVERIFY(m->updateTitle(m->elementToIndex(m->idToElement("node-0-1")), "Second"));
	QVERIFY(!QString::fromStdString(diff->baseBuffer()).contains("Second"));
	QString error;
	QVERIFY(win.refreshDiff(diff, &error, true));
	QCOMPARE(runs, 2);
	QCOMPARE(win.documentCount(), 2);
	QVERIFY(QString::fromStdString(diff->baseBuffer()).contains("Second"));
	QVERIFY(QString::fromStdString(diff->targetBuffer()).contains("Second"));
	QCOMPARE(diff->changed(), 1);
	// the plugin needs its tab
	QVERIFY(win.closeDocument(source, true));
	QVERIFY(!win.refreshDiff(diff, &error, true));
	QCOMPARE(error, QString("the tab the plugin ran on is closed"));
}

// EDIT-PLUG-60: a language comes with its plugin and goes with it
void TestPluginModes::test_languages()
{
	CyberiadaSMEditorWindow win;
	QVERIFY(CodeStyle::instance().findId("lua"));
	QVERIFY(CodeStyle::instance().findId("words"));
	QVERIFY(menuTexts(win.findChild<QMenu*>("menuModeCode")).contains("Lua"));
	QVERIFY(menuTexts(win.findChild<QMenu*>("menuNewText")).contains("Lua"));
	QVERIFY(menuTexts(win.menuLanguage).contains("Lua"));
	// a plugin of languages only has no entry
	for (const QPair<QString, QString>& ids : win.pluginsMenu()->actionIds()) QVERIFY(ids.first != "words");
	QVERIFY(win.openFile("texts/script.lua"));
	TextDocument* script = win.currentText();
	QCOMPARE(script->mode(), QString("lua"));
	QVERIFY(win.pluginsMenu()->action("lua-tools", "run")->isEnabled());
	QVERIFY(win.openFile("diagrams/hierarchy.graphml"));
	CyberiadaSMEditorDocument* diagram = win.currentDocument();
	QVERIFY(diagram->model()->setPlatformLanguage("Lua"));
	QVERIFY(diagram->scene()->codeLanguage());
	QCOMPARE(diagram->scene()->codeLanguage()->name, QString("Lua"));

	// switched off: the tab turns Plain text, the value an unknown one
	SettingsManager::instance().setPluginEnabled("lua-tools", false);
	PluginRegistry::instance().scan();
	QVERIFY(!CodeStyle::instance().findId("lua"));
	QVERIFY(CodeStyle::instance().findId("words"));
	QCOMPARE(script->mode(), QString(MODE_TEXT));
	QVERIFY(!diagram->scene()->codeLanguage());
	QVERIFY(!menuTexts(win.findChild<QMenu*>("menuModeCode")).contains("Lua"));
	QVERIFY(menuTexts(win.menuLanguage).contains("Lua (no highlighting)"));

	// on again: the same value is the language again
	SettingsManager::instance().setPluginEnabled("lua-tools", true);
	PluginRegistry::instance().scan();
	QVERIFY(diagram->scene()->codeLanguage());
	QCOMPARE(diagram->scene()->codeLanguage()->name, QString("Lua"));
	QVERIFY(!menuTexts(win.menuLanguage).contains("Lua (no highlighting)"));
	QCOMPARE(script->mode(), QString(MODE_TEXT));
}

// a files result links the files a tab opens
void TestPluginModes::test_file_links()
{
	CyberiadaSMEditorWindow win;
	win.show();
	QVERIFY(QTest::qWaitForWindowExposed(&win));
	QTemporaryDir dir;
	PluginValues values;
	values.insert("dir", dir.path());
	PluginResult r = win.runPluginAndWait("files", "make", values);
	QCOMPARE(r.state, PluginResult::stateDone);
	QPlainTextEdit* out = win.pluginDock()->output("files");
	QVERIFY(out);
	QTextCursor link;
	for (QTextBlock b = out->document()->begin(); b.isValid(); b = b.next()) {
		for (QTextBlock::iterator f = b.begin(); !f.atEnd(); ++f) {
			QTextCharFormat format = f.fragment().charFormat();
			if (!format.isAnchor()) continue;
			// the source opens in a tab, the README is no document
			QVERIFY(format.anchorHref().endsWith("src/main.c"));
			link = QTextCursor(out->document());
			link.setPosition(f.fragment().position() + 1);
		}
	}
	QVERIFY(!link.isNull());
	win.pluginDock()->show();
	QTest::qWait(50);
	out->ensureCursorVisible();
	QPoint at = out->cursorRect(link).center();
	QCOMPARE(out->anchorAt(at), QDir(dir.path()).filePath("src/main.c"));
	QTest::mouseClick(out->viewport(), Qt::LeftButton, Qt::NoModifier, at);
	QVERIFY(win.currentText());
	QCOMPARE(win.currentText()->mode(), QString("c"));
	QCOMPARE(win.currentText()->filePath(), QFileInfo(QDir(dir.path()).filePath("src/main.c")).absoluteFilePath());
}

QTEST_MAIN(TestPluginModes)
#include "l4-plugin-modes.moc"
