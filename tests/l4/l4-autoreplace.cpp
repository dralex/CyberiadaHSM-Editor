/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process tests of the typed replacements
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
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QUndoStack>
#include "smeditor_window.h"
#include "autoreplace.h"
#include "code_editor.h"
#include "text_document.h"
#include "document_mode.h"
#include "editable_text_item.h"
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_editor_state_item.h"
#include "cyberiadasm_editor_transition_item.h"
#include "cyberiadasm_editor_view.h"
#include "settings_manager.h"
#include "dialogs/preferences_dialog.h"
#include "dialogs/autoreplace_page.h"
#include "dialogs/multilinetextdialog.h"
#include "cyberiadasm_properties_widget.h"
#include "cyberiadasm_view.h"

// the EDIT-AUTO laws (docs/AUTOREPLACE.md): the rules, the texts they act in, the
// Markdown exclusions, the typing only, the characters given back
class TestAutoreplace: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void test_rules();
	void test_markdown_prose_data();
	void test_markdown_prose();
	void test_canvas_prose();
	void test_canvas_code();
	void test_canvas_gives_back();
	void test_markdown_tab();
	void test_other_tabs();
	void test_settings();
	void test_page();
	void test_line_filter();
	void test_text_dialog();
	void test_properties_name();
	void test_tree_rename();

private:
	static QString u(const char* text) { return QString::fromUtf8(text); }
	static void activate(CyberiadaSMEditorWindow& window);
	// the first text of the kind on the scene, in edit mode with the cursor at its end
	template <class Text> static Text* edit(CyberiadaSMEditorWindow& window, bool (*wanted)(Text*) = nullptr);
	static bool informalBody(EditableTextItem* text) { return text->getCodeRole() == codeRoleMarkup; }
	static CodeEditor* textTab(CyberiadaSMEditorWindow& window, const QString& mode);
	// the string row of the name and a row of another label, of the element shown
	static void rows(QtProperty* parent, const QList<QtProperty*>& list, const QString& name,
	                 QtProperty** found, QtProperty** other);
	QTemporaryDir settings;
};

void TestAutoreplace::initTestCase()
{
	// the changed rules must not reach the profile the other cases share
	QVERIFY(settings.isValid());
	QSettings::setPath(QSettings::defaultFormat(), QSettings::UserScope, settings.path());
	SettingsManager::instance().load();
}

void TestAutoreplace::activate(CyberiadaSMEditorWindow& window)
{
	window.show();
	QApplication::setActiveWindow(&window);
	QVERIFY(QTest::qWaitForWindowActive(&window));
}

template <class Text> Text* TestAutoreplace::edit(CyberiadaSMEditorWindow& window, bool (*wanted)(Text*))
{
	window.sceneView->setFocus();
	for (QGraphicsItem* item : window.getScene()->items()) {
		Text* text = dynamic_cast<Text*>(item);
		if (!text || !text->isVisible() || text->toPlainText().isEmpty()) continue;
		if (wanted && !wanted(text)) continue;
		text->startEditing();
		QTextCursor cursor = text->textCursor();
		cursor.movePosition(QTextCursor::End);
		text->setTextCursor(cursor);
		return text;
	}
	return nullptr;
}

CodeEditor* TestAutoreplace::textTab(CyberiadaSMEditorWindow& window, const QString& mode)
{
	TextDocument* text = window.newTextDocument(mode);
	window.setCurrentDocument(text);
	text->editor()->setFocus();
	return text->editor();
}

