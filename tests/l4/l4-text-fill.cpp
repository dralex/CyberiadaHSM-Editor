/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process test of the fill of a Markdown or a Plain text source: the units,
 * the breaks, the cursor, fill as you type, the actions and the preferences (see
 * docs/TESTING.md, L4; EDIT-MD-18..23)
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
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QToolBar>
#include <QUndoStack>
#include "code_editor.h"
#include "cyberiada_constants.h"
#include "document_mode.h"
#include "settings_manager.h"
#include "smeditor_window.h"
#include "text_document.h"
#include "text_fill.h"
#include "dialogs/preferences_dialog.h"
#include "dialogs/text_scheme_panel.h"

// the cursor in the texts of the cases
static const QChar CURSOR_MARK(0x00a6);

class TestTextFill: public QObject {
Q_OBJECT
private slots:
    void initTestCase();
    void test_fill_data();
    void test_fill();
    void test_fill_unchanged();
    void test_auto_break_data();
    void test_auto_break();
    void test_columns();
    void test_document_fill();
    void test_fill_as_you_type();
    void test_actions();
    void test_preferences();
    void cleanup();

private:
    QTemporaryDir settings;
};

void TestTextFill::initTestCase()
{
    // the stored width and switch must not reach the profile the other cases share
    QVERIFY(settings.isValid());
    QSettings::setPath(QSettings::defaultFormat(), QSettings::UserScope, settings.path());
}

void TestTextFill::cleanup()
{
    SettingsManager::instance().setTextWidth(TEXT_WIDTH_DEFAULT);
    SettingsManager::instance().setFillAsYouType(false);
}

// the cursor of the editor at a place of its text
static void placeCursor(TextDocument& doc, int place)
{
    QTextCursor cursor = doc.editor()->textCursor();
    cursor.setPosition(place);
    doc.editor()->setTextCursor(cursor);
}

// a text with the cursor mark: the text without it and the place of the mark
static QString unmarked(const QString& marked, int* cursor)
{
    *cursor = marked.indexOf(CURSOR_MARK);
    QString text = marked;
    if (*cursor >= 0) text.remove(*cursor, 1);
    return text;
}

// the edit applied to the text, the cursor marked
static QString applied(const QString& text, const TextFillEdit& edit)
{
    QString result = edit.changed ? text.left(edit.from) + edit.text + text.mid(edit.to) : text;
    return result.insert(edit.cursor, CURSOR_MARK);
}

