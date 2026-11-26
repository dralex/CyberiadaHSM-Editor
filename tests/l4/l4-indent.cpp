/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The indentation of the source editors (EDIT-INDENT-1..9)
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
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QUndoStack>
#include "code_editor.h"
#include "code_highlighter.h"
#include "cyberiada_constants.h"
#include "document_mode.h"
#include "settings_manager.h"
#include "smeditor_window.h"
#include "text_document.h"
#include "text_indent.h"
#include "dialogs/multilinetextdialog.h"
#include "dialogs/preferences_dialog.h"
#include "dialogs/stateactiondialog.h"
#include "dialogs/text_scheme_panel.h"

// the cursor in the texts of the cases; the keys of a case: Return, Tab, Shift+Tab
static const QChar CURSOR_MARK(0x00a6);
static const QChar KEY_RETURN(0x23ce);
static const QChar KEY_TAB(0x21e5);
static const QChar KEY_BACKTAB(0x21e4);

class TestIndent: public QObject {
Q_OBJECT
private slots:
    void initTestCase();
    void test_languages();
    void test_reindent_data();
    void test_reindent();
    void test_levels();
    void test_forms();
    void test_points();
    void test_keys_data();
    void test_keys();
    void test_only_whitespace();
    void test_undo();
    void test_read_only();
    void test_switch_off();
    void test_document();
    void test_dialogs();
    void test_actions();
    void test_preferences();
    void cleanup();

private:
    QTemporaryDir settings;
};

void TestIndent::initTestCase()
{
    // the stored indent must not reach the profile the other cases share
    QVERIFY(settings.isValid());
    QSettings::setPath(QSettings::defaultFormat(), QSettings::UserScope, settings.path());
    SettingsManager::instance().setIndentWidth(INDENT_WIDTH_DEFAULT);
    SettingsManager::instance().setIndentTabs(false);
    SettingsManager::instance().setAutoIndent(true);
}

void TestIndent::cleanup()
{
    SettingsManager::instance().setIndentWidth(INDENT_WIDTH_DEFAULT);
    SettingsManager::instance().setIndentTabs(false);
    SettingsManager::instance().setAutoIndent(true);
}

static TextIndentRules rulesOf(const QString& mode)
{
    if (mode == MODE_MARKDOWN) return textIndentRules(textFillMarkdown);
    if (mode == MODE_TEXT) return textIndentRules(textFillPlain);
    return textIndentRules(CodeStyle::instance().findId(mode));
}

// a text with the cursor mark: the text without it and the place of the mark
static QString unmarked(const QString& marked, int* cursor)
{
    *cursor = marked.indexOf(CURSOR_MARK);
    QString text = marked;
    if (*cursor >= 0) text.remove(*cursor, 1);
    return text;
}

static QString marked(TextDocument& doc)
{
    return doc.text().insert(doc.editor()->textCursor().position(), CURSOR_MARK);
}

static void placeCursor(TextDocument& doc, int place)
{
    QTextCursor cursor = doc.editor()->textCursor();
    cursor.setPosition(place);
    doc.editor()->setTextCursor(cursor);
}

// the keys of a case into the editor: Return, Tab, Shift+Tab or a character typed
static void press(QWidget* editor, const QString& keys)
{
    for (QChar c : keys) {
        if (c == KEY_RETURN) QTest::keyClick(editor, Qt::Key_Return);
        else if (c == KEY_TAB) QTest::keyClick(editor, Qt::Key_Tab);
        else if (c == KEY_BACKTAB) QTest::keyClick(editor, Qt::Key_Backtab, Qt::ShiftModifier);
        else QTest::keyClicks(editor, QString(c));
    }
}

// EDIT-INDENT-5: the styles of the built-in language files; a language without the key is relative
void TestIndent::test_languages()
{
    QCOMPARE(rulesOf("cpp").style, textIndentBraces);
    QCOMPARE(rulesOf("c").style, textIndentBraces);
    QCOMPARE(rulesOf("csharp").style, textIndentBraces);
    QCOMPARE(rulesOf("json").style, textIndentBrackets);
    QCOMPARE(rulesOf("python").style, textIndentOffside);
    QVERIFY(rulesOf("cpp").labels.contains("public"));
    QVERIFY(rulesOf("cpp").flat.contains("namespace"));
    QVERIFY(!rulesOf("csharp").flat.contains("namespace"));
    QCOMPARE(rulesOf(MODE_MARKDOWN).style, textIndentRelative);
    QCOMPARE(rulesOf(MODE_MARKDOWN).fill, textFillMarkdown);
    QCOMPARE(textIndentRules(static_cast<const CodeLanguage*>(nullptr)).style, textIndentRelative);
    QVERIFY(!textIndentComputes(rulesOf(MODE_TEXT)));
    QVERIFY(textIndentComputes(rulesOf("json")));
}