// EDIT-AUTO-1: the default rules, the longest pattern, a pattern as written
void TestAutoreplace::test_rules()
{
	AutoreplaceRules rules = defaultAutoreplaceRules();
	QCOMPARE(rules.size(), 18);
	for (const AutoreplaceRule& rule : rules) QVERIFY(rule.enabled && !rule.from.isEmpty() && !rule.to.isEmpty());
	int dash = autoreplaceMatch("a --", rules);
	QVERIFY(dash >= 0);
	QCOMPARE(rules[dash].to, u("\u2013"));
	QCOMPARE(rules[autoreplaceMatch(u("a \u2013-"), rules)].to, u("\u2014"));
	QCOMPARE(rules[autoreplaceMatch("(c)", rules)].to, u("\u00a9"));
	QCOMPARE(rules[autoreplaceMatch(u("(\u0441)"), rules)].to, u("\u00a9"));
	QCOMPARE(rules[autoreplaceMatch("x<<", rules)].to, u("\u00ab"));
	QCOMPARE(rules[autoreplaceMatch("...", rules)].to, u("\u2026"));
	QCOMPARE(autoreplaceMatch("a -", rules), -1);
	QCOMPARE(autoreplaceMatch("(C)", rules), -1);
	QCOMPARE(autoreplaceMatch("-- ", rules), -1);
	QCOMPARE(autoreplaceMatch(QString(), rules), -1);

	AutoreplaceRules own;
	own << AutoreplaceRule("b", "1") << AutoreplaceRule("ab", "2") << AutoreplaceRule("cab", "3", false);
	QCOMPARE(autoreplaceMatch("cab", own), 1);
	QCOMPARE(autoreplaceMatch("b", own), 0);
	own[1].enabled = false;
	QCOMPARE(autoreplaceMatch("cab", own), 0);

	QVERIFY(autoreplaceAllowed(autoreplaceProse, "x--", 1, 3));
	QVERIFY(!autoreplaceAllowed(autoreplaceNone, "x--", 1, 3));
}

// EDIT-AUTO-4: the last two characters of each text, a pattern just typed
void TestAutoreplace::test_markdown_prose_data()
{
	QTest::addColumn<QString>("text");
	QTest::addColumn<bool>("prose");
	QTest::newRow("text") << "a --" << true;
	QTest::newRow("second line") << "one\ntwo --" << true;
	QTest::newRow("open code span") << "a `x--" << false;
	QTest::newRow("closed code span") << "a `x` b--" << true;
	QTest::newRow("double ticks open") << "a ``x ` y--" << false;
	QTest::newRow("open fence") << "```\nx--" << false;
	QTest::newRow("tilde fence") << "~~~ c\nline\nx--" << false;
	QTest::newRow("closed fence") << "```\ncode\n```\nx --" << true;
	QTest::newRow("indented code") << "text\n\n    x--" << false;
	QTest::newRow("indented list text") << "- item\n\n    x--" << true;
	QTest::newRow("formula") << "$a <=" << false;
	QTest::newRow("closed formula") << "$a$ b <=" << true;
	QTest::newRow("price") << "$5 and a <=" << true;
	QTest::newRow("display formula") << "$$\na <=" << false;
	QTest::newRow("comment being opened") << "<!--" << false;
	QTest::newRow("open comment") << "<!-- a\nb --" << false;
	QTest::newRow("closed comment") << "<!-- a -->\nb --" << true;
	QTest::newRow("tag") << "<a href=--" << false;
	QTest::newRow("closed tag") << "<b>a</b> --" << true;
	QTest::newRow("arrow, not a tag") << "a <-" << true;
	QTest::newRow("link target") << "[x](a--" << false;
	QTest::newRow("after a link") << "[x](u) a--" << true;
	QTest::newRow("url") << "see http://a.b/x--" << false;
	QTest::newRow("rule") << "--" << false;
	QTest::newRow("rule under a text") << "Title\n--" << false;
	QTest::newRow("table border") << "|a|b|\n|--" << false;
	QTest::newRow("quote marks") << ">>" << false;
	QTest::newRow("quoted text") << "> a >>" << true;
	QTest::newRow("escaped tick") << "a \\` b--" << true;
}

void TestAutoreplace::test_markdown_prose()
{
	QFETCH(QString, text);
	QFETCH(bool, prose);
	QCOMPARE(markdownProseAt(text, text.length() - 2, text.length()), prose);
	QCOMPARE(autoreplaceAllowed(autoreplaceMarkdown, text, text.length() - 2, text.length()), prose);
}

