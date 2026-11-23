/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process test of the formulas of a Markdown text: the written text, the formula
 * at a place, the dialog, the insert and the edit (see docs/TESTING.md, L4; EDIT-MD-24..27)
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

#include <functional>

#include <QtTest>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QToolBar>
#include <QUndoStack>
#include "code_editor.h"
#include "document_mode.h"
#include "markdown_formula.h"
#include "math_renderer.h"
#include "smeditor_window.h"
#include "text_document.h"
#include "dialogs/insert_formula_dialog.h"

// the dialog is answered once its event loop runs
static const int DIALOG_WAIT_MS = 50;

typedef std::function<bool(InsertFormulaDialog*)> FormulaAnswer;

// the next formula dialog answered: accepted when the answer returns true, else cancelled
static void answerDialog(const FormulaAnswer& answer, bool* seen)
{
    *seen = false;
    QTimer::singleShot(DIALOG_WAIT_MS, [answer, seen]() {
        InsertFormulaDialog* dlg = qobject_cast<InsertFormulaDialog*>(QApplication::activeModalWidget());
        if (!dlg) {
            if (QWidget* other = QApplication::activeModalWidget()) other->close();
            return;
        }
        *seen = true;
        if (answer(dlg)) dlg->accept();
        else dlg->reject();
    });
}

static void placeCursor(TextDocument* doc, int position, int anchor = -1)
{
    QTextCursor cursor = doc->editor()->textCursor();
    cursor.setPosition(anchor < 0 ? position : anchor);
    if (anchor >= 0) cursor.setPosition(position, QTextCursor::KeepAnchor);
    doc->editor()->setTextCursor(cursor);
}

class TestMarkdownFormula: public QObject {
Q_OBJECT
private slots:
    void initTestCase();
    void test_formula_text_data();
    void test_formula_text();
    void test_formula_at();
    void test_replace_range();
    void test_dialog();
    void test_insert();
    void test_edit();
    void test_ctrl_click();
    void test_actions();

private:
    QTemporaryDir settings;
};

void TestMarkdownFormula::initTestCase()
{
    // the window's settings must not reach the profile the other cases share
    QVERIFY(settings.isValid());
    QSettings::setPath(QSettings::defaultFormat(), QSettings::UserScope, settings.path());
}

// EDIT-MD-26: the text written between the text before and after the place
void TestMarkdownFormula::test_formula_text_data()
{
    QTest::addColumn<QString>("tex");
    QTest::addColumn<bool>("display");
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("after");
    QTest::addColumn<QString>("written");

    QTest::newRow("inline") << "x^2" << false << "" << "" << "$x^2$";
    QTest::newRow("trimmed") << "  x  \n" << false << "" << "" << "$x$";
    QTest::newRow("inline lines") << "a +\n  b" << false << "" << "" << "$a + b$";
    QTest::newRow("blank") << " \n\t" << false << "" << "" << "";
    QTest::newRow("display") << "x" << true << "" << "" << "$$x$$";
    QTest::newRow("display lines") << "\\begin{aligned}\na &= b \\\\\nc &= d\n\\end{aligned}" << true << "" << ""
        << "$$\n\\begin{aligned}\na &= b \\\\\nc &= d\n\\end{aligned}\n$$";
    QTest::newRow("blank lines") << "a\n\n  b\n" << true << "" << "" << "$$\na\n  b\n$$";
    QTest::newRow("dollars") << "a $ b \\$ c $" << false << "" << "" << "$a \\$ b \\$ c \\$$";
    QTest::newRow("block starts") << "a\n- b\n+ c" << true << "" << "" << "$$\na - b + c\n$$";
    QTest::newRow("block start first") << "- b\nc" << true << "" << "" << "$$ - b\nc\n$$";
    QTest::newRow("quote and heading") << "a\n> b\n# c" << true << "" << "" << "$$\na > b # c\n$$";
    QTest::newRow("letters") << "S" << false << "area" << "is" << " $S$ ";
    QTest::newRow("spaces") << "S" << false << "area " << " is" << "$S$";
    QTest::newRow("punctuation") << "x" << false << QString::fromUtf8("«") << QString::fromUtf8("»") << "$x$";
    QTest::newRow("digit") << "x" << false << "5" << "." << " $x$";
    QTest::newRow("cyrillic") << "x" << false << QString::fromUtf8("ж") << "," << " $x$";
    QTest::newRow("symbols") << "x" << false << "+" << "=" << "$x$";
    QTest::newRow("dollar before") << "b" << false << "$a$" << "" << " $b$";
    QTest::newRow("dollar after") << "a" << false << "" << "$b$" << "$a$ ";
    QTest::newRow("display in a line") << "x" << true << "a" << "b" << " $$x$$ ";
}

