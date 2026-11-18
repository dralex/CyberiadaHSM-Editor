/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process test of the Markdown tools: the paths, the copies, the image
 * edits, the inserted line and the Insert Image dialog (see docs/TESTING.md, L4;
 * EDIT-MD-1..4)
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
#include <QClipboard>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QToolBar>
#include <QTreeWidget>
#include "plugin_dock.h"
#include <QUndoStack>
#include <QComboBox>
#include <QLabel>
#include <QPainter>
#include <QRadioButton>
#include <QSvgRenderer>
#include "markdown_tools.h"
#include "cyberiadasm_render.h"
#include "svg_source.h"
#include "markdown_view.h"
#include "fontmanager.h"
#include "dialogs/insert_diagram_dialog.h"
#include "dialogs/insert_image_dialog.h"
#include "code_editor.h"
#include "text_document.h"
#include "smeditor_window.h"
#include "cyberiada_constants.h"
#include "document_mode.h"

// a 40x30 image: red, green on the top, blue, yellow at the bottom
static const char* const QUADRANTS = "texts/images/quadrants.png";
static const char* const ARROW = "texts/images/arrow.svg";
// a diagram with geometry, named test-geometry-1
static const char* const DIAGRAM = "diagrams/geometry.graphml";

static QString absolute(const char* path)
{
	return QFileInfo(path).absoluteFilePath();
}

