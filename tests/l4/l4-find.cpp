/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process find test (see docs/TESTING.md, L4)
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
#include <QStackedWidget>
#include <QTextBrowser>
#include <QToolButton>
#include <QUndoStack>
#include "smeditor_window.h"
#include "code_editor.h"
#include "text_document.h"
#include "diff_document.h"
#include "find_bar.h"
#include "find_query.h"
#include "cyberiada_constants.h"

// the EDIT-FIND laws (docs/FIND.md): the matcher, the bar, the marks of the text tabs
class TestFind: public QObject {
	Q_OBJECT

private slots:
	void test_matcher();
	void test_text_marks();
	void test_keys();
	void test_seed();
	void test_no_match();
	void test_text_follows_edit();
	void test_rendered();
	void test_switch_and_diff();

private:
	static int count(const QString& text, const FindQuery& query);
	static FindQuery plain(const QString& text);
	static void search(CyberiadaSMEditorWindow& window, const QString& text);
	static QString markColor(const QTextEdit::ExtraSelection& mark);
};

int TestFind::count(const QString& text, const FindQuery& query)
{
	return findRanges(text, query.expression(), FIND_MATCH_LIMIT).size();
}

FindQuery TestFind::plain(const QString& text)
{
	FindQuery query;
	query.text = text;
	return query;
}

void TestFind::search(CyberiadaSMEditorWindow& window, const QString& text)
{
	window.actionFind->trigger();
	window.findBar()->field()->setText(text);
}

QString TestFind::markColor(const QTextEdit::ExtraSelection& mark)
{
	return mark.format.background().color().name();
}

// EDIT-FIND-2: the text as written, the case, the whole words, the expression
void TestFind::test_matcher()
{
	const QString code = "// the LED\npinMode(LED_BUILTIN, OUTPUT);\ndelay(500);\nDelay(50);";
	QCOMPARE(count(code, plain("delay")), 2);
	FindQuery q = plain("Delay");
	q.matchCase = true;
	QCOMPARE(count(code, q), 1);
	QCOMPARE(count(code, plain("LED")), 2);
	q = plain("LED");
	q.wholeWords = true;
	QCOMPARE(count(code, q), 1);
	// the special characters of a plain text are literal
	QCOMPARE(count("a.b axb", plain("a.b")), 1);
	QCOMPARE(count("f() f(x)", plain("f()")), 1);
	// a whole word starting with a non-word character
	q = plain("/reset");
	q.wholeWords = true;
	QCOMPARE(count("entry/reset() exit/resets()", q), 1);
	// the Cyrillic letters have a case and are word characters
	const QString ru = QString::fromUtf8("Свет горит, светофор");
	QCOMPARE(count(ru, plain(QString::fromUtf8("свет"))), 2);
	q = plain(QString::fromUtf8("свет"));
	q.wholeWords = true;
	QCOMPARE(count(ru, q), 1);
	q.matchCase = true;
	QCOMPARE(count(ru, q), 0);
	// an expression; an invalid one and an empty match take nothing
	q = plain("delay\\(\\d+\\)");
	q.regexp = true;
	QCOMPARE(count(code, q), 2);
	q.matchCase = true;
	QCOMPARE(count(code, q), 1);
	q.matchCase = false;
	q.text = "(";
	QVERIFY(!q.expression().isValid());
	QCOMPARE(count(code, q), 0);
	q.text = "x*";
	QCOMPARE(count("abc", q), 0);
	FindRanges r = findRanges("a xx b", plain("x*").expression(), FIND_MATCH_LIMIT);
	QCOMPARE(r.size(), 0);
	q.text = "x+";
	r = findRanges("a xx b", q.expression(), FIND_MATCH_LIMIT);
	QCOMPARE(r.size(), 1);
	QCOMPARE(r[0].from, 2);
	QCOMPARE(r[0].to, 4);
	// the limit, and what was left beyond it
	bool more = false;
	QCOMPARE(findRanges("aaaa", plain("a").expression(), 3, &more).size(), 3);
	QVERIFY(more);
	QCOMPARE(findRanges("aaa", plain("a").expression(), 3, &more).size(), 3);
	QVERIFY(!more);
}

