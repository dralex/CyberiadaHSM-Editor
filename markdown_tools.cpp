/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The authoring tools of a Markdown text: the paths, the copies and the lines
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

#include "markdown_tools.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QSaveFile>
#include <QTransform>

const char* const MARKDOWN_IMAGE_FOLDER = "images";
const char* const MARKDOWN_PASTED_IMAGE = "image";

static QString absoluteClean(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

static bool samePath(const QString& a, const QString& b)
{
#ifdef Q_OS_WIN
    return QString::compare(absoluteClean(a), absoluteClean(b), Qt::CaseInsensitive) == 0;
#else
    return absoluteClean(a) == absoluteClean(b);
#endif
}

bool isInsideDirectory(const QString& path, const QString& dir)
{
    if (dir.isEmpty()) return false;
    QString d = absoluteClean(dir);
    if (!d.endsWith('/')) d += '/';
#ifdef Q_OS_WIN
    return absoluteClean(path).startsWith(d, Qt::CaseInsensitive);
#else
    return absoluteClean(path).startsWith(d);
#endif
}

QString markdownPath(const QString& path, const QString& baseDir)
{
    QString relative = QDir(baseDir).relativeFilePath(absoluteClean(path));
    if (relative.contains(' ') || relative.contains('(') || relative.contains(')')) {
        return "<" + relative + ">";
    }
    return relative;
}

QString markdownLabel(const QString& text)
{
    QString label;
    for (QChar c : text) {
        if (c == '\\' || c == '[' || c == ']') label += '\\';
        label += c;
    }
    return label;
}

QString markdownImageLine(const QString& alt, const QString& path, const QString& baseDir)
{
    return "![" + markdownLabel(alt) + "](" + markdownPath(path, baseDir) + ")";
}

QString numberedFileName(const QString& dir, const QString& base, const QString& suffix)
{
    QDir d(dir);
    for (int n = 1; ; n++) {
        QString path = d.filePath(QString("%1-%2.%3").arg(base).arg(n).arg(suffix));
        if (!QFileInfo::exists(path)) return QDir::cleanPath(path);
    }
}

bool isSvgFile(const QString& path)
{
    QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == "svg" || suffix == "svgz";
}

bool isEditableImage(const QString& path)
{
    if (isSvgFile(path)) return false;
    QImageReader reader(path);
    return reader.canRead();
}

bool isInsertableImage(const QString& path)
{
    return (isSvgFile(path) && QFileInfo(path).isFile()) || isEditableImage(path);
}

QString imageFileFilter()
{
    QStringList patterns;
    for (const QByteArray& format : QImageReader::supportedImageFormats()) {
        patterns << "*." + QString::fromLatin1(format).toLower();
    }
    if (!patterns.contains("*.svg")) patterns << "*.svg";
    patterns.removeDuplicates();
    return QCoreApplication::translate("MarkdownTools", "Images (%1)").arg(patterns.join(' '));
}

QImage applyImageEdits(const QImage& image, const ImageEdits& edits)
{
    QImage out = image;
    int turn = ((edits.rotation % IMAGE_FULL_TURN) + IMAGE_FULL_TURN) % IMAGE_FULL_TURN;
    if (turn) out = out.transformed(QTransform().rotate(turn));
    if (!edits.crop.isNull()) out = out.copy(edits.crop.intersected(out.rect()));
    if (edits.maxWidth > 0 && out.width() > edits.maxWidth) {
        out = out.scaledToWidth(edits.maxWidth, Qt::SmoothTransformation);
    }
    return out;
}

bool ImageInsert::writesFile() const
{
    return isPasted() || !samePath(source, target);
}

// the format an edited image is written in: its own when Qt writes it, else PNG
static QString writableSuffix(const QString& suffix)
{
    QString s = suffix.toLower();
    if (!s.isEmpty() && QImageWriter::supportedImageFormats().contains(s.toLatin1())) return s;
    return "png";
}

QString proposeImageTarget(const ImageInsert& insert, const QString& folder)
{
    QDir into(QDir(insert.baseDir).filePath(folder));
    if (insert.isPasted()) {
        return numberedFileName(into.path(), MARKDOWN_PASTED_IMAGE, "png");
    }
    QFileInfo source(insert.source);
    bool edited = !insert.edits.isEmpty();
    if (isInsideDirectory(source.absoluteFilePath(), insert.baseDir)) {
        if (!edited) return absoluteClean(source.absoluteFilePath());
        return numberedFileName(source.absolutePath(), source.completeBaseName(),
                                writableSuffix(source.suffix()));
    }
    QString name = edited ? source.completeBaseName() + "." + writableSuffix(source.suffix())
                          : source.fileName();
    return QDir::cleanPath(into.filePath(name));
}

bool imageInsertData(const ImageInsert& insert, QByteArray* data, QString* format, QString* error)
{
    if (!insert.isPasted() && insert.edits.isEmpty()) {
        QFile file(insert.source);
        if (!file.open(QIODevice::ReadOnly)) {
            *error = QCoreApplication::translate("MarkdownTools", "Cannot read %1").arg(insert.source);
            return false;
        }
        *data = file.readAll();
        *format = QFileInfo(insert.source).suffix().toLower();
        return true;
    }
    QImage image = insert.pasted;
    if (!insert.isPasted()) {
        if (isSvgFile(insert.source)) {
            *error = QCoreApplication::translate("MarkdownTools", "An SVG image is copied as it is, without the edits");
            return false;
        }
        QImageReader reader(insert.source);
        reader.setAutoTransform(true);
        image = reader.read();
        if (image.isNull()) {
            *error = QCoreApplication::translate("MarkdownTools", "Cannot read the image %1: %2")
                .arg(insert.source, reader.errorString());
            return false;
        }
    }
    image = applyImageEdits(image, insert.edits);
    *format = writableSuffix(QFileInfo(insert.target).suffix());
    data->clear();
    QBuffer buffer(data);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, format->toLatin1());
    if (*format == "jpg" || *format == "jpeg") writer.setQuality(MARKDOWN_JPEG_QUALITY);
    if (!writer.write(image)) {
        *error = QCoreApplication::translate("MarkdownTools", "Cannot encode the image: %1")
            .arg(writer.errorString());
        return false;
    }
    return true;
}

static bool fileHolds(const QString& path, const QByteArray& data)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    return file.size() == data.size() && file.readAll() == data;
}

bool imageTargetClashes(const ImageInsert& insert, const QByteArray& data)
{
    if (!insert.writesFile() || !QFileInfo::exists(insert.target)) return false;
    return !fileHolds(insert.target, data);
}

bool writeImageInsert(const ImageInsert& insert, const QByteArray& data, QString* error)
{
    if (!insert.writesFile() || fileHolds(insert.target, data)) return true;
    QFileInfo target(insert.target);
    if (!QDir().mkpath(target.absolutePath())) {
        *error = QCoreApplication::translate("MarkdownTools", "Cannot make the folder %1").arg(target.absolutePath());
        return false;
    }
    QSaveFile file(insert.target);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        *error = QCoreApplication::translate("MarkdownTools", "Cannot write %1: %2")
            .arg(insert.target, file.errorString());
        return false;
    }
    return true;
}
