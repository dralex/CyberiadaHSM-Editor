/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process Markdown rendering test (see docs/TESTING.md, L4)
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
#include <QAbstractTextDocumentLayout>
#include <QFile>
#include <QImage>
#include <QPageSize>
#include <QPainter>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextList>
#include <QTextTable>
#include "markdown_importer.h"
#include "markdown_objects.h"
#include "markdown_pdf.h"
#include "markdown_view.h"
#include "svg_markers.h"
#include "text_document.h"
#include "smeditor_window.h"
#include "settings_manager.h"
#include "editable_text_item.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_editor_comment_item.h"

// the laws of the Markdown rendering (EDIT-MODE-14/17, EDIT-TEXT-14, docs/MARKDOWN.md):
// the formula spans, the images, the HTML tags, the markers, the baseline, the image
// paths, the structure, the sizes of the objects, the rendered comments and the PDF; no
// glyph is compared (docs/TESTING.md)
class TestMarkdown: public QObject {
	Q_OBJECT

private slots:
	void test_math_spans();
	void test_images_kept();
	void test_html_tags();
	void test_path_ends();
	void test_expand_markers();
	void test_marker_drawn();
	void test_image_paths();
	void test_inline_baseline();
	void test_structure();
	void test_layout();
	void test_quotes();
	void test_heading_anchor();
	void test_tab_rendered();
	void test_object_sizes();
	void test_comment_rendered();
	void test_pdf();

private:
	static QString read(const QString& path);
	static QString structure(QTextDocument* doc);
	static QList<QTextCharFormat> objects(const QString& text, int type);
	static QImage draw(QTextDocument* doc);
	static QByteArray pdfFile(const QString& text, const QString& baseDir, int* pages);
};

QString TestMarkdown::read(const QString& path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) return QString();
	return QString::fromUtf8(file.readAll());
}

// the blocks and the runs of a document as a structure good file holds them
QString TestMarkdown::structure(QTextDocument* doc)
{
	QString out;
	for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
		QTextBlockFormat bf = b.blockFormat();
		QString line = "block";
		QTextTable* table = QTextCursor(b).currentTable();
		if (table) {
			QTextTableCell cell = table->cellAt(b.position());
			line += QString(" cell %1,%2/%3x%4").arg(cell.row()).arg(cell.column())
				.arg(table->rows()).arg(table->columns());
			if (cell.format().fontWeight() >= QFont::Bold) line += " th";
		}
		if (bf.headingLevel() > 0) line += QString(" h%1").arg(bf.headingLevel());
		if (b.textList())
			line += QString(" list %1/%2 #%3").arg(b.textList()->format().style())
				.arg(b.textList()->format().indent()).arg(b.textList()->itemNumber(b));
		if (bf.hasProperty(MarkdownQuoteLevel))
			line += QString(" quote %1").arg(bf.intProperty(MarkdownQuoteLevel));
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
		if (bf.hasProperty(QTextFormat::BlockCodeLanguage))
			line += " code " + bf.stringProperty(QTextFormat::BlockCodeLanguage);
		if (bf.hasProperty(QTextFormat::BlockMarker)) line += QString(" marker %1").arg(int(bf.marker()));
#endif
		if (bf.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth)) line += " rule";
		if (bf.alignment() & Qt::AlignHCenter) line += " centre";
		line += ":";
		// the runs merged by what is compared, whatever else splits the fragments
		QString lastAttrs, run;
		auto flush = [&]() { if (!run.isEmpty()) line += " {" + run + lastAttrs + "}"; run.clear(); };
		for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it) {
			QTextFragment f = it.fragment();
			QTextCharFormat cf = f.charFormat();
			QString text = f.text();
			if (cf.objectType() == MarkdownMathObject)
				text = "[math " + cf.stringProperty(MarkdownTex) + (cf.boolProperty(MarkdownDisplay) ? " display]" : "]");
			else if (cf.objectType() == MarkdownImageObject)
				text = "[image " + cf.stringProperty(MarkdownImageSource) + "|" + cf.stringProperty(MarkdownImageAlt) + "]";
			QString attrs;
			if (cf.fontWeight() >= QFont::Bold) attrs += " b";
			if (cf.fontItalic()) attrs += " i";
			if (cf.fontStrikeOut()) attrs += " s";
			if (cf.fontFixedPitch()) attrs += " code";
			if (cf.isAnchor()) attrs += " a=" + cf.anchorHref();
			if (attrs != lastAttrs) flush();
			lastAttrs = attrs;
			// a line break is a mark of its own: the run Qt draws it in differs by version
			const QStringList pieces = text.split(QChar::LineSeparator);
			for (int k = 0; k < pieces.size(); k++) {
				if (k > 0) {
					flush();
					line += " <br>";
				}
				run += pieces.at(k);
			}
		}
		flush();
		out += line + "\n";
	}
	return out;
}

