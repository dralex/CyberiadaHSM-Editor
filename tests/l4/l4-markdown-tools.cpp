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
#include <QUndoStack>
#include "markdown_tools.h"
#include "dialogs/insert_image_dialog.h"
#include "code_editor.h"
#include "text_document.h"
#include "smeditor_window.h"
#include "cyberiada_constants.h"
#include "document_mode.h"

// a 40x30 image: red, green on the top, blue, yellow at the bottom
static const char* const QUADRANTS = "texts/images/quadrants.png";
static const char* const ARROW = "texts/images/arrow.svg";

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

QTEST_MAIN(TestMarkdownTools)

#include "l4-markdown-tools.moc"