void TestMarkdownFormula::test_formula_text()
{
    QFETCH(QString, tex);
    QFETCH(bool, display);
    QFETCH(QString, before);
    QFETCH(QString, after);
    QFETCH(QString, written);
    QChar b = before.isEmpty() ? QChar() : before.at(before.length() - 1);
    QChar a = after.isEmpty() ? QChar() : after.at(0);
    QCOMPARE(markdownFormulaText(tex, display, b, a), written);
    if (written.isEmpty()) return;

    // md4c reads the formula where it is written, the TeX as markdownFormulaTex makes it
    QString context = before + written + after;
    int begin = before.length() + (written.startsWith(' ') ? 1 : 0);
    MarkdownFormula formula = markdownFormulaAt(context, begin);
    QVERIFY(!formula.isNull());
    QCOMPARE(formula.begin, begin);
    QCOMPARE(formula.end, begin + written.trimmed().length());
    QCOMPARE(formula.display, display);
    QCOMPARE(formula.tex.simplified(), markdownFormulaTex(tex).simplified());
}

// EDIT-MD-24/25: the formula at a place with its dollars, never in code nor escaped; a cursor
// stands in one between its dollars only
void TestMarkdownFormula::test_formula_at()
{
    QString text = "a $x$ b $$y$$ `$z$` \\$5\n\n$$\nw\n$$\n";
    int x = text.indexOf("$x$");
    MarkdownFormula formula = markdownFormulaAt(text, x);
    QCOMPARE(formula.begin, x);
    QCOMPARE(formula.end, x + 3);
    QCOMPARE(formula.tex, QString("x"));
    QVERIFY(!formula.display);
    QVERIFY(markdownFormulaAt(text, x - 1).isNull());
    QVERIFY(markdownFormulaAt(text, x + 3).isNull());
    formula = markdownFormulaAt(text, text.indexOf('y'));
    QCOMPARE(formula.begin, text.indexOf("$$y"));
    QCOMPARE(formula.tex, QString("y"));
    QVERIFY(formula.display);
    QVERIFY(markdownFormulaAt(text, text.indexOf('z')).isNull());
    QVERIFY(markdownFormulaAt(text, text.indexOf("\\$5") + 1).isNull());
    int w = text.indexOf("$$\nw");
    formula = markdownFormulaAt(text, text.indexOf('w'));
    QCOMPARE(formula.begin, w);
    QCOMPARE(formula.end, w + 7);
    QCOMPARE(formula.tex, QString("w"));
    QVERIFY(formula.display);
    QVERIFY(markdownFormulaAt(text, -1).isNull());
    QVERIFY(markdownFormulaAt(text, text.length()).isNull());

    QVERIFY(markdownFormulaAround(text, x).isNull());
    QCOMPARE(markdownFormulaAround(text, x + 1).begin, x);
    QCOMPARE(markdownFormulaAround(text, x + 2).begin, x);
    QVERIFY(markdownFormulaAround(text, x + 3).isNull());
    QString two = "$a$ $b$";
    QVERIFY(markdownFormulaAround(two, 3).isNull());
    QVERIFY(markdownFormulaAround(two, 4).isNull());
    QCOMPARE(markdownFormulaAround(two, 5).begin, 4);

    QCOMPARE(markdownCharBefore("ab\ncd", 3), QChar());
    QCOMPARE(markdownCharBefore("ab\ncd", 2), QChar('b'));
    QCOMPARE(markdownCharBefore("ab", 0), QChar());
    QCOMPARE(markdownCharAfter("ab\ncd", 2), QChar());
    QCOMPARE(markdownCharAfter("ab", 0), QChar('a'));
    QCOMPARE(markdownCharAfter("ab", 2), QChar());

    // a matrix over lines is one formula, its \\ kept, drawn without errors
    QString matrix = "$$\n\\begin{pmatrix}\na & b \\\\\nc & d\n\\end{pmatrix}\n$$";
    formula = markdownFormulaAt(matrix, matrix.indexOf('c'));
    QCOMPARE(formula.begin, 0);
    QCOMPARE(formula.end, matrix.length());
    QCOMPARE(formula.tex, QString("\\begin{pmatrix}\na & b \\\\\nc & d\n\\end{pmatrix}"));
#ifdef CYBERIADA_WITH_MATH
    QVERIFY(MathRenderer::instance().render(formula.tex, true, QFont(), Qt::black).errors.isEmpty());
#endif
}