QImage TestMarkdown::draw(QTextDocument* doc)
{
	QImage image(doc->size().toSize(), QImage::Format_ARGB32);
	image.fill(Qt::white);
	QPainter painter(&image);
	doc->drawContents(&painter);
	painter.end();
	return image;
}

// the objects of a type in a document read from the text, in their order
QList<QTextCharFormat> TestMarkdown::objects(const QString& text, int type)
{
	QTextDocument doc;
	importMarkdown(&doc, text, QString());
	QList<QTextCharFormat> found;
	for (QTextBlock b = doc.begin(); b.isValid(); b = b.next())
		for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it)
			if (it.fragment().charFormat().objectType() == type)
				for (int k = 0; k < it.fragment().length(); k++) found.append(it.fragment().charFormat());
	return found;
}

// the formulas are where md4c sees them: never in code, an escaped or a lone
// dollar is money
void TestMarkdown::test_math_spans()
{
	QList<QTextCharFormat> m = objects("a $x$ b", MarkdownMathObject);
	QCOMPARE(m.size(), 1);
	QCOMPARE(m.at(0).stringProperty(MarkdownTex), QString("x"));
	QVERIFY(!m.at(0).boolProperty(MarkdownDisplay));

	m = objects("$$\\frac{a}{b}$$", MarkdownMathObject);
	QCOMPARE(m.size(), 1);
	QVERIFY(m.at(0).boolProperty(MarkdownDisplay));
	QCOMPARE(m.at(0).stringProperty(MarkdownTex), QString("\\frac{a}{b}"));

	QCOMPARE(objects("`$x$` and\n\n```\n$y$\n```\n", MarkdownMathObject).size(), 0);
	QCOMPARE(objects("the price is \\$5 and $6", MarkdownMathObject).size(), 0);
	QCOMPARE(objects("| a | $x^2$ |\n|---|---|\n| $y$ | b |\n", MarkdownMathObject).size(), 2);

	m = objects(QString::fromUtf8("состояние $s_1$"), MarkdownMathObject);
	QCOMPARE(m.size(), 1);
	QCOMPARE(m.at(0).stringProperty(MarkdownTex), QString("s_1"));
}

// every image is one object: one without an alt text, one with a formatted alt
// (Qt's own reader dropped the first and split the second)
void TestMarkdown::test_images_kept()
{
	QList<QTextCharFormat> images = objects("![](a.svg) ![alt *b*](b.png \"t\")", MarkdownImageObject);
	QCOMPARE(images.size(), 2);
	QCOMPARE(images.at(0).stringProperty(MarkdownImageSource), QString("a.svg"));
	QCOMPARE(images.at(0).stringProperty(MarkdownImageAlt), QString());
	QCOMPARE(images.at(1).stringProperty(MarkdownImageAlt), QString("alt b"));
	QCOMPARE(images.at(1).toolTip(), QString("t"));
}

