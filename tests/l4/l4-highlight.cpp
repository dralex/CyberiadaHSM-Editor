/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process code highlighting test (see docs/TESTING.md, L4)
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
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_render.h"
#include "editable_text_item.h"
#include "code_highlighter.h"
#include "settings_manager.h"
#include "fontmanager.h"
#include "dialogs/stateactiondialog.h"
#include "dialogs/multilinetextdialog.h"

// EDIT-TEXT-7, EDIT-TEXT-8, EDIT-TEXT-11
class TestHighlight: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void cleanupTestCase();
	void test_languages();
	void test_spans();
	void test_canvas();
	void test_multiline_comment();
	void test_language_change();
	void test_switch_off();
	void test_documents();
	void test_dialog();
	void test_export();
	void test_markup_lookup();
	void test_markdown_spans();
	void test_markdown_fence_lines();
	void test_markdown_canvas();
	void test_markdown_dialog();

private:
	QList<EditableTextItem*> codeItems(CodeRole role);
	CyberiadaSMModel* model;
	CyberiadaSMEditorScene* scene;
};

// the highlight colour drawn at the position, invalid when plain
static QColor colorAt(QTextDocument* doc, int pos)
{
	QTextBlock block = doc->findBlock(pos);
	int rel = pos - block.position();
	for (const QTextLayout::FormatRange& r : block.layout()->formats()) {
		if (rel >= r.start && rel < r.start + r.length && r.format.hasProperty(QTextFormat::ForegroundBrush)) {
			return r.format.foreground().color();
		}
	}
	return QColor();
}

static QColor kindColor(CodeTokenKind kind)
{
	return CodeStyle::instance().format(kind).foreground().color();
}

static bool hasFormats(QTextDocument* doc)
{
	for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
		if (!b.layout()->formats().isEmpty()) return true;
	}
	return false;
}

QList<EditableTextItem*> TestHighlight::codeItems(CodeRole role)
{
	QList<EditableTextItem*> result;
	const QList<QGraphicsItem*> items = scene->items();
	for (QGraphicsItem* item : items) {
		EditableTextItem* text = dynamic_cast<EditableTextItem*>(item);
		if (text && text->getCodeRole() == role) result.append(text);
	}
	return result;
}

void TestHighlight::initTestCase()
{
	FontManager::instance().loadBundledFont();
	SettingsManager::instance().loadDefaults();
	model = new CyberiadaSMModel(this);
	scene = new CyberiadaSMEditorScene(model, this);
	QVERIFY(model->loadDocument("diagrams/code-highlight.graphml"));
	scene->loadScene();
}

void TestHighlight::cleanupTestCase()
{
	SettingsManager::instance().loadDefaults();
}

void TestHighlight::test_languages()
{
	CodeStyle& style = CodeStyle::instance();
	QVERIFY(style.languages().contains("C++"));
	QVERIFY(style.languages().contains("Python"));
	// the scene keeps the language of its document
	QCOMPARE(scene->codeLanguageName(), QString("C++"));
	QVERIFY(scene->codeLanguage());
	QCOMPARE(scene->codeLanguage()->name, QString("C++"));

	// an alias, any case
	QVERIFY(style.find("ARDUINO"));
	QCOMPARE(style.find("ARDUINO")->name, QString("C++"));
	QCOMPARE(style.find("ts")->name, QString("JavaScript"));
	// unknown or empty: plain
	QVERIFY(!style.find("Cobol"));
	QVERIFY(!style.find(""));
}