// EDIT-MD-18..20/23: the units, the prefixes, the breaks, the cursor kept; the cursor of a
// case without the mark selects the whole text
void TestTextFill::test_fill_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("width");
    QTest::addColumn<bool>("markdown");
    QTest::addColumn<bool>("unfill");
    QTest::addColumn<QString>("result");
    QString m(CURSOR_MARK);

    QTest::newRow("paragraph") << "The light has three states red yellow and green and the timer switches them" + m
        << 30 << true << false
        << "The light has three states red\nyellow and green and the timer\nswitches them" + m;
    QTest::newRow("joined") << "The light" + m + "\nhas three\nstates" << 30 << true << false
        << "The light" + m + " has three states";
    QTest::newRow("list item") << "- A list item whose text runs well\npast the width and on" + m << 30 << true << false
        << "- A list item whose text runs\n  well past the width and on" + m;
    QTest::newRow("ordered item") << "10. one two three four" + m << 14 << true << false
        << "10. one two\n    three four" + m;
    QTest::newRow("next item") << "- one two three\n- four five six" + m << 9 << true << false
        << "- one two\n  three\n- four\n  five\n  six" + m;
    QTest::newRow("quote") << "> quoted words that go on and on and on" + m << 30 << true << false
        << "> quoted words that go on and\n> on and on" + m;
    QTest::newRow("lazy quote") << "> quoted words\nthat go on" + m << 30 << true << false
        << "> quoted words that go on" + m;
    QTest::newRow("quoted item") << "> - one two three four five six seven eight" + m << 20 << true << false
        << "> - one two three\n>   four five six\n>   seven eight" + m;
    QTest::newRow("indented paragraph") << "- item\n\n  second paragraph of the item" + m << 20 << true << false
        << "- item\n\n  second paragraph\n  of the item" + m;
    QTest::newRow("hard break spaces") << "Two spaces end a hard break  \nso this line stays its own line." + m
        << 20 << true << false
        << "Two spaces end a\nhard break  \nso this line stays\nits own line." + m;
    QTest::newRow("hard break backslash") << "one two three four five six\\\nseven" + m << 15 << true << false
        << "one two three\nfour five six\\\nseven" + m;
    QTest::newRow("code span and url") << "Run `make all install now` and see https://example.com/a/b/c for more" + m
        << 20 << true << false
        << "Run\n`make all install now`\nand see\nhttps://example.com/a/b/c\nfor more" + m;
    QTest::newRow("link target") << "See [the docs](docs.md \"the title here\") now" + m << 15 << true << false
        << "See [the\ndocs](docs.md \"the title here\")\nnow" + m;
    QTest::newRow("formula") << "Then $a + b = c$ holds" + m << 8 << true << false
        << "Then\n$a + b = c$\nholds" + m;
    QTest::newRow("long word") << "a verylongwordthatexceedsthewidth b" + m << 10 << true << false
        << "a\nverylongwordthatexceedsthewidth\nb" + m;
    QTest::newRow("no break before a marker") << "one two three - four" + m << 13 << true << false
        << "one two\nthree - four" + m;
    QTest::newRow("marker kept on the line") << "one 1. two" + m << 5 << true << false
        << "one 1.\ntwo" + m;
    QTest::newRow("no break before a heading mark") << "aaa bbb # ccc" + m << 7 << true << false
        << "aaa\nbbb #\nccc" + m;
    QTest::newRow("cursor after a word") << "alpha beta gamma" + m + " delta epsilon" << 12 << true << false
        << "alpha beta\ngamma" + m + " delta\nepsilon";
    QTest::newRow("cursor at a word") << "alpha beta gamma " + m + "delta epsilon" << 12 << true << false
        << "alpha beta\ngamma " + m + "delta\nepsilon";
    QTest::newRow("cursor in the prefix") << m + "- one two three" << 9 << true << false
        << m + "- one two\n  three";
    QTest::newRow("unfill") << "- A list item whose text runs\n  well past the width and on" + m << 30 << true << true
        << "- A list item whose text runs well past the width and on" + m;
    QTest::newRow("unfill quote") << "> one\n> two\n> three" + m << 30 << true << true
        << "> one two three" + m;
    QTest::newRow("setext heading") << "Heading text is here\n---" + m << 10 << true << false
        << "Heading text is here\n---" + m;
    QTest::newRow("table after a paragraph") << "intro words here\n| a | b |\n|---|---|\n| c | d |" + m << 10 << true << false
        << "intro\nwords here\n| a | b |\n|---|---|\n| c | d |" + m;
    QTest::newRow("plain indent") << "  First line of an indented paragraph\nwith more words here" + m << 20 << false << false
        << "  First line of an\nindented paragraph\nwith more words here" + m;
    QTest::newRow("plain markers") << "one two - three" + m << 7 << false << false << "one two\n- three" + m;
    QTest::newRow("plain quote") << "> one two" + m << 5 << false << false << "> one\ntwo" + m;
}

void TestTextFill::test_fill()
{
    QFETCH(QString, text);
    QFETCH(int, width);
    QFETCH(bool, markdown);
    QFETCH(bool, unfill);
    QFETCH(QString, result);
    int cursor;
    QString plain = unmarked(text, &cursor);
    TextFillEdit edit = textFill(plain, 0, cursor, width, markdown ? textFillMarkdown : textFillPlain, unfill);
    QCOMPARE(applied(plain, edit), result);
}

