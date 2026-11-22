/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The L4 tests of the Markdown outline: the scan, the model, the dock (EDIT-MD-13..16)
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
#include <QAbstractItemModelTester>
#include <QElapsedTimer>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QUndoStack>

#include "code_editor.h"
#include "cyberiada_constants.h"
#include "document_mode.h"
#include "markdown_outline.h"
#include "markdown_outline_model.h"
#include "markdown_outline_view.h"
#include "markdown_tools.h"
#include "markdown_view.h"
#include "settings_manager.h"
#include "smeditor_window.h"
#include "text_document.h"

static const char* const OUTLINE_TEXT = "texts/outline.md";
static const char* const DIAGRAM = "diagrams/geometry.graphml";

// a text of sections, its lines numbered in the comments
static const char* const SECTIONS =
	"# Introduction\n"      // 1
	"\n"
	"Some words.\n"         // 3
	"\n"
	"## Example\n"          // 5
	"\n"
	"![Pic](pic.png)\n"     // 7
	"\n"
	"## Details\n"          // 9
	"\n"
	"# Summary\n"           // 11
	"\n"
	"## Example\n"          // 13
	"\n"
	"The end.\n";           // 15

// the first row of the label below the parent, depth first
static QModelIndex findRow(const QAbstractItemModel* model, const QString& label,
                           const QModelIndex& parent = QModelIndex())
{
	for (int i = 0; i < model->rowCount(parent); i++) {
		QModelIndex index = model->index(i, 0, parent);
		if (index.data().toString() == label) return index;
		QModelIndex below = findRow(model, label, index);
		if (below.isValid()) return below;
	}
	return QModelIndex();
}

static QStringList labels(const QAbstractItemModel* model, const QModelIndex& parent)
{
	QStringList list;
	for (int i = 0; i < model->rowCount(parent); i++) list << model->index(i, 0, parent).data().toString();
	return list;
}

// the line from 1 replaced, one edit of the text as typing makes it
static void replaceLine(TextDocument* text, int line, const QString& with)
{
	QTextCursor cursor(text->editor()->document()->findBlockByNumber(line - 1));
	cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
	cursor.insertText(with);
}

static void insertLines(TextDocument* text, int before, const QString& lines)
{
	QTextCursor cursor(text->editor()->document()->findBlockByNumber(before - 1));
	cursor.insertText(lines);
}

static void removeLine(TextDocument* text, int line)
{
	QTextDocument* doc = text->editor()->document();
	QTextCursor cursor(doc->findBlockByNumber(line - 1));
	cursor.setPosition(doc->findBlockByNumber(line).position(), QTextCursor::KeepAnchor);
	cursor.removeSelectedText();
}

class TestMarkdownOutline: public QObject {
	Q_OBJECT

private slots:
	void cleanup();
	void test_scan();
	void test_tree();
	void test_model_tester();
	void test_kept_rows();
	void test_reading_rate();
	void test_diagram_state();
	void test_dock_default();
	void test_dock_follows_tab();
	void test_tree_and_source();
	void test_out_of_date_rows();
	void test_out_of_date_rows_data();
	void test_rendered_page();
	void test_diagram_leaf_opens();
};

void TestMarkdownOutline::cleanup()
{
	// the l4 binaries share one config
	SettingsManager::instance().setShowOutline(true);
}

