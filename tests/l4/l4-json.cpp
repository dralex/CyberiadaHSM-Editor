/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The L4 test of the JSON texts: the check of a JSON tab
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
#include <QLineEdit>
#include <QTextBlock>
#include <QUndoStack>
#include "smeditor_window.h"
#include "code_editor.h"
#include "text_document.h"
#include "find_bar.h"
#include "json_text.h"
#include "cyberiada_constants.h"

// the JSON texts (docs/MODES.md, JSON): the check (EDIT-MODE-19)
class TestJson: public QObject {
	Q_OBJECT

private slots:
	void test_check_data();
	void test_check();
	void test_deep();
	void test_tab_marks();
	void test_tab_edit();
	void test_tab_find_and_click();
	void test_tab_mode();

private:
	// a JSON tab of the text, opened from texts/settings.json
	static TextDocument* openJson(CyberiadaSMEditorWindow& window, const QString& text);
	static bool isUnderline(const QTextEdit::ExtraSelection& mark);
};

TextDocument* TestJson::openJson(CyberiadaSMEditorWindow& window, const QString& text)
{
	if (!window.openFile("texts/settings.json")) return nullptr;
	TextDocument* doc = window.currentText();
	if (doc && !text.isNull()) doc->replaceText(text, "replace");
	return doc;
}

bool TestJson::isUnderline(const QTextEdit::ExtraSelection& mark)
{
	return mark.format.underlineStyle() == QTextCharFormat::WaveUnderline &&
		mark.format.underlineColor() == QColor(TEXT_ERROR_COLOR);
}

// the rules and the messages of docs/MODES.md, JSON
void TestJson::test_check_data()
{
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("line");     // 0: valid
	QTest::addColumn<int>("column");
	QTest::addColumn<int>("length");
	QTest::addColumn<QString>("message");

	QTest::newRow("object") << "{\"a\": 1, \"b\": [true, false, null], \"c\": {\"d\": \"x\\\"y\\u00e9\\/\"}}"
		<< 0 << 0 << 0 << QString();
	QTest::newRow("array") << "[1, -2.5, 3e10, 0.1E-2, 0, \"\", {}, []]" << 0 << 0 << 0 << QString();
	QTest::newRow("scalar") << "42" << 0 << 0 << 0 << QString();
	QTest::newRow("string") << "\"x\"" << 0 << 0 << 0 << QString();
	QTest::newRow("literal") << " null \r\n" << 0 << 0 << 0 << QString();
	QTest::newRow("empty") << "" << 0 << 0 << 0 << QString();
	QTest::newRow("blank") << " \n\t " << 0 << 0 << 0 << QString();
	QTest::newRow("byte order mark") << QString(QChar(0xFEFF)) + "{}" << 0 << 0 << 0 << QString();
	QTest::newRow("missing comma") << "{\"a\": 1 \"b\": 2}" << 1 << 9 << 3 << "missing comma";
	QTest::newRow("missing comma, line 3") << "{\n  \"a\": 1\n  \"b\": 2\n}" << 3 << 3 << 3 << "missing comma";
	QTest::newRow("missing comma, array") << "[1 [2]]" << 1 << 4 << 1 << "missing comma";
	QTest::newRow("trailing comma, array") << "[1, 2,]" << 1 << 6 << 1 << "trailing comma";
	QTest::newRow("trailing comma, object") << "{\"a\": 1,}" << 1 << 8 << 1 << "trailing comma";
	QTest::newRow("missing colon") << "{\"a\" 1}" << 1 << 6 << 1 << "missing colon";
	QTest::newRow("missing value") << "{\"a\": }" << 1 << 7 << 1 << "missing value";
	QTest::newRow("double comma") << "[1,,2]" << 1 << 4 << 1 << "missing value";
	QTest::newRow("leading comma") << "[,1]" << 1 << 2 << 1 << "missing value";
	QTest::newRow("bare key") << "{abc: 1}" << 1 << 2 << 3 << "a key must be a string in double quotes";
	QTest::newRow("quoted key") << "{'a': 1}" << 1 << 2 << 3 << "a key must be a string in double quotes";
	QTest::newRow("quoted value") << "['a']" << 1 << 2 << 3 << "a string must be in double quotes";
	QTest::newRow("unterminated string") << "[\"abc\n]" << 1 << 2 << 4 << "unterminated string";
	QTest::newRow("unterminated at the end") << "[\"ab\\\"" << 1 << 2 << 5 << "unterminated string";
	QTest::newRow("invalid escape") << "[\"a\\qb\"]" << 1 << 4 << 2 << "invalid escape sequence";
	QTest::newRow("short unicode escape") << "[\"\\u12\"]" << 1 << 3 << 4 << "invalid escape sequence";
	QTest::newRow("control character") << "[\"a\tb\"]" << 1 << 4 << 1 << "control character in a string";
	QTest::newRow("leading zero") << "[01]" << 1 << 2 << 2 << "invalid number";
	QTest::newRow("plus") << "[+1]" << 1 << 2 << 2 << "invalid number";
	QTest::newRow("dot first") << "[.5]" << 1 << 2 << 2 << "invalid number";
	QTest::newRow("dot last") << "[1.]" << 1 << 2 << 2 << "invalid number";
	QTest::newRow("minus") << "[-]" << 1 << 2 << 1 << "invalid number";
	QTest::newRow("exponent") << "[1e]" << 1 << 2 << 2 << "invalid number";
	QTest::newRow("word") << "[tru]" << 1 << 2 << 3 << "'tru' is not a JSON value";
	QTest::newRow("NaN") << "{\"a\": NaN}" << 1 << 7 << 3 << "'NaN' is not a JSON value";
	QTest::newRow("True") << "True" << 1 << 1 << 4 << "'True' is not a JSON value";
	QTest::newRow("mismatch") << "[1}" << 1 << 3 << 1 << "expected ']'";
	QTest::newRow("mismatch, object") << "{\"a\": 1]" << 1 << 8 << 1 << "expected '}'";
	QTest::newRow("colon in array") << "[1:2]" << 1 << 3 << 1 << "unexpected ':'";
	QTest::newRow("line comment") << "{} // x" << 1 << 4 << 2 << "comments are not allowed in JSON";
	QTest::newRow("block comment") << "/* x */ {}" << 1 << 1 << 2 << "comments are not allowed in JSON";
	QTest::newRow("object not closed") << "{\"a\": 1" << 1 << 1 << 1 << "the object is not closed";
	QTest::newRow("array not closed") << "[1, [2]" << 1 << 1 << 1 << "the array is not closed";
	QTest::newRow("two values") << "{} {}" << 1 << 4 << 1 << "extra text after the value";
	QTest::newRow("scalar list") << "1, 2" << 1 << 2 << 1 << "extra text after the value";
	QTest::newRow("non-ASCII before") << QString::fromUtf8("{\"ключ\": 1 \"b\": 2}") << 1 << 12 << 3
		<< "missing comma";
}

