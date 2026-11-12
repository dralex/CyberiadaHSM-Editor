/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The formulas and the images of a rendered Markdown document
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

#include <QAbstractTextDocumentLayout>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QHash>
#include <QImageReader>
#include <QPainter>
#include <QPalette>
#include <QSharedPointer>
#include <QSvgRenderer>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QUrl>

#include "markdown_objects.h"
#include "math_renderer.h"
#include "svg_markers.h"

namespace {

const char* const HANDLERS_PROPERTY = "cyberiadaMarkdownObjects";
const char* const COLOR_PROPERTY = "cyberiadaMarkdownTextColor";
const char* const BACKGROUND_PROPERTY = "cyberiadaMarkdownBackground";
// a quote: the share of the text colour in its grey, its indent and its bar, in lines
const qreal QUOTE_TEXT_SHARE = 0.55;
const qreal QUOTE_INDENT_LINES = 1.25;
const qreal QUOTE_BAR_OFFSET = 0.25;
const qreal QUOTE_BAR_LINES = 0.2;
const qreal QUOTE_BAR_MIN_WIDTH = 2.0;
// the frame of a missing image around its label
const qreal MISSING_PADDING = 3.0;
// the probe of the placement of an AlignMiddle object: a small one beside a letter
const int   PROBE_OBJECT = QTextFormat::UserObject + 100;
const qreal PROBE_WIDTH = 4.0;
const qreal PROBE_HEIGHT = 2.0;

QColor textColor(QTextDocument* doc, const QTextCharFormat& format)
{
    if (format.hasProperty(QTextFormat::ForegroundBrush)) return format.foreground().color();
    QVariant color = doc ? doc->property(COLOR_PROPERTY) : QVariant();
    if (color.isValid()) return color.value<QColor>();
    return QGuiApplication::palette().color(QPalette::Text);
}

const MathBox& mathBox(QTextDocument* doc, const QTextCharFormat& f)
{
    return MathRenderer::instance().render(f.stringProperty(MarkdownTex), f.boolProperty(MarkdownDisplay),
                                           f.font(), textColor(doc, f));
}

// Qt reserves (h + x/2) / 2 above the baseline for an AlignMiddle object and
// (h - x/2) / 2 below it, x the x-height (QTextDocumentLayout::resizeInlineObject)
qreal halfXHeight(const QTextCharFormat& f)
{
    return QFontMetrics(f.font()).xHeight() / 2.0;
}

// and draws it (QTextLine::draw) at that ascent above the baseline up to Qt 5.15.0,
// in the middle of its line in the later 5.15 releases
enum MiddlePlacement { middleUnknown, middleByAscent, middleInLine };
MiddlePlacement middlePlacement = middleUnknown;

class PlacementProbe : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)
public:
    explicit PlacementProbe(QObject* parent): QObject(parent) {}
    QSizeF intrinsicSize(QTextDocument*, int, const QTextFormat&) override
    {
        return QSizeF(PROBE_WIDTH, PROBE_HEIGHT);
    }
    void drawObject(QPainter*, const QRectF& rect, QTextDocument*, int, const QTextFormat&) override
    {
        top = rect.top();
        drawn = true;
    }
    qreal top = 0;
    bool  drawn = false;
};

