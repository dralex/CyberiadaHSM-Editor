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
#include <QDesktopServices>
#include <QFileDialog>
#include <QMouseEvent>
#include <QHeaderView>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
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
#include "text_diff_document.h"
#include "text_diff_view.h"
#include <QClipboard>
#include <QTextBlock>
#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_editor_items.h"
#include "cyberiadasm_editor_sm_item.h"
#include "editable_text_item.h"
#include "cyberiada_constants.h"
#include "settings_manager.h"

// the EDIT-MODE laws of the text tabs at the widget level: the modes, the page,
// the window parts that follow, the undo, the save and the rendered Markdown
// the URLs the desktop services are asked to open
class UrlRecorder: public QObject {
	Q_OBJECT
public:
	QList<QUrl> urls;
public slots:
	void open(const QUrl& url) { urls << url; }
};

class TestModes: public QObject {
	Q_OBJECT

private slots:
	void test_extension_modes();
	void test_open_text();
	void test_untitled_gives_way();
	void test_text_undo_save();
	void test_json_text();
	void test_text_modes();
	void test_markdown_rendered();
	void test_read_only_button();
	void test_new_markdown_button();
	void test_markdown_export();
	void test_markdown_links();
	void test_back_to_diagram();
	void test_mode_toolbars();
	void test_open_file_mode();
	void test_wrap_action();
	void test_compare_saved();
	void test_compare_refresh();
	void test_compare_tab();
	void test_compare_fields();
	void test_text_compare();

private:
	static QStringList diffTexts(CyberiadaSMEditorScene* scene);
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

// the tinted parts of the drawn texts of a side, sorted
QStringList TestModes::diffTexts(CyberiadaSMEditorScene* scene)
{
	QStringList texts;
	for (QGraphicsItem* item : scene->items()) {
		EditableTextItem* text = dynamic_cast<EditableTextItem*>(item);
		if (!text) continue;
		for (const FindRange& r : text->getDiffRanges()) texts << text->toPlainText().mid(r.from, r.to - r.from);
	}
	texts.sort();
	return texts;
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
	QCOMPARE(modeForFile("plugin.json"), QString("json"));
	QCOMPARE(modeForFile("DARK.JSON"), QString("json"));
	QCOMPARE(modeForFile("README.md"), QString(MODE_MARKDOWN));
	QCOMPARE(modeForFile("notes.txt"), QString(MODE_TEXT));
	QCOMPARE(modeForFile("Main.java"), QString(MODE_TEXT));
	QCOMPARE(modeExtension("cpp"), QString(".cpp"));
	QCOMPARE(modeExtension(MODE_MARKDOWN), QString(".md"));
	QCOMPARE(modeExtension(MODE_TEXT), QString(".txt"));
	QCOMPARE(modeExtension("json"), QString(".json"));
	QVERIFY(isDocumentFile("dark.json"));
	QVERIFY(textFilePatterns().split(' ').contains("*.json"));
	QCOMPARE(modeMarker("json", true, false), QString(" [JSON, ro]"));
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

// EDIT-MODE-13: a .json file opens as JSON; File > New > Text file > JSON
void TestModes::test_json_text()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("texts/settings.json"));
	TextDocument* text = window.currentText();
	QVERIFY(text && text->mode() == "json");
	QCOMPARE(window.tabBar()->tabText(0), QString("settings.json [JSON]"));
	QVERIFY(text->editor()->document()->findChild<QSyntaxHighlighter*>());
	QAction* json = nullptr;
	for (QAction* a : window.menuNew->findChild<QMenu*>("menuNewText")->actions()) {
		if (a->data().toString() == "json") json = a;
	}
	QVERIFY(json);
	QCOMPARE(json->text(), QString("JSON"));
	json->trigger();
	QVERIFY(window.currentText() && window.currentText()->mode() == "json");
	QCOMPARE(window.tabBar()->tabText(1), tr("untitled") + " [JSON]");
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

// EDIT-MODE-15/18: File > Export of a Markdown tab saves its rendered PDF under the name of
// the file; another text tab has nothing to export until it becomes Markdown
void TestModes::test_markdown_links()
{
	QTemporaryDir dir;
	const QStringList names = { "links.md", "pic.png", "other.md" };
	const QList<QByteArray> contents = {
		"# Links\n\nA [site](http://example.org/a) and ![pic](pic.png),\n"
		"[other](other.md), [up](#second-heading).\n\n## Second heading\n",
		QByteArray(), "# Other\n" };
	for (int i = 0; i < names.size(); i++) {
		QFile f(dir.filePath(names[i]));
		QVERIFY(f.open(QIODevice::WriteOnly));
		f.write(contents[i]);
	}
	// nothing leaves the test: the sites and the files are recorded
	UrlRecorder recorder;
	QDesktopServices::setUrlHandler("http", &recorder, "open");
	QDesktopServices::setUrlHandler("file", &recorder, "open");

	CyberiadaSMEditorWindow window;
	window.resize(1000, 600);
	window.show();
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	QVERIFY(window.openFile(dir.filePath("links.md")));
	TextDocument* text = window.currentText();
	QVERIFY(text && !text->showsRendered());
	CodeEditor* editor = text->editor();
	QWidget* viewport = editor->viewport();
	// a point in the second glyph of a part of the text
	auto at = [&](const QString& part) {
		QTextCursor c(editor->document());
		c.setPosition(editor->document()->toPlainText().indexOf(part) + 1);
		QRect gap = editor->cursorRect(c);
		return QPoint(gap.left() + 2, gap.center().y());
	};
	QCOMPARE(editor->linkAt(at("example.org")), QString("http://example.org/a"));
	QVERIFY(editor->linkAt(at("site")).isEmpty());

	// a plain click edits: the cursor there, nothing opened
	QTest::mouseClick(viewport, Qt::LeftButton, Qt::NoModifier, at("example.org"));
	QVERIFY(recorder.urls.isEmpty());
	QCOMPARE(editor->textCursor().position(), editor->document()->toPlainText().indexOf("example.org") + 1);
	// Ctrl+click: the site, the image beside the text
	QTest::mouseClick(viewport, Qt::LeftButton, Qt::ControlModifier, at("example.org"));
	QCOMPARE(recorder.urls, QList<QUrl>() << QUrl("http://example.org/a"));
	QTest::mouseClick(viewport, Qt::LeftButton, Qt::ControlModifier, at("pic.png"));
	QString base = QFileInfo(text->filePath()).absolutePath();
	QCOMPARE(recorder.urls.value(1), QUrl::fromLocalFile(base + "/pic.png"));
	// a #anchor: the cursor on its heading's line
	QTest::mouseClick(viewport, Qt::LeftButton, Qt::ControlModifier, at("#second-heading"));
	QCOMPARE(editor->textCursor().block().text(), QString("## Second heading"));
	// a text the editor opens: its own tab, nothing for the system
	QTest::mouseClick(viewport, Qt::LeftButton, Qt::ControlModifier, at("other.md"));
	QTRY_VERIFY(window.currentText() && window.currentText()->filePath().endsWith("other.md"));
	QCOMPARE(recorder.urls.size(), 2);

	// the hand with Ctrl over a target, the editor's pointer elsewhere
	QVERIFY(window.openFile(dir.filePath("links.md")));
	QCOMPARE(window.currentText(), text);
	Qt::CursorShape shape = viewport->cursor().shape();
	QMouseEvent over(QEvent::MouseMove, at("example.org"), Qt::NoButton, Qt::NoButton, Qt::ControlModifier);
	QApplication::sendEvent(viewport, &over);
	QCOMPARE(viewport->cursor().shape(), Qt::PointingHandCursor);
	QMouseEvent away(QEvent::MouseMove, at("Links"), Qt::NoButton, Qt::NoButton, Qt::ControlModifier);
	QApplication::sendEvent(viewport, &away);
	QCOMPARE(viewport->cursor().shape(), shape);

	QDesktopServices::unsetUrlHandler("http");
	QDesktopServices::unsetUrlHandler("file");
}

void TestModes::test_markdown_export()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("texts/markdown.md"));
	QVERIFY(window.actionExport->isEnabled());
	QTemporaryDir dir;
	QString proposed;
	QStringList filters;
	QTimer::singleShot(50, [&]() {
		QFileDialog* dlg = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
		if (!dlg) return;
		filters = dlg->nameFilters();
		proposed = QFileInfo(dlg->selectedFiles().value(0)).completeBaseName();
		// the proposed name in another directory, the suffix added
		dlg->setDirectory(dir.path());
		QMetaObject::invokeMethod(dlg, "accept");
	});
	window.actionExport->trigger();
	QCOMPARE(filters, QStringList() << "PDF (*.pdf)");
	QCOMPARE(proposed, QString("markdown"));
	QFile file(dir.filePath("markdown.pdf"));
	QVERIFY(file.open(QIODevice::ReadOnly));
	QVERIFY(file.read(4) == "%PDF");

	QVERIFY(window.openFile("texts/blink.cpp"));
	QVERIFY(!window.actionExport->isEnabled());
	QVERIFY(window.setDocumentMode(window.currentText(), MODE_MARKDOWN));
	QVERIFY(window.actionExport->isEnabled());
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