void TestJson::test_check()
{
	QFETCH(QString, text);
	QFETCH(int, line);
	QFETCH(int, column);
	QFETCH(int, length);
	QFETCH(QString, message);
	JsonCheck check = checkJson(text);
	QCOMPARE(check.ok, line == 0);
	if (line == 0) return;
	QCOMPARE(check.line, line);
	QCOMPARE(check.column, column);
	QCOMPARE(check.length, length);
	QCOMPARE(check.message, message);
	// the place is the text's: the line and the column count from it
	QCOMPARE(text.left(check.position).count('\n') + 1, line);
}

// the brackets are kept on a stack, not in the call stack
void TestJson::test_deep()
{
	const int depth = 100000;
	QVERIFY(checkJson(QString(depth, '[') + QString(depth, ']')).ok);
	JsonCheck check = checkJson(QString(depth, '['));
	QVERIFY(!check.ok);
	QCOMPARE(check.column, depth);
	QCOMPARE(check.message, QString("the array is not closed"));
}

// EDIT-MODE-19: the underline, the margin number and the bar; a valid text has none
void TestJson::test_tab_marks()
{
	CyberiadaSMEditorWindow window;
	TextDocument* text = openJson(window, QString());
	QVERIFY(text && text->isJson());
	CodeEditor* editor = text->editor();
	QVERIFY(text->jsonCheck().ok);
	QCOMPARE(editor->errorLine(), 0);
	QVERIFY(editor->errorMessage().isEmpty() && editor->errorBarHeight() == 0);
	QVERIFY(editor->extraSelections().isEmpty());

	// a whole-text replace is checked at once
	QVERIFY(text->replaceText("{\n  \"a\": 1\n  \"b\": 2\n}", "replace"));
	QVERIFY(!text->jsonCheck().ok);
	QCOMPARE(editor->errorLine(), 3);
	QCOMPARE(editor->errorMessage(), QString("Line 3, column 3: missing comma"));
	QVERIFY(editor->errorBarHeight() > 0);
	QList<QTextEdit::ExtraSelection> marks = editor->extraSelections();
	QCOMPARE(marks.size(), 1);
	QVERIFY(isUnderline(marks[0]));
	QCOMPARE(marks[0].cursor.selectedText(), QString("\"b\""));
	// the check changes neither the text nor its history
	QCOMPARE(text->text(), QString("{\n  \"a\": 1\n  \"b\": 2\n}"));
	QCOMPARE(text->undoStack()->count(), 1);

	// an empty text is no error
	QVERIFY(text->replaceText("", "replace"));
	QVERIFY(text->jsonCheck().ok && editor->errorLine() == 0 && editor->extraSelections().isEmpty());
}