// the placement of this Qt, from where it draws a probe: the nearer of the two
MiddlePlacement findMiddlePlacement()
{
    QTextDocument doc;
    PlacementProbe* probe = new PlacementProbe(doc.documentLayout());
    doc.documentLayout()->registerHandler(PROBE_OBJECT, probe);
    QTextCursor cursor(&doc);
    cursor.insertText("x");
    QTextCharFormat f;
    f.setObjectType(PROBE_OBJECT);
    f.setVerticalAlignment(QTextCharFormat::AlignMiddle);
    cursor.insertText(QString(QChar::ObjectReplacementCharacter), f);
    QImage image(doc.size().toSize().expandedTo(QSize(1, 1)), QImage::Format_ARGB32);
    QPainter painter(&image);
    doc.drawContents(&painter);
    painter.end();
    QTextLayout* layout = doc.begin().layout();
    QTextLine line = layout ? layout->lineAt(0) : QTextLine();
    if (!probe->drawn || !line.isValid()) return middleByAscent;
    qreal leading = line.leadingIncluded() ? qMax(line.leading(), qreal(0)) : 0;
    qreal lineTop = layout->position().y() + line.y();
    qreal byAscent = lineTop + line.ascent() + leading -
                     (PROBE_HEIGHT + QFontMetrics(doc.defaultFont()).xHeight() / 2.0) / 2;
    qreal inLine = lineTop + (line.ascent() + line.descent() + leading - PROBE_HEIGHT) / 2;
    return qAbs(probe->top - inLine) < qAbs(probe->top - byAscent) ? middleInLine : middleByAscent;
}

// the baseline of the text an AlignMiddle object of the rect stands in; in the middle
// of its line top + (height - h) / 2 is the top of the object
qreal objectBaseline(QTextDocument* doc, int position, const QRectF& rect, qreal halfX)
{
    if (middlePlacement != middleInLine) return rect.top() + (rect.height() + halfX) / 2;
    QTextBlock block = doc->findBlock(position);
    QTextLayout* layout = block.isValid() ? block.layout() : nullptr;
    QTextLine line = layout ? layout->lineForTextPosition(position - block.position()) : QTextLine();
    if (!line.isValid()) return rect.top() + (rect.height() + halfX) / 2;
    qreal leading = line.leadingIncluded() ? qMax(line.leading(), qreal(0)) : 0;
    qreal height = line.ascent() + line.descent() + leading;
    return rect.top() - (height - rect.height()) / 2 + line.ascent() + leading;
}

class MathObjectHandler : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)
public:
    explicit MathObjectHandler(QObject* parent): QObject(parent) {}

    // the box is as high as the formula needs on both sides of the text baseline
    QSizeF intrinsicSize(QTextDocument* doc, int, const QTextFormat& format) override
    {
        QTextCharFormat f = format.toCharFormat();
        const MathBox& box = mathBox(doc, f);
        qreal halfX = halfXHeight(f);
        return QSizeF(box.width, qMax(2 * box.ascent - halfX, 2 * box.descent + halfX));
    }

    void drawObject(QPainter* painter, const QRectF& rect, QTextDocument* doc, int position,
                    const QTextFormat& format) override
    {
        QTextCharFormat f = format.toCharFormat();
        const MathBox& box = mathBox(doc, f);
        qreal baseline = objectBaseline(doc, position, rect, halfXHeight(f));
        painter->save();
        painter->drawPicture(QPointF(rect.left(), baseline), box.picture);
        painter->restore();
    }
};

// an image file read once: an SVG with its markers expanded or a raster image
struct ImageEntry {
    QDateTime                    modified;
    QSharedPointer<QSvgRenderer> svg;
    QImage                       image;
};

class ImageObjectHandler : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)
public:
    explicit ImageObjectHandler(QObject* parent): QObject(parent) {}

    QSizeF intrinsicSize(QTextDocument* doc, int, const QTextFormat& format) override
    {
        QTextCharFormat f = format.toCharFormat();
        const ImageEntry* e = entry(f.stringProperty(MarkdownImagePath));
        if (!e) {
            QFontMetricsF metrics(f.font());
            return QSizeF(metrics.horizontalAdvance(missingLabel(f)) + 2 * MISSING_PADDING,
                          metrics.height() + 2 * MISSING_PADDING);
        }
        QSizeF size;
        if (e->svg) {
            size = e->svg->defaultSize();
            if (size.isEmpty()) size = e->svg->viewBoxF().size();
        } else {
            size = QSizeF(e->image.size()) / e->image.devicePixelRatio();
        }
        // no wider than the text
        qreal width = doc->textWidth();
        if (width > 0) {
            width -= 2 * doc->documentMargin();
            if (width > 0 && size.width() > width) size *= width / size.width();
        }
        return size;
    }

    void drawObject(QPainter* painter, const QRectF& rect, QTextDocument* doc, int,
                    const QTextFormat& format) override
    {
        QTextCharFormat f = format.toCharFormat();
        const ImageEntry* e = entry(f.stringProperty(MarkdownImagePath));
        painter->save();
        if (e && e->svg) {
            e->svg->render(painter, rect);
        } else if (e) {
            painter->setRenderHint(QPainter::SmoothPixmapTransform);
            painter->drawImage(rect, e->image);
        } else {
            QColor color = textColor(doc, f);
            painter->setPen(QPen(color, 1, Qt::DashLine));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(rect.adjusted(0.5, 0.5, -0.5, -0.5));
            painter->setPen(color);
            painter->setFont(f.font());
            painter->drawText(rect, Qt::AlignCenter, missingLabel(f));
        }
        painter->restore();
    }

