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
#include <QTreeWidget>
#include "smeditor_window.h"
#include "code_editor.h"
#include "code_highlighter.h"
#include "text_document.h"
#include "diff_document.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_editor_items.h"

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
	void test_read_only_button();
	void test_new_markdown_button();
	void test_back_to_diagram();
	void test_compare_saved();
	void test_compare_refresh();
	void test_compare_tab();

private:
	static CyberiadaSMEditorAbstractItem::RunMark mark(CyberiadaSMEditorScene* scene, const char* id);
	static const DiffRow* row(DiffDocument* diff, const QString& first, const QString& second);
};

CyberiadaSMEditorAbstractItem::RunMark TestModes::mark(CyberiadaSMEditorScene* scene, const char* id)
{
	CyberiadaSMEditorAbstractItem* item = dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene->getMap().value(id));
	return item ? item->runMark() : CyberiadaSMEditorAbstractItem::runMarkNone;
}

const DiffRow* TestModes::row(DiffDocument* diff, const QString& first, const QString& second)
{
	for (const DiffRow& r : diff->rows()) {
		if (r.first == first && r.second == second) return &r;
	}
	return nullptr;
}

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

// EDIT-MODE-4/15: the toolbar button with the inspection icon flips the read-only flag, the
// inspection is left to the Mode menu; a text tab greys the switches of the canvas view
void TestModes::test_read_only_button()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.mainToolBar->actions().contains(window.actionReadOnly));
	QVERIFY(!window.mainToolBar->actions().contains(window.actionModeInspection));
	QVERIFY(!window.actionReadOnly->icon().isNull());
	const QList<QAction*> switches = QList<QAction*>()
		<< window.actionGridVisibility << window.actionSnapMode << window.actionServiceObjects
		<< window.actionTransitionText << window.actionFASymbols << window.actionComponentComments;
	QVERIFY(window.openFile("diagrams/hierarchy.graphml"));
	for (QAction* a : switches) QVERIFY2(a->isEnabled(), qPrintable(a->text()));
	QVERIFY(window.openFile("texts/notes.md"));
	TextDocument* text = window.currentText();
	for (QAction* a : switches) QVERIFY2(!a->isEnabled(), qPrintable(a->text()));
	window.actionReadOnly->trigger();
	QVERIFY(text->showsRendered());
	QVERIFY(window.actionReadOnly->isChecked());
	window.actionReadOnly->trigger();
	QVERIFY(!text->showsRendered());
	window.setCurrentDocument(window.tabAt(0));
	for (QAction* a : switches) QVERIFY2(a->isEnabled(), qPrintable(a->text()));
}