static QByteArray fileBytes(const QString& path)
{
	QFile file(path);
	return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

static bool copyInto(const QString& from, const QString& to)
{
	QDir().mkpath(QFileInfo(to).absolutePath());
	return QFile::copy(from, to);
}

// a press, a move with the button held and a release: QTest::mouseMove moves the cursor only
static void drag(QWidget* widget, const QPoint& from, const QPoint& to)
{
	QTest::mousePress(widget, Qt::LeftButton, Qt::NoModifier, from);
	QMouseEvent move(QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
	QApplication::sendEvent(widget, &move);
	QTest::mouseRelease(widget, Qt::LeftButton, Qt::NoModifier, to);
}

class TestMarkdownTools: public QObject {
	Q_OBJECT

private slots:
	void test_paths();
	void test_image_edits();
	void test_propose_target();
	void test_data_and_clash();
	void test_insert_block();
	void test_insert_undo();
	void test_dialog();
	void test_dialog_in_place();
	void test_dialog_svg_and_pasted();
	void test_crop_drag();
	void test_window_actions();
	void test_paste_offer();
	void test_record_svg_source();
	void test_render_diagram_file();
	void test_markdown_images();
	void test_diagram_images();
	void test_update_diagram_images();
	void test_insert_diagram_dialog();
	void test_window_diagram_actions();
	void test_rendered_link_opens_tab();
	void test_markdown_links();
	void test_markdown_headings();
	void test_markdown_check();
	void test_check_diagram_images();
	void test_window_check();
};

// EDIT-MD-4: the paths relative to the text, <path> with a space or a parenthesis, the
// escaped labels, the numbered names
void TestMarkdownTools::test_paths()
{
	QTemporaryDir dir;
	QString base = dir.path();
	QCOMPARE(markdownPath(base + "/images/a.png", base), QString("images/a.png"));
	QCOMPARE(markdownPath(base + "/my images/a (1).png", base), QString("<my images/a (1).png>"));
	QCOMPARE(markdownPath(base + "/../x.png", base), QString("../x.png"));
	QVERIFY(isInsideDirectory(base + "/images/a.png", base));
	QVERIFY(isInsideDirectory(base + "/a.png", base + "/"));
	// a sibling sharing the prefix is outside
	QVERIFY(!isInsideDirectory(base + "x/a.png", base));
	QVERIFY(!isInsideDirectory(base + "/a.png", QString()));
	QCOMPARE(markdownLabel("a [b] \\c"), QString("a \\[b\\] \\\\c"));
	QCOMPARE(markdownImageLine("Wiring [1]", base + "/images/w.png", base),
	         QString("![Wiring \\[1\\]](images/w.png)"));
	QCOMPARE(numberedFileName(base, "image", "png"), base + "/image-1.png");
	QFile one(base + "/image-1.png");
	QVERIFY(one.open(QIODevice::WriteOnly));
	one.close();
	QCOMPARE(numberedFileName(base, "image", "png"), base + "/image-2.png");
}

// EDIT-MD-3: the rotation, then the crop of the rotated image, then the width limit
void TestMarkdownTools::test_image_edits()
{
	QImage image(QUADRANTS);
	QCOMPARE(image.size(), QSize(40, 30));
	ImageEdits edits;
	QVERIFY(edits.isEmpty());
	QCOMPARE(applyImageEdits(image, edits), image);
	// clockwise: the red top left goes to the top right
	edits.rotation = IMAGE_QUARTER_TURN;
	QVERIFY(!edits.isEmpty());
	QImage turned = applyImageEdits(image, edits);
	QCOMPARE(turned.size(), QSize(30, 40));
	QCOMPARE(turned.pixelColor(28, 1), QColor(255, 0, 0));
	QCOMPARE(turned.pixelColor(1, 1), QColor(0, 0, 255));
	// the green quadrant alone
	edits = ImageEdits();
	edits.crop = QRect(20, 0, 20, 15);
	QImage green = applyImageEdits(image, edits);
	QCOMPARE(green.size(), QSize(20, 15));
	QCOMPARE(green.pixelColor(10, 7), QColor(0, 160, 0));
	// scaled down in its proportions
	edits.maxWidth = 10;
	QImage small = applyImageEdits(image, edits);
	QCOMPARE(small.width(), 10);
	QVERIFY(qAbs(small.height() - 7.5) <= 0.5);
	// a wider limit leaves it as it is
	edits.maxWidth = 100;
	QCOMPARE(applyImageEdits(image, edits).size(), QSize(20, 15));
	// a crop past the image is cut to it
	edits = ImageEdits();
	edits.crop = QRect(30, 20, 50, 50);
	QCOMPARE(applyImageEdits(image, edits).size(), QSize(10, 10));
}

// EDIT-MD-2/3: linked in place, an edited one beside it, a copy into the folder, a pasted one
// numbered, a format Qt cannot write as PNG
void TestMarkdownTools::test_propose_target()
{
	QTemporaryDir dir;
	QString base = dir.path();
	QVERIFY(copyInto(QUADRANTS, base + "/pics/q.png"));
	ImageInsert inside;
	inside.baseDir = base;
	inside.source = base + "/pics/q.png";
	QCOMPARE(proposeImageTarget(inside, MARKDOWN_IMAGE_FOLDER), base + "/pics/q.png");
	inside.edits.rotation = IMAGE_QUARTER_TURN;
	QCOMPARE(proposeImageTarget(inside, MARKDOWN_IMAGE_FOLDER), base + "/pics/q-1.png");

	ImageInsert outside;
	outside.baseDir = base;
	outside.source = absolute(QUADRANTS);
	QCOMPARE(proposeImageTarget(outside, MARKDOWN_IMAGE_FOLDER), base + "/images/quadrants.png");
	QCOMPARE(proposeImageTarget(outside, "figures/2"), base + "/figures/2/quadrants.png");
	outside.source = "/nowhere/anim.gif";
	QCOMPARE(proposeImageTarget(outside, MARKDOWN_IMAGE_FOLDER), base + "/images/anim.gif");
	outside.edits.rotation = IMAGE_QUARTER_TURN;
	QCOMPARE(proposeImageTarget(outside, MARKDOWN_IMAGE_FOLDER), base + "/images/anim.png");

	ImageInsert pasted;
	pasted.baseDir = base;
	pasted.pasted = QImage(QUADRANTS);
	QVERIFY(pasted.isPasted());
	QCOMPARE(proposeImageTarget(pasted, MARKDOWN_IMAGE_FOLDER), base + "/images/image-1.png");
	QVERIFY(copyInto(QUADRANTS, base + "/images/image-1.png"));
	QCOMPARE(proposeImageTarget(pasted, MARKDOWN_IMAGE_FOLDER), base + "/images/image-2.png");
}

// EDIT-MD-2/3: the bytes copied or encoded, the same content reused, another one a clash
void TestMarkdownTools::test_data_and_clash()
{
	QTemporaryDir dir;
	QString base = dir.path();
	ImageInsert insert;
	insert.baseDir = base;
	insert.source = absolute(QUADRANTS);
	insert.target = proposeImageTarget(insert, MARKDOWN_IMAGE_FOLDER);
	QByteArray data;
	QString format, error;
	QVERIFY2(imageInsertData(insert, &data, &format, &error), qPrintable(error));
	QCOMPARE(data, fileBytes(QUADRANTS));
	QVERIFY(insert.writesFile());
	QVERIFY(!imageTargetClashes(insert, data));
	QVERIFY2(writeImageInsert(insert, data, &error), qPrintable(error));
	QCOMPARE(fileBytes(insert.target), data);
	// the same content again: no question, nothing written
	QVERIFY(!imageTargetClashes(insert, data));
	QVERIFY(writeImageInsert(insert, data, &error));

	// an edited image is another content
	insert.edits.crop = QRect(0, 0, 20, 15);
	QVERIFY2(imageInsertData(insert, &data, &format, &error), qPrintable(error));
	QCOMPARE(format, QString("png"));
	QVERIFY(imageTargetClashes(insert, data));
	QImage written;
	QVERIFY(written.loadFromData(data));
	QCOMPARE(written.size(), QSize(20, 15));
	QCOMPARE(fileBytes(QUADRANTS).size(), fileBytes(insert.target).size());
	// a JPEG target is encoded as JPEG
	insert.target = base + "/images/q.jpg";
	QVERIFY2(imageInsertData(insert, &data, &format, &error), qPrintable(error));
	QCOMPARE(format, QString("jpg"));
	QVERIFY(written.loadFromData(data, "JPG"));
	QCOMPARE(written.size(), QSize(20, 15));

	// an SVG is never edited
	ImageInsert svg;
	svg.baseDir = base;
	svg.source = absolute(ARROW);
	svg.edits.rotation = IMAGE_QUARTER_TURN;
	QVERIFY(!imageInsertData(svg, &data, &format, &error));
	svg.edits = ImageEdits();
	QVERIFY(imageInsertData(svg, &data, &format, &error));
	QCOMPARE(data, fileBytes(ARROW));

	// an image of the directory linked in place writes nothing
	ImageInsert place;
	place.baseDir = base;
	place.source = base + "/images/quadrants.png";
	place.target = place.source;
	QVERIFY(!place.writesFile());
	QVERIFY(!imageTargetClashes(place, QByteArray("other")));
	QVERIFY(writeImageInsert(place, QByteArray("other"), &error));
	QCOMPARE(fileBytes(place.target), fileBytes(QUADRANTS));
}

static QString insertedAt(const QString& text, int cursorBlock, int before)
{
	TextDocument doc(MODE_MARKDOWN);
	doc.setText(text);
	if (cursorBlock >= 0) {
		QTextCursor cursor(doc.editor()->document()->findBlockByNumber(cursorBlock));
		doc.editor()->setTextCursor(cursor);
	}
	if (!doc.insertBlock("X", before, "insert image")) return "refused";
	return doc.text() + QString("@%1").arg(doc.currentLine());
}

// EDIT-MD-2/12: a paragraph of its own after the paragraph of the cursor, on its blank line,
// before a line, at the end; the cursor at the end of the line
void TestMarkdownTools::test_insert_block()
{
	QCOMPARE(insertedAt("# T\n\npara one\nline two\n\nlast", 2, 0),
	         QString("# T\n\npara one\nline two\n\nX\n\nlast@6"));
	QCOMPARE(insertedAt("a\n\nb", 1, 0), QString("a\n\nX\n\nb@3"));
	QCOMPARE(insertedAt("a\nb", -1, 2), QString("a\n\nX\n\nb@3"));
	QCOMPARE(insertedAt("a\nb", -1, 1), QString("X\n\na\nb@1"));
	QCOMPARE(insertedAt("a", -1, 99), QString("a\n\nX@3"));
	QCOMPARE(insertedAt("a\n", -1, 99), QString("a\n\nX@3"));
	QCOMPARE(insertedAt("", 0, 0), QString("X\n@1"));
	QCOMPARE(insertedAt("last line", 0, 0), QString("last line\n\nX@3"));
	// a read-only text takes nothing
	TextDocument doc(MODE_MARKDOWN);
	doc.setText("a");
	doc.setReadOnlyFlag(true);
	QVERIFY(!doc.insertBlock("X", 0, "insert image"));
	QCOMPARE(doc.text(), QString("a"));
}

// EDIT-MD-2: the insert is one undo step of its own name
void TestMarkdownTools::test_insert_undo()
{
	TextDocument doc(MODE_MARKDOWN);
	doc.setText("a\nb");
	int steps = doc.undoStack()->count();
	QVERIFY(doc.insertBlock("![x](images/x.png)", 2, "insert image"));
	QCOMPARE(doc.undoStack()->count(), steps + 1);
	QCOMPARE(doc.undoStack()->undoText(), QString("insert image"));
	doc.undoStack()->undo();
	QCOMPARE(doc.text(), QString("a\nb"));
	doc.undoStack()->redo();
	QCOMPARE(doc.text(), QString("a\n\n![x](images/x.png)\n\nb"));
}

// EDIT-MD-2/3: a copy proposed into the folder, the edits and their result, the reset
void TestMarkdownTools::test_dialog()
{
	QTemporaryDir dir;
	InsertImageDialog dlg(absolute(QUADRANTS), QImage(), dir.path(), "images");
	QVERIFY(dlg.loaded());
	QCOMPARE(dlg.folderEdit()->text(), QString("images"));
	QCOMPARE(dlg.nameEdit()->text(), QString("quadrants.png"));
	QCOMPARE(dlg.altEdit()->text(), QString("quadrants"));
	QCOMPARE(dlg.insertedLine(), QString("![quadrants](images/quadrants.png)"));
	QCOMPARE(dlg.folder(), QString("images"));
	QVERIFY(dlg.insertButton()->isEnabled());
	QVERIFY(dlg.insert().edits.isEmpty());
	QCOMPARE(dlg.insert().target, dir.path() + "/images/quadrants.png");

	dlg.setCrop(QRect(20, 0, 20, 15));
	QCOMPARE(dlg.insert().edits.crop, QRect(20, 0, 20, 15));
	QCOMPARE(dlg.widthBox()->maximum(), 20);
	dlg.widthBox()->setValue(10);
	QCOMPARE(dlg.insert().edits.maxWidth, 10);
	// the width of the whole crop is no limit
	dlg.widthBox()->setValue(20);
	QCOMPARE(dlg.insert().edits.maxWidth, 0);
	// a rotation takes the whole turned image
	dlg.rotate(IMAGE_QUARTER_TURN);
	QCOMPARE(dlg.insert().edits.rotation, IMAGE_QUARTER_TURN);
	QVERIFY(dlg.insert().edits.crop.isNull());
	QCOMPARE(dlg.cropView()->crop(), QRect(0, 0, 30, 40));
	dlg.reset();
	QVERIFY(dlg.insert().edits.isEmpty());
	// the alt text goes into the line, escaped
	dlg.altEdit()->setText("Wiring [a]");
	QCOMPARE(dlg.insertedLine(), QString("![Wiring \\[a\\]](images/quadrants.png)"));
	// a typed name is kept over the edits
	dlg.nameEdit()->setText("w.png");
	emit dlg.nameEdit()->textEdited("w.png");
	dlg.rotate(IMAGE_QUARTER_TURN);
	QCOMPARE(dlg.insert().target, dir.path() + "/images/w.png");
}

// EDIT-MD-2: an image of the directory is linked where it is until it is edited
void TestMarkdownTools::test_dialog_in_place()
{
	QTemporaryDir dir;
	QVERIFY(copyInto(QUADRANTS, dir.path() + "/images/q.png"));
	InsertImageDialog dlg(dir.path() + "/images/q.png", QImage(), dir.path(), "pics");
	QVERIFY(!dlg.folderEdit()->isEnabled() && !dlg.nameEdit()->isEnabled());
	QCOMPARE(dlg.insertedLine(), QString("![q](images/q.png)"));
	QVERIFY(!dlg.insert().writesFile());
	// its folder is not remembered
	QCOMPARE(dlg.folder(), QString());
	dlg.rotate(IMAGE_QUARTER_TURN);
	QVERIFY(dlg.folderEdit()->isEnabled() && dlg.nameEdit()->isEnabled());
	QCOMPARE(dlg.insert().target, dir.path() + "/images/q-1.png");
	QVERIFY(dlg.insert().writesFile());
	dlg.reset();
	QVERIFY(!dlg.insert().writesFile());
}

// EDIT-MD-2/3: an SVG without the edits, a pasted image numbered
void TestMarkdownTools::test_dialog_svg_and_pasted()
{
	QTemporaryDir dir;
	InsertImageDialog svg(absolute(ARROW), QImage(), dir.path(), "images");
	QVERIFY(svg.loaded());
	QVERIFY(!svg.widthBox()->isEnabled());
	svg.rotate(IMAGE_QUARTER_TURN);
	QVERIFY(svg.insert().edits.isEmpty());
	QCOMPARE(svg.insertedLine(), QString("![arrow](images/arrow.svg)"));

	InsertImageDialog pasted(QString(), QImage(QUADRANTS), dir.path(), "images");
	QVERIFY(pasted.loaded());
	QCOMPARE(pasted.nameEdit()->text(), QString("image-1.png"));
	QCOMPARE(pasted.altEdit()->text(), QString());
	QCOMPARE(pasted.insertedLine(), QString("![](images/image-1.png)"));
	QCOMPARE(pasted.folder(), QString("images"));
	QVERIFY(pasted.insert().isPasted() && pasted.insert().writesFile());
}

// EDIT-MD-3: a handle dragged changes the crop and the numbers
void TestMarkdownTools::test_crop_drag()
{
	QTemporaryDir dir;
	InsertImageDialog dlg(absolute(QUADRANTS), QImage(), dir.path(), "images");
	dlg.show();
	QVERIFY(QTest::qWaitForWindowExposed(&dlg));
	ImageCropView* view = dlg.cropView();
	QRectF area = view->imageArea();
	QPoint from = area.bottomRight().toPoint();
	QPoint to = area.center().toPoint();
	drag(view, from, to);
	QRect crop = view->crop();
	QCOMPARE(crop.topLeft(), QPoint(0, 0));
	QVERIFY2(qAbs(crop.width() - 20) <= 1 && qAbs(crop.height() - 15) <= 1,
	         qPrintable(QString("%1x%2").arg(crop.width()).arg(crop.height())));
	QCOMPARE(dlg.insert().edits.crop, crop);
	// moved from inside, clear of the handles, and kept in the image
	dlg.setCrop(QRect(0, 0, 30, 20));
	crop = view->crop();
	QPoint inside = (area.topLeft() + QPointF(12, 10)).toPoint();
	drag(view, inside, inside + QPoint(100, 100));
	QCOMPARE(view->crop().size(), crop.size());
	QCOMPARE(view->crop().bottomRight(), QPoint(39, 29));
}

// EDIT-MD-1/2: Insert Image in Edit and on the text toolbar of a writable Markdown tab only
void TestMarkdownTools::test_window_actions()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("texts/notes.md"));
	QVERIFY(window.actionInsertImage->isEnabled());
	QVERIFY(window.textToolBar->actions().contains(window.actionInsertImage));
	QVERIFY(window.menuEdit->actions().contains(window.actionInsertImage));
	QVERIFY(!window.actionInsertImage->icon().isNull());
	QVERIFY(window.actionInsertImage->shortcuts().contains(QKeySequence("Ctrl+Shift+I")));
	window.actionReadOnly->trigger();
	QVERIFY(!window.actionInsertImage->isEnabled());
	window.actionReadOnly->trigger();
	QVERIFY(window.actionInsertImage->isEnabled());
	// a C++ text has Go to Line alone
	QVERIFY(window.openFile("texts/blink.cpp"));
	QVERIFY(!window.actionInsertImage->isEnabled());
	QVERIFY(!window.textToolBar->actions().contains(window.actionInsertImage));
	QVERIFY(window.textToolBar->actions().contains(window.actionGoToLine));
	// a mode switch to Markdown brings it
	QVERIFY(window.setDocumentMode(window.currentText(), MODE_MARKDOWN));
	QVERIFY(window.textToolBar->actions().contains(window.actionInsertImage));
	QVERIFY(window.openFile("diagrams/hierarchy.graphml"));
	QVERIFY(!window.actionInsertImage->isEnabled());
}