private:
    static QString missingLabel(const QTextCharFormat& f)
    {
        QString alt = f.stringProperty(MarkdownImageAlt);
        QString source = f.stringProperty(MarkdownImageSource);
        return alt.isEmpty() ? source : alt + " (" + source + ")";
    }

    // the file read again when it changes; null for a missing or unreadable one
    const ImageEntry* entry(const QString& path)
    {
        QFileInfo info(path);
        if (path.isEmpty() || !info.isFile()) return nullptr;
        QHash<QString, ImageEntry>::iterator it = cache.find(path);
        if (it != cache.end() && it->modified == info.lastModified()) return &it.value();
        ImageEntry e;
        e.modified = info.lastModified();
        QString suffix = info.suffix().toLower();
        if (suffix == "svg" || suffix == "svgz") {
            QSharedPointer<QSvgRenderer> svg;
            QFile file(path);
            if (suffix == "svgz") {
                svg.reset(new QSvgRenderer(path));
            } else if (file.open(QIODevice::ReadOnly)) {
                svg.reset(new QSvgRenderer(expandSvgMarkers(file.readAll())));
            }
            if (!svg || !svg->isValid()) {
                cache.remove(path);
                return nullptr;
            }
            e.svg = svg;
        } else {
            QImageReader reader(path);
            reader.setAutoTransform(true);
            e.image = reader.read();
            if (e.image.isNull()) {
                cache.remove(path);
                return nullptr;
            }
        }
        return &cache.insert(path, e).value();
    }

    QHash<QString, ImageEntry> cache;
};

QTextCharFormat objectFormat(const QTextCharFormat& base, int type)
{
    // the text attributes stay (the font, a link around it), the image ones go
    static const int imageProperties[] = {
        QTextFormat::ImageName, QTextFormat::ImageWidth, QTextFormat::ImageHeight,
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QTextFormat::ImageTitle, QTextFormat::ImageAltText,
#endif
        QTextFormat::ImageQuality
    };
    QTextCharFormat f = base;
    for (size_t i = 0; i < sizeof(imageProperties) / sizeof(imageProperties[0]); i++)
        f.clearProperty(imageProperties[i]);
    f.setObjectType(type);
    return f;
}

} // namespace

QTextCharFormat markdownMathFormat(const QTextCharFormat& base, const QString& tex, bool display)
{
    QTextCharFormat f = objectFormat(base, MarkdownMathObject);
    f.setProperty(MarkdownTex, tex);
    f.setProperty(MarkdownDisplay, display);
    f.setVerticalAlignment(QTextCharFormat::AlignMiddle);
    const MathBox& box = MathRenderer::instance().render(tex, display, f.font(), Qt::black);
    if (!box.errors.isEmpty()) f.setToolTip(box.errors);
    return f;
}

QTextCharFormat markdownImageFormat(const QTextCharFormat& base, const QString& source,
                                    const QString& alt, const QString& baseDir)
{
    QTextCharFormat f = objectFormat(base, MarkdownImageObject);
    f.setProperty(MarkdownImagePath, markdownImagePath(source, baseDir));
    f.setProperty(MarkdownImageSource, source);
    f.setProperty(MarkdownImageAlt, alt);
    return f;
}