// EDIT-MODE-13: the toolbar button after New HSM diagram opens an untitled Markdown text; it
// is the Markdown entry of File > New > Text file
void TestModes::test_new_markdown_button()
{
	CyberiadaSMEditorWindow window;
	QList<QAction*> tools = window.mainToolBar->actions();
	QCOMPARE(tools.indexOf(window.actionNewMarkdown), tools.indexOf(window.actionNew) + 1);
	QVERIFY(!window.actionNewMarkdown->icon().isNull());
	QVERIFY(window.menuNew->findChild<QMenu*>("menuNewText")->actions().contains(window.actionNewMarkdown));
	window.actionNewMarkdown->trigger();
	TextDocument* text = window.currentText();
	QVERIFY(text && text->mode() == "markdown" && text->isUntitled());
	QCOMPARE(window.tabBar()->tabText(window.tabBar()->currentIndex()), tr("untitled") + " [Markdown]");
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

// EDIT-MODE-10/11: a diff tab of an edited diagram against its saved file
void TestModes::test_compare_saved()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("diagrams/geometry.graphml"));
	CyberiadaSMEditorDocument* source = window.currentDocument();
	CyberiadaSMModel* m = source->model();
	QVERIFY(m->updateTitle(m->elementToIndex(m->idToElement("node-0-0-1")), "Idle"));
	QVERIFY(window.actionCompare->isEnabled() && !window.actionRefresh->isEnabled());
	DiffOrigin saved;
	saved.kind = DiffOrigin::originSaved;
	saved.tab = source;
	saved.path = source->filePath();
	saved.label = "saved";
	QString error;
	DiffDocument* diff = window.compareDocument(source, saved, &error);
	QVERIFY2(diff, qPrintable(error));
	QCOMPARE(window.documentCount(), 2);
	QCOMPARE(window.currentDiff(), diff);
	QCOMPARE(diff->mode(), QString(MODE_DIFF));
	QCOMPARE(diff->title(), QString::fromUtf8("geometry.graphml ↔ saved"));
	QCOMPARE(window.tabBar()->tabText(1), QString::fromUtf8("geometry.graphml ↔ saved [diff]"));
	// the rename is a changed title, marked on both sides
	const DiffRow* renamed = row(diff, "node-0-0-1", "node-0-0-1");
	QVERIFY(renamed);
	QCOMPARE(renamed->change, QString("title"));
	QCOMPARE(mark(diff->baseScene(), "node-0-0-1"), CyberiadaSMEditorAbstractItem::runMarkChanged);
	QCOMPARE(mark(diff->targetScene(), "node-0-0-1"), CyberiadaSMEditorAbstractItem::runMarkChanged);
	QCOMPARE(mark(diff->targetScene(), "node-0-1"), CyberiadaSMEditorAbstractItem::runMarkNone);
	// both sides read-only and as stored
	QVERIFY(diff->isReadOnly() && diff->baseModel()->readOnly() && diff->targetModel()->inspected());
	QVERIFY(window.actionReadOnly->isChecked() && !window.actionReadOnly->isEnabled());
	QVERIFY(!window.actionSave->isEnabled() && !window.actionSaveAs->isEnabled() &&
			!window.actionExport->isEnabled() && !window.actionCompare->isEnabled());
	QVERIFY(window.actionZoomIn->isEnabled() && window.actionGeometry->isEnabled() &&
			window.actionRefresh->isEnabled());
	QVERIFY(window.actionModeDiff->isChecked() && !window.actionModeHSM->isEnabled());
	QVERIFY(!window.setDocumentMode(diff, MODE_HSM));
	// the change list stands in the structure tree dock
	QVERIFY(window.changeList()->isVisibleTo(&window) || !window.changeList()->isHidden());
	QVERIFY(window.SMView->isHidden());
	QVERIFY(window.changeList()->topLevelItemCount() > 0);
	diff->showElement("node-0-0-1", "node-0-0-1");
	QVERIFY(diff->baseScene()->getMap().value("node-0-0-1")->isSelected());
	QVERIFY(diff->targetScene()->getMap().value("node-0-0-1")->isSelected());
	// the geometry, the ids and the view state with Document > Geometry only
	int rows = diff->rows().size();
	window.actionGeometry->trigger();
	QVERIFY(diff->geometry());
	QVERIFY(diff->rows().size() >= rows);
	QVERIFY(row(diff, "node-0-0-1", "node-0-0-1")->element.contains("(node-0-0-1)"));
	// a diagram tab gets its tree back
	window.setCurrentDocument(source);
	QVERIFY(!window.SMView->isHidden() && window.changeList()->isHidden());
}

// EDIT-MODE-12: Refresh takes both sides again; a closed source keeps the target
void TestModes::test_compare_refresh()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("diagrams/geometry.graphml"));
	CyberiadaSMEditorDocument* source = window.currentDocument();
	CyberiadaSMModel* m = source->model();
	DiffOrigin saved;
	saved.kind = DiffOrigin::originSaved;
	saved.tab = source;
	saved.path = source->filePath();
	saved.label = "saved";
	DiffDocument* diff = window.compareDocument(source, saved);
	QVERIFY(diff);
	QVERIFY(!row(diff, "node-0-1", "node-0-1"));
	QVERIFY(m->updateTitle(m->elementToIndex(m->idToElement("node-0-1")), "Second"));
	// the tab keeps its snapshot until Refresh
	QVERIFY(!row(diff, "node-0-1", "node-0-1"));
	window.actionRefresh->trigger();
	QVERIFY(row(diff, "node-0-1", "node-0-1"));
	QVERIFY(window.closeDocument(source, true));
	QVERIFY(window.refreshDiff(diff));
	QVERIFY(row(diff, "node-0-1", "node-0-1"));
}

// EDIT-MODE-10: the base may be another tab
void TestModes::test_compare_tab()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("diagrams/hierarchy.graphml"));
	CyberiadaSMEditorDocument* base = window.currentDocument();
	QVERIFY(window.openFile("diagrams/geometry.graphml"));
	DiffOrigin tab;
	tab.kind = DiffOrigin::originTab;
	tab.tab = base;
	tab.label = base->title();
	DiffDocument* diff = window.compareDocument(window.currentDocument(), tab);
	QVERIFY(diff);
	QCOMPARE(diff->title(), QString::fromUtf8("geometry.graphml ↔ hierarchy.graphml"));
	QVERIFY(diff->added() > 0 && diff->removed() > 0);
	QCOMPARE(window.documentCount(), 3);
}

QTEST_MAIN(TestModes)
#include "l4-modes.moc"