// EDIT-INDENT-1/5/6: the whole text re-indented, each line with those above as re-indented
void TestIndent::test_reindent_data()
{
    QTest::addColumn<QString>("mode");
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("result");

    QTest::newRow("block") << "cpp" << "void f() {\nfoo();\n        bar();\n}"
        << "void f() {\n    foo();\n    bar();\n}";
    QTest::newRow("allman") << "cpp" << "void f()\n    {\nfoo();\n    }" << "void f()\n{\n    foo();\n}";
    QTest::newRow("switch labels") << "cpp" << "switch (a) {\n    case 1:\nfoo();\n  break;\ndefault: {\nbar();\n}\n}"
        << "switch (a) {\ncase 1:\n    foo();\n    break;\ndefault: {\n    bar();\n}\n}";
    QTest::newRow("access labels") << "cpp" << "class A : public B {\n    public:\nA();\n  private slots:\nvoid s();\n};"
        << "class A : public B {\npublic:\n    A();\nprivate slots:\n    void s();\n};";
    QTest::newRow("flat namespace") << "cpp" << "namespace {\n    int counter;\n}\nnamespace a\n{\n  int b;\n}"
        << "namespace {\nint counter;\n}\nnamespace a\n{\nint b;\n}";
    QTest::newRow("csharp namespace") << "csharp" << "namespace A\n{\nclass B\n{\n}\n}"
        << "namespace A\n{\n    class B\n    {\n    }\n}";
    QTest::newRow("extern c") << "c" << "extern \"C\" {\n    void f(void);\n}" << "extern \"C\" {\nvoid f(void);\n}";
    QTest::newRow("substatements") << "cpp" << "if (a)\nif (b)\nc();\nelse\nd();\ne();"
        << "if (a)\n    if (b)\n        c();\nelse\n    d();\ne();";
    QTest::newRow("else if") << "cpp" << "if (a) {\nx();\n} else if (b) {\ny();\n}"
        << "if (a) {\n    x();\n} else if (b) {\n    y();\n}";
    QTest::newRow("do while") << "cpp" << "do {\nx();\n} while (a);\ny();" << "do {\n    x();\n} while (a);\ny();";
    QTest::newRow("continued") << "cpp" << "foo(a) +\nb +\nc;\nint y;" << "foo(a) +\n    b +\n    c;\nint y;";
    QTest::newRow("assignment") << "cpp" << "int x = a +\nb;\nx = y\n? 1\n: 2;" << "int x = a +\n        b;\nx = y\n    ? 1\n    : 2;";
    QTest::newRow("assignment ends its line") << "cpp" << "label =\nvalue;" << "label =\n    value;";
    QTest::newRow("cascade") << "cpp" << "{\nm = tr(\"%1 %2\").arg(a)\n.arg(b);\n}" << "{\n    m = tr(\"%1 %2\").arg(a)\n        .arg(b);\n}";
    QTest::newRow("cascade in a call") << "cpp" << "return fail(error, tr(\"%1\")\n.arg(x));" << "return fail(error, tr(\"%1\")\n            .arg(x));";
    QTest::newRow("return type above") << "cpp" << "A::Side\n    A::side(int s)\n{\n}\ntemplate <typename T>\n  T f();"
        << "A::Side\nA::side(int s)\n{\n}\ntemplate <typename T>\nT f();";
    QTest::newRow("return") << "cpp" << "return a &&\nb;" << "return a &&\n       b;";
    QTest::newRow("ternary") << "cpp" << "return on ? one\n: two;" << "return on ? one\n          : two;";
    QTest::newRow("ternary chain") << "cpp" << "c = a ? x :\nb ? y\n: z;" << "c = a ? x :\n    b ? y\n      : z;";
    QTest::newRow("stream") << "cpp" << "list << a << b\n<< c;\nf(QStringList() << x\n<< y);"
        << "list << a << b\n     << c;\nf(QStringList() << x\n                << y);";
    QTest::newRow("argument continued") << "cpp" << "f(at, A |\nB);\ng(a,\nb + c +\nd);"
        << "f(at, A |\n      B);\ng(a,\n  b + c +\n  d);";
    QTest::newRow("template arguments") << "cpp" << "f(QMap<A, B> m,\nx);\nfor (QMap<A, B>::iterator i = m.begin();\ni != m.end(); i++) {\n}"
        << "f(QMap<A, B> m,\n  x);\nfor (QMap<A, B>::iterator i = m.begin();\n     i != m.end(); i++) {\n}";
    QTest::newRow("macro") << "cpp" << "class A {\n    Q_OBJECT\n    public:\nA();\n};\nQ_DECLARE_METATYPE(A)\nint b;"
        << "class A {\n    Q_OBJECT\npublic:\n    A();\n};\nQ_DECLARE_METATYPE(A)\nint b;";
    QTest::newRow("raw string") << "cpp" << "{\nauto r = R\"x(a \" ( { b)x\";\nfoo();\n}"
        << "{\n    auto r = R\"x(a \" ( { b)x\";\n    foo();\n}";
    QTest::newRow("raw string lines") << "cpp" << "{\nauto r = R\"(one {\n  two)\";\nfoo();\n}"
        << "{\n    auto r = R\"(one {\n  two)\";\n    foo();\n}";
    QTest::newRow("verbatim string") << "csharp" << "{\nvar p = @\"c:\\dir\\\";\nfoo();\n}"
        << "{\n    var p = @\"c:\\dir\\\";\n    foo();\n}";
    QTest::newRow("initializers") << "cpp" << "A::A():\nb(1),\nc(2)\n{\n}" << "A::A():\n    b(1),\n    c(2)\n{\n}";
    QTest::newRow("aligned call") << "cpp" << "foo(a,\nb,\n        c);" << "foo(a,\n    b,\n    c);";
    QTest::newRow("condition") << "cpp" << "if (a &&\nb) {\ncall(x,\ny);\n}"
        << "if (a &&\n    b) {\n    call(x,\n         y);\n}";
    QTest::newRow("opener ends") << "cpp" << "foo(\nx, y\n);" << "foo(\n    x, y\n);";
    QTest::newRow("closer aligned") << "cpp" << "foo(a,\nb\n);" << "foo(a,\n    b\n   );";
    QTest::newRow("list") << "cpp" << "int v[] = {\n1,\n2\n};\nenum E {\nA,\nB\n};"
        << "int v[] = {\n    1,\n    2\n};\nenum E {\n    A,\n    B\n};";
    QTest::newRow("lambda") << "cpp" << "connect(a, b, [this]() {\nfoo();\n});"
        << "connect(a, b, [this]() {\n    foo();\n});";
    QTest::newRow("preprocessor") << "cpp" << "void f() {\n    #ifdef X\nfoo();\n  #endif\n}"
        << "void f() {\n#ifdef X\n    foo();\n#endif\n}";
    QTest::newRow("continued directive") << "cpp" << "#define M(a) \\\n        (a)\nint b;" << "#define M(a) \\\n        (a)\nint b;";
    QTest::newRow("block comment") << "cpp" << "{\n/* one\n      two */\nx();\n}" << "{\n    /* one\n      two */\n    x();\n}";
    QTest::newRow("strings and chars") << "cpp" << "{\ns = \"{(\";\nc = '{';\n// {\nx();\n}"
        << "{\n    s = \"{(\";\n    c = '{';\n    // {\n    x();\n}";
    QTest::newRow("comment under a comment") << "cpp" << "{\n    x(); // one\n         // two\n// three\n}"
        << "{\n    x(); // one\n         // two\n    // three\n}";
    QTest::newRow("blank lines emptied") << "cpp" << "{\n   \nx();\n}" << "{\n\n    x();\n}";
    QTest::newRow("json") << "json" << "{\n\"a\": 1,\n\"b\": [1,\n2],\n\"c\": {\n\"d\": null\n}\n}"
        << "{\n    \"a\": 1,\n    \"b\": [1,\n          2],\n    \"c\": {\n        \"d\": null\n    }\n}";
    QTest::newRow("python block") << "python" << "def f(a):\nif a:\nreturn 1\n      x = 2"
        << "def f(a):\n    if a:\n        return 1\n    x = 2";
    QTest::newRow("python dedenters") << "python" << "if a:\nx()\n  elif b:\ny()\n   else:\nz()"
        << "if a:\n    x()\nelif b:\n    y()\nelse:\n    z()";
    QTest::newRow("python try") << "python" << "try:\npass\n  except E:\nx()\n finally:\ny()"
        << "try:\n    pass\nexcept E:\n    x()\nfinally:\n    y()";
    QTest::newRow("python brackets") << "python" << "x = [1,\n2]\ny = (\n3\n)\nz = 1"
        << "x = [1,\n     2]\ny = (\n    3\n)\nz = 1";
    QTest::newRow("python backslash") << "python" << "if a:\nz = 1 + \\\n2\n      w = 3" << "if a:\n    z = 1 + \\\n        2\n    w = 3";
    QTest::newRow("python docstring") << "python" << "def f():\n\"\"\"Doc\n   kept\"\"\"\n      return 1"
        << "def f():\n    \"\"\"Doc\n   kept\"\"\"\n    return 1";
    QTest::newRow("python blocks kept") << "python" << "class A:\n    def f(self):\n        x = 1\n    def g(self):\n          y = 2\n  z = 3"
        << "class A:\n    def f(self):\n        x = 1\n    def g(self):\n        y = 2\n    z = 3";
    QTest::newRow("python outer else") << "python" << "if a:\n    if b:\n        x\nelse:\n    y" << "if a:\n    if b:\n        x\nelse:\n    y";
    QTest::newRow("python delimiter") << "python" << "check(a, b and\nc, d)" << "check(a, b and\n      c, d)";
    QTest::newRow("python keyword value") << "python" << "add(x, help='one '\n'two')" << "add(x, help='one '\n            'two')";
    QTest::newRow("python header backslash") << "python" << "if a and \\\nb:\nx = 1" << "if a and \\\n        b:\n    x = 1";
    QTest::newRow("python header bracket") << "python" << "for a in (\n1, 2):\nx = a\n    y = (\n3)"
        << "for a in (\n        1, 2):\n    x = a\n    y = (\n        3)";
    QTest::newRow("python comment") << "python" << "if a:\n# note\nx = 1" << "if a:\n    # note\n    x = 1";
}

