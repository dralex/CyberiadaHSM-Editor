/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process tests of the color schemes of the text editors
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
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextEdit>
#include <QTextLayout>
#include <QToolButton>
#include <QUndoStack>
#include "code_editor.h"
#include "code_highlighter.h"
#include "cyberiada_constants.h"
#include "document_mode.h"
#include "find_query.h"
#include "fontmanager.h"
#include "markdown_objects.h"
#include "markdown_view.h"
#include "settings_manager.h"
#include "text_document.h"
#include "text_diff_view.h"
#include "text_scheme.h"
#include "dialogs/preferences_dialog.h"
#include "dialogs/text_scheme_panel.h"

// the EDIT-COLOR laws (docs/COLOR-SCHEMES.md): the built-in schemes, the text editors
// in the current one, everything else in White, the storage
class TestColors: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void init();
	void test_builtin_schemes();
	void test_canvas_keeps_white();
	void test_text_tab_follows();
	void test_change_keeps_text();
	void test_storage();
	void test_fallbacks();
	void test_rendered_page();
	void test_page();
	void test_page_schemes();
	void test_reset();
	void test_editor_font();
	void test_page_font();
	void test_editor_cursor();
	void test_editor_wrap();
	void test_mode_line();

private:
	static QPushButton* button(QWidget* parent, const char* name) { return parent->findChild<QPushButton*>(name); }
	// the foreground the highlighting gives the first occurrence of the word
	static QColor colorAt(QTextDocument* doc, const QString& word);
	static QColor dark(TextSchemeRole role) { return builtinTextScheme("dark").color(role); }
	QTemporaryDir settings;
};

void TestColors::initTestCase()
{
	// the stored schemes must not reach the profile the other cases share
	QVERIFY(settings.isValid());
	QSettings::setPath(QSettings::defaultFormat(), QSettings::UserScope, settings.path());
	FontManager::instance().loadBundledFont();
}

void TestColors::init()
{
	QSettings s;
	s.remove("textSchemes");
	s.remove("display/editorFontFamily");
	s.remove("display/editorFontSize");
	s.remove("display/editorCursor");
	s.remove("display/editorWrap");
	s.remove("display/showModeLine");
	s.remove("editor/textWidth");
	s.remove("editor/fillAsYouType");
	SettingsManager::instance().load();
}

QColor TestColors::colorAt(QTextDocument* doc, const QString& word)
{
	QTextCursor cursor = doc->find(word);
	if (cursor.isNull()) return QColor();
	QTextBlock block = cursor.block();
	int at = cursor.selectionStart() - block.position();
	for (const QTextLayout::FormatRange& r : block.layout()->formats()) {
		if (r.start <= at && at < r.start + r.length && r.format.hasProperty(QTextFormat::ForegroundBrush)) {
			return r.format.foreground().color();
		}
	}
	return QColor();
}

// EDIT-COLOR-2: White as the canvas colors, Dark yellow on blue, every color given
void TestColors::test_builtin_schemes()
{
	const TextSchemes& builtins = builtinTextSchemes();
	QCOMPARE(builtins.size(), 2);
	QCOMPARE(builtins[0].id, QString(TEXT_SCHEME_DEFAULT));
	QCOMPARE(builtins[1].id, QString("dark"));
	QCOMPARE(builtins[0].displayName(), QString("White"));
	QCOMPARE(builtins[1].displayName(), QString("Dark"));
	QSet<QString> keys;
	for (int role = 0; role < textSchemeRolesCount; role++) {
		QVERIFY(!textSchemeRoleName(role).isEmpty());
		keys.insert(textSchemeRoleKey(role));
	}
	QCOMPARE(keys.size(), int(textSchemeRolesCount));
	for (const TextScheme& scheme : builtins) {
		QVERIFY(scheme.isBuiltin());
		for (int role = 0; role < textSchemeRolesCount; role++) QVERIFY2(scheme.colors[role].isValid(), qPrintable(scheme.id));
	}
	// the colors the canvas always had
	const char* const canvas[codeTokenKindsCount] = {
		"#0033b3", "#007f7f", "#067d17", "#1750eb", "#8c8c8c", "#871094",
		"#9e5a00", "#067d17", "#1750eb", "#8c8c8c", "#5d6d7e", "#a31b5a"
	};
	for (int kind = 0; kind < codeTokenKindsCount; kind++) {
		QCOMPARE(builtins[0].tokenColor(CodeTokenKind(kind)), QColor(canvas[kind]));
	}
	QCOMPARE(builtins[0].color(textSchemeBackground), QColor(Qt::white));
	QCOMPARE(builtins[0].color(textSchemeText), QColor(Qt::black));
	QCOMPARE(builtins[1].color(textSchemeBackground), QColor("#000099"));
	QCOMPARE(builtins[1].color(textSchemeText), QColor("#ffff00"));
	// White is current by default
	QCOMPARE(SettingsManager::instance().currentTextScheme().id, QString(TEXT_SCHEME_DEFAULT));
}