// EDIT-MD-13: the headings and the images in the order of the text, a heading before the images
// of its line, nothing in a code span or a fence; a linked diagram image
void TestMarkdownOutline::test_scan()
{
	QFile file(OUTLINE_TEXT);
	QVERIFY(file.open(QIODevice::ReadOnly));
	QList<OutlineEntry> entries = markdownOutline(QString::fromUtf8(file.readAll()), QFileInfo(OUTLINE_TEXT).absolutePath());
	QCOMPARE(entries.size(), 11);
	QCOMPARE(int(entries.at(0).kind), int(OutlineEntry::outlineImage));
	QCOMPARE(entries.at(0).title, QString("Cover"));
	QCOMPARE(entries.at(1).title, QString("Outline"));
	QCOMPARE(entries.at(1).level, 1);
	// two images of one line by their columns, one without an alt shown by its file name
	QCOMPARE(entries.at(3).line, 9);
	QCOMPARE(entries.at(4).line, 9);
	QVERIFY(entries.at(3).column < entries.at(4).column);
	QCOMPARE(entries.at(4).label(), QString("arrow.svg"));
	QCOMPARE(entries.at(5).level, 4);
	QCOMPARE(int(entries.at(6).kind), int(OutlineEntry::outlineDiagram));
	QCOMPARE(entries.at(6).graphml, QFileInfo(DIAGRAM).absoluteFilePath());
	QVERIFY(!entries.at(6).upToDate);
	// setext: an underline of - is level 2, of = level 1
	QCOMPARE(entries.at(7).title, QString("Setext heading"));
	QCOMPARE(entries.at(7).level, 2);
	QCOMPARE(entries.at(10).level, 1);
	// the image of a heading's line comes after the heading
	QCOMPARE(entries.at(8).line, entries.at(9).line);
	QCOMPARE(int(entries.at(8).kind), int(OutlineEntry::outlineHeading));
	QCOMPARE(entries.at(9).title, QString("Icon"));
	for (const OutlineEntry& e : entries) QVERIFY(!e.title.startsWith("not a"));
}

// EDIT-MD-13: the document row on top, a heading under the nearest earlier lower level, an image
// under the heading of its section, before the first heading under the document
void TestMarkdownOutline::test_tree()
{
	TextDocument text(MODE_MARKDOWN);
	QVERIFY(text.load(OUTLINE_TEXT, nullptr));
	MarkdownOutlineModel* model = text.outline();
	QVERIFY(model);
	QVERIFY(text.outlineOutOfDate());
	text.readOutline();
	QVERIFY(!text.outlineOutOfDate());
	QCOMPARE(model->rowCount(), 1);
	QModelIndex doc = model->documentIndex();
	QCOMPARE(doc.data().toString(), QString("outline.md"));
	QCOMPARE(labels(model, doc), QStringList() << "Cover" << "Outline" << "Second ![Icon](images/arrow.svg)" << "Another");
	QModelIndex images = findRow(model, "Images");
	QCOMPARE(images.parent(), findRow(model, "Outline"));
	QCOMPARE(labels(model, images), QStringList() << "Quadrants" << "arrow.svg" << "A skipped level");
	QModelIndex skipped = findRow(model, "A skipped level");
	QCOMPARE(labels(model, skipped), QStringList() << "Geometry");
	// read-only: selectable, nothing else
	QCOMPARE(model->flags(skipped), Qt::ItemIsEnabled | Qt::ItemIsSelectable);
	QVERIFY(!findRow(model, "Geometry").data(Qt::DecorationRole).isNull());
	QVERIFY(findRow(model, "Geometry").data(Qt::ToolTipRole).toString().contains("geometry.graphml"));
	QVERIFY(skipped.data(Qt::ToolTipRole).toString().contains("#a-skipped-level"));
	// the row of a place: the image on its line, the heading of a line before it, the document
	QCOMPARE(model->indexAt(9, 1), findRow(model, "Quadrants"));
	QCOMPARE(model->indexAt(9, 60), findRow(model, "arrow.svg"));
	QCOMPARE(model->indexAt(12, 1), images);
	QCOMPARE(model->indexAt(23, 1), findRow(model, "Second ![Icon](images/arrow.svg)"));
	QCOMPARE(model->indexAt(23, 15), findRow(model, "Icon"));
	QCOMPARE(model->indexAt(1, 1), findRow(model, "Cover"));
	QCOMPARE(model->indexAt(2, 1), doc);
}