void TestIndent::test_reindent()
{
    QFETCH(QString, mode);
    QFETCH(QString, text);
    QFETCH(QString, result);
    int lines = text.count('\n') + 1;
    TextFillEdit edit = textReindent(text, 0, lines - 1, rulesOf(mode), INDENT_WIDTH_DEFAULT, false, 0, 0);
    QString now = edit.changed ? text.left(edit.from) + edit.text + text.mid(edit.to) : text;
    QCOMPARE(now, result);
    // the result stays as it is
    QVERIFY(!textReindent(result, 0, lines - 1, rulesOf(mode), INDENT_WIDTH_DEFAULT, false, 0, 0).changed);
}

// EDIT-INDENT-1: the Python levels of a line, the column first
void TestIndent::test_levels()
{
    QString text = "def f():\n    if a:\n        x = 1\n        y";
    QCOMPARE(textIndentLevels(text, 3, rulesOf("python"), 4), QVector<int>({ 8, 4, 0 }));
    // the body of a block has its column alone; a dedenter the openers it may close
    QCOMPARE(textIndentLevels(text, 1, rulesOf("python"), 4), QVector<int>({ 4 }));
    QCOMPARE(textIndentLevels("if a:\n    if b:\n        x\n        else:", 3, rulesOf("python"), 4),
             QVector<int>({ 4, 0 }));
    QCOMPARE(textIndentLevels("{\nx", 1, rulesOf("cpp"), 4), QVector<int>({ 4 }));
    // a line in a string has no column
    QCOMPARE(textIndentColumn("s = \"\"\"a\n  b", 1, rulesOf("python"), 4), -1);
    QCOMPARE(textIndentColumn("/* a\n  b", 1, rulesOf("cpp"), 4), -1);
}