// EDIT-COLOR-1: the canvas, the dialogs and the exports keep White whatever the scheme
void TestColors::test_canvas_keeps_white()
{
	SettingsManager::instance().setTextScheme("dark");
	const TextScheme& white = builtinTextScheme(TEXT_SCHEME_DEFAULT);
	for (int kind = 0; kind < codeTokenKindsCount; kind++) {
		QCOMPARE(CodeStyle::instance().format(CodeTokenKind(kind)).foreground().color(),
		         white.tokenColor(CodeTokenKind(kind)));
	}
	QTextDocument doc;
	// a document without a layout emits no contentsChange; the items have one
	doc.documentLayout();
	CodeHighlighter highlighter(&doc, codeRoleFormalComment, CodeStyle::instance().findId("cpp"));
	doc.setPlainText("int main() { return 0; }");
	QCOMPARE(colorAt(&doc, "int"), white.tokenColor(codeTokenType));
	QCOMPARE(colorAt(&doc, "return"), white.tokenColor(codeTokenKeyword));
}

// EDIT-COLOR-1: the source editor, its margin and its highlighting by the current scheme
void TestColors::test_text_tab_follows()
{
	SettingsManager& sm = SettingsManager::instance();
	TextDocument text("cpp");
	text.setText("int main() { return 0; }");
	CodeEditor* editor = text.editor();
	const TextScheme& white = builtinTextScheme(TEXT_SCHEME_DEFAULT);
	QCOMPARE(editor->palette().color(QPalette::Base), QColor(Qt::white));
	QCOMPARE(colorAt(editor->document(), "return"), white.tokenColor(codeTokenKeyword));

	sm.setTextScheme("dark");
	QCOMPARE(editor->palette().color(QPalette::Base), dark(textSchemeBackground));
	QCOMPARE(editor->palette().color(QPalette::Text), dark(textSchemeText));
	QCOMPARE(editor->palette().color(QPalette::Highlight), dark(textSchemeSelection));
	QCOMPARE(editor->palette().color(QPalette::HighlightedText), dark(textSchemeSelectionText));
	QCOMPARE(editor->palette().color(QPalette::AlternateBase), dark(textSchemeLineNumbersBackground));
	QCOMPARE(editor->palette().color(QPalette::PlaceholderText), dark(textSchemeLineNumbers));
	QCOMPARE(colorAt(editor->document(), "return"), QColor("#ffffff"));
	QCOMPARE(colorAt(editor->document(), "int"), QColor("#00ffff"));

	// another mode keeps the scheme
	QVERIFY(text.setMode(MODE_MARKDOWN));
	text.setText("# Title\n\n*stress*");
	QCOMPARE(colorAt(editor->document(), "Title"), dark(TextSchemeRole(textSchemeFirstToken + codeTokenHeading)));
	QCOMPARE(colorAt(editor->document(), "stress"), dark(TextSchemeRole(textSchemeFirstToken + codeTokenEmphasis)));

	// a tab opened later takes it too
	TextDocument later("cpp");
	later.setText("int x;");
	QCOMPARE(later.editor()->palette().color(QPalette::Base), dark(textSchemeBackground));
	QCOMPARE(colorAt(later.editor()->document(), "int"), QColor("#00ffff"));

	sm.setTextScheme(TEXT_SCHEME_DEFAULT);
	QCOMPARE(editor->palette().color(QPalette::Base), QColor(Qt::white));
	QCOMPARE(colorAt(editor->document(), "Title"), white.tokenColor(codeTokenHeading));
}

// EDIT-COLOR-5: a change re-colours an open tab; the text, the history and the marks stay
void TestColors::test_change_keeps_text()
{
	SettingsManager& sm = SettingsManager::instance();
	TextDocument text("cpp");
	text.setText("int a; int b;");
	QTextCursor cursor(text.editor()->document());
	cursor.movePosition(QTextCursor::End);
	cursor.insertText(" int c;");
	const QString before = text.text();
	const int steps = text.undoStack()->count();
	const bool clean = text.isClean();
	FindQuery query;
	query.text = "int";
	text.setFindQuery(query, false);
	QCOMPARE(text.findMatches().size(), 3);
	const int current = text.findCurrent();

	sm.setTextScheme("dark");
	QCoreApplication::processEvents();
	QCOMPARE(text.text(), before);
	QCOMPARE(text.undoStack()->count(), steps);
	QCOMPARE(text.isClean(), clean);
	QCOMPARE(text.findMatches().size(), 3);
	QCOMPARE(text.findCurrent(), current);
	const QList<QTextEdit::ExtraSelection> marks = text.editor()->extraSelections();
	QCOMPARE(marks.size(), 3);
	for (int i = 0; i < marks.size(); i++) {
		QCOMPARE(marks[i].format.background().color(),
		         QColor(i == current ? FIND_CURRENT_COLOR : FIND_MATCH_COLOR));
		QCOMPARE(marks[i].format.foreground().color(), QColor(Qt::black));
	}

	// a color of the current scheme changed
	TextSchemes schemes = sm.getTextSchemes();
	schemes[1].colors[textSchemeFirstToken + codeTokenType] = QColor("#ff0000");
	sm.setTextSchemes(schemes);
	QCOMPARE(colorAt(text.editor()->document(), "int"), QColor("#ff0000"));
	QCOMPARE(text.text(), before);
}