// EDIT-MD-18/20: the blocks never filled; a selection to the start of a line ends before it;
// no kind, no edit
void TestTextFill::test_fill_unchanged()
{
    const QString fixed =
        "# A heading that is very long indeed and goes on\n\n"
        "```\ncode code code code code code code code code\n```\n\n"
        "| a | b |\n|---|---|\n| one two three four five six seven | x |\n\n"
        "<div>\nhtml html html html html html html html\n</div>\n\n"
        "    indented code code code code code code code\n\n"
        "[ref]: http://example.com/a/very/long/url/that/goes/on\n\n"
        "$$\nx + y + z + w + v + u + t + s + r + q\n$$\n\n"
        "---\n\n"
        "<!-- a comment\n\nwith words words words words words -->\n\n"
        "Setext heading text that is long and goes on\n====\n\n"
        "> ```\n> quoted code code code code code code\n> ```";
    TextFillEdit edit = textFill(fixed, 0, fixed.length(), 20, textFillMarkdown, false);
    QVERIFY(!edit.changed);
    QCOMPARE(edit.cursor, fixed.length());

    const QString two = "aaa bbb ccc\n\nddd eee fff";
    edit = textFill(two, 0, two.indexOf("ddd"), 7, textFillMarkdown, false);
    QVERIFY(edit.changed);
    QCOMPARE(two.left(edit.from) + edit.text + two.mid(edit.to), QString("aaa bbb\nccc\n\nddd eee fff"));
    // all the units of a selection, the text between them kept
    edit = textFill(two, two.length(), 0, 7, textFillMarkdown, false);
    QCOMPARE(two.left(edit.from) + edit.text + two.mid(edit.to), QString("aaa bbb\nccc\n\nddd eee\nfff"));
    QCOMPARE(edit.anchor, two.length());
    QCOMPARE(edit.cursor, 0);
    // a filled text stays as it is
    QVERIFY(!textFill("aaa bbb\nccc", 0, 0, 7, textFillMarkdown, false).changed);
    QVERIFY(!textFill("aaa bbb ccc", 0, 0, 7, textFillNone, false).changed);
}

// EDIT-MD-21: the break of the line before a typed key; the prefix of its unit
void TestTextFill::test_auto_break_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("width");
    QTest::addColumn<bool>("markdown");
    QTest::addColumn<QString>("result");
    QString m(CURSOR_MARK);

    QTest::newRow("space") << "The light has three states red yellow " + m << 30 << true
        << "The light has three states red\nyellow " + m;
    QTest::newRow("list item") << "- A list item whose text runs well " + m << 30 << true
        << "- A list item whose text runs\n  well " + m;
    QTest::newRow("quote") << "> one two three " + m << 10 << true << "> one two\n> three " + m;
    QTest::newRow("return") << "aaa bbb ccc\n" + m << 7 << true << "aaa bbb\nccc\n" + m;
    QTest::newRow("in the middle") << "aaa bbb ccc " + m + "ddd" << 7 << true << "aaa bbb\nccc " + m + "ddd";
    QTest::newRow("after the width") << "- https://example.com/very/long and " + m << 10 << true
        << "- https://example.com/very/long\n  and " + m;
    QTest::newRow("fits") << "aaa bbb " + m << 7 << true << "aaa bbb " + m;
    QTest::newRow("no break point") << "verylongword " + m << 5 << true << "verylongword " + m;
    QTest::newRow("in code") << "```\naaa bbb ccc " + m + "\n```" << 7 << true << "```\naaa bbb ccc " + m + "\n```";
    QTest::newRow("in a heading") << "# aaa bbb ccc " + m << 7 << true << "# aaa bbb ccc " + m;
    QTest::newRow("in a code span") << "`aaa bbb` ccc " + m << 7 << true << "`aaa bbb`\nccc " + m;
    QTest::newRow("before a marker") << "aaa bbb - ccc " + m << 8 << true << "aaa\nbbb - ccc " + m;
    QTest::newRow("plain") << "  aaa bbb ccc " + m << 9 << false << "  aaa bbb\n  ccc " + m;
}

void TestTextFill::test_auto_break()
{
    QFETCH(QString, text);
    QFETCH(int, width);
    QFETCH(bool, markdown);
    QFETCH(QString, result);
    int cursor;
    QString plain = unmarked(text, &cursor);
    TextFillBreak at;
    if (textAutoFillBreak(plain, cursor, width, markdown ? textFillMarkdown : textFillPlain, &at)) {
        QVERIFY(at.from < at.to && at.to < cursor);
        plain.replace(at.from, at.to - at.from, "\n" + at.prefix);
        cursor += 1 + at.prefix.length() - (at.to - at.from);
    }
    QCOMPARE(plain.insert(cursor, CURSOR_MARK), result);
    QVERIFY(!textAutoFillBreak(plain, 0, width, textFillMarkdown, &at));
}

