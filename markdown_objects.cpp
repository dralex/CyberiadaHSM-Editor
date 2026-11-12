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
#include <QTextDocument>
#include <QTextLayout>
#include <QUrl>

#include "markdown_objects.h"
#include "math_renderer.h"
#include "svg_markers.h"

namespace {

const char* const HANDLERS_PROPERTY = "cyberiadaMarkdownObjects";
const char* const COLOR_PROPERTY = "cyberiadaMarkdownTextColor";
// the frame of a missing image around its label
const qreal MISSING_PADDING = 3.0;

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

// but draws it in the middle of its line (QTextLine::draw): the baseline is found
// from the line, top + (height - h) / 2 being the top of the object
qreal objectBaseline(QTextDocument* doc, int position, const QRectF& rect, qreal halfX)
{
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
    QAbstractTextDocumentLayout* layout = doc->documentLayout();
    if (layout->property(HANDLERS_PROPERTY).toBool()) return;
    layout->registerHandler(MarkdownMathObject, new MathObjectHandler(layout));
    layout->registerHandler(MarkdownImageObject, new ImageObjectHandler(layout));
    layout->setProperty(HANDLERS_PROPERTY, true);
}

void setMarkdownTextColor(QTextDocument* doc, const QColor& color)
{
    doc->setProperty(COLOR_PROPERTY, color);
}

#include "markdown_objects.moc"