// a <br> breaks the line, a foreign tag is text, the text inside tags keeps its
// order and an unclosed tag ends with its paragraph: no text is lost after them
void TestMarkdown::test_html_tags()
{
	QCOMPARE(closeVoidTags("<br>"), QString("<br/>"));
	QCOMPARE(closeVoidTags("<BR >"), QString("<BR/>"));
	QCOMPARE(closeVoidTags("<b>x</b>"), QString("<b>x</b>"));
	QVERIFY(isForeignTag("<rundir>"));
	QVERIFY(isForeignTag("</name>"));
	QVERIFY(!isForeignTag("<sub>"));
	QVERIFY(!isForeignTag("<br>"));
	QTextDocument doc;
	importMarkdown(&doc, "| a | b<br>c |\n|---|---|\n| d | e |\n\nthe end", QString());
	// a line separator, which toPlainText() gives as a newline
	QVERIFY(doc.toPlainText().contains("b\nc"));
	QVERIFY(doc.toPlainText().contains("the end"));
	QVERIFY(doc.toPlainText().contains("d"));
	importMarkdown(&doc, "the file <rundir>/input.md and the rest", QString());
	QVERIFY(doc.toPlainText().contains("<rundir>/input.md and the rest"));
	// a comment shows nothing, over its lines too
	importMarkdown(&doc, "[a remark]: #\n\n<!-- first\nsecond -->\n\nx <!-- in\nline --> y", QString());
	QVERIFY2(!doc.toPlainText().contains("remark") && !doc.toPlainText().contains("second") &&
			 !doc.toPlainText().contains("line"), qPrintable(doc.toPlainText()));
	QVERIFY(doc.toPlainText().contains("x") && doc.toPlainText().contains("y"));
	importMarkdown(&doc, "a <b>bold `code`</b> c\n\nan <i>open tag\n\nthe last", QString());
	QString text = doc.toPlainText();
	QVERIFY2(text.contains("a bold code c"), qPrintable(text));
	QVERIFY2(text.contains("an open tag"), qPrintable(text));
	QVERIFY2(text.contains("the last"), qPrintable(text));
}

// the ends of the path data and the directions there, the arcs and the packed flags
void TestMarkdown::test_path_ends()
{
	SvgPathEnd start, end;
	svgPathEnds("m 30.5,18.5 h 51.25", &start, &end);
	QVERIFY(end.valid);
	QCOMPARE(end.point, QPointF(81.75, 18.5));
	QVERIFY(end.direction.x() > 0 && qFuzzyIsNull(end.direction.y()));

	start = end = SvgPathEnd();
	svgPathEnds("M0 0 C 0 10 10 10 10 0", &start, &end);
	QVERIFY(qFuzzyIsNull(start.direction.x()) && start.direction.y() > 0);
	QVERIFY(qFuzzyIsNull(end.direction.x()) && end.direction.y() < 0);

	start = end = SvgPathEnd();
	svgPathEnds("M 0 0 a10 10 0 0120 0", &start, &end);
	QCOMPARE(end.point, QPointF(20, 0));
	QVERIFY(qAbs(start.direction.x()) < 1e-6 && start.direction.y() < 0);
	QVERIFY(qAbs(end.direction.x()) < 1e-6 && end.direction.y() > 0);

	start = end = SvgPathEnd();
	svgPathEnds("m 0,0 10,0 0,10", &start, &end);
	QCOMPARE(end.point, QPointF(10, 10));
	QVERIFY(qFuzzyIsNull(end.direction.x()) && end.direction.y() > 0);

	start = end = SvgPathEnd();
	svgPathEnds("M 0 0 L 10 0 L 10 10 Z", &start, &end);
	QCOMPARE(end.point, QPointF(0, 0));
	QVERIFY(end.direction.x() < 0 && end.direction.y() < 0);
}