// EDIT-INDENT-4: spaces, or a tab per stop of 4 and spaces; a line rewritten in the form; one at
// its column in the form is no edit
void TestIndent::test_forms()
{
    QCOMPARE(textIndentString(6, false), QString(6, ' '));
    QCOMPARE(textIndentString(6, true), QString("\t  "));
    QCOMPARE(textIndentString(8, true), QString("\t\t"));
    QCOMPARE(textIndentRun(2, 8, true), QString("\t\t"));
    QCOMPARE(textIndentRun(5, 7, true), QString("  "));
    QCOMPARE(textIndentRun(5, 7, false), QString("  "));
    QCOMPARE(textNextStop(5, 4), 8);
    QCOMPARE(textPreviousStop(5, 4), 4);
    QCOMPARE(textPreviousStop(4, 4), 0);
    QCOMPARE(textPreviousStop(0, 4), 0);
    QCOMPARE(textLineIndent("\t  x"), 6);

    QString tabbed = "{\n\tx();\n}";
    TextFillEdit spaces = textReindent(tabbed, 0, 2, rulesOf("cpp"), 4, false, 0, 0);
    QVERIFY(spaces.changed);
    QCOMPARE(spaces.text, QString("    x();"));
    QVERIFY(!textReindent(tabbed, 0, 2, rulesOf("cpp"), 4, true, 0, 0).changed);
    TextFillEdit deep = textReindent("{\n{\n{\nx();", 0, 3, rulesOf("cpp"), 4, true, 0, 0);
    QCOMPARE(deep.text, QString("\t{\n\t\t{\n\t\t\tx();"));
    // the width of a level is its own, the stops stay 4
    TextFillEdit two = textReindent("{\n{\nx();", 0, 2, rulesOf("cpp"), 2, true, 0, 0);
    QCOMPARE(two.text, QString("  {\n\tx();"));

    // Convert Tabs to Spaces keeps the columns, inside the lines too
    TextFillEdit untab = textUntabify("a\tb\n\tc", 0, 6, 0, 0);
    QVERIFY(untab.changed);
    QCOMPARE(untab.text, QString("a   b\n    c"));
    QVERIFY(!textUntabify("a b", 0, 3, 0, 0).changed);
    // only the tabs of the range
    QCOMPARE(textUntabify("\ta\tb", 2, 3, 0, 0).text, QString("\ta   b"));
    // the cursor after a tab stays after it
    QCOMPARE(textUntabify("\tx", 0, 2, 1, 1).cursor, 4);
}

// EDIT-INDENT-3: the indent points of the non-blank line above
void TestIndent::test_points()
{
    QCOMPARE(textIndentPoints("- a long item\n\n- b", 2), QVector<int>({ 0, 2, 4, 9 }));
    QCOMPARE(textIndentPoints("  aaa\tbb\nx", 1), QVector<int>({ 2, 8 }));
    QCOMPARE(textIndentPoints("x", 0), QVector<int>());
}