// EDIT-MD-2: a pasted image file or image is offered, not typed; another text pastes the text
void TestMarkdownTools::test_paste_offer()
{
	TextDocument doc(MODE_MARKDOWN);
	doc.setText("a");
	QSignalSpy spy(&doc, &TextDocument::imageOffered);
	QMimeData* files = new QMimeData;
	files->setUrls(QList<QUrl>() << QUrl::fromLocalFile(absolute(QUADRANTS)));
	files->setText(absolute(QUADRANTS));
	QApplication::clipboard()->setMimeData(files);
	doc.editor()->paste();
	QCOMPARE(doc.text(), QString("a"));
	QTRY_COMPARE(spy.count(), 1);
	QCOMPARE(spy.at(0).at(0).toString(), absolute(QUADRANTS));

	QMimeData* image = new QMimeData;
	image->setImageData(QImage(QUADRANTS));
	QApplication::clipboard()->setMimeData(image);
	doc.editor()->paste();
	QTRY_COMPARE(spy.count(), 2);
	QVERIFY(spy.at(1).at(0).toString().isEmpty());
	QCOMPARE(qvariant_cast<QImage>(spy.at(1).at(1)).size(), QSize(40, 30));

	// a file that is no image is pasted as its text
	QMimeData* other = new QMimeData;
	other->setUrls(QList<QUrl>() << QUrl::fromLocalFile(absolute("texts/notes.md")));
	other->setText("notes");
	QApplication::clipboard()->setMimeData(other);
	doc.editor()->paste();
	QVERIFY(doc.text().contains("notes"));
	QCOMPARE(spy.count(), 2);

	// a plain text pastes the path of an image file
	QVERIFY(doc.setMode(MODE_TEXT));
	doc.setText("");
	QMimeData* again = new QMimeData;
	again->setUrls(QList<QUrl>() << QUrl::fromLocalFile(absolute(QUADRANTS)));
	again->setText("q");
	QApplication::clipboard()->setMimeData(again);
	doc.editor()->paste();
	QCOMPARE(doc.text(), QString("q"));
	QCOMPARE(spy.count(), 2);
}