// a marker becomes a group after its element, in the element's frame, its ids dropped
void TestMarkdown::test_expand_markers()
{
	const QByteArray head =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:inkscape=\"http://www.inkscape.org/namespaces/inkscape\">"
		"<defs><marker id=\"m\" refX=\"1\" refY=\"2\" orient=\"auto\" style=\"overflow:visible\">"
		"<path id=\"mp\" d=\"M 0,0 L -2,1 V -1 Z\" inkscape:x=\"1\"/></marker>"
		"<marker id=\"r\" orient=\"auto-start-reverse\"><path d=\"M 0,0 L -1,1 Z\"/></marker></defs>";
	QByteArray svg = head +
		"<path d=\"M 0,0 H 10\" style=\"stroke-width:2;marker-end:url(#m)\" transform=\"translate(5,5)\"/></svg>";
	QString out = QString::fromUtf8(expandSvgMarkers(svg));
	QVERIFY(out.contains("<g transform=\"translate(5,5) translate(10,0) rotate(0) scale(2) translate(-1,-2)\">"
		"<path d=\"M 0,0 L -2,1 V -1 Z\" inkscape:x=\"1\"/></g>"));
	QCOMPARE(out.count("id=\"mp\""), 1);
	QVERIFY(QSvgRenderer(out.toUtf8()).isValid());

	// inherited from a group, a line
	out = QString::fromUtf8(expandSvgMarkers(head +
		"<g style=\"marker-end:url(#m)\"><line x1=\"0\" y1=\"0\" x2=\"0\" y2=\"10\"/></g></svg>"));
	QVERIFY(out.contains("translate(0,10) rotate(90) scale(1)"));

	// the start reversed
	out = QString::fromUtf8(expandSvgMarkers(head + "<path d=\"M 0,0 H 10\" marker-start=\"url(#r)\"/></svg>"));
	QVERIFY(out.contains("translate(0,0) rotate(180)"));

	// none, an unmarked shape, a file without markers
	svg = head + "<path d=\"M 0,0 H 10\" style=\"marker-end:none\"/><rect marker-end=\"url(#m)\"/></svg>";
	QCOMPARE(expandSvgMarkers(svg), svg);
	svg = "<svg xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M 0,0 H 10\"/></svg>";
	QCOMPARE(expandSvgMarkers(svg), svg);
}

// QtSvg draws the arrowhead only after the expansion
void TestMarkdown::test_marker_drawn()
{
	QFile file("texts/images/arrow.svg");
	QVERIFY(file.open(QIODevice::ReadOnly));
	QByteArray svg = file.readAll();
	auto pixel = [](const QByteArray& data) {
		const int scale = 4;
		QSvgRenderer renderer(data);
		QImage image(renderer.defaultSize() * scale, QImage::Format_ARGB32);
		image.fill(Qt::white);
		QPainter painter(&image);
		renderer.render(&painter);
		painter.end();
		// inside the head, off the line it ends
		return qGray(image.pixel(95 * scale, 23 * scale));
	};
	QVERIFY(pixel(svg) > 128);
	QVERIFY(pixel(expandSvgMarkers(svg)) < 128);
}

// an image is found next to the file, an untitled text finds only an absolute one
void TestMarkdown::test_image_paths()
{
	QString base = QDir("texts").absolutePath();
	QCOMPARE(markdownImagePath("images/arrow.svg", base), base + "/images/arrow.svg");
	QCOMPARE(markdownImagePath("images/arrow.svg", QString()), QString());
	QCOMPARE(markdownImagePath(base + "/images/arrow.svg", QString()), base + "/images/arrow.svg");
	QCOMPARE(markdownImagePath(QUrl::fromLocalFile(base + "/a b.svg").toString(), QString()), base + "/a b.svg");
	QCOMPARE(markdownImagePath("https://example.org/a.svg", base), QString());

	QTextDocument doc;
	importMarkdown(&doc, "![x](images/arrow.svg)", base);
	QTextBlock::iterator it = doc.begin().begin();
	QCOMPARE(it.fragment().charFormat().objectType(), int(MarkdownImageObject));
	QCOMPARE(it.fragment().charFormat().stringProperty(MarkdownImagePath), base + "/images/arrow.svg");
}

// an inline formula stands on the baseline of the text around it, lower or
// higher than the text
void TestMarkdown::test_inline_baseline()
{
	// without the engine a formula is its source, whose braces go below the baseline
#ifdef CYBERIADA_WITH_MATH
	const char* const examples[] = {"x $x$ x", "x $x^{2^{2^{2}}}$ x"};
#else
	const char* const examples[] = {"x $x$ x"};
#endif
	for (size_t i = 0; i < sizeof(examples) / sizeof(examples[0]); i++) {
		QTextDocument doc;
		doc.setTextWidth(400);
		importMarkdown(&doc, examples[i], QString());
		QImage image = draw(&doc);
		QTextLayout* layout = doc.begin().layout();
		QTextLine line = layout->lineAt(0);
		qreal left = layout->position().x();
		auto bottom = [&](qreal x0, qreal x1) {
			for (int y = image.height() - 1; y >= 0; y--)
				for (int x = int(left + x0); x < int(left + x1); x++)
					if (qGray(image.pixel(x, y)) < 128) return y;
			return -1;
		};
		int text = bottom(line.cursorToX(0), line.cursorToX(1));
		int formula = bottom(line.cursorToX(2), line.cursorToX(3));
		QVERIFY(text > 0);
		QVERIFY2(qAbs(text - formula) <= 1,
				 qPrintable(QString("%1: text %2, formula %3").arg(examples[i]).arg(text).arg(formula)));
	}
}