// EDIT-MD-25: a range replaced in one step, the cursor after it; a block of more lines leaves
// the cursor after its last line
void TestMarkdownFormula::test_replace_range()
{
    TextDocument doc(MODE_MARKDOWN);
    doc.setText("a $x$ b");
    int steps = doc.undoStack()->count();
    QVERIFY(doc.replaceRange(2, 5, "$y^2$", "edit formula"));
    QCOMPARE(doc.text(), QString("a $y^2$ b"));
    QCOMPARE(doc.editor()->textCursor().position(), 7);
    QCOMPARE(doc.undoStack()->count(), steps + 1);
    QCOMPARE(doc.undoStack()->undoText(), QString("edit formula"));
    doc.undoStack()->undo();
    QCOMPARE(doc.text(), QString("a $x$ b"));
    QVERIFY(!doc.replaceRange(5, 2, "z", "edit formula"));
    QVERIFY(!doc.replaceRange(0, 100, "z", "edit formula"));
    doc.setReadOnlyFlag(true);
    QVERIFY(!doc.replaceRange(0, 1, "z", "edit formula"));

    TextDocument block(MODE_MARKDOWN);
    block.setText("One.\nTwo.");
    placeCursor(&block, 0);
    QVERIFY(block.insertBlock("$$\na\n$$", 0, "insert formula"));
    QCOMPARE(block.text(), QString("One.\nTwo.\n\n$$\na\n$$"));
    QCOMPARE(block.editor()->textCursor().position(), block.text().length());
}

// EDIT-MD-24/26: the text written as typed, Insert greyed while blank, the preview after the
// pause, the errors told and still inserted, the kind, a display paragraph without neighbours
void TestMarkdownFormula::test_dialog()
{
    InsertFormulaDialog dlg(QString(), false, FormulaPlace(), false);
    QCOMPARE(dlg.windowTitle(), QString("Insert Formula"));
    QCOMPARE(dlg.insertButton()->text(), QString("Insert"));
    QVERIFY(dlg.inlineChoice()->isChecked());
    QVERIFY(!dlg.insertButton()->isEnabled());
    QVERIFY(dlg.lineLabel()->text().isEmpty());
    dlg.texEdit()->setPlainText("x^2");
    QVERIFY(dlg.insertButton()->isEnabled());
    QCOMPARE(dlg.lineLabel()->text(), QString("$x^2$"));
    QCOMPARE(dlg.insertedText(), QString("$x^2$"));
    dlg.renderPreview();
    QVERIFY(!dlg.preview().isNull());
#ifdef CYBERIADA_WITH_MATH
    QVERIFY(dlg.errorLabel()->text().isEmpty());
#else
    QVERIFY(!dlg.errorLabel()->text().isEmpty());
#endif
    dlg.displayChoice()->setChecked(true);
    QCOMPARE(dlg.insertedText(), QString("$$x^2$$"));
    QCOMPARE(dlg.tex(), QString("x^2"));
    QVERIFY(dlg.display());

    dlg.texEdit()->setPlainText("\\badcommand{x}");
    QTRY_VERIFY(!dlg.errorLabel()->text().isEmpty());
    QVERIFY(dlg.insertButton()->isEnabled());
    dlg.texEdit()->setPlainText(" \n ");
    QVERIFY(!dlg.insertButton()->isEnabled());
    dlg.renderPreview();
    QVERIFY(dlg.preview().isNull());
    QVERIFY(dlg.errorLabel()->text().isEmpty());

    FormulaPlace place;
    place.before = 'a';
    place.after = 'b';
    InsertFormulaDialog between("S", false, place, false);
    QCOMPARE(between.insertedText(), QString(" $S$ "));
    place.displayBlock = true;
    InsertFormulaDialog block("S", true, place, false);
    QCOMPARE(block.insertedText(), QString("$$S$$"));
    block.inlineChoice()->setChecked(true);
    QCOMPARE(block.insertedText(), QString(" $S$ "));

    InsertFormulaDialog edit("y", true, FormulaPlace(), true);
    QCOMPARE(edit.windowTitle(), QString("Edit Formula"));
    QCOMPARE(edit.insertButton()->text(), QString("Replace"));
    QVERIFY(edit.displayChoice()->isChecked());
    QCOMPARE(edit.texEdit()->toPlainText(), QString("y"));
}