// EDIT-MD-19: the columns, a tab to the next stop, a mark and a surrogate pair as one
void TestTextFill::test_columns()
{
    QCOMPARE(textColumns("abc"), 3);
    QCOMPARE(textColumns("\tx"), 5);
    QCOMPARE(textColumns("ab\tx"), 5);
    QCOMPARE(textColumns(QString::fromUtf8("e\xcc\x81")), 1);
    QCOMPARE(textColumns(QString::fromUtf8("\xf0\x9f\x98\x80")), 1);
    QCOMPARE(textColumns(QString::fromUtf8("Привет")), 6);
}

// EDIT-MD-20/23: Fill and Unfill of the unit of the cursor, one undo step each, the cursor kept;
// a filled unit takes no step; the lines of the batch; a read-only and a language tab do not fill
void TestTextFill::test_document_fill()
{
    SettingsManager::instance().setTextWidth(20);
    TextDocument doc(MODE_MARKDOWN);
    QCOMPARE(doc.fillKind(), textFillMarkdown);
    doc.setText("aaa bbb ccc ddd eee fff ggg\n\nhhh");
    placeCursor(doc, 5);
    QUndoStack* undo = doc.undoStack();
    int steps = undo->count();
    QVERIFY(doc.canFill() && doc.fill(false));
    QCOMPARE(doc.text(), QString("aaa bbb ccc ddd eee\nfff ggg\n\nhhh"));
    QCOMPARE(undo->count(), steps + 1);
    QCOMPARE(undo->undoText(), QString("fill paragraph"));
    QCOMPARE(doc.editor()->textCursor().position(), 5);
    QVERIFY(doc.fill(false));
    QCOMPARE(undo->count(), steps + 1);
    QVERIFY(doc.fill(true));
    QCOMPARE(doc.text(), QString("aaa bbb ccc ddd eee fff ggg\n\nhhh"));
    QCOMPARE(undo->undoText(), QString("unfill paragraph"));
    undo->undo();
    QCOMPARE(doc.text(), QString("aaa bbb ccc ddd eee\nfff ggg\n\nhhh"));
    QCOMPARE(doc.editor()->textCursor().position(), 5);
    // the lines of the batch, the cursor after them
    QVERIFY(doc.fillLines(1, 2, true));
    QCOMPARE(doc.text(), QString("aaa bbb ccc ddd eee fff ggg\n\nhhh"));
    QVERIFY(!doc.editor()->textCursor().hasSelection());
    QCOMPARE(doc.editor()->textCursor().position(), 27);
    QVERIFY(!doc.fillLines(0, 1, false));
    QVERIFY(!doc.fillLines(2, 99, false));
    QVERIFY(!doc.fillLines(3, 2, false));

    doc.setReadOnlyFlag(true);
    QVERIFY(!doc.canFill());
    QVERIFY(!doc.fill(false));
    TextDocument plain(MODE_TEXT);
    QCOMPARE(plain.fillKind(), textFillPlain);
    QCOMPARE(plain.editor()->fillKind(), textFillPlain);
    TextDocument json(LANGUAGE_JSON);
    QCOMPARE(json.fillKind(), textFillNone);
    QCOMPARE(json.editor()->fillKind(), textFillNone);
    json.setText("aaa bbb ccc ddd eee fff ggg");
    QVERIFY(!json.canFill() && !json.fill(false));
    // a mode switch takes the fill of the mode
    QVERIFY(json.setMode(MODE_MARKDOWN));
    QCOMPARE(json.editor()->fillKind(), textFillMarkdown);
}