// EDIT-MD-14: the model keeps the contract of Qt through the edits, undo and redo, a heading
// retyped, a level changed, a section removed and a mode switch
void TestMarkdownOutline::test_model_tester()
{
	TextDocument text(MODE_MARKDOWN);
	text.setText(SECTIONS);
	MarkdownOutlineModel* model = text.outline();
	new QAbstractItemModelTester(model, QAbstractItemModelTester::FailureReportingMode::QtTest, model);
	text.readOutline();
	insertLines(&text, 3, "## New one\n\n");
	text.readOutline();
	QVERIFY(findRow(model, "New one").isValid());
	replaceLine(&text, 1, "# Intro");
	text.readOutline();
	replaceLine(&text, 7, "### Example");
	text.readOutline();
	QCOMPARE(findRow(model, "Example").parent(), findRow(model, "New one"));
	removeLine(&text, 11);
	text.readOutline();
	text.undoStack()->undo();
	text.readOutline();
	text.undoStack()->undo();
	text.readOutline();
	text.undoStack()->redo();
	text.readOutline();
	text.setText(QString());
	text.readOutline();
	QCOMPARE(model->rowCount(model->documentIndex()), 0);
	text.setText(SECTIONS);
	text.readOutline();
	QCOMPARE(labels(model, model->documentIndex()), QStringList() << "Introduction" << "Summary");
	// another mode drops the outline, Markdown again makes a new one
	QPointer<MarkdownOutlineModel> gone(model);
	QVERIFY(text.setMode(MODE_TEXT));
	QVERIFY(!text.outline() && !gone);
	QVERIFY(text.setMode(MODE_MARKDOWN));
	QVERIFY(text.outline() && text.outlineOutOfDate());
}

// EDIT-MD-14: a reading keeps the rows of the same and of a similar heading, with their expanded
// state; a heading retyped beyond similar is another row
void TestMarkdownOutline::test_kept_rows()
{
	TextDocument text(MODE_MARKDOWN);
	text.setText(SECTIONS);
	text.readOutline();
	MarkdownOutlineModel* model = text.outline();
	QPersistentModelIndex intro = findRow(model, "Introduction");
	QPersistentModelIndex details = findRow(model, "Details");
	model->setExpanded(intro, false);
	QSignalSpy removed(model, &QAbstractItemModel::rowsRemoved);
	QSignalSpy reset(model, &QAbstractItemModel::modelReset);
	// the body typed: the lines move, the rows stay
	insertLines(&text, 3, "More words.\n\n");
	text.readOutline();
	QVERIFY(intro.isValid() && details.isValid());
	QCOMPARE(model->entry(details)->line, 11);
	QCOMPARE(removed.count(), 0);
	// a similar title keeps the row and its state
	replaceLine(&text, 1, "# Introductions");
	text.readOutline();
	QVERIFY(intro.isValid());
	QCOMPARE(intro.data().toString(), QString("Introductions"));
	QVERIFY(!model->isExpanded(intro));
	// another title is another row
	replaceLine(&text, 11, "## Zzzz");
	text.readOutline();
	QVERIFY(!details.isValid());
	QVERIFY(findRow(model, "Zzzz").isValid());
	QCOMPARE(reset.count(), 0);
	// another level under the same parent keeps the row
	QPersistentModelIndex example = findRow(model, "Example");
	replaceLine(&text, 7, "### Example");
	text.readOutline();
	QVERIFY(example.isValid());
	QCOMPARE(model->entry(example)->level, 3);
	// a short title is not kept by the kind sign of its key alone
	TextDocument letters(MODE_MARKDOWN);
	letters.setText("# A\n");
	letters.readOutline();
	QPersistentModelIndex a = findRow(letters.outline(), "A");
	letters.setText("# B\n");
	letters.readOutline();
	QVERIFY(!a.isValid());
}

// EDIT-MD-14: read at most once a second after the first change, the next changes not delaying
// it; nothing read while hidden, at once when shown
void TestMarkdownOutline::test_reading_rate()
{
	TextDocument text(MODE_MARKDOWN);
	text.setText(SECTIONS);
	QSignalSpy reads(&text, &TextDocument::outlineRead);
	text.setOutlineShown(true);
	QCOMPARE(reads.count(), 1);
	QElapsedTimer clock;
	clock.start();
	replaceLine(&text, 3, "Typed.");
	QVERIFY(text.outlineOutOfDate());
	QCOMPARE(reads.count(), 1);
	// typed on before the second is over: a delay restarted on each change would read at 1.8 s
	const int typedOn = MARKDOWN_OUTLINE_INTERVAL_MS * 4 / 5;
	QTest::qWait(typedOn);
	replaceLine(&text, 3, "Typed on.");
	QTRY_COMPARE_WITH_TIMEOUT(reads.count(), 2, MARKDOWN_OUTLINE_INTERVAL_MS * 2);
	QVERIFY(clock.elapsed() < typedOn + MARKDOWN_OUTLINE_INTERVAL_MS);
	QVERIFY(!text.outlineOutOfDate());
	// hidden: marked only
	text.setOutlineShown(false);
	replaceLine(&text, 1, "# Hidden");
	QTest::qWait(MARKDOWN_OUTLINE_INTERVAL_MS * 3 / 2);
	QCOMPARE(reads.count(), 2);
	QVERIFY(text.outlineOutOfDate());
	text.setOutlineShown(true);
	QCOMPARE(reads.count(), 3);
	QVERIFY(findRow(text.outline(), "Hidden").isValid());
}