// EDIT-AUTO-2: a name and the body of an informal comment
void TestAutoreplace::test_canvas_prose()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("diagrams/comment-transition.graphml"));
	activate(window);
	StateTitle* title = edit<StateTitle>(window);
	QVERIFY(title);
	QCOMPARE(title->autoreplaceKind(), autoreplaceProse);
	const QString name = title->toPlainText();
	QTest::keyClicks(window.sceneView, " -- a->b (tm)...");
	QCOMPARE(title->toPlainText(), name + u(" \u2013 a\u2192b \u2122\u2026"));
	title->clearFocus();
	// the name of the model is the text typed
	bool found = false;
	const QString renamed = name + u(" \u2013 a\u2192b \u2122\u2026");
	for (const char* id : { "n0", "n1" }) {
		found = found || QString::fromStdString(window.getModel()->idToElement(id)->get_name()) == renamed;
	}
	QVERIFY(found);

	EditableTextItem* body = edit<EditableTextItem>(window, informalBody);
	QVERIFY(body);
	QCOMPARE(body->autoreplaceKind(), autoreplaceProse);
	const QString note = body->toPlainText();
	QTest::keyClicks(window.sceneView, " <<x>> --- (c) +-1");
	QCOMPARE(body->toPlainText(), note + u(" \u00abx\u00bb \u2014 \u00a9 \u00b11"));
	QTest::keyClick(window.sceneView, Qt::Key_Escape);
}

// EDIT-AUTO-3: an action and a transition label are code
void TestAutoreplace::test_canvas_code()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("diagrams/internal-transition.graphml"));
	activate(window);
	StateAction* action = edit<StateAction>(window);
	QVERIFY(action);
	QCOMPARE(action->autoreplaceKind(), autoreplaceNone);
	const QString code = action->toPlainText();
	QTest::keyClicks(window.sceneView, " x-- >> 1; a->b; ...");
	QCOMPARE(action->toPlainText(), code + " x-- >> 1; a->b; ...");
	QTest::keyClick(window.sceneView, Qt::Key_Escape);

	QVERIFY(window.openFile("diagrams/lift.graphml"));
	TransitionAction* label = edit<TransitionAction>(window);
	QVERIFY(label);
	QCOMPARE(label->autoreplaceKind(), autoreplaceNone);
	const QString event = label->toPlainText();
	QTest::keyClicks(window.sceneView, " [a <= b]/ c--");
	QCOMPARE(label->toPlainText(), event + " [a <= b]/ c--");
	QTest::keyClick(window.sceneView, Qt::Key_Escape);
}

// EDIT-AUTO-6: Backspace and Undo right after give the characters back, which stay
void TestAutoreplace::test_canvas_gives_back()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("diagrams/comment-transition.graphml"));
	activate(window);
	StateTitle* title = edit<StateTitle>(window);
	const QString name = title->toPlainText();
	QTest::keyClicks(window.sceneView, "--");
	QCOMPARE(title->toPlainText(), name + u("\u2013"));
	QTest::keyClick(window.sceneView, Qt::Key_Backspace);
	QCOMPARE(title->toPlainText(), name + "--");
	QCOMPARE(title->textCursor().position(), name.length() + 2);
	// the pattern given back is not replaced again, a later one is
	QTest::keyClicks(window.sceneView, "- ");
	QCOMPARE(title->toPlainText(), name + "--- ");
	QTest::keyClicks(window.sceneView, "->");
	QCOMPARE(title->toPlainText(), name + u("--- \u2192"));
	// a Backspace after another key deletes a character
	QTest::keyClicks(window.sceneView, "x");
	QTest::keyClick(window.sceneView, Qt::Key_Backspace);
	QTest::keyClick(window.sceneView, Qt::Key_Backspace);
	QCOMPARE(title->toPlainText(), name + "--- ");

	// the Undo of the text edited
	QTest::keyClicks(window.sceneView, "(c)");
	QCOMPARE(title->toPlainText(), name + u("--- \u00a9"));
	QTest::keyClick(window.sceneView, Qt::Key_Z, Qt::ControlModifier);
	QCOMPARE(title->toPlainText(), name + "--- (c)");
	QTest::keyClick(window.sceneView, Qt::Key_Escape);
	QCOMPARE(title->toPlainText(), name);
}