// EDIT-INDENT-1..3: the keys in a tab of each kind
void TestIndent::test_keys_data()
{
    QTest::addColumn<QString>("mode");
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("keys");
    QTest::addColumn<QString>("result");
    QString m(CURSOR_MARK);
    QString r(KEY_RETURN);
    QString t(KEY_TAB);
    QString b(KEY_BACKTAB);

    QTest::newRow("return in a block") << "cpp" << "void f() {" + m << r << "void f() {\n    " + m;
    QTest::newRow("return before a closer") << "cpp" << "void f() {" + m + "}" << r << "void f() {\n" + m + "}";
    QTest::newRow("return trailing spaces") << "cpp" << "{\n    x();   " + m + "\n}" << r << "{\n    x();\n    " + m + "\n}";
    QTest::newRow("return blank line") << "cpp" << "{\n    " + m + "\n}" << r << "{\n\n    " + m + "\n}";
    QTest::newRow("return splits") << "cpp" << "foo(a, " + m + "  b);" << r << "foo(a,\n    " + m + "b);";
    QTest::newRow("return in comment") << "cpp" << "/* one\n   two" + m << r << "/* one\n   two\n   " + m;
    QTest::newRow("tab anywhere") << "cpp" << "{\nfo" + m + "o();\n}" << t << "{\n    fo" + m + "o();\n}";
    QTest::newRow("tab in whitespace") << "cpp" << "{\n " + m + " foo();\n}" << t << "{\n    " + m + "foo();\n}";
    QTest::newRow("tab tabs to spaces") << "cpp" << "{\n\t" + m + "foo();\n}" << t << "{\n    " + m + "foo();\n}";
    QTest::newRow("shift tab") << "cpp" << "{\n    fo" + m + "o();\n}" << b << "{\nfo" + m + "o();\n}";
    QTest::newRow("electric brace") << "cpp" << "{\n    x();\n    " + m << "}" << "{\n    x();\n}" + m;
    QTest::newRow("electric open brace") << "cpp" << "if (a)\n    " + m << "{" << "if (a)\n{" + m;
    QTest::newRow("electric label") << "cpp" << "switch (a) {\n    case 1" + m << ":" << "switch (a) {\ncase 1:" + m;
    QTest::newRow("electric directive") << "cpp" << "{\n    " + m << "#" << "{\n#" + m;
    QTest::newRow("no electric scope") << "cpp" << "{\n    a" + m << "::b" << "{\n    a::b" + m;
    QTest::newRow("typed function") << "cpp" << m << "void f() {" + r + "if (a) {" + r + "x();" + r + "}" + r + "}"
        << "void f() {\n    if (a) {\n        x();\n    }\n}" + m;
    QTest::newRow("python colon") << "python" << "if a:" + m << r << "if a:\n    " + m;
    QTest::newRow("python return") << "python" << "def f():\n    return 1" + m << r << "def f():\n    return 1\n" + m;
    QTest::newRow("python dedenter") << "python" << "if a:\n    x = 1\n    else" + m << ":"
        << "if a:\n    x = 1\nelse:" + m;
    QTest::newRow("python cycle") << "python" << "def f():\n    if a:\n        x = 1\n        " + m + "y"
        << t + t << "def f():\n    if a:\n        x = 1\n    " + m + "y";
    QTest::newRow("python cycle back") << "python" << "def f():\n    if a:\n        x = 1\n        " + m + "y"
        << t + t + t + t << "def f():\n    if a:\n        x = 1\n        " + m + "y";
    QTest::newRow("json return") << "json" << "{" + m << r << "{\n    " + m;
    QTest::newRow("json closer") << "json" << "[\n    1,\n    " + m << "]" << "[\n    1,\n]" + m;
    QTest::newRow("markdown item") << MODE_MARKDOWN << "- item" + m << r << "- item\n  " + m;
    QTest::newRow("markdown numbered") << MODE_MARKDOWN << "10. item" + m << r << "10. item\n    " + m;
    QTest::newRow("markdown quote") << MODE_MARKDOWN << "> quoted" + m << r << "> quoted\n" + m;
    QTest::newRow("markdown hard break") << MODE_MARKDOWN << "two  " + m << r << "two  \n" + m;
    QTest::newRow("markdown blank") << MODE_MARKDOWN << "- a\n  " + m << r << "- a\n\n  " + m;
    QTest::newRow("markdown nest") << MODE_MARKDOWN << "- a\n" + m + "- b" << t << "- a\n  " + m + "- b";
    QTest::newRow("markdown unnest") << MODE_MARKDOWN << "- a\n  " + m + "- b" << b << "- a\n" + m + "- b";
    QTest::newRow("plain return") << MODE_TEXT << "  indented" + m << r << "  indented\n  " + m;
    QTest::newRow("plain hanging") << MODE_TEXT << "first" + m + "\n    second" << r << "first\n    " + m + "\n    second";
    QTest::newRow("plain align") << MODE_TEXT << "aaa bbb\nx" + m << t << "aaa bbb\nx   " + m;
    QTest::newRow("plain past points") << MODE_TEXT << "aaa\n    " + m + "b" << t << "aaa\n        " + m + "b";
    QTest::newRow("plain back") << MODE_TEXT << "aaa bbb\n      " + m + "c" << b << "aaa bbb\n    " + m + "c";
}