// EDIT-MD-13: a diagram leaf up to date while its SVG records the diagram as it is, out of date
// after the diagram changes, with another icon
void TestMarkdownOutline::test_diagram_state()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	QString graphml = dir.filePath("geometry.graphml");
	QVERIFY(QFile::copy(DIAGRAM, graphml));
	QString error;
	QVERIFY2(renderDiagramFile(graphml, dir.filePath("geometry.svg"), QString(), &error), qPrintable(error));
	QString path = dir.filePath("text.md");
	QFile md(path);
	QVERIFY(md.open(QIODevice::WriteOnly));
	md.write("# Diagram\n\n![Geometry](geometry.svg)\n");
	md.close();
	TextDocument text(MODE_MARKDOWN);
	QVERIFY(text.load(path, &error));
	text.readOutline();
	QModelIndex leaf = findRow(text.outline(), "Geometry");
	QVERIFY(leaf.isValid());
	QCOMPARE(int(text.outline()->entry(leaf)->kind), int(OutlineEntry::outlineDiagram));
	QVERIFY(text.outline()->entry(leaf)->upToDate);
	QIcon upToDate = qvariant_cast<QIcon>(leaf.data(Qt::DecorationRole));
	// the diagram edited: the digest is read again, the size changed
	QFile diagram(graphml);
	QVERIFY(diagram.open(QIODevice::Append));
	diagram.write("\n");
	diagram.close();
	text.editor()->appendPlainText("A line.");
	text.readOutline();
	QVERIFY(!text.outline()->entry(leaf)->upToDate);
	QIcon outOfDate = qvariant_cast<QIcon>(leaf.data(Qt::DecorationRole));
	QVERIFY(upToDate.cacheKey() != outOfDate.cacheKey());
	QVERIFY(leaf.data(Qt::ToolTipRole).toString().contains("out of date"));
}

// EDIT-MD-13, EDIT-UI-6: a Markdown tab opens with the Outline shown, View > Outline checked
void TestMarkdownOutline::test_dock_default()
{
	// a first start: no choice stored
	SettingsManager& sm = SettingsManager::instance();
	sm.setShowOutline(false);
	QSettings().remove("display/showOutline");
	sm.load();
	QVERIFY(sm.getShowOutline());
	CyberiadaSMEditorWindow window;
	window.show();
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	QVERIFY(window.openFile(OUTLINE_TEXT));
	QVERIFY(window.outlineDock->isVisible());
	QVERIFY(window.actionOutline->isEnabled() && window.actionOutline->isChecked());
	QVERIFY(window.menuView->actions().contains(window.actionOutline));
	QVERIFY(window.outlineDock->font().bold() && !window.outlineDock->widget()->font().bold());
	QCOMPARE(window.outlineView()->model(), window.currentText()->outline());
	QVERIFY(!window.currentText()->outlineOutOfDate());
	QVERIFY(findRow(window.outlineView()->model(), "Images").isValid());
	// the close button stores the choice, the entry shows it again
	window.outlineDock->close();
	QVERIFY(!SettingsManager::instance().getShowOutline() && !window.actionOutline->isChecked());
	window.actionOutline->trigger();
	QVERIFY(window.outlineDock->isVisible() && SettingsManager::instance().getShowOutline());
}