void TestHighlight::test_spans()
{
	const CodeLanguage* cpp = CodeStyle::instance().find("C++");
	QVERIFY(cpp);

	// the keyword prefix is not code
	QString text = "entry/ int x = 0; // c";
	QVector<CodeSpan> spans = CodeStyle::spans(text, codeRoleBehaviour, *cpp);
	QCOMPARE(spans.size(), 3);
	QCOMPARE(spans[0].start, text.indexOf("int"));
	QCOMPARE(spans[0].kind, codeTokenType);
	QCOMPARE(spans[1].kind, codeTokenNumber);
	QCOMPARE(spans[2].kind, codeTokenComment);
	QCOMPARE(spans[2].start + spans[2].length, text.length());

	// the event is not code, the guard and the behaviour are
	text = "int [int > 0] / return \"s\";";
	spans = CodeStyle::spans(text, codeRoleTransition, *cpp);
	QCOMPARE(spans.size(), 4);
	QCOMPARE(spans[0].start, text.indexOf("int", 1));
	QCOMPARE(spans[0].kind, codeTokenType);
	QCOMPARE(spans[1].kind, codeTokenNumber);
	QCOMPARE(spans[2].kind, codeTokenKeyword);
	QCOMPARE(spans[3].kind, codeTokenString);

	// a lonely event has no code
	QVERIFY(CodeStyle::spans("return", codeRoleTransition, *cpp).isEmpty());
	// the formal comment is code from the start; the preprocessor is a keyword
	spans = CodeStyle::spans("#include x", codeRoleFormalComment, *cpp);
	QCOMPARE(spans.size(), 1);
	QCOMPARE(spans[0].start, 0);
	QCOMPARE(spans[0].kind, codeTokenKeyword);
	// nothing for the plain role
	QVERIFY(CodeStyle::spans("int x", codeRoleNone, *cpp).isEmpty());
}

void TestHighlight::test_canvas()
{
	// entry/exit, internal transition, edge label, formal comment
	QList<EditableTextItem*> behaviours = codeItems(codeRoleBehaviour);
	QList<EditableTextItem*> transitions = codeItems(codeRoleTransition);
	QList<EditableTextItem*> formal = codeItems(codeRoleFormalComment);
	QCOMPARE(behaviours.size(), 1);
	QCOMPARE(transitions.size(), 2);
	QCOMPARE(formal.size(), 1);

	QTextDocument* doc = behaviours[0]->document();
	QString text = doc->toPlainText();
	QCOMPARE(text, QString("entry/ int x = 0; // reset"));
	QVERIFY(!colorAt(doc, 0).isValid());
	QCOMPARE(colorAt(doc, text.indexOf("int")), kindColor(codeTokenType));
	QCOMPARE(colorAt(doc, text.indexOf("//")), kindColor(codeTokenComment));
	// the bold prefix survives (EDIT-TEXT-2)
	QTextCursor cursor(doc);
	cursor.setPosition(1);
	QCOMPARE(cursor.charFormat().fontWeight(), int(QFont::Bold));

	for (EditableTextItem* item : transitions) {
		QTextDocument* d = item->document();
		QString t = d->toPlainText();
		// the event is plain
		QVERIFY(!colorAt(d, 0).isValid());
		if (t.startsWith("TICK")) {
			QCOMPARE(colorAt(d, t.indexOf("0")), kindColor(codeTokenNumber));
			QCOMPARE(colorAt(d, t.indexOf("return")), kindColor(codeTokenKeyword));
		} else {
			QVERIFY(t.startsWith("GO"));
			QCOMPARE(colorAt(d, t.indexOf("\"now\"")), kindColor(codeTokenString));
		}
	}

	doc = formal[0]->document();
	QCOMPARE(colorAt(doc, 0), kindColor(codeTokenComment));
	QCOMPARE(colorAt(doc, doc->toPlainText().indexOf("static")), kindColor(codeTokenKeyword));
}