void TestIndent::test_keys()
{
    QFETCH(QString, mode);
    QFETCH(QString, text);
    QFETCH(QString, keys);
    QFETCH(QString, result);
    TextDocument doc(mode);
    int cursor = 0;
    doc.setText(unmarked(text, &cursor));
    placeCursor(doc, cursor);
    press(doc.editor(), keys);
    QCOMPARE(marked(doc), result);
}

// EDIT-INDENT-1/3: only spaces and tabs are inserted, the other characters as typed
void TestIndent::test_only_whitespace()
{
    const QString typed = "void f() {\nif (a) {\nx = \"{\";\n}\n/* c\n*/\n}\n";
    for (const QString& mode : QStringList({ "cpp", "python", "json", MODE_MARKDOWN, MODE_TEXT })) {
        TextDocument doc(mode);
        press(doc.editor(), QString(typed).replace('\n', KEY_RETURN));
        QString written = doc.text();
        written.remove(' ');
        written.remove('\t');
        QString expected = typed;
        expected.remove(' ');
        QCOMPARE(written, expected);
    }
}

// EDIT-INDENT-2/4: one step each, Undo puts the text and the cursor back; no step for no edit
void TestIndent::test_undo()
{
    TextDocument doc("cpp");
    doc.setText("{\nfoo();\n}");
    placeCursor(doc, 4);
    int steps = doc.undoStack()->count();
    QTest::keyClick(doc.editor(), Qt::Key_Tab);
    QCOMPARE(doc.text(), QString("{\n    foo();\n}"));
    QCOMPARE(doc.undoStack()->count(), steps + 1);
    QCOMPARE(doc.undoStack()->undoText(), QString("indent"));
    // a line at its column is no step
    QTest::keyClick(doc.editor(), Qt::Key_Tab);
    QCOMPARE(doc.undoStack()->count(), steps + 1);
    doc.undoStack()->undo();
    QCOMPARE(doc.text(), QString("{\nfoo();\n}"));
    QCOMPARE(doc.editor()->textCursor().position(), 4);

    // nothing to move left: no step, the redo stays
    QTest::keyClick(doc.editor(), Qt::Key_Backtab, Qt::ShiftModifier);
    QVERIFY(doc.undoStack()->canRedo());

    // Return and its indent are one step
    TextDocument ret("cpp");
    ret.setText("{");
    placeCursor(ret, 1);
    int before = ret.undoStack()->count();
    QTest::keyClick(ret.editor(), Qt::Key_Return);
    QCOMPARE(ret.text(), QString("{\n    "));
    QCOMPARE(ret.undoStack()->count(), before + 1);
    ret.undoStack()->undo();
    QCOMPARE(ret.text(), QString("{"));
    QCOMPARE(ret.editor()->textCursor().position(), 1);

    // an electric re-indent goes with its character
    TextDocument brace("cpp");
    brace.setText("{\n    x();\n    ");
    placeCursor(brace, brace.text().length());
    QTest::keyClicks(brace.editor(), "}");
    QCOMPARE(brace.text(), QString("{\n    x();\n}"));
    brace.undoStack()->undo();
    QCOMPARE(brace.text(), QString("{\n    x();\n    "));
}

// EDIT-INDENT-4: nothing is indented in a read-only text
void TestIndent::test_read_only()
{
    TextDocument doc("cpp");
    doc.setText("{\nfoo();\n}");
    doc.setReadOnlyFlag(true);
    placeCursor(doc, 4);
    QTest::keyClick(doc.editor(), Qt::Key_Tab);
    QTest::keyClick(doc.editor(), Qt::Key_Return);
    QCOMPARE(doc.text(), QString("{\nfoo();\n}"));
    QVERIFY(!doc.canIndent());
    QVERIFY(!doc.canUntabify());
    QVERIFY(!doc.indentText(false));
}

// EDIT-INDENT-7: with the switch off Return breaks plainly, nothing is electric, Tab inserts
// the whitespace to the next stop, Shift+Tab stays
void TestIndent::test_switch_off()
{
    SettingsManager& sm = SettingsManager::instance();
    sm.setAutoIndent(false);
    TextDocument doc("cpp");
    doc.setText("{");
    placeCursor(doc, 1);
    QTest::keyClick(doc.editor(), Qt::Key_Return);
    QCOMPARE(doc.text(), QString("{\n"));
    QTest::keyClicks(doc.editor(), "ab");
    QTest::keyClick(doc.editor(), Qt::Key_Tab);
    QCOMPARE(doc.text(), QString("{\nab  "));
    sm.setIndentTabs(true);
    QTest::keyClick(doc.editor(), Qt::Key_Tab);
    QCOMPARE(doc.text(), QString("{\nab  \t"));
    QTest::keyClicks(doc.editor(), "}");
    QCOMPARE(doc.text(), QString("{\nab  \t}"));
    sm.setIndentTabs(false);
    doc.setText("        x");
    placeCursor(doc, 9);
    QTest::keyClick(doc.editor(), Qt::Key_Backtab, Qt::ShiftModifier);
    QCOMPARE(doc.text(), QString("    x"));
    // Indent Region stays
    QVERIFY(doc.canIndent());
    doc.setText("{\nx;\n}");
    QVERIFY(doc.indentText(false));
    QCOMPARE(doc.text(), QString("{\n    x;\n}"));
}