// EDIT-COLOR-6: a built-in scheme by its changed colors, a user one by all, nothing when unchanged
void TestColors::test_storage()
{
	SettingsManager& sm = SettingsManager::instance();
	QVERIFY(QSettings().allKeys().filter("textSchemes/").isEmpty());
	TextSchemes schemes = sm.getTextSchemes();
	QCOMPARE(schemes.size(), 2);
	schemes[1].colors[textSchemeFirstToken + codeTokenKeyword] = QColor("#123456");
	TextScheme mine = schemes[1];
	mine.id = TEXT_SCHEME_USER_PREFIX "1";
	mine.name = "Mine";
	schemes << mine;
	QSignalSpy spy(&sm, &SettingsManager::textSchemeChanged);
	sm.setTextSchemes(schemes);
	QCOMPARE(spy.count(), 1);
	sm.setTextScheme(mine.id);
	QCOMPARE(spy.count(), 2);
	QCOMPARE(sm.currentTextScheme().name, QString("Mine"));

	QSettings s;
	const QStringList keys = s.allKeys().filter("textSchemes/");
	QCOMPARE(s.value("textSchemes/current").toString(), mine.id);
	QCOMPARE(s.value("textSchemes/list/size").toInt(), 2);
	QCOMPARE(s.value("textSchemes/list/1/id").toString(), QString("dark"));
	QCOMPARE(s.value("textSchemes/list/1/keyword").toString(), QString("#123456"));
	QCOMPARE(keys.filter("textSchemes/list/1/").size(), 2);
	QCOMPARE(s.value("textSchemes/list/2/name").toString(), QString("Mine"));
	QCOMPARE(keys.filter("textSchemes/list/2/").size(), 2 + int(textSchemeRolesCount));

	sm.load();
	QVERIFY(sm.getTextSchemes() == schemes);
	QCOMPARE(sm.getTextScheme(), mine.id);

	// the built-in colors again: nothing stored, the user scheme gone, White current
	sm.setTextSchemes(builtinTextSchemes());
	QVERIFY(QSettings().allKeys().filter("textSchemes/list").isEmpty());
	QCOMPARE(sm.currentTextScheme().id, QString(TEXT_SCHEME_DEFAULT));
}

// EDIT-COLOR-6: an unknown current scheme is White; a bad color is the built-in one's
void TestColors::test_fallbacks()
{
	SettingsManager& sm = SettingsManager::instance();
	{
		QSettings s;
		s.setValue("textSchemes/current", "nothing");
		s.beginWriteArray("textSchemes/list");
		s.setArrayIndex(0);
		s.setValue("id", "dark");
		s.setValue("text", "not a color");
		s.setValue("keyword", "#010203");
		s.setArrayIndex(1);
		s.setValue("id", TEXT_SCHEME_USER_PREFIX "7");
		s.setValue("name", "Half");
		s.setValue("background", "#102030");
		// without a name, and neither built in nor a user one
		s.setArrayIndex(2);
		s.setValue("id", TEXT_SCHEME_USER_PREFIX "8");
		s.setArrayIndex(3);
		s.setValue("id", "other");
		s.setValue("name", "Other");
		s.endArray();
	}
	sm.load();
	QCOMPARE(sm.currentTextScheme().id, QString(TEXT_SCHEME_DEFAULT));
	QCOMPARE(sm.getTextSchemes().size(), 3);
	const TextScheme* changed = findTextScheme(sm.getTextSchemes(), "dark");
	QVERIFY(changed);
	QCOMPARE(changed->color(textSchemeText), dark(textSchemeText));
	QCOMPARE(changed->tokenColor(codeTokenKeyword), QColor("#010203"));
	const TextScheme* half = findTextScheme(sm.getTextSchemes(), TEXT_SCHEME_USER_PREFIX "7");
	QVERIFY(half);
	QCOMPARE(half->name, QString("Half"));
	QCOMPARE(half->color(textSchemeBackground), QColor("#102030"));
	QCOMPARE(half->color(textSchemeText), builtinTextScheme(TEXT_SCHEME_DEFAULT).color(textSchemeText));
}