// EDIT-MODE-17: the examples read into the structure of their reviewed good files;
// a mismatch keeps the drawn structure for the review
void TestMarkdown::test_structure()
{
	const char* const examples[] = {"markdown", "notes"};
	QString base = QDir("texts").absolutePath();
	for (size_t i = 0; i < sizeof(examples) / sizeof(examples[0]); i++) {
		QString text = read(QString("texts/%1.md").arg(examples[i]));
		QVERIFY(!text.isEmpty());
		QTextDocument doc;
		importMarkdown(&doc, text, base);
		QString drawn = structure(&doc);
		QString good = read(QString("good/%1-structure-output.txt").arg(examples[i]));
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
		// no code language and marker before Qt 5.14: the block headers without them
		QStringList goodLines = good.split('\n');
		for (QString& l : goodLines) {
			int colon = l.indexOf(':');
			if (colon > 0)
				l = l.left(colon).remove(QRegularExpression(" (code [^ ]*|marker \\d+)")) + l.mid(colon);
		}
		good = goodLines.join('\n');
#endif
		if (drawn == good) continue;
		QTemporaryDir dir;
		dir.setAutoRemove(false);
		QFile out(dir.filePath(QString("%1-structure-output.txt").arg(examples[i])));
		QVERIFY(out.open(QIODevice::WriteOnly));
		out.write(drawn.toUtf8());
		out.close();
		QStringList a = drawn.split('\n'), b = good.split('\n');
		int n = 0;
		while (n < qMin(a.size(), b.size()) && a.at(n) == b.at(n)) n++;
		QFAIL(qPrintable(QString("%1, line %2:\n drawn: %3\n good:  %4\n (the drawn structure: %5)")
						 .arg(examples[i]).arg(n + 1).arg(a.value(n), b.value(n), out.fileName())));
	}
}

// the space around the blocks in text lines: more above a heading than around a
// paragraph, less between the items of a tight list, none above the first block
void TestMarkdown::test_layout()
{
	QTextDocument doc;
	importMarkdown(&doc, "# First\n\nA paragraph.\n\n## Second\n\n- one\n- two\n\n> quoted\n", QString());
	auto format = [&](const QString& text) {
		for (QTextBlock b = doc.begin(); b.isValid(); b = b.next())
			if (b.text() == text) return b.blockFormat();
		return QTextBlockFormat();
	};
	QCOMPARE(format("First").topMargin(), qreal(0));
	qreal paragraph = format("A paragraph.").topMargin();
	QVERIFY(paragraph > 0);
	QCOMPARE(format("A paragraph.").bottomMargin(), paragraph);
	QVERIFY(format("Second").topMargin() > paragraph);
	QVERIFY(format("Second").bottomMargin() < paragraph);
	QVERIFY(format("one").topMargin() < paragraph);
	QCOMPARE(format("quoted").intProperty(MarkdownQuoteLevel), 1);
	QCOMPARE(format("quoted").leftMargin(), markdownQuoteIndent(&doc));
}