// EDIT-IO-8: the title, the description, the namespaces and the RDF description written
void TestMarkdownTools::test_record_svg_source()
{
	QByteArray svg = "<?xml version=\"1.0\"?>\n<svg width=\"10\" height=\"10\"\n xmlns=\"http://www.w3.org/2000/svg\" version=\"1.2\">\n"
	                 "<title>Qt SVG Document</title>\n<desc>Generated with Qt</desc>\n<defs>\n</defs>\n</svg>\n";
	QTemporaryDir dir;
	QVERIFY(copyInto(DIAGRAM, dir.path() + "/d.graphml"));
	QString out = QString::fromUtf8(recordSvgSource(svg, dir.path() + "/images/d.svg", dir.path() + "/d.graphml",
	                                                "Lights & <signals>", "Courier New"));
	QVERIFY(out.contains("<title>Lights &amp; &lt;signals&gt;</title>"));
	QVERIFY(out.contains("<desc>Generated by CyberiadaPro from d.graphml</desc>"));
	QVERIFY(out.contains("xmlns:cyb=\"https://cyberiada.dev/ns/svg#\""));
	QVERIFY(out.contains("xmlns:dc=\"http://purl.org/dc/elements/1.1/\""));
	QVERIFY(out.contains("<dc:source>../d.graphml</dc:source>"));
	QVERIFY(out.contains("<cyb:sourceDigest>" + fileDigest(dir.path() + "/d.graphml") + "</cyb:sourceDigest>"));
	QVERIFY(out.indexOf("<metadata>") > out.indexOf("</desc>") && out.indexOf("<metadata>") < out.indexOf("<defs>"));
	QVERIFY(fileDigest(dir.path() + "/d.graphml").startsWith("sha256:"));
	// sha256: and 32 bytes in hex
	QCOMPARE(fileDigest(dir.path() + "/d.graphml").size(), QString("sha256:").size() + 64);
	QVERIFY(fileDigest(dir.path() + "/none.graphml").isEmpty());
	// read back
	QFile file(dir.path() + "/images/d.svg");
	QDir().mkpath(dir.path() + "/images");
	QVERIFY(file.open(QIODevice::WriteOnly));
	file.write(out.toUtf8());
	file.close();
	SvgSource read = readSvgSource(dir.path() + "/images/d.svg");
	QCOMPARE(read.graphml, QDir::cleanPath(dir.path() + "/d.graphml"));
	QCOMPARE(read.digest, fileDigest(dir.path() + "/d.graphml"));
	QCOMPARE(read.font, QString("Courier New"));
	// an SVG without metadata: no source, the font it draws in
	SvgSource plain = readSvgSource(ARROW);
	QVERIFY(plain.graphml.isEmpty() && plain.digest.isEmpty());
}