// EDIT-UI-6, EDIT-MD-14: the dock is a Markdown tab's; another tab hides it with its entry greyed,
// a Markdown tab in the background reads nothing
void TestMarkdownOutline::test_dock_follows_tab()
{
	CyberiadaSMEditorWindow window;
	window.show();
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	QVERIFY(window.openFile(OUTLINE_TEXT));
	TextDocument* text = window.currentText();
	QVERIFY(window.openFile("diagrams/hierarchy.graphml"));
	QVERIFY(window.outlineDock->isHidden() && !window.actionOutline->isEnabled());
	QVERIFY(window.treeDock->isVisible());
	QVERIFY(!window.outlineView()->model() || window.outlineView()->model() != text->outline());
	// in the background: marked, not read
	QSignalSpy reads(text, &TextDocument::outlineRead);
	replaceLine(text, 3, "# Background");
	QTest::qWait(MARKDOWN_OUTLINE_INTERVAL_MS * 3 / 2);
	QCOMPARE(reads.count(), 0);
	QVERIFY(text->outlineOutOfDate());
	window.setCurrentDocument(text);
	QCOMPARE(reads.count(), 1);
	QVERIFY(window.outlineDock->isVisible() && window.actionOutline->isEnabled());
	QVERIFY(findRow(window.outlineView()->model(), "Background").isValid());
	// a text mode has none, Markdown again has it
	QVERIFY(window.setDocumentMode(text, MODE_TEXT));
	QVERIFY(window.outlineDock->isHidden() && !window.actionOutline->isEnabled());
	QVERIFY(window.setDocumentMode(text, MODE_MARKDOWN));
	QVERIFY(window.outlineDock->isVisible());
	QCOMPARE(window.outlineView()->model(), text->outline());
	QVERIFY(window.closeDocument(text, true));
}

// EDIT-MD-15: a row made current puts the cursor at its place, the focus kept in the tree; Enter
// focuses the source; the cursor moved makes its section's row current
void TestMarkdownOutline::test_tree_and_source()
{
	CyberiadaSMEditorWindow window;
	window.show();
	QVERIFY(QTest::qWaitForWindowActive(&window));
	QVERIFY(window.openFile(OUTLINE_TEXT));
	TextDocument* text = window.currentText();
	MarkdownOutlineView* view = window.outlineView();
	QModelIndex skipped = findRow(view->model(), "A skipped level");
	view->setFocus();
	QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->visualRect(skipped).center());
	QTRY_COMPARE(text->currentLine(), 16);
	QVERIFY(view->hasFocus() && !text->editor()->hasFocus());
	QCOMPARE(view->currentIndex(), skipped);
	// an image leaf: its column
	view->setCurrentIndex(findRow(view->model(), "arrow.svg"));
	QTRY_COMPARE(text->currentLine(), 9);
	QCOMPARE(text->editor()->textCursor().positionInBlock() + 1, text->outline()->entry(view->currentIndex())->column);
	// Enter: the source focused
	QTest::keyClick(view, Qt::Key_Return);
	QTRY_VERIFY(text->editor()->hasFocus());
	// the source's cursor: its section's row current
	QVERIFY(text->goToLine(21));
	QCOMPARE(view->currentIndex(), findRow(view->model(), "Setext heading"));
	QVERIFY(text->goToLine(1));
	QCOMPARE(view->currentIndex(), findRow(view->model(), "Cover"));
	// out of date: the tree waits for the reading
	replaceLine(text, 4, "## Fresh");
	QVERIFY(text->goToLine(5));
	QCOMPARE(view->currentIndex(), findRow(view->model(), "Cover"));
	QTRY_COMPARE_WITH_TIMEOUT(view->currentIndex(), findRow(view->model(), "Fresh"), MARKDOWN_OUTLINE_INTERVAL_MS * 2);
	QVERIFY(window.closeDocument(text, true));
}

// EDIT-MD-16, the table: a row made current on an out-of-date outline goes to what it named
void TestMarkdownOutline::test_out_of_date_rows_data()
{
	QTest::addColumn<QString>("row");
	QTest::addColumn<int>("line");
	QTest::addColumn<QString>("with");
	// the line the cursor goes to, 0: nowhere
	QTest::addColumn<int>("goes");
	QTest::newRow("lines added elsewhere") << "Details" << 3 << "More.\n\nAnd more.\n\nSome words." << 13;
	QTest::newRow("retitled") << "Details" << 9 << "## Detailing" << 9;
	QTest::newRow("removed") << "Details" << 9 << "" << 0;
	QTest::newRow("retyped beyond similar") << "Details" << 9 << "## Zzzz" << 0;
	QTest::newRow("level changed, the same parent") << "Example" << 5 << "### Example" << 5;
	QTest::newRow("level changed past a sibling") << "Details" << 9 << "### Details" << 9;
	// the Example of Summary removed, the one of Introduction older
	QTest::newRow("older namesake") << "Example@13" << 13 << "" << 0;
}