// EDIT-COLOR-7: the rendered page black on white under Dark, the source in Dark
void TestColors::test_rendered_page()
{
	SettingsManager::instance().setTextScheme("dark");
	TextDocument text(MODE_MARKDOWN);
	text.setText("> a quote\n\ntext");
	text.setReadOnlyFlag(true);
	QVERIFY(text.showsRendered());
	MarkdownView* view = text.page()->findChild<MarkdownView*>();
	QVERIFY(view);
	QCOMPARE(view->palette().color(QPalette::Base), QColor(Qt::white));
	QCOMPARE(view->palette().color(QPalette::Text), QColor(Qt::black));
	QTextDocument page;
	setMarkdownColors(&page, Qt::black, Qt::white);
	QCOMPARE(markdownQuoteColor(view->document()), markdownQuoteColor(&page));
	QCOMPARE(text.editor()->palette().color(QPalette::Base), dark(textSchemeBackground));
}

// EDIT-COLOR-3: the group beside the font, the swatches, the preview, Apply, Cancel, Ok
void TestColors::test_page()
{
	SettingsManager& sm = SettingsManager::instance();
	TextDocument text("cpp");
	text.setText("int main() { return 0; }");
	{
		PreferencesDialog dialog;
		TextSchemePanel* panel = dialog.textSchemePanel();
		QVERIFY(panel);
		QWidget* tab = dialog.findChild<QWidget*>("textTab");
		QVERIFY(tab && tab->isAncestorOf(panel) && tab->isAncestorOf(dialog.findChild<QWidget*>("fontFamilyComboBox")));
		QComboBox* list = panel->findChild<QComboBox*>("textSchemeList");
		QCOMPARE(list->count(), 2);
		QCOMPARE(list->itemText(0), QString("White"));
		QCOMPARE(list->itemText(1), QString("Dark"));
		QCOMPARE(panel->shownId(), QString(TEXT_SCHEME_DEFAULT));
		for (int role = 0; role < textSchemeRolesCount; role++) {
			QVERIFY(panel->findChild<QToolButton*>("textSchemeSwatch-" + textSchemeRoleKey(role)));
		}

		// the preview follows the scheme shown and its edits
		list->setCurrentIndex(1);
		QCOMPARE(panel->shownId(), QString("dark"));
		QCOMPARE(panel->preview()->palette().color(QPalette::Base), dark(textSchemeBackground));
		QCOMPARE(colorAt(panel->preview()->document(), "int"), QColor("#00ffff"));
		panel->setColor(textSchemeFirstToken + codeTokenKeyword, QColor("#ff0000"));
		QCOMPARE(colorAt(panel->preview()->document(), "return"), QColor("#ff0000"));
		QCOMPARE(panel->findChild<QToolButton*>("textSchemeSwatch-keyword")->toolTip(), QString("#ff0000"));

		// Cancel: nothing changes
		dialog.reject();
	}
	QCOMPARE(sm.getTextScheme(), QString(TEXT_SCHEME_DEFAULT));
	QVERIFY(sm.getTextSchemes() == builtinTextSchemes());
	QCOMPARE(text.editor()->palette().color(QPalette::Base), QColor(Qt::white));
	{
		PreferencesDialog dialog;
		TextSchemePanel* panel = dialog.textSchemePanel();
		QVERIFY(panel->showScheme("dark"));
		panel->setColor(textSchemeFirstToken + codeTokenKeyword, QColor("#ff0000"));
		dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
		QCOMPARE(sm.getTextScheme(), QString("dark"));
		QCOMPARE(sm.currentTextScheme().tokenColor(codeTokenKeyword), QColor("#ff0000"));
		QCOMPARE(QSettings().value("textSchemes/list/1/keyword").toString(), QString("#ff0000"));
		// the open tab follows at once
		QCOMPARE(text.editor()->palette().color(QPalette::Base), dark(textSchemeBackground));
		QCOMPARE(colorAt(text.editor()->document(), "return"), QColor("#ff0000"));

		// Ok makes the scheme shown current
		QVERIFY(panel->showScheme(TEXT_SCHEME_DEFAULT));
		dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
		QCOMPARE(sm.getTextScheme(), QString(TEXT_SCHEME_DEFAULT));
		QCOMPARE(text.editor()->palette().color(QPalette::Base), QColor(Qt::white));
	}
	// a new dialog shows the stored schemes
	PreferencesDialog again;
	QVERIFY(again.textSchemePanel()->showScheme("dark"));
	QCOMPARE(findTextScheme(again.textSchemePanel()->schemes(), "dark")->tokenColor(codeTokenKeyword), QColor("#ff0000"));
}