// EDIT-MODE-30: View > Wrap Long Lines, checkable, enabled in a text tab, the setting both ways
void TestModes::test_wrap_action()
{
	SettingsManager& sm = SettingsManager::instance();
	CyberiadaSMEditorWindow window;
	QVERIFY(window.menuView->actions().contains(window.actionWrapLines));
	QVERIFY(window.actionWrapLines->isCheckable() && !window.actionWrapLines->isChecked());
	QVERIFY(!window.actionWrapLines->isEnabled());
	QVERIFY(window.openFile("texts/blink.cpp"));
	QVERIFY(window.actionWrapLines->isEnabled());
	window.actionWrapLines->trigger();
	QVERIFY(sm.getEditorWrap());
	QVERIFY(window.currentText()->editor()->wrapLines());
	sm.setEditorWrap(false);
	QVERIFY(!window.actionWrapLines->isChecked());
	QVERIFY(!window.currentText()->editor()->wrapLines());
}

// EDIT-MODE-13: File > Open shows first the files of the active tab's mode, a text diff's language
void TestModes::test_open_file_mode()
{
	CyberiadaSMEditorWindow window;
	QCOMPARE(window.openFileMode(), QString(MODE_HSM));
	QVERIFY(window.openFile("diagrams/geometry.graphml"));
	QCOMPARE(window.openFileMode(), QString(MODE_HSM));
	QVERIFY(window.openFile("texts/notes.md"));
	QCOMPARE(window.openFileMode(), QString(MODE_MARKDOWN));
	QVERIFY(window.openFile("texts/blink.cpp"));
	QCOMPARE(window.openFileMode(), QString("cpp"));
	QVERIFY(window.openFile("texts/diff-target.py"));
	DiffOrigin base;
	base.kind = DiffOrigin::originFile;
	base.path = "texts/diff-base.py";
	base.label = "diff-base.py";
	QString error;
	QVERIFY2(window.compareText(window.currentText(), base, &error), qPrintable(error));
	QCOMPARE(window.openFileMode(), QString("python"));
}