// EDIT-AUTO-2/4/5/6: a Markdown tab, its exclusions, a paste, the Undo of the window
void TestAutoreplace::test_markdown_tab()
{
	CyberiadaSMEditorWindow window;
	activate(window);
	CodeEditor* editor = textTab(window, MODE_MARKDOWN);
	TextDocument* text = window.currentText();
	QCOMPARE(editor->autoreplaceKind(), autoreplaceMarkdown);
	QTest::keyClicks(editor, "a -- b `x--` c->d");
	QCOMPARE(text->text(), u("a \u2013 b `x--` c\u2192d"));
	QTest::keyClick(editor, Qt::Key_Return);
	QTest::keyClicks(editor, "---");
	QTest::keyClick(editor, Qt::Key_Return);
	QTest::keyClicks(editor, "<!-- note -->");
	QVERIFY(text->text().endsWith("\n---\n<!-- note -->"));

	// a paste is not typed
	QTest::keyClick(editor, Qt::Key_Return);
	editor->insertPlainText("p -- q (c)");
	QVERIFY(text->text().endsWith("p -- q (c)"));

	// Undo gives the pattern back, which stays; Backspace alike
	QTest::keyClicks(editor, " z<<");
	QVERIFY(text->text().endsWith(u(" z\u00ab")));
	window.actionUndo->trigger();
	QVERIFY(text->text().endsWith(" z<<"));
	// the cursor is after the characters given back
	QVERIFY(editor->textCursor().atEnd());
	QTest::keyClicks(editor, "<");
	QVERIFY(text->text().endsWith(" z<<<"));
	QTest::keyClicks(editor, " ...");
	QCOMPARE(text->text().section(QChar(10), -1), u("p -- q (c) z<<< \u2026"));
	QTest::keyClick(editor, Qt::Key_Backspace);
	QVERIFY(text->text().endsWith(" ..."));

	// a rendered page is not typed into
	text->setReadOnlyFlag(true);
	const QString shown = text->text();
	QTest::keyClicks(editor, "--");
	QCOMPARE(text->text(), shown);
}

// EDIT-AUTO-3: a language and a plain text are not replaced
void TestAutoreplace::test_other_tabs()
{
	CyberiadaSMEditorWindow window;
	activate(window);
	CodeEditor* plain = textTab(window, MODE_TEXT);
	QCOMPARE(plain->autoreplaceKind(), autoreplaceNone);
	QTest::keyClicks(plain, "a -- b (c) ...");
	QCOMPARE(window.currentText()->text(), QString("a -- b (c) ..."));
	QVERIFY(window.openFile("texts/blink.cpp"));
	CodeEditor* code = window.currentText()->editor();
	QCOMPARE(code->autoreplaceKind(), autoreplaceNone);
	code->setFocus();
	code->moveCursor(QTextCursor::End);
	QTest::keyClicks(code, "i--; a << 2; p->q");
	QVERIFY(window.currentText()->text().endsWith("i--; a << 2; p->q"));
	// the mode of a tab changed: its kind follows
	QVERIFY(window.currentText()->setMode(MODE_MARKDOWN));
	QCOMPARE(code->autoreplaceKind(), autoreplaceMarkdown);
}