// EDIT-MD-24: at the cursor with spaces where md4c needs them, the selection as the TeX and
// replaced, a display paragraph after the paragraph of the cursor, one step, Cancel, an
// untitled tab
void TestMarkdownFormula::test_insert()
{
    CyberiadaSMEditorWindow window;
    TextDocument* text = window.newTextDocument(MODE_MARKDOWN);
    window.setCurrentDocument(text);
    QVERIFY(!text->hasFile());
    bool seen = false;
    QString title, proposed;

    text->setText("area is");
    placeCursor(text, 5);
    int steps = text->undoStack()->count();
    answerDialog([&](InsertFormulaDialog* dlg) {
        title = dlg->windowTitle();
        dlg->texEdit()->setPlainText("S");
        return true;
    }, &seen);
    window.insertFormula(text);
    QVERIFY(seen);
    QCOMPARE(title, QString("Insert Formula"));
    QCOMPARE(text->text(), QString("area $S$ is"));
    QCOMPARE(text->undoStack()->count(), steps + 1);
    QCOMPARE(text->undoStack()->undoText(), QString("insert formula"));
    text->undoStack()->undo();
    QCOMPARE(text->text(), QString("area is"));

    text->setText("x = a^2 + b^2");
    placeCursor(text, 13, 4);
    answerDialog([&](InsertFormulaDialog* dlg) {
        proposed = dlg->tex();
        return true;
    }, &seen);
    window.insertFormula(text);
    QVERIFY(seen);
    QCOMPARE(proposed, QString("a^2 + b^2"));
    QCOMPARE(text->text(), QString("x = $a^2 + b^2$"));

    text->setText("One.\nTwo.");
    placeCursor(text, 0);
    answerDialog([&](InsertFormulaDialog* dlg) {
        dlg->displayChoice()->setChecked(true);
        dlg->texEdit()->setPlainText("x");
        return true;
    }, &seen);
    window.insertFormula(text);
    QVERIFY(seen);
    QCOMPARE(text->text(), QString("One.\nTwo.\n\n$$x$$"));

    // a selection is replaced in its place, a display formula too
    text->setText("see it here");
    placeCursor(text, 6, 4);
    answerDialog([&](InsertFormulaDialog* dlg) {
        dlg->displayChoice()->setChecked(true);
        return true;
    }, &seen);
    window.insertFormula(text);
    QVERIFY(seen);
    QCOMPARE(text->text(), QString("see $$it$$ here"));

    text->setText("plain");
    steps = text->undoStack()->count();
    answerDialog([&](InsertFormulaDialog* dlg) {
        dlg->texEdit()->setPlainText("x");
        return false;
    }, &seen);
    window.insertFormula(text);
    QVERIFY(seen);
    QCOMPARE(text->text(), QString("plain"));
    QCOMPARE(text->undoStack()->count(), steps);

    // a read-only tab opens no dialog
    text->setReadOnlyFlag(true);
    window.insertFormula(text);
    QCOMPARE(text->text(), QString("plain"));
}