static QImage drawn(const QString& svg)
{
	QSvgRenderer renderer(svg);
	QImage image(renderer.defaultSize(), QImage::Format_ARGB32);
	image.fill(Qt::white);
	QPainter painter(&image);
	renderer.render(&painter);
	return image;
}

// EDIT-IO-8, EDIT-MD-5: a diagram drawn off any tab records its file, the same drawing as
// without the metadata
void TestMarkdownTools::test_render_diagram_file()
{
	QTemporaryDir dir;
	QString graphml = dir.path() + "/diagrams/g.graphml";
	QVERIFY(copyInto(DIAGRAM, graphml));
	QString error;
	QVERIFY2(renderDiagramFile(graphml, dir.path() + "/diagrams/g.svg", QString(), &error), qPrintable(error));
	SvgSource read = readSvgSource(dir.path() + "/diagrams/g.svg");
	QCOMPARE(read.graphml, graphml);
	QCOMPARE(read.digest, fileDigest(graphml));
	QCOMPARE(read.font, FontManager::instance().bundledFamily());
	QString text = QString::fromUtf8(fileBytes(dir.path() + "/diagrams/g.svg"));
	QVERIFY(text.contains("<title>test-geometry-1</title>"));
	QVERIFY(text.contains("<dc:source>g.graphml</dc:source>"));
	// the plain export draws the same picture
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile(graphml));
	QVERIFY(renderScene(window.getScene(), dir.path() + "/plain.svg", &error, RENDER_DEFAULT_DPI,
	                    FontManager::instance().bundledFamily()));
	QVERIFY(!QString::fromUtf8(fileBytes(dir.path() + "/plain.svg")).contains("<metadata>"));
	QCOMPARE(drawn(dir.path() + "/diagrams/g.svg"), drawn(dir.path() + "/plain.svg"));
	QCOMPARE(diagramFileTitle(graphml), QString("test-geometry-1"));
	QVERIFY(!renderDiagramFile(dir.path() + "/none.graphml", dir.path() + "/none.svg", QString(), &error));
}

// EDIT-MD-6: the images, linked or bare, their lines and columns; none in code
void TestMarkdownTools::test_markdown_images()
{
	QString text =
		"# Images\n"
		"A ![bare](images/a.png \"title\") and [![linked \\] one](d/x.svg)](d/x.graphml) here.\n"
		"`![code](no.png)` and ![spaced](<my dir/b.svg>)\n"
		"```\n![fenced](no.png)\n```\n"
		"[text](not-an-image.md)\n";
	QList<MarkdownImage> images = markdownImages(text);
	QCOMPARE(images.size(), 3);
	QCOMPARE(images[0].line, 2);
	QCOMPARE(images[0].column, 3);
	QCOMPARE(images[0].alt, QString("bare"));
	QCOMPARE(images[0].target, QString("images/a.png"));
	QVERIFY(images[0].link.isEmpty());
	QCOMPARE(images[1].column, 37);
	QCOMPARE(images[1].alt, QString("linked \\] one"));
	QCOMPARE(images[1].target, QString("d/x.svg"));
	QCOMPARE(images[1].link, QString("d/x.graphml"));
	QCOMPARE(images[2].line, 3);
	QCOMPARE(images[2].target, QString("my dir/b.svg"));
	QCOMPARE(markdownDiagramLine("Lights", "/t/d/x.svg", "/t/d/x.graphml", "/t"),
	         QString("[![Lights](d/x.svg)](d/x.graphml)"));
}