// EDIT-FIND-1/3/4/5: the bar, the marks, the steps with the wrap, the document unchanged
void TestFind::test_text_marks()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("texts/blink.cpp"));
	TextDocument* text = window.currentText();
	QVERIFY(text);
	const QString source = text->text();
	int steps = text->undoStack()->count();
	FindBar* bar = window.findBar();
	QVERIFY(bar->isHidden());
	QVERIFY(window.actionFind->isEnabled());
	QVERIFY(window.menuEdit->actions().contains(window.actionFind));
	QVERIFY(window.actionFind->shortcuts().contains(QKeySequence(Qt::CTRL + Qt::Key_F)));
	QVERIFY(window.actionFindNext->shortcuts().contains(QKeySequence(Qt::Key_F3)));
	QVERIFY(window.actionFindPrevious->shortcuts().contains(QKeySequence(Qt::SHIFT + Qt::Key_F3)));

	search(window, "delay");
	QVERIFY(!bar->isHidden());
	QList<QTextEdit::ExtraSelection> marks = text->editor()->extraSelections();
	QCOMPARE(marks.size(), 2);
	QCOMPARE(markColor(marks[0]), QString(FIND_CURRENT_COLOR));
	QCOMPARE(markColor(marks[1]), QString(FIND_MATCH_COLOR));
	QCOMPARE(marks[0].format.foreground().color(), QColor(Qt::black));
	QCOMPARE(marks[0].cursor.selectedText(), QString("delay"));
	QCOMPARE(bar->counterText(), QString("1 of 2"));
	// the cursor goes to the match without a selection, drawn over the marks
	QTextCursor cursor = text->editor()->textCursor();
	QVERIFY(!cursor.hasSelection());
	QCOMPARE(cursor.position(), text->findMatches()[0].from);

	window.actionFindNext->trigger();
	QCOMPARE(bar->counterText(), QString("2 of 2"));
	marks = text->editor()->extraSelections();
	QCOMPARE(markColor(marks[0]), QString(FIND_MATCH_COLOR));
	QCOMPARE(markColor(marks[1]), QString(FIND_CURRENT_COLOR));
	window.actionFindNext->trigger();
	QCOMPARE(bar->counterText(), QString("1 of 2"));
	window.actionFindPrevious->trigger();
	QCOMPARE(bar->counterText(), QString("2 of 2"));

	// the case option searches again
	bar->caseButton()->setChecked(true);
	bar->field()->setText("Delay");
	QCOMPARE(text->editor()->extraSelections().size(), 0);
	bar->caseButton()->setChecked(false);
	QCOMPARE(text->editor()->extraSelections().size(), 2);

	// the marks are not a part of the text
	QCOMPARE(text->text(), source);
	QCOMPARE(text->undoStack()->count(), steps);
	QVERIFY(text->isClean());

	// the bar closed: the marks gone, the current match selected
	int from = text->findMatches()[text->findCurrent()].from;
	bar->closeButton()->click();
	QVERIFY(bar->isHidden());
	QCOMPARE(text->editor()->extraSelections().size(), 0);
	cursor = text->editor()->textCursor();
	QCOMPARE(cursor.selectedText(), QString("Delay").toLower());
	QCOMPARE(cursor.selectionStart(), from);
}

// EDIT-FIND-1/3: Enter, Shift+Enter and Esc in the field; a step shows a hidden bar
void TestFind::test_keys()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("texts/blink.cpp"));
	FindBar* bar = window.findBar();
	search(window, "LED_BUILTIN");
	QCOMPARE(bar->counterText(), QString("1 of 3"));
	QTest::keyClick(bar->field(), Qt::Key_Return);
	QCOMPARE(bar->counterText(), QString("2 of 3"));
	QTest::keyClick(bar->field(), Qt::Key_Return, Qt::ShiftModifier);
	QCOMPARE(bar->counterText(), QString("1 of 3"));
	bar->nextButton()->click();
	QCOMPARE(bar->counterText(), QString("2 of 3"));
	bar->previousButton()->click();
	QCOMPARE(bar->counterText(), QString("1 of 3"));
	QTest::keyClick(bar->field(), Qt::Key_Escape);
	QVERIFY(bar->isHidden());
	QCOMPARE(window.currentText()->editor()->extraSelections().size(), 0);
	// Find Next on a hidden bar shows it and searches from the cursor
	window.actionFindNext->trigger();
	QVERIFY(!bar->isHidden());
	QCOMPARE(window.currentText()->editor()->extraSelections().size(), 3);
}

// EDIT-FIND-1: the one selected line fills the field, a longer selection does not
void TestFind::test_seed()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("texts/blink.cpp"));
	TextDocument* text = window.currentText();
	QTextCursor cursor = text->editor()->document()->find("pinMode");
	text->editor()->setTextCursor(cursor);
	window.actionFind->trigger();
	QCOMPARE(window.findBar()->field()->text(), QString("pinMode"));
	QCOMPARE(window.findBar()->field()->selectedText(), QString("pinMode"));
	QCOMPARE(window.findBar()->counterText(), QString("1 of 1"));
	window.findBar()->closeButton()->click();
	cursor = text->editor()->document()->find("void setup");
	cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);
	text->editor()->setTextCursor(cursor);
	window.actionFind->trigger();
	QCOMPARE(window.findBar()->field()->text(), QString("pinMode"));
}

// EDIT-FIND-2: no match is told and tinted
void TestFind::test_no_match()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("texts/blink.cpp"));
	FindBar* bar = window.findBar();
	search(window, "zzz");
	QCOMPARE(bar->counterText(), QString("No matches"));
	QCOMPARE(bar->field()->palette().color(QPalette::Base), QColor(FIND_NO_MATCH_COLOR));
	bar->field()->setText("loop");
	QCOMPARE(bar->counterText(), QString("1 of 1"));
	QVERIFY(bar->field()->palette().color(QPalette::Base) != QColor(FIND_NO_MATCH_COLOR));
	bar->field()->clear();
	QCOMPARE(bar->counterText(), QString());
	QCOMPARE(window.currentText()->editor()->extraSelections().size(), 0);
}