// EDIT-MD-24/25: the formula of the cursor or of a selection end edited in its place, its kind
// changed, more lines, the same formula and Cancel changing nothing
void TestMarkdownFormula::test_edit()
{
    CyberiadaSMEditorWindow window;
    TextDocument* text = window.newTextDocument(MODE_MARKDOWN);
    window.setCurrentDocument(text);
    bool seen = false;
    QString title, button, proposed;
    bool proposedDisplay = true;

    text->setText("a $x$ b");
    placeCursor(text, 3);
    int steps = text->undoStack()->count();
    answerDialog([&](InsertFormulaDialog* dlg) {
        title = dlg->windowTitle();
        button = dlg->insertButton()->text();
        proposed = dlg->tex();
        proposedDisplay = dlg->display();
        dlg->texEdit()->setPlainText("y");
        return true;
    }, &seen);
    window.insertFormula(text);
    QVERIFY(seen);
    QCOMPARE(title, QString("Edit Formula"));
    QCOMPARE(button, QString("Replace"));
    QCOMPARE(proposed, QString("x"));
    QVERIFY(!proposedDisplay);
    QCOMPARE(text->text(), QString("a $y$ b"));
    QCOMPARE(text->undoStack()->count(), steps + 1);
    QCOMPARE(text->undoStack()->undoText(), QString("edit formula"));

    // the same formula takes no step
    steps = text->undoStack()->count();
    answerDialog([&](InsertFormulaDialog*) { return true; }, &seen);
    window.editFormulaAt(text, 2);
    QVERIFY(seen);
    QCOMPARE(text->text(), QString("a $y$ b"));
    QCOMPARE(text->undoStack()->count(), steps);

    // an end of the selection inside a formula
    placeCursor(text, 3, 0);
    answerDialog([&](InsertFormulaDialog* dlg) {
        proposed = dlg->tex();
        dlg->displayChoice()->setChecked(true);
        return true;
    }, &seen);
    window.insertFormula(text);
    QVERIFY(seen);
    QCOMPARE(proposed, QString("y"));
    QCOMPARE(text->text(), QString("a $$y$$ b"));

    answerDialog([&](InsertFormulaDialog* dlg) {
        dlg->texEdit()->setPlainText("z");
        return false;
    }, &seen);
    window.editFormulaAt(text, 3);
    QVERIFY(seen);
    QCOMPARE(text->text(), QString("a $$y$$ b"));

    text->setText("$$\nw\n$$");
    answerDialog([&](InsertFormulaDialog* dlg) {
        proposed = dlg->tex();
        proposedDisplay = dlg->display();
        dlg->texEdit()->setPlainText("a\nb");
        return true;
    }, &seen);
    window.editFormulaAt(text, 3);
    QVERIFY(seen);
    QCOMPARE(proposed, QString("w"));
    QVERIFY(proposedDisplay);
    QCOMPARE(text->text(), QString("$$\na\nb\n$$"));

    // no formula there: no dialog
    text->setText("plain `$x$`");
    window.editFormulaAt(text, 0);
    window.editFormulaAt(text, 8);
    QCOMPARE(text->text(), QString("plain `$x$`"));
}