// EDIT-MD-6: the diagram of the link, the recorded one, the sibling; up to date by the digest
void TestMarkdownTools::test_diagram_images()
{
	QTemporaryDir dir;
	QString base = dir.path();
	QString error;
	// a: linked, recorded; b: a sibling without metadata; c: recorded elsewhere, bare
	QVERIFY(copyInto(DIAGRAM, base + "/d/a.graphml"));
	QVERIFY(renderDiagramFile(base + "/d/a.graphml", base + "/d/a.svg", QString(), &error));
	QVERIFY(copyInto(DIAGRAM, base + "/d/b.graphml"));
	QVERIFY(copyInto(ARROW, base + "/d/b.svg"));
	QVERIFY(copyInto(DIAGRAM, base + "/src/c.graphml"));
	QVERIFY(renderDiagramFile(base + "/src/c.graphml", base + "/img/c.svg", QString(), &error));
	QVERIFY(copyInto(ARROW, base + "/img/plain.svg"));
	QString text = "[![A](d/a.svg)](d/a.graphml)\n\n![B](d/b.svg)\n\n![C](img/c.svg)\n\n![P](img/plain.svg)\n";
	QList<DiagramImage> images = markdownDiagramImages(text, base);
	QCOMPARE(images.size(), 3);
	QCOMPARE(images[0].graphml, base + "/d/a.graphml");
	QVERIFY(images[0].upToDate);
	QCOMPARE(images[1].graphml, base + "/d/b.graphml");
	QVERIFY(!images[1].upToDate);
	QCOMPARE(images[2].graphml, base + "/src/c.graphml");
	QVERIFY(images[2].upToDate);
	QCOMPARE(images[2].image.line, 5);
	// a changed diagram makes its image out of date
	QFile a(base + "/d/a.graphml");
	QVERIFY(a.open(QIODevice::Append));
	a.write("\n");
	a.close();
	QVERIFY(!markdownDiagramImages(text, base)[0].upToDate);
	// the link wins over the recorded source
	QString other = "[![A](d/a.svg)](d/b.graphml)\n";
	QCOMPARE(markdownDiagramImages(other, base)[0].graphml, base + "/d/b.graphml");
}

// EDIT-MD-7: the out-of-date images once each, all of them on demand, the font kept
void TestMarkdownTools::test_update_diagram_images()
{
	QTemporaryDir dir;
	QString base = dir.path();
	QString error;
	QVERIFY(copyInto(DIAGRAM, base + "/d/a.graphml"));
	QVERIFY(renderDiagramFile(base + "/d/a.graphml", base + "/d/a.svg", "DejaVu Sans Mono", &error));
	QVERIFY(copyInto(DIAGRAM, base + "/d/b.graphml"));
	QVERIFY(copyInto(ARROW, base + "/d/b.svg"));
	QString text = "![A](d/a.svg)\n\n![B](d/b.svg)\n\n![B again](d/b.svg)\n\n![C](d/c.svg)\n";
	QVERIFY(copyInto(ARROW, base + "/d/c.svg"));
	QVERIFY(copyInto(DIAGRAM, base + "/d/c.graphml"));
	QFile::remove(base + "/d/c.graphml");
	QList<DiagramImage> images = markdownDiagramImages(text, base);
	QCOMPARE(images.size(), 3);
	QList<DiagramUpdate> updates = updateDiagramImages(images, false, QString());
	QCOMPARE(updates.size(), 1);
	QVERIFY(updates[0].updated);
	QCOMPARE(updates[0].image.svg, base + "/d/b.svg");
	QVERIFY(markdownDiagramImages(text, base)[1].upToDate);
	// all: each SVG once, a's font kept
	QFile a(base + "/d/a.graphml");
	QVERIFY(a.open(QIODevice::Append));
	a.write("\n");
	a.close();
	updates = updateDiagramImages(markdownDiagramImages(text, base), true, QString());
	QCOMPARE(updates.size(), 2);
	QCOMPARE(readSvgSource(base + "/d/a.svg").font, QString("DejaVu Sans Mono"));
	for (const DiagramImage& i : markdownDiagramImages(text, base)) QVERIFY(i.upToDate);
	// a recorded diagram that is gone fails
	QVERIFY(copyInto(DIAGRAM, base + "/e/e.graphml"));
	QVERIFY(renderDiagramFile(base + "/e/e.graphml", base + "/e/e.svg", QString(), &error));
	QFile::remove(base + "/e/e.graphml");
	updates = updateDiagramImages(markdownDiagramImages("![E](e/e.svg)\n", base), false, QString());
	QCOMPARE(updates.size(), 1);
	QVERIFY(!updates[0].updated && !updates[0].error.isEmpty());
}

// EDIT-MD-5: a file outside copied into the folder, one inside drawn beside itself, a tab
void TestMarkdownTools::test_insert_diagram_dialog()
{
	QTemporaryDir dir;
	QString base = dir.path() + "/text";
	QDir().mkpath(base);
	InsertDiagramDialog dlg(QList<DiagramTab>(), base, "diagrams/2", "Courier New");
	QVERIFY(dlg.fileChoice()->isChecked() && !dlg.tabChoice()->isEnabled());
	QVERIFY(!dlg.insertButton()->isEnabled());
	dlg.setFile(absolute(DIAGRAM));
	QVERIFY(dlg.copies());
	QCOMPARE(dlg.target(), base + "/diagrams/2/geometry.graphml");
	QCOMPARE(dlg.svg(), base + "/diagrams/2/geometry.svg");
	QCOMPARE(dlg.alt(), QString("test-geometry-1"));
	QCOMPARE(dlg.folder(), QString("diagrams/2"));
	QCOMPARE(dlg.insertedLine(), QString("[![test-geometry-1](diagrams/2/geometry.svg)](diagrams/2/geometry.graphml)"));
	QVERIFY(dlg.insertButton()->isEnabled());
	// a typed alt text stays over another source
	dlg.altEdit()->setText("Lights");
	emit dlg.altEdit()->textEdited("Lights");
	QVERIFY(copyInto(DIAGRAM, base + "/own/g.graphml"));
	dlg.setFile(base + "/own/g.graphml");
	QVERIFY(!dlg.copies() && !dlg.folderEdit()->isEnabled());
	QCOMPARE(dlg.target(), base + "/own/g.graphml");
	QCOMPARE(dlg.folder(), QString());
	QCOMPARE(dlg.insertedLine(), QString("[![Lights](own/g.svg)](own/g.graphml)"));
	QVERIFY(!dlg.noteLabel()->text().isEmpty());
	dlg.setFile(base + "/none.graphml");
	QVERIFY(!dlg.insertButton()->isEnabled());

	DiagramTab tab;
	tab.title = "geometry.graphml";
	tab.path = absolute(DIAGRAM);
	tab.modified = true;
	InsertDiagramDialog tabs(QList<DiagramTab>() << tab, base, "diagrams", "Courier New");
	QVERIFY(tabs.tabChoice()->isChecked());
	QCOMPARE(tabs.source(), absolute(DIAGRAM));
	QCOMPARE(tabs.target(), base + "/diagrams/geometry.graphml");
	QVERIFY(tabs.noteLabel()->text().contains("unsaved"));
}