// EDIT-MD-1: the toolbars follow the tab - Elements for a diagram, Diagram also for a diff,
// Text for a text, none of them for a text diff; a toolbar the user closed stays closed
void TestModes::test_mode_toolbars()
{
	CyberiadaSMEditorWindow window;
	window.show();
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	const QList<QAction*> diagramActions = QList<QAction*>()
		<< window.actionDeleteElement << window.actionReconstructGeometry
		<< window.actionUpdateGeometry << window.actionCenterHorizontally
		<< window.actionCenterVertically << window.actionGridVisibility << window.actionSnapMode
		<< window.actionServiceObjects << window.actionLogSession;
	for (QAction* a : diagramActions) {
		QVERIFY2(window.diagramToolBar->actions().contains(a), qPrintable(a->text()));
		QVERIFY2(!window.mainToolBar->actions().contains(a), qPrintable(a->text()));
	}
	QVERIFY(window.textToolBar->actions().contains(window.actionGoToLine));
	QVERIFY(!window.actionGoToLine->icon().isNull());
	QCOMPARE(window.toolBarArea(window.textToolBar), Qt::LeftToolBarArea);
	auto shown = [&window](bool elements, bool diagram, bool text) {
		return window.elementToolBar->isHidden() == !elements &&
			window.diagramToolBar->isHidden() == !diagram &&
			window.textToolBar->isHidden() == !text && !window.mainToolBar->isHidden();
	};
	QVERIFY(window.openFile("diagrams/geometry.graphml"));
	QVERIFY(shown(true, true, false));
	QVERIFY(window.actionLogSession->isEnabled());
	QVERIFY(window.openFile("texts/notes.md"));
	QVERIFY(shown(false, false, true));
	QVERIFY(window.actionGoToLine->isEnabled());
	// the entries of the hidden toolbars stay in the menus, greyed
	QVERIFY(!window.actionLogSession->isEnabled() && !window.actionGridVisibility->isEnabled());
	window.actionReadOnly->trigger();
	QVERIFY(!window.actionGoToLine->isEnabled());
	window.actionReadOnly->trigger();
	// a diff tab draws diagrams but creates nothing
	window.setCurrentDocument(window.tabAt(0));
	DiffOrigin base;
	base.kind = DiffOrigin::originFile;
	base.path = "diagrams/hierarchy.graphml";
	base.label = "base";
	QString error;
	QVERIFY2(window.compareDocument(window.currentDocument(), base, &error), qPrintable(error));
	QVERIFY(shown(false, true, false));
	// a text diff has nothing to draw and no line to go to
	window.setCurrentDocument(window.tabAt(1));
	DiffOrigin textBase;
	textBase.kind = DiffOrigin::originFile;
	textBase.path = "texts/markdown.md";
	textBase.label = "markdown.md";
	QVERIFY2(window.compareText(window.currentText(), textBase, &error), qPrintable(error));
	QVERIFY(shown(false, false, false));
	// the text toolbar closed by the user stays closed over a switch
	window.setCurrentDocument(window.tabAt(1));
	QVERIFY(shown(false, false, true));
	window.textToolBar->hide();
	window.setCurrentDocument(window.tabAt(0));
	QVERIFY(shown(true, true, false));
	window.setCurrentDocument(window.tabAt(1));
	QVERIFY(shown(false, false, false));
	// and the diagram one too
	window.setCurrentDocument(window.tabAt(0));
	window.diagramToolBar->hide();
	window.setCurrentDocument(window.tabAt(1));
	window.setCurrentDocument(window.tabAt(0));
	QVERIFY(shown(true, false, false));
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
	// the change list in its dock, the tree and the properties hidden (EDIT-UI-6)
	QVERIFY(!window.changesDock->isHidden() && window.changesDock->widget() == window.changeList());
	QVERIFY(window.treeDock->isHidden() && window.propertiesDock->isHidden());
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
	// a diagram tab gets its tree and properties back, the changes go
	window.setCurrentDocument(source);
	QVERIFY(!window.treeDock->isHidden() && !window.propertiesDock->isHidden());
	QVERIFY(window.changesDock->isHidden() && !window.actionChanges->isEnabled());
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

// EDIT-MODE-11: what differs is shown - the machine's border, the changed fields
// tinted on both sides, the change column in view
void TestModes::test_compare_fields()
{
	CyberiadaSMEditorWindow window;
	window.resize(640, 480);
	window.show();
	QVERIFY(window.openFile("diagrams/diff-fields-target.graphml"));
	DiffOrigin base;
	base.kind = DiffOrigin::originFile;
	base.path = "diagrams/diff-fields-base.graphml";
	base.label = "base";
	QString error;
	DiffDocument* diff = window.compareDocument(window.currentDocument(), base, &error);
	QVERIFY2(diff, qPrintable(error));
	QCOMPARE(row(diff, "G0", "G0")->change, QString("title"));
	QCOMPARE(row(diff, "n0-n1", "n0-n1")->change, QString("guard"));
	// the name, the guards, the changed entry and the internal guard; an action
	// without a pair whole; the unchanged exit and the other transition not
	QCOMPARE(diffTexts(diff->baseScene()),
	         QStringList() << "Lamp" << "level() < 10" << "light(1)" << "ready()");
	QCOMPARE(diffTexts(diff->targetScene()),
	         QStringList() << "Lamp 2" << "PRESS / beep()" << "led.level() < 10" << "led.on()" << "led.ready()");
	// the border of the machine is drawn in the changed colour
	CyberiadaSMEditorSMItem* sm = dynamic_cast<CyberiadaSMEditorSMItem*>(diff->targetScene()->getMap().value("G0"));
	QVERIFY(sm);
	QRectF r = sm->boundingRect();
	QImage image(r.size().toSize(), QImage::Format_ARGB32);
	image.fill(Qt::white);
	{
		QPainter painter(&image);
		painter.translate(-r.topLeft());
		QStyleOptionGraphicsItem option;
		sm->paint(&painter, &option, nullptr);
	}
	QCOMPARE(QColor(image.pixel(1, image.height() / 2)), QColor(DIFF_CHANGED_COLOR));
	// the change column fits the dock
	QTreeWidget* list = window.findChild<QTreeWidget*>("changeList");
	QVERIFY(list && list->isVisible());
	QCoreApplication::processEvents();
	QHeaderView* header = list->header();
	QVERIFY(header->sectionViewportPosition(1) + header->sectionSize(1) <= list->viewport()->width());
}

// EDIT-MODE-19..24: a text compared with a file, two columns, line by line
void TestModes::test_text_compare()
{
	CyberiadaSMEditorWindow window;
	window.resize(1000, 600);
	window.show();
	QVERIFY(window.openFile("texts/diff-target.py"));
	TextDocument* source = window.currentText();
	QVERIFY(source && window.actionCompare->isEnabled() && !window.actionNextChange->isEnabled());
	DiffOrigin base;
	base.kind = DiffOrigin::originFile;
	base.path = "texts/diff-base.py";
	base.label = "diff-base.py";
	QString error;
	TextDiffDocument* diff = window.compareText(source, base, &error);
	QVERIFY2(diff, qPrintable(error));
	QCOMPARE(window.currentTextDiff(), diff);
	QCOMPARE(diff->mode(), QString(MODE_TEXT_DIFF));
	QCOMPARE(diff->language(), QString("python"));
	QCOMPARE(window.tabBar()->tabText(1), QString::fromUtf8("diff-target.py ↔ diff-base.py [text diff]"));
	// a row a block on both sides; a filler has no number and no text
	const TextDiff::Result& r = diff->result();
	QCOMPARE(r.hunks.size(), 3);
	QCOMPARE(r.changed, 3);
	QCOMPARE(r.added, 1);
	QCOMPARE(r.removed, 1);
	QCOMPARE(diff->basePane()->document()->blockCount(), r.rows.size());
	QCOMPARE(diff->targetPane()->document()->blockCount(), r.rows.size());
	int added = -1;
	for (int i = 0; i < r.rows.size(); i++) {
		if (r.rows[i].kind == TextDiff::rowAdded) added = i;
	}
	QVERIFY(added > 0);
	QCOMPARE(diff->basePane()->lineLabel(added), QString());
	QCOMPARE(diff->targetPane()->lineLabel(added), QString("9"));
	QCOMPARE(diff->basePane()->lineLabel(added + 1), QString("9"));
	QCOMPARE(diff->basePane()->document()->findBlockByNumber(added).text(), QString());
	// a row per hunk in the Changes dock, the tree and the properties hidden (EDIT-UI-6)
	QTreeWidget* list = window.findChild<QTreeWidget*>("changeList");
	QVERIFY(list && list->isVisible());
	QVERIFY(window.changesDock->isVisible() && window.actionChanges->isEnabled());
	QVERIFY(window.treeDock->isHidden() && window.propertiesDock->isHidden());
	// closed in a diff tab, the changes stay closed in the next one until View shows them
	SettingsManager& sm = SettingsManager::instance();
	window.changesDock->close();
	QVERIFY(window.changesDock->isHidden() && !sm.getShowChanges());
	window.setCurrentDocument(source);
	QVERIFY(window.changesDock->isHidden() && !window.actionChanges->isEnabled());
	window.setCurrentDocument(diff);
	QVERIFY(window.changesDock->isHidden() && window.actionChanges->isEnabled());
	window.actionChanges->trigger();
	QVERIFY(window.changesDock->isVisible() && sm.getShowChanges());
	QCOMPARE(list->topLevelItemCount(), 3);
	QCOMPARE(list->topLevelItem(0)->text(0), QString::fromUtf8("8 → 8-9"));
	QCOMPARE(list->topLevelItem(0)->text(1), QString::fromUtf8("changed 1 → 2"));
	// read-only, no canvas parts; Refresh and the changes
	QVERIFY(window.actionReadOnly->isChecked() && !window.actionReadOnly->isEnabled());
	QVERIFY(window.actionModeTextDiff->isChecked() && !window.actionModeTextDiff->isEnabled());
	QVERIFY(!window.actionSave->isEnabled() && !window.actionCompare->isEnabled());
	QVERIFY(!window.actionZoomIn->isEnabled() && !window.actionGeometry->isEnabled());
	QVERIFY(window.actionRefresh->isEnabled() && window.actionNextChange->isEnabled());
	QVERIFY(window.actionIgnoreWhitespace->isEnabled() && !window.actionIgnoreWhitespace->isChecked());
	// Next and Previous Change stop at the ends; the row is selected and shown
	window.actionNextChange->trigger();
	QCOMPARE(diff->currentChange(), 0);
	QCOMPARE(list->currentItem(), list->topLevelItem(0));
	QCOMPARE(diff->targetPane()->textCursor().blockNumber(), r.hunks[0].firstRow);
	window.actionPreviousChange->trigger();
	QCOMPARE(diff->currentChange(), 0);
	for (int i = 0; i < r.hunks.size(); i++) window.actionNextChange->trigger();
	QCOMPARE(diff->currentChange(), 2);
	emit list->itemClicked(list->topLevelItem(1), 0);
	QCOMPARE(diff->currentChange(), 1);
	// the spaces of the reset line ignored, then compared again
	window.actionIgnoreWhitespace->trigger();
	QVERIFY(diff->ignoreWhitespace());
	QCOMPARE(diff->result().changed, 2);
	QCOMPARE(diff->currentChange(), -1);
	window.actionIgnoreWhitespace->trigger();
	QCOMPARE(diff->result().changed, 3);
	// Copy and Find act on the side the focus was in last
	QCOMPARE(diff->focusedPane(), diff->targetPane());
	emit diff->basePane()->focused();
	QCOMPARE(diff->focusedPane(), diff->basePane());
	QTextCursor cursor(diff->basePane()->document()->firstBlock());
	cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
	diff->basePane()->setTextCursor(cursor);
	QVERIFY(window.actionCopy->isEnabled());
	window.actionCopy->trigger();
	QCOMPARE(QApplication::clipboard()->text(), QString("import time"));
	FindQuery query;
	query.text = "print";
	diff->setFindQuery(query, true);
	QCOMPARE(diff->findMatches().size(), 1);
	emit diff->targetPane()->focused();
	QCOMPARE(diff->findMatches().size(), 0);
	diff->clearFind(false);
	// Refresh takes the text of the source tab with its edits
	source->editor()->moveCursor(QTextCursor::End);
	source->editor()->insertPlainText("# end\n");
	window.actionRefresh->trigger();
	QCOMPARE(diff->result().added, 2);
	QVERIFY(window.grab().save(QT_TESTCASE_BUILDDIR "/tests/text-diff.png"));
	// the text tab again: Compare on, the changes off
	window.setCurrentDocument(source);
	QVERIFY(window.actionCompare->isEnabled() && !window.actionNextChange->isEnabled());
}

QTEST_MAIN(TestModes)
#include "l4-modes.moc"