// EDIT-MODE-19: an edit is checked JSON_CHECK_DELAY_MS later; the underline follows it
void TestJson::test_tab_edit()
{
	CyberiadaSMEditorWindow window;
	TextDocument* text = openJson(window, "[1 2]");
	QVERIFY(text);
	CodeEditor* editor = text->editor();
	QCOMPARE(editor->errorMessage(), QString("Line 1, column 4: missing comma"));
	// an edit before the error moves the underline with the text until the check
	QTextCursor cursor(text->editor()->document());
	cursor.insertText(" ");
	QCOMPARE(editor->extraSelections().value(0).cursor.selectedText(), QString("2"));
	cursor.setPosition(3);
	cursor.insertText(",");
	QVERIFY(!text->jsonCheck().ok);
	QTRY_VERIFY(text->jsonCheck().ok);
	QCOMPARE(editor->errorLine(), 0);
	QVERIFY(editor->errorMessage().isEmpty() && editor->extraSelections().isEmpty());
	QCOMPARE(text->text(), QString(" [1, 2]"));
	// broken again by typing
	cursor.movePosition(QTextCursor::End);
	cursor.insertText(" // x");
	QTRY_VERIFY(!text->jsonCheck().ok);
	QCOMPARE(editor->errorMessage(), QString("Line 1, column 9: comments are not allowed in JSON"));
}

// EDIT-MODE-19: the find marks beside the underline; a click on the bar goes to the error
void TestJson::test_tab_find_and_click()
{
	CyberiadaSMEditorWindow window;
	TextDocument* text = openJson(window, "{\n  \"a\": \"aa\"\n  \"b\": 2\n}");
	QVERIFY(text);
	CodeEditor* editor = text->editor();
	window.actionFind->trigger();
	window.findBar()->field()->setText("a");
	QList<QTextEdit::ExtraSelection> marks = editor->extraSelections();
	QCOMPARE(marks.size(), 4);
	QVERIFY(isUnderline(marks[0]));
	QCOMPARE(marks[1].format.background().color().name(), QString(FIND_CURRENT_COLOR));
	QCOMPARE(marks[3].format.background().color().name(), QString(FIND_MATCH_COLOR));
	window.findBar()->field()->setText(QString());
	QCOMPARE(editor->extraSelections().size(), 1);

	// the bar under the viewport takes the click; a hidden editor is laid out by the events
	// a shown one gets: its own resize, then its viewport's, which reaches resizeEvent()
	editor->resize(400, 300);
	QResizeEvent resize(editor->size(), QSize());
	QCoreApplication::sendEvent(editor, &resize);
	QResizeEvent viewResize(editor->viewport()->size(), QSize());
	QCoreApplication::sendEvent(editor->viewport(), &viewResize);
	QRect view = editor->viewport()->geometry();
	QWidget* bar = editor->childAt(QPoint(view.center().x(), view.bottom() + 1 + editor->errorBarHeight() / 2));
	QVERIFY(bar && bar != editor->viewport());
	QCOMPARE(bar->toolTip(), QString("Line 3, column 3: missing comma"));
	editor->moveCursor(QTextCursor::Start);
	QTest::mouseClick(bar, Qt::LeftButton);
	QCOMPARE(editor->textCursor().position(), text->jsonCheck().position);
	QCOMPARE(editor->textCursor().blockNumber() + 1, 3);
}

// EDIT-MODE-19: another mode drops the marks and the bar, JSON brings them back
void TestJson::test_tab_mode()
{
	CyberiadaSMEditorWindow window;
	TextDocument* text = openJson(window, "[1,]");
	QVERIFY(text);
	CodeEditor* editor = text->editor();
	QCOMPARE(editor->errorLine(), 1);
	QVERIFY(window.setDocumentMode(text, MODE_TEXT));
	QVERIFY(!text->isJson() && text->jsonCheck().ok);
	QVERIFY(editor->errorLine() == 0 && editor->errorMessage().isEmpty());
	QVERIFY(editor->extraSelections().isEmpty());
	// no check of a plain text after an edit
	QTextCursor(editor->document()).insertText("x");
	QTest::qWait(2 * JSON_CHECK_DELAY_MS);
	QVERIFY(text->jsonCheck().ok);
	QVERIFY(window.setDocumentMode(text, LANGUAGE_JSON));
	QCOMPARE(editor->errorMessage(), QString("Line 1, column 1: 'x' is not a JSON value"));
}

QTEST_MAIN(TestJson)
#include "l4-json.moc"