// EDIT-MD-1/7: Insert Diagram and Update Diagram Images in a Markdown tab; Update in a
// read-only one too; the results in Diagnostics by their lines
void TestMarkdownTools::test_window_diagram_actions()
{
	QTemporaryDir dir;
	QString base = dir.path();
	QVERIFY(copyInto(DIAGRAM, base + "/d/a.graphml"));
	QVERIFY(copyInto(ARROW, base + "/d/a.svg"));
	QFile md(base + "/text.md");
	QVERIFY(md.open(QIODevice::WriteOnly));
	md.write("# Text\n\n![A](d/a.svg)\n");
	md.close();
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile(base + "/text.md"));
	TextDocument* text = window.currentText();
	QVERIFY(window.actionInsertDiagram->isEnabled() && window.actionUpdateDiagramImages->isEnabled());
	QVERIFY(window.textToolBar->actions().contains(window.actionInsertDiagram));
	QVERIFY(window.textToolBar->actions().contains(window.actionUpdateDiagramImages));
	QVERIFY(window.menuEdit->actions().contains(window.actionInsertDiagram));
	QVERIFY(window.menuDocument->actions().contains(window.actionUpdateDiagramImages));
	window.actionReadOnly->trigger();
	QVERIFY(!window.actionInsertDiagram->isEnabled() && window.actionUpdateDiagramImages->isEnabled());
	QList<PluginDiagnostic> results = window.updateDiagramImages(text, false);
	QCOMPARE(results.size(), 1);
	QCOMPARE(results[0].severity, QString("info"));
	QCOMPARE(results[0].line, 3);
	QVERIFY(results[0].message.contains("d/a.svg"));
	QVERIFY(window.updateDiagramImages(text, false).isEmpty());
	QCOMPARE(window.updateDiagramImages(text, true).size(), 1);
	window.actionReadOnly->trigger();
	// an untitled text has no images to update
	window.actionNewMarkdown->trigger();
	QVERIFY(!window.actionUpdateDiagramImages->isEnabled());
}

// EDIT-MODE-14: a click on a rendered link to a file the editor opens goes to the tab, not the system
void TestMarkdownTools::test_rendered_link_opens_tab()
{
	QTemporaryDir dir;
	QVERIFY(copyInto(DIAGRAM, dir.path() + "/d.graphml"));
	QFile md(dir.path() + "/text.md");
	QVERIFY(md.open(QIODevice::WriteOnly));
	md.write("[![A](d.svg)](d.graphml)\n");
	md.close();
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile(dir.path() + "/text.md"));
	TextDocument* text = window.currentText();
	window.actionReadOnly->trigger();
	QVERIFY(text->showsRendered());
	MarkdownView* view = text->page()->findChild<MarkdownView*>();
	QVERIFY(view);
	emit view->anchorClicked(QUrl("d.graphml"));
	QTRY_VERIFY(window.currentDocument() && window.currentDocument()->filePath().endsWith("d.graphml"));
}

static QStringList linkTexts(const QString& text)
{
	QStringList out;
	for (const MarkdownLink& l : markdownLinks(text)) {
		out << QString("%1:%2 %3%4").arg(l.line).arg(l.column).arg(l.image ? "!" : "").arg(l.target);
	}
	return out;
}

// EDIT-MD-8: the links, the images inside them too, the definitions; none in code
void TestMarkdownTools::test_markdown_links()
{
	QString text =
		"See [a](x.md) and [![i](d/x.svg)](d/x.graphml), <https://site.org>.\n"
		"`[code](no.md)` [b](<my file.md> \"t\")\n"
		"```\n[fenced](no.md)\n```\n"
		"[ref]: defs/r.md \"title\"\n"
		"[c][ref] is a reference, not a target\n";
	QCOMPARE(linkTexts(text), QStringList()
	         << "1:5 x.md" << "1:19 d/x.graphml" << "1:20 !d/x.svg" << "2:17 my file.md" << "6:1 defs/r.md");
}

static QStringList headingTexts(const QString& text)
{
	QStringList out;
	for (const MarkdownHeading& h : markdownHeadings(text)) {
		out << QString("%1 %2 %3").arg(h.line).arg(h.level).arg(h.anchor);
	}
	return out;
}

// EDIT-MD-8/9: the ATX and the setext headings, their anchors as GitHub numbers them
void TestMarkdownTools::test_markdown_headings()
{
	QString text = QString::fromUtf8(
		"# Title #\n"
		"Intro\n\n"
		"## 4.2 Составные состояния\n"
		"## A [link](x.md) and `code`\n"
		"## Title\n"
		"Setext one\n===\n\n"
		"Setext two\n---\n\n"
		"- item\n---\n\n"
		"| a | b |\n|---|---|\n\n"
		"```\n# not a heading\n```\n"
		"# Title\n");
	QCOMPARE(headingTexts(text), QStringList()
	         << "1 1 title"
	         << QString::fromUtf8("4 2 42-составные-состояния")
	         << "5 2 a-link-and-code"
	         << "6 2 title-1"
	         << "7 1 setext-one"
	         << "10 2 setext-two"
	         << "22 1 title-2");
	// Ctrl+click and the check read the same anchors
	TextDocument doc(MODE_MARKDOWN);
	doc.setText(text);
	doc.openLink("#title-2");
	QCOMPARE(doc.currentLine(), 22);
	doc.openLink("#setext-two");
	QCOMPARE(doc.currentLine(), 10);
}