// a quote: its levels, its text and formula in the grey, a link in its own colour,
// a bar drawn at the left of each level (a shape, no glyph)
void TestMarkdown::test_quotes()
{
	QTextDocument doc;
	doc.setTextWidth(400);
	setMarkdownColors(&doc, Qt::black, Qt::white);
	importMarkdown(&doc, "> quoted *words* $x$ and [a link](http://a)\n>> deeper\n\nplain", QString());
	QColor grey = markdownQuoteColor(&doc);
	QVERIFY(grey != QColor(Qt::black) && grey != QColor(Qt::white));
	QTextBlock outer, inner, plain;
	for (QTextBlock b = doc.begin(); b.isValid(); b = b.next()) {
		if (b.text().startsWith("quoted")) outer = b;
		if (b.text() == "deeper") inner = b;
		if (b.text() == "plain") plain = b;
	}
	QVERIFY(outer.isValid() && inner.isValid() && plain.isValid());
	QCOMPARE(outer.blockFormat().intProperty(MarkdownQuoteLevel), 1);
	QCOMPARE(inner.blockFormat().intProperty(MarkdownQuoteLevel), 2);
	QCOMPARE(plain.blockFormat().intProperty(MarkdownQuoteLevel), 0);
	for (QTextBlock::iterator it = outer.begin(); !it.atEnd(); ++it) {
		QTextCharFormat f = it.fragment().charFormat();
		if (f.isAnchor()) QVERIFY(f.foreground().color() != grey);
		else QCOMPARE(f.foreground().color(), grey);
	}
	for (QTextBlock::iterator it = plain.begin(); !it.atEnd(); ++it)
		QVERIFY(!it.fragment().charFormat().hasProperty(QTextFormat::ForegroundBrush));

	QImage image = draw(&doc);
	QPainter painter(&image);
	drawMarkdownQuotes(&painter, &doc);
	painter.end();
	qreal indent = markdownQuoteIndent(&doc);
	// a pixel inside the bar of a level: QUOTE_BAR_OFFSET of markdown_objects.cpp, a pixel in
	const qreal barOffset = 0.25;
	const int inside = 1;
	auto barAt = [&](const QTextBlock& b, int level) {
		QTextLayout* l = b.layout();
		int x = int(l->position().x() + (level - 1) * indent + indent * barOffset) + inside;
		int y = int(l->position().y() + l->lineAt(0).y() + l->lineAt(0).height() / 2);
		return qAbs(qGray(image.pixel(x, y)) - qGray(grey.rgb())) <= 8;
	};
	QVERIFY(barAt(outer, 1));
	QVERIFY(barAt(inner, 1));
	QVERIFY(barAt(inner, 2));
	QVERIFY(!barAt(plain, 1));
}

void TestMarkdown::test_heading_anchor()
{
	QCOMPARE(MarkdownView::headingAnchor("Hello, World!"), QString("hello-world"));
	QCOMPARE(MarkdownView::headingAnchor(QString::fromUtf8("Первый раздел")), QString::fromUtf8("первый-раздел"));
	MarkdownView view;
	view.setMarkdownText(QString::fromUtf8("# Первый раздел\n\ntext\n\n## Second Part\n\n## Second Part\n"), QString());
	QVERIFY(view.scrollToHeading(QString::fromUtf8("первый-раздел")));
	QVERIFY(view.scrollToHeading("second-part"));
	QVERIFY(view.scrollToHeading("second-part-1"));
	QVERIFY(!view.scrollToHeading("missing"));
}

// EDIT-MODE-14: the read-only tab draws the formulas and finds the images next to its file
void TestMarkdown::test_tab_rendered()
{
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("texts/markdown.md"));
	TextDocument* text = window.currentText();
	window.setDocumentReadOnly(text, true);
	QVERIFY(text->showsRendered());
	MarkdownView* view = qobject_cast<MarkdownView*>(qobject_cast<QStackedWidget*>(text->page())->currentWidget());
	QVERIFY(view);
	QCOMPARE(view->baseDir(), QDir("texts").absolutePath());
	int formulas = 0, images = 0, found = 0;
	for (QTextBlock b = view->document()->begin(); b.isValid(); b = b.next())
		for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it) {
			QTextCharFormat f = it.fragment().charFormat();
			if (f.objectType() == MarkdownMathObject) formulas++;
			if (f.objectType() == MarkdownImageObject) {
				images++;
				if (QFileInfo(f.stringProperty(MarkdownImagePath)).isFile()) found++;
			}
		}
	QCOMPARE(formulas, 6);
	QCOMPARE(images, 3);
	QCOMPARE(found, 2);
}