// EDIT-MD-21: off by default; a Space or a Return past the width breaks the line, an undo step of
// its own, after the autoreplace; the prefix of a list item and the indent of a plain text; no
// break in code, in a language tab or by a paste
void TestTextFill::test_fill_as_you_type()
{
    SettingsManager& sm = SettingsManager::instance();
    QVERIFY(!sm.getFillAsYouType());
    QCOMPARE(sm.getTextWidth(), TEXT_WIDTH_DEFAULT);
    sm.setTextWidth(20);
    const QString typed = "aaa bbb ccc ddd eee fff ";
    TextDocument off(MODE_MARKDOWN);
    QTest::keyClicks(off.editor(), typed);
    QCOMPARE(off.text(), typed);

    sm.setFillAsYouType(true);
    TextDocument doc(MODE_MARKDOWN);
    QTest::keyClicks(doc.editor(), typed);
    QCOMPARE(doc.text(), QString("aaa bbb ccc ddd eee\nfff "));
    QCOMPARE(doc.undoStack()->undoText(), QString("fill"));
    // Undo right after the break joins the line, the space typed stays
    doc.undoStack()->undo();
    QCOMPARE(doc.text(), typed);
    QCOMPARE(doc.editor()->textCursor().position(), typed.length());

    TextDocument back(MODE_MARKDOWN);
    QTest::keyClicks(back.editor(), typed.trimmed());
    QTest::keyClick(back.editor(), Qt::Key_Return);
    QCOMPARE(back.text(), QString("aaa bbb ccc ddd eee\nfff\n"));

    TextDocument item(MODE_MARKDOWN);
    QTest::keyClicks(item.editor(), "- one -- two three four five ");
    QCOMPARE(item.text(), QString::fromUtf8("- one \xe2\x80\x93 two three\n  four five "));

    TextDocument code(MODE_MARKDOWN);
    code.setText("```\n");
    code.editor()->moveCursor(QTextCursor::End);
    QTest::keyClicks(code.editor(), typed);
    QCOMPARE(code.text(), "```\n" + typed);

    TextDocument plain(MODE_TEXT);
    QTest::keyClicks(plain.editor(), "  aaa bbb ccc ddd eee fff ");
    QCOMPARE(plain.text(), QString("  aaa bbb ccc ddd\n  eee fff "));

    TextDocument cpp("cpp");
    QTest::keyClicks(cpp.editor(), typed);
    QCOMPARE(cpp.text(), typed);

    TextDocument paste(MODE_MARKDOWN);
    paste.editor()->insertPlainText(typed + "ggg ");
    QCOMPARE(paste.text(), typed + "ggg ");
}