// EDIT-COLOR-4: Copy, Rename and Delete of a user scheme, Restore of a built-in one
void TestColors::test_page_schemes()
{
	TextSchemePanel panel;
	QPushButton* rename = button(&panel, "textSchemeRename");
	QPushButton* remove = button(&panel, "textSchemeDelete");
	QPushButton* restore = button(&panel, "textSchemeRestore");
	QComboBox* list = panel.findChild<QComboBox*>("textSchemeList");
	QVERIFY(rename && remove && restore && list && button(&panel, "textSchemeCopy"));
	QVERIFY(!rename->isEnabled() && !remove->isEnabled() && !restore->isEnabled());

	// a name not empty and no other scheme's in any case
	QVERIFY(panel.showScheme("dark"));
	QVERIFY(!panel.copyScheme("  "));
	QVERIFY(!panel.copyScheme("white"));
	QVERIFY(panel.copyScheme(" Mine "));
	const QString mine = TEXT_SCHEME_USER_PREFIX "1";
	QCOMPARE(panel.shownId(), mine);
	QCOMPARE(list->count(), 3);
	QCOMPARE(list->currentText(), QString("Mine"));
	const TextScheme* copy = findTextScheme(panel.schemes(), mine);
	QVERIFY(copy && copy->sameColors(builtinTextScheme("dark")));
	QVERIFY(rename->isEnabled() && remove->isEnabled() && !restore->isEnabled());
	QVERIFY(!panel.copyScheme("MINE"));

	QVERIFY(!panel.renameScheme("Dark"));
	QVERIFY(panel.renameScheme("mine"));
	QVERIFY(panel.renameScheme("Ours"));
	QCOMPARE(list->currentText(), QString("Ours"));

	// a built-in one is not renamed; Restore while its colors differ
	QVERIFY(panel.showScheme("dark"));
	QVERIFY(!panel.renameScheme("Other"));
	QVERIFY(!rename->isEnabled() && !remove->isEnabled() && !restore->isEnabled());
	panel.setColor(textSchemeBackground, QColor("#000000"));
	QVERIFY(restore->isEnabled());
	restore->click();
	QVERIFY(findTextScheme(panel.schemes(), "dark")->sameColors(builtinTextScheme("dark")));
	QVERIFY(!restore->isEnabled());

	// Delete gives way to White; the id is free again
	QVERIFY(panel.showScheme(mine));
	remove->click();
	QCOMPARE(panel.shownId(), QString(TEXT_SCHEME_DEFAULT));
	QCOMPARE(list->count(), 2);
	QVERIFY(!findTextScheme(panel.schemes(), mine));
	QVERIFY(panel.copyScheme("Again"));
	QCOMPARE(panel.shownId(), mine);
	QVERIFY(findTextScheme(panel.schemes(), mine)->sameColors(builtinTextScheme(TEXT_SCHEME_DEFAULT)));
}

// EDIT-COLOR-8: Reset restores the built-in schemes, drops the user ones, White current
void TestColors::test_reset()
{
	SettingsManager& sm = SettingsManager::instance();
	TextSchemes schemes = sm.getTextSchemes();
	schemes[1].colors[textSchemeText] = QColor("#ffffff");
	TextScheme mine = schemes[1];
	mine.id = TEXT_SCHEME_USER_PREFIX "1";
	mine.name = "Mine";
	schemes << mine;
	sm.setTextSchemes(schemes);
	sm.setTextScheme(mine.id);
	PreferencesDialog dialog;
	QCOMPARE(dialog.textSchemePanel()->shownId(), mine.id);
	dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Reset)->click();
	QVERIFY(sm.getTextSchemes() == builtinTextSchemes());
	QCOMPARE(sm.getTextScheme(), QString(TEXT_SCHEME_DEFAULT));
	QVERIFY(QSettings().allKeys().filter("textSchemes/list").isEmpty());
	QCOMPARE(dialog.textSchemePanel()->shownId(), QString(TEXT_SCHEME_DEFAULT));
	QCOMPARE(dialog.textSchemePanel()->findChild<QComboBox*>("textSchemeList")->count(), 2);
}

// EDIT-MODE-27: the editors in their own font, bundled 12 pt by default, apart from the diagram font
void TestColors::test_editor_font()
{
	SettingsManager& sm = SettingsManager::instance();
	FontManager& fm = FontManager::instance();
	QCOMPARE(sm.getEditorFontFamily(), QString());
	QCOMPARE(sm.getEditorFontSize(), FONT_SIZE);
	QCOMPARE(fm.editorFont().family(), fm.bundledFamily());
	QCOMPARE(fm.editorFont().pointSize(), FONT_SIZE);
	TextDocument text("cpp");
	text.setText("\tint x;");
	CodeEditor* editor = text.editor();
	QCOMPARE(editor->font().family(), fm.bundledFamily());
	QCOMPARE(editor->font().pointSize(), FONT_SIZE);

	// a change re-fonts the open tab, its tab stop in the new metrics; the diagram font stays
	const QFont diagram = fm.font(fontRoleStateAction);
	QSignalSpy diagramSpy(&fm, &FontManager::fontsChanged);
	QSignalSpy spy(&sm, &SettingsManager::editorFontChanged);
	const int size = FONT_SIZE * 2;
	sm.setEditorFontSize(size);
	QCOMPARE(spy.count(), 1);
	QCOMPARE(diagramSpy.count(), 0);
	QCOMPARE(editor->font().pointSize(), size);
	QCOMPARE(editor->tabStopDistance(), qreal(editor->fontMetrics().horizontalAdvance(' ') * 4));
	QCOMPARE(fm.font(fontRoleStateAction), diagram);
	sm.setEditorFontSize(FONT_SIZE_MAX + 1);
	QCOMPARE(sm.getEditorFontSize(), size);

	// another monospaced family, when the system has one
	QString other;
	QFontDatabase fonts;
	for (const QString& family : fonts.families()) {
		if (family != fm.bundledFamily() && fonts.isFixedPitch(family)) {
			other = family;
			break;
		}
	}
	if (!other.isEmpty()) {
		sm.setEditorFontFamily(other);
		QCOMPARE(editor->font().family(), other);
	}

	// a new editor takes it, as a side of a text diff tab does
	CodeEditor fresh;
	QCOMPARE(fresh.font(), editor->font());
	// the rendered page keeps its own font
	MarkdownView page;
	QVERIFY(page.font() != editor->font());

	QSettings s;
	QCOMPARE(s.value("display/editorFontSize").toInt(), size);
	QCOMPARE(s.value("display/editorFontFamily").toString(), other);
	sm.load();
	QCOMPARE(sm.getEditorFontSize(), size);
	QCOMPARE(sm.getEditorFontFamily(), other);
}