// EDIT-MD-8: missing files and images, anchors of this and of another text, an open tab's
// unsaved heading; the sites, the comments and the code are passed over
void TestMarkdownTools::test_markdown_check()
{
	QTemporaryDir dir;
	QString base = dir.path();
	QVERIFY(copyInto(QUADRANTS, base + "/images/q.png"));
	QFile other(base + "/other.md");
	QVERIFY(other.open(QIODevice::WriteOnly));
	other.write("# Other\n\n## Saved heading\n");
	other.close();
	QString text =
		"# Main\n\n"
		"[ok](#main) [bad](#gone) ![ok](images/q.png) ![bad](images/none.png)\n"
		"[ok](other.md#saved-heading) [bad](other.md#unsaved-heading) [gone](none.md)\n"
		"[site](https://example.org/none) [mail](mailto:a@b.c) [//]: # (comment)\n"
		"`[code](none.md)`\n";
	auto none = [](const QString&) { return QString(); };
	QList<MarkdownFinding> findings = markdownCheck(text, base, none);
	QStringList messages;
	for (const MarkdownFinding& f : findings) messages << QString("%1:%2 %3").arg(f.line).arg(f.column).arg(f.message);
	QCOMPARE(messages, QStringList()
	         << "3:13 no heading for #gone"
	         << "3:46 image not found: images/none.png"
	         << "4:30 no heading for other.md#unsaved-heading"
	         << "4:62 file not found: none.md");
	// the open tab's text counts, its unsaved heading too
	auto open = [&base](const QString& path) {
		return path == base + "/other.md" ? QString("# Other\n\n## Unsaved heading\n") : QString();
	};
	findings = markdownCheck(text, base, open);
	QCOMPARE(findings.size(), 4);
	QCOMPARE(findings[2].message, QString("no heading for other.md#saved-heading"));
	// an untitled text checks its anchors only
	findings = markdownCheck(text, QString(), none);
	QCOMPARE(findings.size(), 1);
	QCOMPARE(findings[0].message, QString("no heading for #gone"));
}

// EDIT-MD-6/8: a diagram image without a recorded source, out of date, linked to another
// diagram than it records, and one whose recorded diagram is gone
void TestMarkdownTools::test_check_diagram_images()
{
	QTemporaryDir dir;
	QString base = dir.path();
	QString error;
	QVERIFY(copyInto(DIAGRAM, base + "/d/a.graphml"));
	QVERIFY(copyInto(ARROW, base + "/d/a.svg"));
	QVERIFY(copyInto(DIAGRAM, base + "/d/b.graphml"));
	QVERIFY(renderDiagramFile(base + "/d/b.graphml", base + "/d/b.svg", QString(), &error));
	QVERIFY(copyInto(DIAGRAM, base + "/d/c.graphml"));
	QVERIFY(renderDiagramFile(base + "/d/c.graphml", base + "/d/c.svg", QString(), &error));
	QVERIFY(copyInto(DIAGRAM, base + "/d/e.graphml"));
	QVERIFY(renderDiagramFile(base + "/d/e.graphml", base + "/img/e.svg", QString(), &error));
	QFile::remove(base + "/d/e.graphml");
	QFile c(base + "/d/c.graphml");
	QVERIFY(c.open(QIODevice::Append));
	c.write("\n");
	c.close();
	QString text = "![a](d/a.svg)\n\n[![b](d/b.svg)](d/a.graphml)\n\n![c](d/c.svg)\n\n![e](img/e.svg)\n\n![b](d/b.svg)\n";
	QStringList messages;
	for (const MarkdownFinding& f : markdownCheck(text, base, nullptr)) messages << QString("%1 %2").arg(f.line).arg(f.message);
	QCOMPARE(messages, QStringList()
	         << "1 diagram image without a recorded source: Update Diagram Images records d/a.graphml"
	         << "3 linked to d/a.graphml, its SVG names d/b.graphml"
	         << "5 diagram image out of date: d/c.graphml changed"
	         << "7 no diagram file d/e.graphml");
}

// EDIT-MD-8: F7 on a Markdown tab, read-only too; the findings in Diagnostics by line:column,
// a click putting the cursor there
void TestMarkdownTools::test_window_check()
{
	QTemporaryDir dir;
	QFile md(dir.path() + "/text.md");
	QVERIFY(md.open(QIODevice::WriteOnly));
	md.write("# Text\n\nSee [nothing](#nowhere) here.\n");
	md.close();
	CyberiadaSMEditorWindow window;
	QVERIFY(!window.actionCheck->isEnabled());
	QVERIFY(window.openFile(dir.path() + "/text.md"));
	TextDocument* text = window.currentText();
	QVERIFY(window.actionCheck->isEnabled());
	QVERIFY(window.actionCheck->shortcuts().contains(QKeySequence("F7")));
	window.actionReadOnly->trigger();
	QVERIFY(window.actionCheck->isEnabled());
	window.actionReadOnly->trigger();
	QList<PluginDiagnostic> findings = window.checkMarkdown(text);
	QCOMPARE(findings.size(), 1);
	QCOMPARE(findings[0].severity, QString("warning"));
	QTreeWidget* list = window.pluginDock()->diagnostics();
	QCOMPARE(list->topLevelItemCount(), 1);
	QTreeWidgetItem* item = list->topLevelItem(0);
	QVERIFY(item->text(1).contains("3:5") || item->text(2).contains("3:5") || item->text(3).contains("3:5"));
	emit list->itemActivated(item, 0);
	QCOMPARE(text->currentLine(), 3);
	QCOMPARE(text->editor()->textCursor().positionInBlock(), 4);
	// a plain text has nothing to check
	QVERIFY(window.openFile("texts/blink.cpp"));
	QVERIFY(!window.actionCheck->isEnabled());
}

QTEST_MAIN(TestMarkdownTools)

#include "l4-markdown-tools.moc"