void TestMarkdownOutline::test_out_of_date_rows()
{
	QFETCH(QString, row);
	QFETCH(int, line);
	QFETCH(QString, with);
	QFETCH(int, goes);
	QTemporaryDir dir;
	QString path = dir.filePath("sections.md");
	QFile file(path);
	QVERIFY(file.open(QIODevice::WriteOnly));
	file.write(SECTIONS);
	file.close();
	CyberiadaSMEditorWindow window;
	window.show();
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	QVERIFY(window.openFile(path));
	TextDocument* text = window.currentText();
	MarkdownOutlineView* view = window.outlineView();
	QModelIndex target = findRow(view->model(), row.section('@', 0, 0));
	if (row.contains('@')) target = text->outline()->indexAt(row.section('@', 1).toInt(), 1);
	QVERIFY(target.isValid());
	// the cursor's row is never the one made current
	QVERIFY(text->goToLine(1));
	replaceLine(text, line, with);
	QVERIFY(text->outlineOutOfDate());
	view->setCurrentIndex(target);
	QTRY_VERIFY(!text->outlineOutOfDate());
	if (goes > 0) {
		QTRY_COMPARE(text->currentLine(), goes);
		QVERIFY(window.outlineMessage().isEmpty());
		QCOMPARE(text->outline()->entry(view->currentIndex())->line, goes);
	} else {
		QTRY_VERIFY(!window.outlineMessage().isEmpty());
		QVERIFY(window.outlineMessage().contains(row.section('@', 0, 0)));
		QCOMPARE(text->currentLine(), 1);
		QCOMPARE(view->currentIndex(), text->outlineRowAtCursor());
	}
	QVERIFY(window.closeDocument(text, true));
}

// EDIT-MD-15: a rendered page scrolls to the heading of the row
void TestMarkdownOutline::test_rendered_page()
{
	QString longer;
	for (int i = 1; i <= 40; i++) longer += QString("# Part %1\n\nSome words of part %1.\n\n").arg(i);
	QTemporaryDir dir;
	QString path = dir.filePath("long.md");
	QFile file(path);
	QVERIFY(file.open(QIODevice::WriteOnly));
	file.write(longer.toUtf8());
	file.close();
	CyberiadaSMEditorWindow window;
	window.resize(800, 600);
	window.show();
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	QVERIFY(window.openFile(path));
	TextDocument* text = window.currentText();
	window.actionReadOnly->trigger();
	QVERIFY(text->showsRendered());
	QVERIFY(window.outlineDock->isVisible());
	MarkdownOutlineView* view = window.outlineView();
	QModelIndex last = findRow(view->model(), "Part 40");
	QVERIFY(last.isValid());
	QWidget* page = text->page();
	QList<MarkdownView*> views = page->findChildren<MarkdownView*>();
	QVERIFY(!views.isEmpty());
	MarkdownView* rendered = nullptr;
	for (MarkdownView* v : views) if (v->isVisible()) rendered = v;
	QVERIFY(rendered);
	QCOMPARE(rendered->verticalScrollBar()->value(), 0);
	view->setCurrentIndex(last);
	QTRY_VERIFY(rendered->verticalScrollBar()->value() > 0);
}

// EDIT-MD-15: a double click on a diagram leaf opens its diagram in a tab
void TestMarkdownOutline::test_diagram_leaf_opens()
{
	CyberiadaSMEditorWindow window;
	window.show();
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	QVERIFY(window.openFile(OUTLINE_TEXT));
	MarkdownOutlineView* view = window.outlineView();
	QModelIndex leaf = findRow(view->model(), "Geometry");
	QVERIFY(leaf.isValid());
	view->scrollTo(leaf);
	QTest::mouseDClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->visualRect(leaf).center());
	QTRY_VERIFY(window.currentDocument() != nullptr);
	QCOMPARE(QFileInfo(window.currentDocument()->filePath()).absoluteFilePath(), QFileInfo(DIAGRAM).absoluteFilePath());
	QVERIFY(window.outlineDock->isHidden());
}

QTEST_MAIN(TestMarkdownOutline)

#include "l4-markdown-outline.moc"