// EDIT-MODE-27, EDIT-COLOR-3: the font row of Text editors, its preview, Apply, Cancel, Reset
void TestColors::test_page_font()
{
	SettingsManager& sm = SettingsManager::instance();
	FontManager& fm = FontManager::instance();
	TextDocument text("cpp");
	{
		PreferencesDialog dialog;
		QCOMPARE(dialog.findChild<QLabel*>("fontFamilyLabel")->text(), QString("Diagram font"));
		QCOMPARE(dialog.findChild<QLabel*>("textSchemeTitle")->text(), QString("Text editors"));
		TextSchemePanel* panel = dialog.textSchemePanel();
		QFontComboBox* family = panel->findChild<QFontComboBox*>("textEditorFontFamily");
		QSpinBox* size = panel->findChild<QSpinBox*>("textEditorFontSize");
		QVERIFY(family && size);
		QCOMPARE(family->currentFont().family(), fm.bundledFamily());
		QCOMPARE(size->value(), FONT_SIZE);
		QCOMPARE(panel->preview()->font().pointSize(), FONT_SIZE);

		// the preview follows, the open tab not before Apply
		size->setValue(FONT_SIZE + 4);
		QCOMPARE(panel->preview()->font().pointSize(), FONT_SIZE + 4);
		QCOMPARE(text.editor()->font().pointSize(), FONT_SIZE);
		dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
		QCOMPARE(sm.getEditorFontSize(), FONT_SIZE + 4);
		QCOMPARE(text.editor()->font().pointSize(), FONT_SIZE + 4);
		// the bundled family is stored empty
		QCOMPARE(sm.getEditorFontFamily(), QString());

		// Cancel changes nothing
		size->setValue(FONT_SIZE_MAX);
		dialog.reject();
	}
	QCOMPARE(sm.getEditorFontSize(), FONT_SIZE + 4);
	QCOMPARE(text.editor()->font().pointSize(), FONT_SIZE + 4);

	// Reset of the Preferences: the bundled font at 12 pt
	PreferencesDialog dialog;
	dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Reset)->click();
	QCOMPARE(sm.getEditorFontSize(), FONT_SIZE);
	QCOMPARE(sm.getEditorFontFamily(), QString());
	QCOMPARE(text.editor()->font().pointSize(), FONT_SIZE);
	QCOMPARE(dialog.textSchemePanel()->findChild<QSpinBox*>("textEditorFontSize")->value(), FONT_SIZE);
}