// EDIT-MD-25: Ctrl+click on a formula of the source opens it, a display one over lines too; a
// plain click edits; the hand with Ctrl; code and a locked tab open none
void TestMarkdownFormula::test_ctrl_click()
{
    CyberiadaSMEditorWindow window;
    window.resize(1000, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    TextDocument* text = window.newTextDocument(MODE_MARKDOWN);
    window.setCurrentDocument(text);
    text->setText("a $x$ b `$y$`\n\n$$\nw\n$$");
    CodeEditor* editor = text->editor();
    QWidget* viewport = editor->viewport();
    // a point in the glyph at a text position
    auto at = [&](int position) {
        QTextCursor c(editor->document());
        c.setPosition(position + 1);
        QRect gap = editor->cursorRect(c);
        return QPoint(gap.left() - 2, gap.center().y());
    };
    int x = text->text().indexOf('x');
    int w = text->text().indexOf('w');
    QCOMPARE(editor->formulaAt(at(x)), x);
    QCOMPARE(editor->formulaAt(at(x - 1)), x - 1);
    QCOMPARE(editor->formulaAt(at(x + 2)), -1);
    QCOMPARE(editor->formulaAt(at(text->text().indexOf('y'))), -1);
    QCOMPARE(editor->formulaAt(at(w)), w);

    QTest::mouseClick(viewport, Qt::LeftButton, Qt::NoModifier, at(x));
    QCOMPARE(text->text(), QString("a $x$ b `$y$`\n\n$$\nw\n$$"));

    bool seen = false;
    QString title, proposed;
    bool proposedDisplay = false;
    answerDialog([&](InsertFormulaDialog* dlg) {
        title = dlg->windowTitle();
        proposed = dlg->tex();
        dlg->texEdit()->setPlainText("z");
        return true;
    }, &seen);
    QTest::mouseClick(viewport, Qt::LeftButton, Qt::ControlModifier, at(x));
    QTRY_VERIFY(seen);
    QCOMPARE(title, QString("Edit Formula"));
    QCOMPARE(proposed, QString("x"));
    QCOMPARE(text->text(), QString("a $z$ b `$y$`\n\n$$\nw\n$$"));

    answerDialog([&](InsertFormulaDialog* dlg) {
        proposed = dlg->tex();
        proposedDisplay = dlg->display();
        return false;
    }, &seen);
    QTest::mouseClick(viewport, Qt::LeftButton, Qt::ControlModifier, at(w));
    QTRY_VERIFY(seen);
    QCOMPARE(proposed, QString("w"));
    QVERIFY(proposedDisplay);

    Qt::CursorShape shape = viewport->cursor().shape();
    QMouseEvent over(QEvent::MouseMove, at(x), Qt::NoButton, Qt::NoButton, Qt::ControlModifier);
    QApplication::sendEvent(viewport, &over);
    QCOMPARE(viewport->cursor().shape(), Qt::PointingHandCursor);
    QMouseEvent away(QEvent::MouseMove, at(0), Qt::NoButton, Qt::NoButton, Qt::ControlModifier);
    QApplication::sendEvent(viewport, &away);
    QCOMPARE(viewport->cursor().shape(), shape);

    // a locked tab only underlines
    text->setLocked(true);
    QCOMPARE(editor->formulaAt(at(x)), -1);
    QTest::mouseClick(viewport, Qt::LeftButton, Qt::ControlModifier, at(x));
    QTest::qWait(DIALOG_WAIT_MS);
    QCOMPARE(text->text(), QString("a $z$ b `$y$`\n\n$$\nw\n$$"));
    text->setLocked(false);
    window.hide();
}

// EDIT-MD-1/24: Insert Formula in Edit, on the text toolbar after Insert Link, Ctrl+Shift+M, a
// writable Markdown tab only
void TestMarkdownFormula::test_actions()
{
    CyberiadaSMEditorWindow window;
    QVERIFY(window.menuEdit->actions().contains(window.actionInsertFormula));
    QCOMPARE(window.actionInsertFormula->shortcuts(), QList<QKeySequence>() << QKeySequence("Ctrl+Shift+M"));
    QVERIFY(!window.actionInsertFormula->icon().isNull());
    QVERIFY(!window.actionInsertFormula->isEnabled());
    QVERIFY(window.openFile("texts/notes.md"));
    QVERIFY(window.actionInsertFormula->isEnabled());
    QList<QAction*> bar = window.textToolBar->actions();
    QVERIFY(bar.indexOf(window.actionInsertLink) < bar.indexOf(window.actionInsertFormula));
    QVERIFY(bar.indexOf(window.actionInsertFormula) < bar.indexOf(window.actionUpdateDiagramImages));
    window.actionReadOnly->trigger();
    QVERIFY(!window.actionInsertFormula->isEnabled());
    window.actionReadOnly->trigger();
    QVERIFY(window.actionInsertFormula->isEnabled());

    TextDocument* text = window.currentText();
    window.show();
    QApplication::setActiveWindow(&window);
    QVERIFY(QTest::qWaitForWindowActive(&window));
    text->editor()->setFocus();
    bool seen = false;
    QString title;
    answerDialog([&](InsertFormulaDialog* dlg) {
        title = dlg->windowTitle();
        return false;
    }, &seen);
    QTest::keyClick(text->editor(), Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
    QVERIFY(seen);
    QCOMPARE(title, QString("Insert Formula"));
    window.hide();

    window.setCurrentDocument(window.newTextDocument(MODE_TEXT));
    QVERIFY(!window.actionInsertFormula->isEnabled());
    QVERIFY(!window.textToolBar->actions().contains(window.actionInsertFormula));
    QVERIFY(window.openFile("diagrams/hierarchy.graphml"));
    QVERIFY(!window.actionInsertFormula->isEnabled());
}

QTEST_MAIN(TestMarkdownFormula)
#include "l4-markdown-formula.moc"