// EDIT-AUTO-7/8: the switch and the rules of the settings, stored when not the default
void TestAutoreplace::test_settings()
{
	SettingsManager& sm = SettingsManager::instance();
	QSignalSpy changed(&sm, &SettingsManager::autoreplaceChanged);
	QVERIFY(sm.getAutoreplaceEnabled());
	QCOMPARE(sm.getAutoreplaceRules(), defaultAutoreplaceRules());
	QVERIFY(QSettings().allKeys().filter("autoreplace/").isEmpty());

	CyberiadaSMEditorWindow window;
	activate(window);
	CodeEditor* editor = textTab(window, MODE_MARKDOWN);
	sm.setAutoreplaceEnabled(false);
	QCOMPARE(changed.count(), 1);
	QCOMPARE(QSettings().value("autoreplace/enabled").toBool(), false);
	QTest::keyClicks(editor, "a -- ");
	QCOMPARE(window.currentText()->text(), QString("a -- "));
	sm.setAutoreplaceEnabled(true);

	AutoreplaceRules rules = defaultAutoreplaceRules();
	rules[0].enabled = false;
	rules << AutoreplaceRule(":)", u("\u263a"));
	sm.setAutoreplaceRules(rules);
	QCOMPARE(changed.count(), 3);
	QTest::keyClicks(editor, "b -- :)");
	QCOMPARE(window.currentText()->text(), u("a -- b -- \u263a"));
	QCOMPARE(QSettings().value("autoreplace/rules/size").toInt(), rules.size());
	// read back as stored
	sm.load();
	QCOMPARE(sm.getAutoreplaceRules(), rules);
	QVERIFY(sm.getAutoreplaceEnabled());

	// the default rules are not stored
	sm.setAutoreplaceRules(defaultAutoreplaceRules());
	QVERIFY(QSettings().allKeys().filter("autoreplace/rules").isEmpty());
	sm.load();
	QCOMPARE(sm.getAutoreplaceRules(), defaultAutoreplaceRules());
	sm.setAutoreplaceEnabled(false);
	sm.loadDefaults();
	QVERIFY(sm.getAutoreplaceEnabled());
}

// EDIT-AUTO-7: the switch and the table of the Preferences on a copy, Apply, Cancel,
// Restore defaults and Reset
void TestAutoreplace::test_page()
{
	SettingsManager& sm = SettingsManager::instance();
	sm.loadDefaults();
	const AutoreplaceRules defaults = defaultAutoreplaceRules();
	CyberiadaSMEditorWindow window;
	activate(window);
	CodeEditor* editor = textTab(window, MODE_MARKDOWN);
	{
		PreferencesDialog dialog(&window);
		AutoreplacePage* page = dialog.autoreplacePage();
		QVERIFY(page);
		QTableWidget* table = dialog.findChild<QTableWidget*>("autoreplaceTable");
		QCheckBox* enabled = dialog.findChild<QCheckBox*>("autoreplaceEnabled");
		QVERIFY(table && enabled);
		QVERIFY(enabled->isChecked());
		// the rules and a free row
		QCOMPARE(table->rowCount(), defaults.size() + 1);
		QCOMPARE(page->rules(), defaults);
		QCOMPARE(table->item(0, 0)->text(), QString("--"));
		QCOMPARE(table->item(0, 0)->checkState(), Qt::Checked);

		// a rule switched off, one changed, one added in the free row, which is kept last
		table->item(0, 0)->setCheckState(Qt::Unchecked);
		table->item(2, 1)->setText("<");
		int last = table->rowCount() - 1;
		table->item(last, 1)->setText(u("\u263a"));
		table->item(last, 0)->setText(":)");
		QCOMPARE(table->rowCount(), defaults.size() + 2);
		QVERIFY(table->item(last + 1, 0)->text().isEmpty());
		// a pattern is never twice: the row stays free, the first one is shown
		table->item(last + 1, 0)->setText("--");
		QCOMPARE(table->rowCount(), defaults.size() + 2);
		QVERIFY(table->item(last + 1, 0)->text().isEmpty());
		QCOMPARE(table->currentRow(), 0);
		// a pattern emptied drops its row
		table->item(1, 0)->setText(QString());
		QCOMPARE(table->rowCount(), defaults.size() + 1);
		AutoreplaceRules edited = page->rules();
		QCOMPARE(edited.size(), defaults.size());
		QCOMPARE(edited[0], AutoreplaceRule("--", defaults[0].to, false));
		QCOMPARE(edited[1], AutoreplaceRule("<<", "<"));
		QCOMPARE(edited.last(), AutoreplaceRule(":)", u("\u263a")));

		// the settings keep theirs until Apply, which acts on the open text at once
		QCOMPARE(sm.getAutoreplaceRules(), defaults);
		dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
		QCOMPARE(sm.getAutoreplaceRules(), edited);
		QCOMPARE(QSettings().value("autoreplace/rules/size").toInt(), edited.size());
		QTest::keyClicks(editor, "a -- :)");
		QCOMPARE(window.currentText()->text(), u("a -- \u263a"));

		// Cancel: the switch turned off is dropped
		enabled->setChecked(false);
		QVERIFY(!table->isEnabled());
	}
	QVERIFY(sm.getAutoreplaceEnabled());
	{
		PreferencesDialog dialog(&window);
		QVERIFY(dialog.autoreplacePage()->isEnabledChecked());
		QCOMPARE(dialog.autoreplacePage()->rules(), sm.getAutoreplaceRules());
		dialog.findChild<QCheckBox*>("autoreplaceEnabled")->setChecked(false);
		dialog.findChild<QPushButton*>("autoreplaceDefaults")->click();
		QCOMPARE(dialog.autoreplacePage()->rules(), defaults);
		dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
	}
	QVERIFY(!sm.getAutoreplaceEnabled());
	QCOMPARE(sm.getAutoreplaceRules(), defaults);
	QVERIFY(QSettings().allKeys().filter("autoreplace/rules").isEmpty());
	QTest::keyClicks(editor, " (c)");
	QVERIFY(window.currentText()->text().endsWith(" (c)"));
	{
		// Reset of the Preferences turns the switch on again
		PreferencesDialog dialog(&window);
		QVERIFY(!dialog.autoreplacePage()->isEnabledChecked());
		dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Reset)->click();
		QVERIFY(dialog.autoreplacePage()->isEnabledChecked());
	}
	QVERIFY(sm.getAutoreplaceEnabled());
}