// the objects of the example take their sizes from the files and the formulas,
// whatever the glyphs: an SVG at its own size or the text width, a missing image
// as its label, a bad formula with its errors, a display formula centred (the
// widths are measured outside the tables, whose cell lines span the column)
void TestMarkdown::test_object_sizes()
{
	auto objectWidth = [](const QTextBlock& b, int position) {
		QTextLine line = b.layout()->lineForTextPosition(position - b.position());
		return line.cursorToX(position - b.position() + 1) - line.cursorToX(position - b.position());
	};
	QTextDocument doc;
	doc.setTextWidth(640);
	importMarkdown(&doc, read("texts/markdown.md"), QDir("texts").absolutePath());
	doc.documentLayout()->documentSize();
	int svgs = 0, missing = 0, bad = 0, centred = 0;
	for (QTextBlock b = doc.begin(); b.isValid(); b = b.next()) {
		bool inTable = QTextCursor(b).currentTable() != nullptr;
		for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it) {
			QTextCharFormat f = it.fragment().charFormat();
			int position = it.fragment().position();
			if (f.objectType() == MarkdownImageObject && QFileInfo(f.stringProperty(MarkdownImagePath)).isFile()) {
				if (!inTable) QCOMPARE(objectWidth(b, position), qreal(120));
				svgs++;
			} else if (f.objectType() == MarkdownImageObject) {
				missing++;
			} else if (f.objectType() == MarkdownMathObject && f.stringProperty(MarkdownTex).contains("badcommand")) {
				QVERIFY(!f.toolTip().isEmpty());
				bad++;
			} else if (f.objectType() == MarkdownMathObject && f.boolProperty(MarkdownDisplay)) {
				QVERIFY(b.blockFormat().alignment() & Qt::AlignHCenter);
				QVERIFY(objectWidth(b, position) > 0);
				centred++;
			}
		}
	}
	QCOMPARE(svgs, 2);
	QCOMPARE(missing, 1);
	QCOMPARE(bad, 1);
	QCOMPARE(centred, 1);

	// a narrow text scales the image down to its width
	QTextDocument narrow;
	narrow.setTextWidth(80);
	importMarkdown(&narrow, "![arrow](images/arrow.svg)", QDir("texts").absolutePath());
	narrow.documentLayout()->documentSize();
	QVERIFY(objectWidth(narrow.begin(), 0) <= 80 - 2 * narrow.documentMargin() + 0.5);

	// a missing image is as wide as its label in a frame
	QTextDocument gone;
	gone.setTextWidth(640);
	importMarkdown(&gone, "![gone](images/missing.svg)", QDir("texts").absolutePath());
	gone.documentLayout()->documentSize();
	QFontMetricsF metrics(gone.begin().begin().fragment().charFormat().font());
	QVERIFY(objectWidth(gone.begin(), 0) > metrics.horizontalAdvance("gone (images/missing.svg)"));
}

// EDIT-TEXT-14: a Markdown comment body is drawn rendered with the preference, its
// source kept for the model and shown while edited; a plain one stays plain
void TestMarkdown::test_comment_rendered()
{
	SettingsManager& sm = SettingsManager::instance();
	sm.overrideRenderMarkdownComments(false);
	CyberiadaSMEditorWindow window;
	QVERIFY(window.openFile("diagrams/markdown-render-comments.graphml"));
	CyberiadaSMEditorScene* scene = window.getScene();
	QVERIFY(scene);
	CyberiadaSMEditorCommentItem* notes = dynamic_cast<CyberiadaSMEditorCommentItem*>(scene->getMap().value("notes"));
	CyberiadaSMEditorCommentItem* plain = dynamic_cast<CyberiadaSMEditorCommentItem*>(scene->getMap().value("plain"));
	QVERIFY(notes && plain);
	SubjectFragment::Field field;
	field.kind = SubjectFragment::fieldBody;
	int start = -1;
	EditableTextItem* body = notes->fieldText(field, &start);
	EditableTextItem* plainBody = plain->fieldText(field, &start);
	QVERIFY(body && plainBody);
	const QString source = body->toPlainText();
	const QString plainSource = plainBody->toPlainText();
	QVERIFY(source.startsWith("## The output"));
	QVERIFY(!body->isRendered());

	sm.overrideRenderMarkdownComments(true);
	QVERIFY(body->isRendered());
	QVERIFY(!plainBody->isRendered());
	QCOMPARE(plainBody->toPlainText(), plainSource);
	QCOMPARE(body->sourceText(), source);
	QVERIFY(!body->toPlainText().contains("##"));
	int formulas = 0;
	QString image;
	for (QTextBlock b = body->document()->begin(); b.isValid(); b = b.next())
		for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it) {
			QTextCharFormat f = it.fragment().charFormat();
			if (f.objectType() == MarkdownMathObject) formulas++;
			if (f.objectType() == MarkdownImageObject) image = f.stringProperty(MarkdownImagePath);
		}
	QCOMPARE(formulas, 1);
	QCOMPARE(image, QFileInfo("texts/images/arrow.svg").absoluteFilePath());
	// a fragment link has no offsets in a rendered body
	QVERIFY(!notes->fieldText(field, &start));

	// an edit shows the source, its end renders again and changes nothing
	body->startEditing();
	QVERIFY(!body->isRendered());
	QCOMPARE(body->toPlainText(), source);
	QFocusEvent out(QEvent::FocusOut);
	scene->sendEvent(body, &out);
	QVERIFY(body->isRendered());
	QVERIFY(window.getModel()->undoStack()->isClean());

	sm.overrideRenderMarkdownComments(false);
	QVERIFY(!body->isRendered());
	QCOMPARE(body->toPlainText(), source);
	QVERIFY(notes->fieldText(field, &start) == body);
}