// EDIT-MD-1/20/22: the Edit entries and their keys, Alt+Q through the window, the toolbar group of
// a Markdown and a Plain text tab before the Markdown tools, greyed in a read-only and a language
// tab, Fill as You Type the setting both ways
void TestTextFill::test_actions()
{
    SettingsManager& sm = SettingsManager::instance();
    CyberiadaSMEditorWindow window;
    QList<QAction*> edit = window.menuEdit->actions();
    QVERIFY(edit.contains(window.actionFillParagraph));
    QVERIFY(edit.contains(window.actionUnfillParagraph));
    QVERIFY(edit.contains(window.actionFillAsYouType));
    QCOMPARE(window.actionFillParagraph->shortcuts(), QList<QKeySequence>() << QKeySequence("Alt+Q"));
    QCOMPARE(window.actionUnfillParagraph->shortcuts(), QList<QKeySequence>() << QKeySequence("Alt+Shift+Q"));
    QVERIFY(!window.actionFillParagraph->icon().isNull());
    QVERIFY(window.actionFillAsYouType->isCheckable() && !window.actionFillAsYouType->isChecked());
    QVERIFY(!window.actionFillParagraph->isEnabled());
    QVERIFY(!window.actionFillAsYouType->isEnabled());

    QVERIFY(window.openFile("texts/notes.md"));
    QVERIFY(window.actionFillParagraph->isEnabled());
    QVERIFY(window.actionUnfillParagraph->isEnabled());
    QVERIFY(window.actionFillAsYouType->isEnabled());
    QList<QAction*> bar = window.textToolBar->actions();
    QVERIFY(bar.indexOf(window.actionGoToLine) < bar.indexOf(window.actionFillParagraph));
    QVERIFY(bar.indexOf(window.actionFillParagraph) < bar.indexOf(window.actionInsertImage));
    window.actionReadOnly->trigger();
    QVERIFY(!window.actionFillParagraph->isEnabled());
    QVERIFY(!window.actionUnfillParagraph->isEnabled());
    window.actionReadOnly->trigger();
    QVERIFY(window.actionFillParagraph->isEnabled());

    // Alt+Q fills the paragraph of the cursor, one step
    sm.setTextWidth(TEXT_WIDTH_MIN);
    TextDocument* text = window.currentText();
    text->setText("aaa bbb ccc ddd eee fff ggg hhh");
    window.show();
    QApplication::setActiveWindow(&window);
    QVERIFY(QTest::qWaitForWindowActive(&window));
    text->editor()->setFocus();
    QTest::keyClick(text->editor(), Qt::Key_Q, Qt::AltModifier);
    QCOMPARE(text->text(), QString("aaa bbb ccc ddd eee\nfff ggg hhh"));
    QTest::keyClick(text->editor(), Qt::Key_Q, Qt::AltModifier | Qt::ShiftModifier);
    QCOMPARE(text->text(), QString("aaa bbb ccc ddd eee fff ggg hhh"));
    window.actionFillParagraph->trigger();
    QCOMPARE(text->text(), QString("aaa bbb ccc ddd eee\nfff ggg hhh"));

    window.actionFillAsYouType->trigger();
    QVERIFY(sm.getFillAsYouType());
    sm.setFillAsYouType(false);
    QVERIFY(!window.actionFillAsYouType->isChecked());

    // a Plain text has the fill alone, a C++ text none
    window.setCurrentDocument(window.newTextDocument(MODE_TEXT));
    bar = window.textToolBar->actions();
    QVERIFY(bar.contains(window.actionFillParagraph));
    QVERIFY(!bar.contains(window.actionInsertImage));
    QVERIFY(window.actionFillParagraph->isEnabled());
    QVERIFY(window.openFile("texts/blink.cpp"));
    QVERIFY(!window.textToolBar->actions().contains(window.actionFillParagraph));
    QVERIFY(!window.actionFillParagraph->isEnabled());
    QVERIFY(!window.actionFillAsYouType->isEnabled());
    window.hide();
}

// EDIT-MD-22: the row of the Text editors page, its range, Apply storing both, Reset giving 90
// and off; the stored keys
void TestTextFill::test_preferences()
{
    SettingsManager& sm = SettingsManager::instance();
    PreferencesDialog dialog;
    TextSchemePanel* panel = dialog.textSchemePanel();
    QSpinBox* width = panel->findChild<QSpinBox*>("textEditorWidth");
    QCheckBox* fill = panel->findChild<QCheckBox*>("textEditorFill");
    QVERIFY(width && fill);
    QCOMPARE(width, panel->textWidthBox());
    QCOMPARE(fill, panel->fillBox());
    QCOMPARE(width->minimum(), TEXT_WIDTH_MIN);
    QCOMPARE(width->maximum(), TEXT_WIDTH_MAX);
    QCOMPARE(width->value(), TEXT_WIDTH_DEFAULT);
    QVERIFY(!fill->isChecked());
    width->setValue(72);
    fill->setChecked(true);
    QCOMPARE(sm.getTextWidth(), TEXT_WIDTH_DEFAULT);
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    QCOMPARE(sm.getTextWidth(), 72);
    QVERIFY(sm.getFillAsYouType());
    QSettings stored;
    QCOMPARE(stored.value("editor/textWidth").toInt(), 72);
    QVERIFY(stored.value("editor/fillAsYouType").toBool());
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Reset)->click();
    QCOMPARE(sm.getTextWidth(), TEXT_WIDTH_DEFAULT);
    QVERIFY(!sm.getFillAsYouType());
    QCOMPARE(width->value(), TEXT_WIDTH_DEFAULT);
    QVERIFY(!fill->isChecked());
    // a width out of the range is kept within it
    sm.setTextWidth(TEXT_WIDTH_MAX * 2);
    QCOMPARE(sm.getTextWidth(), TEXT_WIDTH_MAX);
}

QTEST_MAIN(TestTextFill)

#include "l4-text-fill.moc"
