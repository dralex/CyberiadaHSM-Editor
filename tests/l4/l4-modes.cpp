/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process text tab test (see docs/TESTING.md, L4)
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
#include <QStackedWidget>
#include <QSyntaxHighlighter>
#include <QTabBar>
#include <QTextBrowser>
#include "smeditor_window.h"
#include "code_editor.h"
#include "code_highlighter.h"
#include "text_document.h"

// the EDIT-MODE laws of the text tabs at the widget level: the modes, the page,
// the window parts that follow, the undo, the save and the rendered Markdown
class TestModes: public QObject {
	Q_OBJECT

private slots:
	void test_extension_modes();
	void test_open_text();
	void test_untitled_gives_way();
	void test_text_undo_save();
	void test_text_modes();
	void test_markdown_rendered();
	void test_back_to_diagram();
};

// EDIT-MODE-13: the extension names the mode, the mode its extension
void TestModes::test_extension_modes()
{
	QCOMPARE(modeForFile("a.graphml"), QString(MODE_HSM));
	QCOMPARE(modeForFile("blink.cpp"), QString("cpp"));
	QCOMPARE(modeForFile("blink.INO"), QString("cpp"));
	QCOMPARE(modeForFile("main.c"), QString("c"));
	QCOMPARE(modeForFile("App.cs"), QString("csharp"));
	QCOMPARE(modeForFile("run.py"), QString("python"));
	QCOMPARE(modeForFile("README.md"), QString(MODE_MARKDOWN));
	QCOMPARE(modeForFile("notes.txt"), QString(MODE_TEXT));
	QCOMPARE(modeForFile("Main.java"), QString(MODE_TEXT));
	QCOMPARE(modeExtension("cpp"), QString(".cpp"));
	QCOMPARE(modeExtension(MODE_MARKDOWN), QString(".md"));
	QCOMPARE(modeExtension(MODE_TEXT), QString(".txt"));
	QVERIFY(isTextMode("python") && isTextMode(MODE_TEXT) && !isTextMode(MODE_FA));
	QCOMPARE(modeMarker("cpp", false, false), QString(" [C++]"));
	QCOMPARE(modeMarker(MODE_MARKDOWN, true, false), QString(" [Markdown, ro]"));
}

// EDIT-MODE-13/15: a source file is a text tab of its language; the canvas parts go
void TestModes::test_open_text()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("diagrams/hierarchy.graphml"));
	QVERIFY(window.openFile("texts/blink.cpp"));
	QCOMPARE(window.documentCount(), 2);
	TextDocument* text = window.currentText();
	QVERIFY(text && !window.currentDocument());
	QCOMPARE(text->mode(), QString("cpp"));
	QCOMPARE(window.tabBar()->tabText(1), QString("blink.cpp [C++]"));
	QVERIFY(window.windowTitle().startsWith("blink.cpp [C++]"));
	QVERIFY(text->text().contains("digitalWrite"));
	QVERIFY(text->isClean());
	QVERIFY(text->editor()->document()->findChild<QSyntaxHighlighter*>());
	// the page is the editor; no tree, no element tools, no image
	QCOMPARE(window.documentStack->currentWidget(), text->page());
	QVERIFY(!window.SMView->model());
	QVERIFY(!window.actionNewState->isEnabled() && !window.actionExport->isEnabled());
	QVERIFY(!window.actionZoomIn->isEnabled() && !window.actionReconstructGeometry->isEnabled());
	QVERIFY(!window.menuLanguage->menuAction()->isEnabled());
	QVERIFY(!window.getModel() && !window.getScene() && !window.sceneView);
	// the same file again activates its tab
	window.setCurrentDocument(window.tabAt(0));
	QVERIFY(window.openFile("texts/blink.cpp"));
	QCOMPARE(window.documentCount(), 2);
	QCOMPARE(window.currentTab(), (EditorDocument*)text);
}

// EDIT-DOC-2: a clean untitled tab gives way to an opened text file
void TestModes::test_untitled_gives_way()
{
	CyberiadaSMEditorWindow window;
	QCOMPARE(window.documentCount(), 1);
	QVERIFY(window.openFile("texts/notes.md"));
	QCOMPARE(window.documentCount(), 1);
	QCOMPARE(window.currentTab()->mode(), QString(MODE_MARKDOWN));
	// a text tab with a file does not give way to a diagram
	QVERIFY(window.openFile("diagrams/hierarchy.graphml"));
	QCOMPARE(window.documentCount(), 2);
	QCOMPARE(window.currentTab()->mode(), QString(MODE_HSM));
}