QTEST_MAIN(TestMarkdown)
// the PDF of the text, its pages counted by the export
QByteArray TestMarkdown::pdfFile(const QString& text, const QString& baseDir, int* pages)
{
	QTemporaryDir dir;
	QString path = dir.filePath("text.pdf"), error;
	*pages = exportMarkdownPdf(text, baseDir, "text.md", path, &error);
	if (!*pages) qWarning("%s", qPrintable(error));
	QFile file(path);
	return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// EDIT-MODE-18: the rendered text on A4 pages, a longer text on more of them, the formulas
// (their font) and the images (a raster one an image object) drawn on the pages
void TestMarkdown::test_pdf()
{
	QRegularExpression page("/Type /Page\\b");
	QRegularExpression box("/MediaBox \\[0 0 ([0-9.]+) ([0-9.]+)\\]");
	int pages = 0;
	QByteArray pdf = pdfFile(read("texts/markdown.md"), QDir("texts").absolutePath(), &pages);
	QVERIFY(pdf.startsWith("%PDF"));
	QCOMPARE(pages, 1);
	// the whole file: the compressed streams hold zero bytes
	QString raw = QString::fromLatin1(pdf.constData(), pdf.size());
	QCOMPARE(raw.count(page), pages);
	QSize a4 = QPageSize(QPageSize::A4).sizePoints();
	QRegularExpressionMatchIterator boxes = box.globalMatch(raw);
	QVERIFY(boxes.hasNext());
	while (boxes.hasNext()) {
		QRegularExpressionMatch m = boxes.next();
		QCOMPARE(qRound(m.captured(1).toDouble()), a4.width());
		QCOMPARE(qRound(m.captured(2).toDouble()), a4.height());
	}
#ifdef CYBERIADA_WITH_MATH
	QVERIFY(raw.contains("XITS"));
#endif

	// the paragraphs of a long text, as many as the lines of some pages, go on to the next pages
	const int LONG_TEXT_PAGES = 4;
	QString paragraphs;
	for (int i = 0; i < LONG_TEXT_PAGES * a4.height() / QFontMetrics(QFont()).height(); i++)
		paragraphs += QString("Paragraph %1 with $x_{%1}$.\n\n").arg(i);
	int longPages = 0;
	QByteArray longPdf = pdfFile(paragraphs, QString(), &longPages);
	QString longRaw = QString::fromLatin1(longPdf.constData(), longPdf.size());
	QVERIFY(longPages > pages);
	QCOMPARE(longRaw.count(page), longPages);

	// a raster image is an image object of the file
	QTemporaryDir dir;
	QImage image(QSize(8, 8), QImage::Format_RGB32);
	image.fill(Qt::darkCyan);
	QVERIFY(image.save(dir.filePath("dot.png")));
	QByteArray imagePdf = pdfFile("![dot](dot.png)\n", dir.path(), &pages);
	QString withImage = QString::fromLatin1(imagePdf.constData(), imagePdf.size());
	QCOMPARE(pages, 1);
	QVERIFY(withImage.contains("/Subtype /Image"));
	QVERIFY(!longRaw.contains("/Subtype /Image"));
}

#include "l4-markdown.moc"