void TestHighlight::test_multiline_comment()
{
	QTextDocument doc;
	// a document without a layout emits no contentsChange; the items have one
	doc.documentLayout();
	CodeHighlighter highlighter(&doc, codeRoleFormalComment, CodeStyle::instance().find("C++"));
	doc.setPlainText("a /* x\ny */ int");
	int second = doc.toPlainText().indexOf("y");
	QCOMPARE(colorAt(&doc, second), kindColor(codeTokenComment));
	QCOMPARE(colorAt(&doc, doc.toPlainText().indexOf("int")), kindColor(codeTokenType));

	// closing the comment on the first line re-highlights the next one
	QTextCursor cursor(&doc);
	cursor.setPosition(doc.toPlainText().indexOf("x"));
	cursor.insertText("*/ ");
	second = doc.toPlainText().indexOf("y");
	QVERIFY(colorAt(&doc, second) != kindColor(codeTokenComment));
	QCOMPARE(doc.toPlainText(), QString("a /* */ x\ny */ int"));
}

void TestHighlight::test_language_change()
{
	QTextDocument* doc = codeItems(codeRoleBehaviour)[0]->document();
	int pos = doc->toPlainText().indexOf("//");

	QVERIFY(model->updateMetainformation(model->documentIndex(), "platformLanguage", "Python"));
	QCOMPARE(scene->codeLanguageName(), QString("Python"));
	// "//" is no Python comment
	QVERIFY(colorAt(doc, pos) != kindColor(codeTokenComment));

	model->undoStack()->undo();
	QCOMPARE(scene->codeLanguageName(), QString("C++"));
	doc = codeItems(codeRoleBehaviour)[0]->document();
	QCOMPARE(colorAt(doc, pos), kindColor(codeTokenComment));
}

void TestHighlight::test_switch_off()
{
	SettingsManager& sm = SettingsManager::instance();
	QTextDocument* doc = codeItems(codeRoleBehaviour)[0]->document();
	QString text = doc->toPlainText();
	sm.setHighlightCode(false);
	QVERIFY(!hasFormats(doc));
	sm.setHighlightCode(true);
	QVERIFY(hasFormats(doc));
	// the text itself never changes
	QCOMPARE(doc->toPlainText(), text);
}

void TestHighlight::test_documents()
{
	// a second document with another language keeps its own colours (EDIT-TEXT-7)
	CyberiadaSMModel python(this);
	CyberiadaSMEditorScene pythonScene(&python);
	QVERIFY(python.loadDocument("diagrams/code-highlight.graphml"));
	pythonScene.loadScene();
	QVERIFY(python.updateMetainformation(python.documentIndex(), "platformLanguage", "Python"));
	QCOMPARE(pythonScene.codeLanguage()->name, QString("Python"));
	QCOMPARE(scene->codeLanguage()->name, QString("C++"));

	QTextDocument* cppDoc = codeItems(codeRoleBehaviour)[0]->document();
	int pos = cppDoc->toPlainText().indexOf("//");
	QCOMPARE(colorAt(cppDoc, pos), kindColor(codeTokenComment));
	for (QGraphicsItem* item : pythonScene.items()) {
		EditableTextItem* text = dynamic_cast<EditableTextItem*>(item);
		if (!text || text->getCodeRole() != codeRoleBehaviour) continue;
		QCOMPARE(text->getCodeLanguage(), pythonScene.codeLanguage());
		QVERIFY(colorAt(text->document(), pos) != kindColor(codeTokenComment));
	}

	// a document without the parameter is plain
	CyberiadaSMModel plain(this);
	CyberiadaSMEditorScene plainScene(&plain);
	QVERIFY(plain.loadDocument("diagrams/internal-transition.graphml"));
	plainScene.loadScene();
	QVERIFY(plainScene.codeLanguageName().isEmpty());
	QVERIFY(!plainScene.codeLanguage());
	for (QGraphicsItem* item : plainScene.items()) {
		EditableTextItem* text = dynamic_cast<EditableTextItem*>(item);
		if (text) QVERIFY(!hasFormats(text->document()));
	}
}