// EDIT-AUTO-1/5/6: a line edit watched: only the typing, the pattern given back
void TestAutoreplace::test_line_filter()
{
	SettingsManager::instance().loadDefaults();
	QLineEdit line;
	new AutoreplaceFilter(&line, autoreplaceProse);
	QSignalSpy edited(&line, &QLineEdit::textEdited);
	QTest::keyClicks(&line, "a -- b");
	QCOMPARE(line.text(), u("a \u2013 b"));
	// the replacement is an edit of the user too
	QCOMPARE(edited.count(), 7);
	QCOMPARE(edited.last().first().toString(), u("a \u2013 b"));
	QTest::keyClicks(&line, "(c)");
	QCOMPARE(line.text(), u("a \u2013 b\u00a9"));
	QTest::keyClick(&line, Qt::Key_Backspace);
	QCOMPARE(line.text(), u("a \u2013 b(c)"));
	QCOMPARE(line.cursorPosition(), line.text().length());
	QTest::keyClicks(&line, " ");
	QCOMPARE(line.text(), u("a \u2013 b(c) "));
	// in the middle of the text
	line.setCursorPosition(1);
	QTest::keyClicks(&line, "->");
	QCOMPARE(line.text(), u("a\u2192 \u2013 b(c) "));
	QCOMPARE(line.cursorPosition(), 2);
	// a text set or pasted is not typed
	line.setText("x--");
	QCOMPARE(line.text(), QString("x--"));
	line.insert("--");
	QCOMPARE(line.text(), QString("x----"));

	QLineEdit value;
	new AutoreplaceFilter(&value, autoreplaceNone);
	QTest::keyClicks(&value, "id--1 (c)");
	QCOMPARE(value.text(), QString("id--1 (c)"));
}

