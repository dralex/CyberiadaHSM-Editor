/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process Edit > Language menu test (see docs/TESTING.md, L4)
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
#include <QMenu>
#include "smeditor_window.h"
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"
#include "code_highlighter.h"
#include "settings_manager.h"
#include "gesture_log.h"

// EDIT-TEXT-9: the menu writes the platformLanguage of the active document and
// follows it on load, tab switch, properties edit and undo
class TestLanguage: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void cleanupTestCase();
	void test_menu_layout();
	void test_untitled_creates_document();
	void test_load_and_choose();
	void test_properties_path();
	void test_tabs_follow();
	void test_unknown_kept();
	void test_transient_unknown();
	void test_inspector_disables();

private:
	QAction* entry(const QString& text) const;
	QString checkedText() const;
	QString meta() const;
	bool setMeta(const QString& value);

	CyberiadaSMEditorWindow* window;
	QTemporaryDir logRoot;
};

QAction* TestLanguage::entry(const QString& text) const
{
	const QList<QAction*> actions = window->menuLanguage->actions();
	for (QAction* action : actions) {
		if (action->isCheckable() && action->text() == text) return action;
	}
	return nullptr;
}

QString TestLanguage::checkedText() const
{
	const QList<QAction*> actions = window->menuLanguage->actions();
	for (QAction* action : actions) {
		if (action->isChecked()) return action->text();
	}
	return QString();
}

QString TestLanguage::meta() const
{
	const Cyberiada::LocalDocument* doc = window->getModel()->rootDocument();
	if (!doc) return QString();
	return QString::fromStdString(doc->meta().get_string(METAINFORMATION_KEY_PLATFORM_LANGUAGE));
}

// the properties panel path
bool TestLanguage::setMeta(const QString& value)
{
	CyberiadaSMModel* model = window->getModel();
	return model->updateMetainformation(model->documentIndex(),
	                                    METAINFORMATION_KEY_PLATFORM_LANGUAGE, value);
}

void TestLanguage::initTestCase()
{
	QVERIFY(logRoot.isValid());
	qputenv("CYBERIADA_SESSION_LOG_DIR", logRoot.path().toUtf8());
	SettingsManager::instance().setInspectorMode(false);
	window = new CyberiadaSMEditorWindow();
	window->resize(1200, 800);
	window->show();
	QVERIFY(QTest::qWaitForWindowExposed(window));
}

void TestLanguage::cleanupTestCase()
{
	while (window->documentCount() > 1) {
		window->closeDocument(window->currentDocument(), true);
	}
	delete window;
}

// right after Edit > Tools: Undefined and the supported languages
void TestLanguage::test_menu_layout()
{
	const QList<QAction*> edit = window->menuEdit->actions();
	int tools = edit.indexOf(window->menuTools->menuAction());
	QVERIFY(tools >= 0);
	QCOMPARE(edit.value(tools + 1), window->menuLanguage->menuAction());

	QStringList texts;
	const QList<QAction*> actions = window->menuLanguage->actions();
	for (QAction* action : actions) {
		if (action->isCheckable()) texts.append(action->text());
	}
	QStringList expected = QStringList() << "Undefined" << CodeStyle::instance().languages();
	QCOMPARE(texts, expected);
	QVERIFY(texts.contains("C++"));
	QCOMPARE(checkedText(), QString("Undefined"));
}

// a fresh untitled document is created by the choice; one undo step
void TestLanguage::test_untitled_creates_document()
{
	CyberiadaSMModel* model = window->getModel();
	QVERIFY(!model->rootDocument());
	entry("C")->trigger();
	QVERIFY(model->rootDocument());
	// the document exists with the state machine the first drawn element makes
	QCOMPARE(int(model->rootDocument()->get_state_machines().size()), 1);
	QCOMPARE(meta(), QString("C"));
	QCOMPARE(checkedText(), QString("C"));
	QCOMPARE(window->getScene()->codeLanguage()->name, QString("C"));
	QCOMPARE(model->undoStack()->count(), 1);

	model->undoStack()->undo();
	QCOMPARE(meta(), QString());
	QCOMPARE(checkedText(), QString("Undefined"));
	QVERIFY(model->rootDocument()->get_state_machines().empty());
	QVERIFY(window->currentDocument()->isClean());
}