void TestHighlight::test_dialog()
{
	// no language: plain
	StateActionDialog none("entry");
	QPlainTextEdit* plainEdit = none.findChild<QPlainTextEdit*>();
	plainEdit->setPlainText("entry/ return 1;");
	QVERIFY(!hasFormats(plainEdit->document()));

	const CodeLanguage* cpp = CodeStyle::instance().find("C++");
	StateActionDialog entry("entry", cpp);
	QPlainTextEdit* edit = entry.findChild<QPlainTextEdit*>();
	QVERIFY(edit);
	QVERIFY(edit->document()->findChild<CodeHighlighter*>());
	edit->setPlainText("entry/ return 1;");
	QVERIFY(!colorAt(edit->document(), 0).isValid());
	QCOMPARE(colorAt(edit->document(), 7), kindColor(codeTokenKeyword));
	QVERIFY(entry.parseInput());
	QCOMPARE(entry.getBehaviour(), QString("return 1;"));

	StateActionDialog transition(StateActionDialog::Mode::Transition, cpp);
	edit = transition.findChild<QPlainTextEdit*>();
	CodeHighlighter* h = edit->document()->findChild<CodeHighlighter*>();
	QVERIFY(h);
	QCOMPARE(h->getRole(), codeRoleTransition);
	edit->setPlainText("return [true] / f()");
	QVERIFY(!colorAt(edit->document(), 0).isValid());
	QCOMPARE(colorAt(edit->document(), 8), kindColor(codeTokenKeyword));
}

void TestHighlight::test_export()
{
	SettingsManager& sm = SettingsManager::instance();
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	QString keyword = kindColor(codeTokenKeyword).name();
	QTextDocument* doc = codeItems(codeRoleFormalComment)[0]->document();

	// plain by default; the canvas gets its colours back
	QString path = dir.filePath("plain.svg");
	QVERIFY(renderScene(scene, path, nullptr));
	QFile plain(path);
	QVERIFY(plain.open(QIODevice::ReadOnly));
	QVERIFY(!QString(plain.readAll()).contains(keyword, Qt::CaseInsensitive));
	QVERIFY(hasFormats(doc));

	sm.setHighlightInExports(true);
	path = dir.filePath("colour.svg");
	QVERIFY(renderScene(scene, path, nullptr));
	QFile colour(path);
	QVERIFY(colour.open(QIODevice::ReadOnly));
	QVERIFY(QString(colour.readAll()).contains(keyword, Qt::CaseInsensitive));
	sm.setHighlightInExports(false);
}

// the kind of the span covering the position, -1 when none
static int kindAt(const QVector<CodeSpan>& spans, int pos)
{
	for (const CodeSpan& span : spans) {
		if (pos >= span.start && pos < span.start + span.length) return span.kind;
	}
	return -1;
}

void TestHighlight::test_markup_lookup()
{
	bool known = false;
	QCOMPARE(CodeStyle::findMarkup("Markdown", &known), codeMarkupMarkdown);
	QVERIFY(known);
	QCOMPARE(CodeStyle::findMarkup("md", &known), codeMarkupMarkdown);
	QCOMPARE(CodeStyle::findMarkup("", &known), codeMarkupPlain);
	QVERIFY(known);
	QCOMPARE(CodeStyle::findMarkup(" PLAIN ", &known), codeMarkupPlain);
	QVERIFY(known);
	QCOMPARE(CodeStyle::findMarkup("html", &known), codeMarkupPlain);
	QVERIFY(!known);
	QCOMPARE(CodeStyle::markupValue(codeMarkupMarkdown), QString("markdown"));
	QVERIFY(CodeStyle::markupValue(codeMarkupPlain).isEmpty());
	// the comment's own markup wins over the document one
	QCOMPARE(CodeStyle::commentMarkup("plain", codeMarkupMarkdown), codeMarkupPlain);
	QCOMPARE(CodeStyle::commentMarkup("", codeMarkupMarkdown), codeMarkupMarkdown);
	QCOMPARE(CodeStyle::commentMarkup("markdown", codeMarkupPlain), codeMarkupMarkdown);
}