// EDIT-INDENT-6/9: Indent Region and Convert Tabs to Spaces of a text tab: the selection, the
// whole text, the lines; one named step; the modes
void TestIndent::test_document()
{
    TextDocument doc("cpp");
    doc.setText("{\nx();\ny();\nz();\n}");
    QVERIFY(doc.canIndent());
    QTextCursor cursor = doc.editor()->textCursor();
    cursor.setPosition(2);
    cursor.setPosition(8, QTextCursor::KeepAnchor);
    doc.editor()->setTextCursor(cursor);
    QVERIFY(doc.indentText(false));
    QCOMPARE(doc.text(), QString("{\n    x();\n    y();\nz();\n}"));
    QCOMPARE(doc.undoStack()->undoText(), QString("indent region"));
    // the selection kept at its characters
    QCOMPARE(doc.editor()->textCursor().anchor(), 2);
    QCOMPARE(doc.editor()->textCursor().position(), 16);
    QVERIFY(doc.indentLines(4, 4, false));
    QCOMPARE(doc.text(), QString("{\n    x();\n    y();\n    z();\n}"));
    QVERIFY(!doc.indentLines(4, 9, false));
    // a Tab with a selection re-indents its lines
    doc.setText("{\nx();\ny();\n}");
    cursor = doc.editor()->textCursor();
    cursor.setPosition(2);
    cursor.setPosition(9, QTextCursor::KeepAnchor);
    doc.editor()->setTextCursor(cursor);
    QTest::keyClick(doc.editor(), Qt::Key_Tab);
    QCOMPARE(doc.text(), QString("{\n    x();\n    y();\n}"));

    TextDocument tabs(MODE_MARKDOWN);
    QVERIFY(!tabs.canIndent());
    QVERIFY(tabs.canUntabify());
    tabs.setText("a\tb\n\tc");
    QVERIFY(tabs.indentText(true));
    QCOMPARE(tabs.text(), QString("a   b\n    c"));
    QCOMPARE(tabs.undoStack()->undoText(), QString("convert tabs"));
    tabs.setText("\ta\n\tb");
    QVERIFY(tabs.indentLines(2, 2, true));
    QCOMPARE(tabs.text(), QString("\ta\n    b"));
    // a Markdown selection shifts by a width
    tabs.setText("a\nb");
    cursor = tabs.editor()->textCursor();
    cursor.setPosition(0);
    cursor.setPosition(3, QTextCursor::KeepAnchor);
    tabs.editor()->setTextCursor(cursor);
    QTest::keyClick(tabs.editor(), Qt::Key_Tab);
    QCOMPARE(tabs.text(), QString("    a\n    b"));
    QTest::keyClick(tabs.editor(), Qt::Key_Backtab, Qt::ShiftModifier);
    QCOMPARE(tabs.text(), QString("a\nb"));
}

// EDIT-INDENT-8: the code dialogs indent by the document language, the header no code; the
// other dialogs do not
void TestIndent::test_dialogs()
{
    const CodeLanguage* cpp = CodeStyle::instance().findId("cpp");
    StateActionDialog entry("entry", cpp);
    QPlainTextEdit* edit = entry.findChild<QPlainTextEdit*>();
    QVERIFY(edit);
    edit->setPlainText("entry/");
    edit->moveCursor(QTextCursor::End);
    QTest::keyClick(edit, Qt::Key_Return);
    QTest::keyClicks(edit, "if (a) {");
    QTest::keyClick(edit, Qt::Key_Return);
    QTest::keyClicks(edit, "x();");
    QTest::keyClick(edit, Qt::Key_Return);
    QTest::keyClicks(edit, "}");
    QCOMPARE(edit->toPlainText(), QString("entry/\nif (a) {\n    x();\n}"));
    // the header stays
    edit->moveCursor(QTextCursor::Start);
    QTest::keyClick(edit, Qt::Key_Tab);
    QCOMPARE(edit->toPlainText(), QString("entry/\nif (a) {\n    x();\n}"));

    StateActionDialog transition(StateActionDialog::Mode::Transition, cpp);
    edit = transition.findChild<QPlainTextEdit*>();
    edit->setPlainText("TICK [a > 1] / foo(a,");
    edit->moveCursor(QTextCursor::End);
    QTest::keyClick(edit, Qt::Key_Return);
    QCOMPARE(edit->toPlainText(), QString("TICK [a > 1] / foo(a,\n                   "));

    const CodeLanguage* python = CodeStyle::instance().findId("python");
    MultilineTextDialog formal("Edit text", "Body", "if a:", codeRoleFormalComment, python);
    edit = formal.findChild<QPlainTextEdit*>();
    QTest::keyClick(edit, Qt::Key_Return);
    QCOMPARE(edit->toPlainText(), QString("if a:\n    "));

    MultilineTextDialog informal("Edit text", "Body", "- item", codeRoleMarkup, nullptr, nullptr,
                                 codeMarkupMarkdown, autoreplaceMarkdown);
    edit = informal.findChild<QPlainTextEdit*>();
    QTest::keyClick(edit, Qt::Key_Return);
    QCOMPARE(edit->toPlainText(), QString("- item\n"));
}