QString markdownImagePath(const QString& source, const QString& baseDir)
{
    QUrl url(source);
    if (url.isLocalFile()) return QDir::cleanPath(url.toLocalFile());
    // a one-letter scheme is a Windows drive
    if (url.scheme().size() > 1) return QString();
    QString path = url.scheme().isEmpty() ? url.path() : source;
    if (QDir::isAbsolutePath(path)) return QDir::cleanPath(path);
    if (baseDir.isEmpty()) return QString();
    return QDir::cleanPath(QDir(baseDir).absoluteFilePath(path));
}

void installMarkdownObjects(QTextDocument* doc)
{
    // found before any drawing, so the probe paints outside a paint
    if (middlePlacement == middleUnknown) middlePlacement = findMiddlePlacement();
    QAbstractTextDocumentLayout* layout = doc->documentLayout();
    if (layout->property(HANDLERS_PROPERTY).toBool()) return;
    layout->registerHandler(MarkdownMathObject, new MathObjectHandler(layout));
    layout->registerHandler(MarkdownImageObject, new ImageObjectHandler(layout));
    layout->setProperty(HANDLERS_PROPERTY, true);
}

void setMarkdownColors(QTextDocument* doc, const QColor& text, const QColor& background)
{
    doc->setProperty(COLOR_PROPERTY, text);
    doc->setProperty(BACKGROUND_PROPERTY, background);
}

QColor markdownQuoteColor(const QTextDocument* doc)
{
    QVariant t = doc->property(COLOR_PROPERTY), b = doc->property(BACKGROUND_PROPERTY);
    QColor text = t.isValid() ? t.value<QColor>() : QGuiApplication::palette().color(QPalette::Text);
    QColor back = b.isValid() ? b.value<QColor>() : QGuiApplication::palette().color(QPalette::Base);
    auto mix = [](qreal x, qreal y) { return x * QUOTE_TEXT_SHARE + y * (1 - QUOTE_TEXT_SHARE); };
    return QColor::fromRgbF(mix(text.redF(), back.redF()), mix(text.greenF(), back.greenF()),
                            mix(text.blueF(), back.blueF()));
}

qreal markdownQuoteIndent(const QTextDocument* doc)
{
    return QFontMetricsF(doc->defaultFont()).height() * QUOTE_INDENT_LINES;
}

void drawMarkdownQuotes(QPainter* painter, QTextDocument* doc)
{
    qreal line = QFontMetricsF(doc->defaultFont()).height();
    qreal indent = markdownQuoteIndent(doc);
    qreal width = qMax(QUOTE_BAR_MIN_WIDTH, line * QUOTE_BAR_LINES);
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(markdownQuoteColor(doc));
    // the bars of a level run on through the following blocks of the same quote
    int previousLevel = 0;
    qreal previousBottom = 0;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        int level = b.blockFormat().intProperty(MarkdownQuoteLevel);
        QTextLayout* layout = b.layout();
        // the blocks of the document's own frame only: a table keeps its coordinates
        if (level <= 0 || !layout || layout->lineCount() == 0 ||
            QTextCursor(b).currentFrame() != doc->rootFrame()) {
            previousLevel = 0;
            continue;
        }
        QTextLine last = layout->lineAt(layout->lineCount() - 1);
        qreal top = layout->position().y() + layout->lineAt(0).y();
        qreal bottom = layout->position().y() + last.y() + last.height();
        for (int i = 1; i <= level; i++) {
            qreal from = previousLevel >= i ? previousBottom : top;
            qreal x = layout->position().x() + (i - 1) * indent + indent * QUOTE_BAR_OFFSET;
            painter->drawRect(QRectF(x, from, width, bottom - from));
        }
        previousLevel = level;
        previousBottom = bottom;
    }
    painter->restore();
}

#include "markdown_objects.moc"