void TestHighlight::test_markdown_spans()
{
	const CodeStyle& style = CodeStyle::instance();
	QString text = "# Head\n> quote\n- item **bold** *em* `x` [a](http://b)\n---\n1. one ~~gone~~ <http://c>";
	QVector<CodeSpan> spans = style.markdownSpans(text, nullptr);
	QCOMPARE(kindAt(spans, text.indexOf("Head")), int(codeTokenHeading));
	QCOMPARE(kindAt(spans, text.indexOf(">")), int(codeTokenMarker));
	QCOMPARE(kindAt(spans, text.indexOf("quote")), -1);
	QCOMPARE(kindAt(spans, text.indexOf("- item")), int(codeTokenMarker));
	QCOMPARE(kindAt(spans, text.indexOf("item")), -1);
	QCOMPARE(kindAt(spans, text.indexOf("bold")), int(codeTokenEmphasis));
	QCOMPARE(kindAt(spans, text.indexOf("em*")), int(codeTokenEmphasis));
	QCOMPARE(kindAt(spans, text.indexOf("`x`")), int(codeTokenCode));
	QCOMPARE(kindAt(spans, text.indexOf("[a]")), int(codeTokenLink));
	QCOMPARE(kindAt(spans, text.indexOf("---")), int(codeTokenMarker));
	QCOMPARE(kindAt(spans, text.indexOf("1.")), int(codeTokenMarker));
	QCOMPARE(kindAt(spans, text.indexOf("gone")), int(codeTokenEmphasis));
	QCOMPARE(kindAt(spans, text.indexOf("<http")), int(codeTokenLink));

	// an inline code span hides the emphasis inside it
	text = "`*x*` and 2*3*4";
	spans = style.markdownSpans(text, nullptr);
	QCOMPARE(spans.size(), 1);
	QCOMPARE(spans[0].kind, codeTokenCode);

	// a fence by its tag, else by the fallback, else one code colour
	const CodeLanguage* python = style.find("Python");
	text = "```cpp\nreturn 0;\n```\n```\nreturn None\n```\n~~~lua\nlocal x\n~~~";
	spans = style.markdownSpans(text, python);
	QCOMPARE(kindAt(spans, 0), int(codeTokenMarker));
	QCOMPARE(kindAt(spans, text.indexOf("return 0")), int(codeTokenKeyword));
	QCOMPARE(kindAt(spans, text.indexOf("0;")), int(codeTokenNumber));
	QCOMPARE(kindAt(spans, text.indexOf("None")), int(codeTokenKeyword));
	// an unknown tag falls back to the document language: no Python keyword
	QCOMPARE(kindAt(spans, text.indexOf("local")), -1);
	spans = style.markdownSpans(text, nullptr);
	QCOMPARE(kindAt(spans, text.indexOf("None")), int(codeTokenCode));
	QCOMPARE(kindAt(spans, text.indexOf("local")), int(codeTokenCode));
	// inside a fence there is no Markdown
	text = "```\n# not a heading\n";
	spans = style.markdownSpans(text, nullptr);
	QCOMPARE(kindAt(spans, text.indexOf("# not")), int(codeTokenCode));
}

void TestHighlight::test_markdown_fence_lines()
{
	QTextDocument doc;
	doc.documentLayout();
	CodeHighlighter highlighter(&doc, codeRoleMarkup, nullptr, codeMarkupMarkdown);
	doc.setPlainText("```\nx\ny");
	int y = doc.toPlainText().indexOf("y");
	QCOMPARE(colorAt(&doc, y), kindColor(codeTokenCode));

	// closing the fence on the middle line re-highlights the last one
	QTextCursor cursor(&doc);
	cursor.setPosition(y);
	cursor.insertText("```\n");
	y = doc.toPlainText().indexOf("y");
	QVERIFY(colorAt(&doc, y) != kindColor(codeTokenCode));

	// a plain markup draws nothing
	highlighter.setMarkup(codeMarkupPlain);
	QVERIFY(!hasFormats(&doc));
}