void TestLanguage::test_load_and_choose()
{
	QVERIFY(window->openFile("diagrams/code-highlight.graphml"));
	QCOMPARE(checkedText(), QString("C++"));
	CyberiadaSMModel* model = window->getModel();
	int steps = model->undoStack()->count();

	entry("Python")->trigger();
	QCOMPARE(meta(), QString("Python"));
	QCOMPARE(window->getScene()->codeLanguage()->name, QString("Python"));
	QCOMPARE(model->undoStack()->count(), steps + 1);
	// the checked entry again changes nothing
	entry("Python")->trigger();
	QCOMPARE(model->undoStack()->count(), steps + 1);

	// Undefined removes the parameter
	entry("Undefined")->trigger();
	QCOMPARE(meta(), QString());
	QVERIFY(!window->getScene()->codeLanguage());

	model->undoStack()->undo();
	model->undoStack()->undo();
	QCOMPARE(meta(), QString("C++"));
	QCOMPARE(checkedText(), QString("C++"));
}

void TestLanguage::test_properties_path()
{
	CyberiadaSMModel* model = window->getModel();
	// an alias checks its language; choosing that language keeps the value
	QVERIFY(setMeta("Arduino"));
	QCOMPARE(checkedText(), QString("C++"));
	int steps = model->undoStack()->count();
	entry("C++")->trigger();
	QCOMPARE(meta(), QString("Arduino"));
	QCOMPARE(model->undoStack()->count(), steps);

	QVERIFY(setMeta("java"));
	QCOMPARE(checkedText(), QString("Java"));
	QVERIFY(setMeta(""));
	QCOMPARE(checkedText(), QString("Undefined"));
	QVERIFY(setMeta("C++"));
	QCOMPARE(checkedText(), QString("C++"));
}

// each tab keeps its language; the menu follows the active one
void TestLanguage::test_tabs_follow()
{
	CyberiadaSMEditorDocument* cpp = window->currentDocument();
	window->actionNew->trigger();
	CyberiadaSMEditorDocument* fresh = window->currentDocument();
	QVERIFY(fresh != cpp);
	QCOMPARE(checkedText(), QString("Undefined"));
	entry("Python")->trigger();
	QCOMPARE(checkedText(), QString("Python"));

	window->setCurrentDocument(cpp);
	QCOMPARE(checkedText(), QString("C++"));
	QCOMPARE(cpp->scene()->codeLanguage()->name, QString("C++"));
	QCOMPARE(fresh->scene()->codeLanguage()->name, QString("Python"));

	window->setCurrentDocument(fresh);
	QCOMPARE(checkedText(), QString("Python"));
	QVERIFY(window->closeDocument(fresh, true));
	QCOMPARE(window->currentDocument(), cpp);
	QCOMPARE(checkedText(), QString("C++"));
}

// an unknown value of a loaded file is shown and kept for the session
void TestLanguage::test_unknown_kept()
{
	QVERIFY(window->openFile("diagrams/language-lua.graphml"));
	QString lua = "Lua (no highlighting)";
	QVERIFY(entry(lua));
	QCOMPARE(checkedText(), lua);
	QVERIFY(!window->getScene()->codeLanguage());

	entry("C")->trigger();
	QCoreApplication::processEvents();
	QCOMPARE(meta(), QString("C"));
	QVERIFY(entry(lua));
	QVERIFY(!entry(lua)->isChecked());

	entry(lua)->trigger();
	QCoreApplication::processEvents();
	QCOMPARE(meta(), QString("Lua"));
	QCOMPARE(checkedText(), lua);
}

// a hand-typed unknown value is shown while current; replaced from the menu, it is kept
void TestLanguage::test_transient_unknown()
{
	QString rust = "Rust (no highlighting)";
	QVERIFY(setMeta("Rust"));
	QCOMPARE(checkedText(), rust);
	QVERIFY(setMeta("C++"));
	QCoreApplication::processEvents();
	QVERIFY(!entry(rust));

	QString go = "Go (no highlighting)";
	QVERIFY(setMeta("Go"));
	QCOMPARE(checkedText(), go);
	entry("C")->trigger();
	QCoreApplication::processEvents();
	QCOMPARE(meta(), QString("C"));
	QVERIFY(entry(go));
	// the session list is shared by the tabs
	window->actionNew->trigger();
	QVERIFY(entry(go));
	QVERIFY(entry("Lua (no highlighting)"));
	QCOMPARE(checkedText(), QString("Undefined"));
	QVERIFY(window->closeDocument(window->currentDocument(), true));
}

void TestLanguage::test_inspector_disables()
{
	SettingsManager::instance().setInspectorMode(true);
	QVERIFY(!window->menuLanguage->menuAction()->isEnabled());
	SettingsManager::instance().setInspectorMode(false);
	QVERIFY(window->menuLanguage->menuAction()->isEnabled());
}

QTEST_MAIN(TestLanguage)
#include "l4-language.moc"