// EDIT-MODE-29: the cursor a line by default, a block one space of the font wide in every
// source editor at once, one character after a font change too; stored; the page row with its
// preview before Apply, Apply and Reset
void TestColors::test_editor_cursor()
{
	SettingsManager& sm = SettingsManager::instance();
	TextDocument text(MODE_MARKDOWN);
	TextDocument code("cpp");
	QVERIFY(!sm.getEditorBlockCursor());
	QCOMPARE(text.editor()->cursorWidth(), 1);
	sm.setEditorBlockCursor(true);
	int space = QFontMetrics(text.editor()->font()).horizontalAdvance(' ');
	QVERIFY(space > 1);
	QCOMPARE(text.editor()->cursorWidth(), space);
	QCOMPARE(code.editor()->cursorWidth(), space);
	QCOMPARE(QSettings().value("display/editorCursor").toString(), QString(EDITOR_CURSOR_BLOCK));
	// a larger font: still one character
	sm.setEditorFontSize(FONT_SIZE * 2);
	int larger = QFontMetrics(text.editor()->font()).horizontalAdvance(' ');
	QVERIFY(larger > space);
	QCOMPARE(text.editor()->cursorWidth(), larger);
	// read back; a new editor starts with it
	sm.load();
	QVERIFY(sm.getEditorBlockCursor());
	CodeEditor fresh;
	QVERIFY(fresh.blockCursor());
	sm.setEditorBlockCursor(false);
	QCOMPARE(text.editor()->cursorWidth(), 1);
	QCOMPARE(QSettings().value("display/editorCursor").toString(), QString(EDITOR_CURSOR_LINE));

	PreferencesDialog dialog;
	TextSchemePanel* panel = dialog.textSchemePanel();
	QComboBox* cursor = panel->findChild<QComboBox*>("textEditorCursor");
	QVERIFY(cursor);
	QCOMPARE(cursor->count(), 2);
	QCOMPARE(cursor->itemText(0), QString("Line"));
	QVERIFY(!cursor->currentData().toBool());
	// the preview shows the choice, the open tabs not before Apply
	cursor->setCurrentIndex(cursor->findData(true));
	QVERIFY(panel->preview()->blockCursor());
	QVERIFY(!text.editor()->blockCursor());
	dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
	QVERIFY(sm.getEditorBlockCursor() && text.editor()->blockCursor());
	// Reset of the Preferences: the line
	dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Reset)->click();
	QVERIFY(!sm.getEditorBlockCursor() && !text.editor()->blockCursor());
	QVERIFY(!cursor->currentData().toBool());
}

// EDIT-MODE-30: the long lines scrolled by default; wrapped at any character with a fringe sign
// on each row that continues, the number once; a text diff side never wrapped; stored; the
// page row with its preview, Apply and Reset
void TestColors::test_editor_wrap()
{
	SettingsManager& sm = SettingsManager::instance();
	TextDocument text(MODE_MARKDOWN);
	text.setText(QString("short\n") + QString(400, QChar('x')) + "\nend\n");
	text.page()->resize(400, 300);
	text.page()->show();
	QVERIFY(QTest::qWaitForWindowExposed(text.page()));
	CodeEditor* editor = text.editor();
	QCOMPARE(editor->lineWrapMode(), QPlainTextEdit::NoWrap);
	QCOMPARE(editor->fringeWidth(), 0);
	QVERIFY(editor->continuationRows().isEmpty());
	sm.setEditorWrap(true);
	QCOMPARE(QSettings().value("display/editorWrap").toString(), QString(EDITOR_LINES_WRAP));
	QVERIFY(editor->wrapLines());
	QCOMPARE(editor->lineWrapMode(), QPlainTextEdit::WidgetWidth);
	QCOMPARE(editor->wordWrapMode(), QTextOption::WrapAnywhere);
	QVERIFY(editor->fringeWidth() > 0);
	// the long line takes several rows, all but its last marked; the short ones none
	QTextBlock longLine = editor->document()->findBlockByNumber(1);
	int rows = longLine.layout()->lineCount();
	QVERIFY(rows > 1);
	QCOMPARE(editor->continuationRows().size(), rows - 1);
	QCOMPARE(text.lineCount(), 4);
	// a text diff side keeps its rows
	DiffTextPane side;
	QCOMPARE(side.lineWrapMode(), QPlainTextEdit::NoWrap);
	QVERIFY(!side.wrapLines());
	sm.setEditorWrap(false);
	QCOMPARE(editor->lineWrapMode(), QPlainTextEdit::NoWrap);
	QCOMPARE(editor->fringeWidth(), 0);
	text.page()->hide();

	PreferencesDialog dialog;
	TextSchemePanel* panel = dialog.textSchemePanel();
	QComboBox* wrap = panel->findChild<QComboBox*>("textEditorWrap");
	QVERIFY(wrap);
	QCOMPARE(wrap->count(), 2);
	QCOMPARE(wrap->itemText(0), QString("Scroll"));
	QVERIFY(!wrap->currentData().toBool());
	wrap->setCurrentIndex(wrap->findData(true));
	QVERIFY(panel->preview()->wrapLines());
	QVERIFY(!editor->wrapLines());
	dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
	QVERIFY(sm.getEditorWrap() && editor->wrapLines());
	dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Reset)->click();
	QVERIFY(!sm.getEditorWrap() && !editor->wrapLines());
	QVERIFY(!wrap->currentData().toBool());
}