// EDIT-INDENT-6/7: the Edit entries, the key of Indent Region, the enabling per tab, Indent
// Automatically the setting both ways
void TestIndent::test_actions()
{
    SettingsManager& sm = SettingsManager::instance();
    CyberiadaSMEditorWindow window;
    QList<QAction*> edit = window.menuEdit->actions();
    QVERIFY(edit.indexOf(window.actionFillAsYouType) < edit.indexOf(window.actionIndentRegion));
    QVERIFY(edit.indexOf(window.actionIndentRegion) < edit.indexOf(window.actionUntabify));
    QVERIFY(edit.indexOf(window.actionUntabify) < edit.indexOf(window.actionAutoIndent));
    QVERIFY(edit.indexOf(window.actionAutoIndent) < edit.indexOf(window.actionInsertImage));
    QCOMPARE(window.actionIndentRegion->shortcuts(), QList<QKeySequence>() << QKeySequence("Ctrl+Alt+\\"));
    QVERIFY(window.actionAutoIndent->isCheckable() && window.actionAutoIndent->isChecked());
    QVERIFY(!window.actionIndentRegion->isEnabled());
    QVERIFY(!window.actionUntabify->isEnabled());
    QVERIFY(!window.actionAutoIndent->isEnabled());

    QVERIFY(window.openFile("texts/blink.cpp"));
    QVERIFY(window.actionIndentRegion->isEnabled());
    QVERIFY(window.actionUntabify->isEnabled());
    QVERIFY(window.actionAutoIndent->isEnabled());
    window.actionReadOnly->trigger();
    QVERIFY(!window.actionIndentRegion->isEnabled());
    QVERIFY(!window.actionUntabify->isEnabled());
    window.actionReadOnly->trigger();
    TextDocument* text = window.currentText();
    QString before = text->text();
    text->setText("{\nx;\n}");
    window.actionIndentRegion->trigger();
    QCOMPARE(text->text(), QString("{\n    x;\n}"));
    text->setText("\tx;");
    window.actionUntabify->trigger();
    QCOMPARE(text->text(), QString("    x;"));
    Q_UNUSED(before);

    window.setCurrentDocument(window.newTextDocument(MODE_MARKDOWN));
    QVERIFY(!window.actionIndentRegion->isEnabled());
    QVERIFY(window.actionUntabify->isEnabled());

    window.actionAutoIndent->trigger();
    QVERIFY(!sm.getAutoIndent());
    sm.setAutoIndent(true);
    QVERIFY(window.actionAutoIndent->isChecked());
}

// EDIT-INDENT-7: the row of the Text editors page, its range, Apply storing it, Reset
void TestIndent::test_preferences()
{
    SettingsManager& sm = SettingsManager::instance();
    PreferencesDialog dialog;
    TextSchemePanel* panel = dialog.textSchemePanel();
    QSpinBox* width = panel->findChild<QSpinBox*>("textEditorIndentWidth");
    QComboBox* with = panel->findChild<QComboBox*>("textEditorIndentWith");
    QCheckBox* automatic = panel->findChild<QCheckBox*>("textEditorAutoIndent");
    QVERIFY(width && with && automatic);
    QCOMPARE(width, panel->indentWidthBox());
    QCOMPARE(with, panel->indentWithBox());
    QCOMPARE(automatic, panel->autoIndentBox());
    QCOMPARE(width->minimum(), INDENT_WIDTH_MIN);
    QCOMPARE(width->maximum(), INDENT_WIDTH_MAX);
    QCOMPARE(width->value(), INDENT_WIDTH_DEFAULT);
    QCOMPARE(with->currentData().toBool(), false);
    QVERIFY(automatic->isChecked());
    width->setValue(2);
    with->setCurrentIndex(with->findData(true));
    automatic->setChecked(false);
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    QCOMPARE(sm.getIndentWidth(), 2);
    QVERIFY(sm.getIndentTabs());
    QVERIFY(!sm.getAutoIndent());
    QSettings stored;
    QCOMPARE(stored.value("editor/indentWidth").toInt(), 2);
    QCOMPARE(stored.value("editor/indentWith").toString(), QString(EDITOR_INDENT_TABS));
    QVERIFY(!stored.value("editor/autoIndent").toBool());
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Reset)->click();
    QCOMPARE(sm.getIndentWidth(), INDENT_WIDTH_DEFAULT);
    QVERIFY(!sm.getIndentTabs());
    QVERIFY(sm.getAutoIndent());
    QCOMPARE(width->value(), INDENT_WIDTH_DEFAULT);
    QVERIFY(automatic->isChecked());
    sm.setIndentWidth(INDENT_WIDTH_MAX * 2);
    QCOMPARE(sm.getIndentWidth(), INDENT_WIDTH_MAX);
}

QTEST_MAIN(TestIndent)

#include "l4-indent.moc"