// EDIT-FIND-8: an edit and an undo search again, the cursor stays
void TestFind::test_text_follows_edit()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("texts/blink.cpp"));
	TextDocument* text = window.currentText();
	search(window, "delay");
	window.actionFindNext->trigger();
	int from = text->findMatches()[1].from;
	QTextCursor cursor(text->editor()->document());
	text->editor()->setTextCursor(cursor);
	const QString added = "delay(1);\n";
	text->editor()->insertPlainText(added);
	int position = text->editor()->textCursor().position();
	QCoreApplication::processEvents();
	QCOMPARE(text->findMatches().size(), 3);
	QCOMPARE(text->editor()->extraSelections().size(), 3);
	// the same match is current, the cursor where the edit left it
	QCOMPARE(text->findMatches()[text->findCurrent()].from, from + added.size());
	QCOMPARE(window.findBar()->counterText(), QString("3 of 3"));
	QCOMPARE(text->editor()->textCursor().position(), position);
	window.actionUndo->trigger();
	QCoreApplication::processEvents();
	QCOMPARE(text->findMatches().size(), 2);
	QCOMPARE(window.findBar()->counterText(), QString("2 of 2"));
}

// EDIT-FIND-5: a read-only Markdown tab is searched on its rendered page
void TestFind::test_rendered()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("texts/notes.md"));
	TextDocument* text = window.currentText();
	QStackedWidget* page = qobject_cast<QStackedWidget*>(text->page());
	search(window, "states");
	int source = text->editor()->extraSelections().size();
	QVERIFY(source > 0);
	window.setDocumentReadOnly(text, true);
	QCoreApplication::processEvents();
	QTextBrowser* rendered = qobject_cast<QTextBrowser*>(page->currentWidget());
	QVERIFY(rendered && page->currentWidget() != text->editor());
	QCOMPARE(text->editor()->extraSelections().size(), 0);
	int shown = rendered->toPlainText().count("states", Qt::CaseInsensitive);
	QCOMPARE(rendered->extraSelections().size(), shown);
	QCOMPARE(text->findMatches().size(), shown);
	// a marker of the source is not on the page
	window.findBar()->field()->setText("**red**");
	QCOMPARE(rendered->extraSelections().size(), 0);
	window.findBar()->field()->setText("red");
	QVERIFY(rendered->extraSelections().size() > 0);
	window.findBar()->closeButton()->click();
	QCOMPARE(rendered->textCursor().selectedText().toLower(), QString("red"));
	QCOMPARE(rendered->extraSelections().size(), 0);
	window.setDocumentReadOnly(text, false);
	QCOMPARE(window.findBar()->isHidden(), true);
	QCOMPARE(text->editor()->extraSelections().size(), 0);
	Q_UNUSED(source);
}

// EDIT-FIND-9: a switch drops the marks of the tab left; a diff tab cannot search
void TestFind::test_switch_and_diff()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("texts/blink.cpp"));
	TextDocument* blink = window.currentText();
	search(window, "the");
	QCOMPARE(blink->editor()->extraSelections().size(), 2);
	int cursor = blink->editor()->textCursor().position();
	QVERIFY(window.openFile("texts/counter.py"));
	TextDocument* counter = window.currentText();
	QVERIFY(counter != blink);
	QCOMPARE(blink->editor()->extraSelections().size(), 0);
	QCOMPARE(counter->findMatches().size(), 1);
	QCOMPARE(counter->editor()->extraSelections().size(), 1);
	window.setCurrentDocument(blink);
	QCOMPARE(blink->editor()->extraSelections().size(), 2);
	QCOMPARE(window.findBar()->counterText(), QString("1 of 2"));
	// the switch searches without moving the cursor
	QCOMPARE(blink->editor()->textCursor().position(), cursor);

	QVERIFY(window.openFile("diagrams/geometry.graphml"));
	CyberiadaSMEditorDocument* source = window.currentDocument();
	QVERIFY(source);
	DiffOrigin saved;
	saved.kind = DiffOrigin::originSaved;
	saved.tab = source;
	saved.path = source->filePath();
	saved.label = "saved";
	QString error;
	DiffDocument* diff = window.compareDocument(source, saved, &error);
	QVERIFY2(diff, qPrintable(error));
	QCOMPARE(window.currentDiff(), diff);
	QVERIFY(!window.actionFind->isEnabled());
	QVERIFY(!window.actionFindNext->isEnabled());
	QVERIFY(!window.actionFindPrevious->isEnabled());
	QVERIFY(!window.findBar()->isEnabled());
	window.setCurrentDocument(blink);
	QVERIFY(window.actionFind->isEnabled() && window.findBar()->isEnabled());
	QCOMPARE(window.findBar()->counterText(), QString("1 of 2"));
}

QTEST_MAIN(TestFind)
#include "l4-find.moc"