// EDIT-AUTO-2/3: the text dialog of a description and a body, not of code
void TestAutoreplace::test_text_dialog()
{
	{
		MultilineTextDialog dialog("t", "l", "The lift", codeRoleNone, nullptr, nullptr,
		                           codeMarkupPlain, autoreplaceProse);
		QPlainTextEdit* edit = dialog.findChild<QPlainTextEdit*>();
		QVERIFY(edit);
		QTest::keyClicks(edit, " -- up <<and>> down...");
		QCOMPARE(dialog.text(), u("The lift \u2013 up \u00aband\u00bb down\u2026"));
		QTest::keyClick(edit, Qt::Key_Backspace);
		QCOMPARE(dialog.text(), u("The lift \u2013 up \u00aband\u00bb down..."));
		edit->insertPlainText(" (c)");
		QVERIFY(dialog.text().endsWith(" (c)"));
	}
	{
		MultilineTextDialog dialog("t", "l", QString(), codeRoleMarkup, nullptr, nullptr,
		                           codeMarkupMarkdown, autoreplaceMarkdown);
		QPlainTextEdit* edit = dialog.findChild<QPlainTextEdit*>();
		QTest::keyClicks(edit, "a -- `b--` c->d");
		QCOMPARE(dialog.text(), u("a \u2013 `b--` c\u2192d"));
	}
	{
		// a behaviour, a formal body, the stream of a session
		MultilineTextDialog dialog("t", "l", QString(), codeRoleBehaviour);
		QPlainTextEdit* edit = dialog.findChild<QPlainTextEdit*>();
		QTest::keyClicks(edit, "i--; a << 2; p->q");
		QCOMPARE(dialog.text(), QString("i--; a << 2; p->q"));
	}
}

void TestAutoreplace::rows(QtProperty* parent, const QList<QtProperty*>& list, const QString& name,
                           QtProperty** found, QtProperty** other)
{
	Q_UNUSED(parent);
	for (QtProperty* p : list) {
		QtStringPropertyManager* m = dynamic_cast<QtStringPropertyManager*>(p->propertyManager());
		if (m && p->propertyName() == "Name" && m->value(p) == name) *found = p;
		else if (m && p->propertyName() == "ID") *other = p;
		rows(p, p->subProperties(), name, found, other);
	}
}

// EDIT-AUTO-2/3: the name row of the properties panel, not the id row
void TestAutoreplace::test_properties_name()
{
	SettingsManager::instance().loadDefaults();
	SettingsManager::instance().setShowProperties(true);
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("diagrams/comment-transition.graphml"));
	activate(window);
	CyberiadaSMModel* model = window.getModel();
	const Cyberiada::Element* element = model->idToElement("n0");
	const QString name = QString::fromStdString(element->get_name());
	window.SMView->select(model->elementToIndex(element));
	CyberiadaSMPropertiesWidget* panel = window.propertiesWidget;
	QtProperty* row = nullptr;
	QtProperty* id = nullptr;
	rows(nullptr, panel->properties(), name, &row, &id);
	QVERIFY(row && id);
	QVERIFY(panel->isNameProperty(row));
	QVERIFY(!panel->isNameProperty(id));

	panel->editItem(panel->items(row).first());
	QLineEdit* line = qobject_cast<QLineEdit*>(QApplication::focusWidget());
	QVERIFY(line);
	QCOMPARE(line->text(), name);
	line->end(false);
	QTest::keyClicks(line, " -- on");
	QCOMPARE(line->text(), name + u(" \u2013 on"));
	// the row commits every key: the model has the name typed
	QCOMPARE(QString::fromStdString(model->idToElement("n0")->get_name()), name + u(" \u2013 on"));
}

// EDIT-AUTO-2: the rename of the structure tree
void TestAutoreplace::test_tree_rename()
{
	SettingsManager::instance().setShowStructureTree(true);
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("diagrams/comment-transition.graphml"));
	activate(window);
	CyberiadaSMModel* model = window.getModel();
	const QString name = QString::fromStdString(model->idToElement("n1")->get_name());
	QModelIndex index = model->elementToIndex(model->idToElement("n1"));
	window.SMView->setCurrentIndex(index);
	window.SMView->edit(index);
	QLineEdit* line = window.SMView->findChild<QLineEdit*>();
	QVERIFY(line);
	QCOMPARE(line->text(), name);
	line->end(false);
	QTest::keyClicks(line, " (tm)");
	QCOMPARE(line->text(), name + u(" \u2122"));
	// the delegate commits the line from the event loop
	QTest::keyClick(line, Qt::Key_Return);
	QTRY_COMPARE(QString::fromStdString(model->idToElement("n1")->get_name()), name + u(" \u2122"));
}

QTEST_MAIN(TestAutoreplace)
#include "l4-autoreplace.moc"