// EDIT-MODE-31: the parts of the mode line by the mode, the position with a tab, the state after
// an edit, an undo to clean and read-only, the selection count, the fill part and its click, the
// error bar above it, none on a diff side and the scheme preview, the scheme colors, the setting
void TestColors::test_mode_line()
{
	SettingsManager& sm = SettingsManager::instance();
	QVERIFY(sm.getShowModeLine());
	TextDocument doc(MODE_MARKDOWN);
	CodeEditor* editor = doc.editor();
	QWidget* bar = editor->findChild<QWidget*>("modeLine");
	QVERIFY(bar);
	QVERIFY(editor->modeLineShown());
	QVERIFY(editor->modeLineHeight() > 0);
	QCOMPARE(editor->modeLineText(), QString("--   untitled   (1,1)   Markdown   Fill 90"));
	// a tab counts to the next stop of 4, as the text width does
	QTest::keyClicks(editor, "ab\tc");
	QCOMPARE(doc.currentColumn(), 6);
	QCOMPARE(editor->modeLineText(), QString("**   untitled   (1,6)   Markdown   Fill 90"));
	while (doc.undoStack()->canUndo()) doc.undoStack()->undo();
	QCOMPARE(editor->modeLineText(), QString("--   untitled   (1,1)   Markdown   Fill 90"));
	// a selection: its characters, a line break one
	doc.setText("one\ntwo");
	editor->selectAll();
	QCOMPARE(editor->modeLineText(), QString("--   untitled   (2,4)   Markdown   Fill 90   7 selected"));
	doc.setReadOnlyFlag(true);
	QVERIFY(editor->modeLineText().startsWith("%%   "));
	doc.setReadOnlyFlag(false);
	doc.setLocked(true);
	QVERIFY(editor->modeLineText().startsWith("%%   "));
	doc.setLocked(false);
	QVERIFY(editor->modeLineText().startsWith("--   "));

	// the fill part: dim while off, a click switches it, the width follows
	QRect fill = editor->modeLineFillRect();
	QVERIFY(fill.isValid());
	QVERIFY(editor->modeLineToolTip(fill.center()).contains("off"));
	QVERIFY(editor->modeLineToolTip(QPoint(1, 1)).isEmpty());
	editor->modeLineClicked(QPoint(1, 1));
	QVERIFY(!sm.getFillAsYouType());
	doc.page()->resize(800, 300);
	doc.page()->show();
	QVERIFY(QTest::qWaitForWindowExposed(doc.page()));
	QVERIFY(bar->isVisible());
	QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, editor->modeLineFillRect().center());
	QVERIFY(sm.getFillAsYouType());
	QVERIFY(editor->modeLineToolTip(editor->modeLineFillRect().center()).contains("on"));
	sm.setTextWidth(72);
	QVERIFY(editor->modeLineText().contains("Fill 72"));
	sm.setFillAsYouType(false);
	sm.setTextWidth(TEXT_WIDTH_DEFAULT);
	// the colors of the line numbers in every scheme
	for (const char* id : { "white", "dark" }) {
		sm.setTextScheme(id);
		QImage image = bar->grab().toImage();
		QCOMPARE(QColor(image.pixel(image.width() - 2, image.height() / 2)),
				 builtinTextScheme(id).color(textSchemeLineNumbersBackground));
		QCOMPARE(QColor(image.pixel(image.width() / 2, 0)), builtinTextScheme(id).color(textSchemeLineNumbers));
	}
	sm.setTextScheme(TEXT_SCHEME_DEFAULT);
	// under the text and the error bar
	QCOMPARE(bar->geometry().top(), editor->viewport()->geometry().bottom() + 1 + editor->errorBarHeight());
	editor->setError(1, "a message");
	QVERIFY(editor->errorBarHeight() > 0);
	QCOMPARE(bar->geometry().top(), editor->viewport()->geometry().bottom() + 1 + editor->errorBarHeight());
	editor->setError(0, QString());
	doc.page()->hide();

	// the mode of each tab; no fill but in Markdown and Plain text
	TextDocument plain(MODE_TEXT);
	QCOMPARE(plain.editor()->modeLineText(), QString("--   untitled   (1,1)   Plain text   Fill 90"));
	TextDocument cpp("cpp");
	QCOMPARE(cpp.editor()->modeLineText(), QString("--   untitled   (1,1)   C++"));
	QVERIFY(!cpp.editor()->modeLineFillRect().isValid());
	TextDocument json(LANGUAGE_JSON);
	QCOMPARE(json.editor()->modeLineText(), QString("--   untitled   (1,1)   JSON"));
	QVERIFY(json.setMode(MODE_MARKDOWN));
	QCOMPARE(json.editor()->modeLineText(), QString("--   untitled   (1,1)   Markdown   Fill 90"));
	// a text diff side and the scheme preview have none
	DiffTextPane side;
	QVERIFY(!side.modeLineShown());
	QCOMPARE(side.modeLineHeight(), 0);
	PreferencesDialog dialog;
	QVERIFY(!dialog.textSchemePanel()->preview()->modeLineShown());

	// View > Mode Line: the setting hides it in every editor, stored, Reset shows it
	sm.setShowModeLine(false);
	QVERIFY(!editor->modeLineShown());
	QCOMPARE(editor->modeLineHeight(), 0);
	QVERIFY(editor->modeLineText().isEmpty());
	QVERIFY(!QSettings().value("display/showModeLine").toBool());
	dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Reset)->click();
	QVERIFY(sm.getShowModeLine());
	QVERIFY(editor->modeLineShown());
}

QTEST_MAIN(TestColors)
#include "l4-colors.moc"