void TestHighlight::test_markdown_canvas()
{
	CyberiadaSMModel notes(this);
	CyberiadaSMEditorScene notesScene(&notes);
	QVERIFY(notes.loadDocument("diagrams/markdown-comments.graphml"));
	notesScene.loadScene();
	QCOMPARE(notesScene.markupLanguage(), codeMarkupMarkdown);

	EditableTextItem* readme = nullptr;
	EditableTextItem* plain = nullptr;
	EditableTextItem* formal = nullptr;
	for (QGraphicsItem* item : notesScene.items()) {
		EditableTextItem* text = dynamic_cast<EditableTextItem*>(item);
		if (!text) continue;
		if (text->getCodeRole() == codeRoleMarkup) {
			if (text->toPlainText().startsWith("# Title")) readme = text;
			else plain = text;
		} else if (text->getCodeRole() == codeRoleFormalComment) {
			formal = text;
		}
	}
	QVERIFY(readme && plain && formal);

	QTextDocument* doc = readme->document();
	QString text = doc->toPlainText();
	QCOMPARE(readme->getCodeMarkup(), codeMarkupMarkdown);
	QCOMPARE(colorAt(doc, 0), kindColor(codeTokenHeading));
	QCOMPARE(colorAt(doc, text.indexOf("this")), kindColor(codeTokenEmphasis));
	QCOMPARE(colorAt(doc, text.indexOf("`code`")), kindColor(codeTokenCode));
	QCOMPARE(colorAt(doc, text.indexOf("return")), kindColor(codeTokenKeyword));
	// the comment's own dMarkup "plain" wins
	QCOMPARE(plain->getCodeMarkup(), codeMarkupPlain);
	QVERIFY(!hasFormats(plain->document()));
	// a formal comment stays code (Python: "#" starts a comment)
	QCOMPARE(colorAt(formal->document(), 0), kindColor(codeTokenComment));

	// the document markup removed: plain; undo: Markdown again
	QVERIFY(notes.removeMetainformation(notes.documentIndex(), METAINFORMATION_KEY_MARKUP_LANGUAGE));
	QCOMPARE(notesScene.markupLanguage(), codeMarkupPlain);
	QVERIFY(!hasFormats(readme->document()));
	notes.undoStack()->undo();
	QCOMPARE(notesScene.markupLanguage(), codeMarkupMarkdown);
	for (QGraphicsItem* item : notesScene.items()) {
		EditableTextItem* t = dynamic_cast<EditableTextItem*>(item);
		if (t && t->getCodeRole() == codeRoleMarkup && t->toPlainText().startsWith("# Title")) readme = t;
	}
	QCOMPARE(colorAt(readme->document(), 0), kindColor(codeTokenHeading));
	QCOMPARE(readme->document()->toPlainText(), text);

	// the preference turns it off as well
	SettingsManager::instance().setHighlightCode(false);
	QVERIFY(!hasFormats(readme->document()));
	SettingsManager::instance().setHighlightCode(true);
	QVERIFY(hasFormats(readme->document()));
}

void TestHighlight::test_markdown_dialog()
{
	MultilineTextDialog markdown("Edit text", "Body", "# Title", codeRoleMarkup, nullptr,
	                             nullptr, codeMarkupMarkdown);
	QPlainTextEdit* edit = markdown.findChild<QPlainTextEdit*>();
	QVERIFY(edit);
	QCOMPARE(colorAt(edit->document(), 0), kindColor(codeTokenHeading));

	MultilineTextDialog plainDialog("Edit text", "Body", "# Title", codeRoleMarkup, nullptr,
	                                nullptr, codeMarkupPlain);
	QVERIFY(!hasFormats(plainDialog.findChild<QPlainTextEdit*>()->document()));
}

QTEST_MAIN(TestHighlight)
#include "l4-highlight.moc"