// EDIT-MODE-13: the edits go through the window's Undo and Redo; Save As writes UTF-8
void TestModes::test_text_undo_save()
{
	CyberiadaSMEditorWindow window;
	QAction* python = nullptr;
	for (QAction* a : window.menuNew->findChild<QMenu*>("menuNewText")->actions()) {
		if (a->data().toString() == "python") python = a;
	}
	QVERIFY(python);
	python->trigger();
	TextDocument* text = window.currentText();
	QVERIFY(text && text->mode() == "python" && text->isClean());
	QCOMPARE(window.tabBar()->tabText(1), tr("untitled") + " [Python]");
	text->editor()->insertPlainText(QString::fromUtf8("print(\"привет\")\n"));
	QVERIFY(!text->isClean());
	QCOMPARE(window.tabBar()->tabText(1), tr("untitled") + " [Python]*");
	QVERIFY(window.actionUndo->isEnabled() && window.actionSave->isEnabled());
	window.actionUndo->trigger();
	QVERIFY(text->text().isEmpty());
	QVERIFY(text->isClean());
	window.actionRedo->trigger();
	QVERIFY(text->text().startsWith("print"));
	QTemporaryDir dir;
	QString path = dir.filePath("hello.py");
	QString error;
	QVERIFY2(text->saveAs(path, &error), qPrintable(error));
	QVERIFY(text->isClean() && !window.actionSave->isEnabled());
	QCOMPARE(window.tabBar()->tabText(1), QString("hello.py [Python]"));
	QFile file(path);
	QVERIFY(file.open(QIODevice::ReadOnly));
	QCOMPARE(QString::fromUtf8(file.readAll()), QString::fromUtf8("print(\"привет\")\n"));
	// a read-only text takes no input; the flag is the tab's
	window.actionReadOnly->trigger();
	QVERIFY(text->editor()->isReadOnly() && !window.actionPaste->isEnabled());
	QCOMPARE(window.tabBar()->tabText(1), QString("hello.py [Python, ro]"));
	QVERIFY(!window.actionUndo->isEnabled());
}

// EDIT-MODE-3/13: a text switches between the text modes only
void TestModes::test_text_modes()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("texts/blink.cpp"));
	TextDocument* text = window.currentText();
	QString before = text->text();
	QVERIFY(window.setDocumentMode(text, MODE_MARKDOWN));
	QCOMPARE(text->mode(), QString(MODE_MARKDOWN));
	QCOMPARE(text->text(), before);
	QVERIFY(text->isClean());
	QCOMPARE(window.tabBar()->tabText(0), QString("blink.cpp [Markdown]"));
	QString error;
	QVERIFY(!window.setDocumentMode(text, MODE_HSM, &error));
	QVERIFY(!error.isEmpty());
	QVERIFY(!window.actionModeHSM->isEnabled() && !window.actionModeFA->isEnabled());
	QVERIFY(window.setDocumentMode(text, MODE_TEXT));
	QCOMPARE(window.tabBar()->tabText(0), QString("blink.cpp [text]"));
	QVERIFY(!text->editor()->document()->findChild<QSyntaxHighlighter*>());
	// a diagram cannot become a text
	window.actionNew->trigger();
	QVERIFY(!window.setDocumentMode(window.currentTab(), "cpp"));
}

// EDIT-MODE-14: a read-only Markdown tab shows the rendered document
void TestModes::test_markdown_rendered()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("texts/notes.md"));
	TextDocument* text = window.currentText();
	QStackedWidget* page = qobject_cast<QStackedWidget*>(text->page());
	QVERIFY(page);
	QCOMPARE(page->currentWidget(), (QWidget*)text->editor());
	window.setDocumentReadOnly(text, true);
	QVERIFY(text->showsRendered());
	QTextBrowser* rendered = qobject_cast<QTextBrowser*>(page->currentWidget());
	QVERIFY(rendered);
	QVERIFY(!rendered->toPlainText().contains("**"));
	QVERIFY(rendered->toPlainText().contains("The traffic light"));
	window.setDocumentReadOnly(text, false);
	QCOMPARE(page->currentWidget(), (QWidget*)text->editor());
}

// EDIT-MODE-15: a diagram tab gets its canvas parts back
void TestModes::test_back_to_diagram()
{
	CyberiadaSMEditorWindow window;
	window.show();
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	QVERIFY(window.openFile("diagrams/hierarchy.graphml"));
	QVERIFY(!window.elementToolBar->isHidden());
	QVERIFY(window.openFile("texts/counter.py"));
	QVERIFY(window.elementToolBar->isHidden());
	window.setCurrentDocument(window.tabAt(0));
	QVERIFY(!window.elementToolBar->isHidden());
	QVERIFY(window.SMView->model() == window.getModel());
	QVERIFY(window.actionNewState->isEnabled() && window.actionZoomIn->isEnabled());
	QVERIFY(window.sceneView == window.currentDocument()->view());
	// closing the text tab leaves the diagram as it was
	QVERIFY(window.closeDocument(window.tabAt(1)));
	QCOMPARE(window.documentCount(), 1);
}

QTEST_MAIN(TestModes)
#include "l4-modes.moc"
