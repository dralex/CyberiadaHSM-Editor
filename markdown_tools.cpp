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

#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QUrl>

#include "cyberiadasm_editor_document.h"
#include "cyberiadasm_model.h"
#include "cyberiadasm_render.h"
#include "fontmanager.h"
#include "markdown_objects.h"
#include "markdown_view.h"

#include <algorithm>

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
const char* const MARKDOWN_DIAGRAM_FOLDER = "diagrams";

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

// ---------------------------------------------------------------------------- diagram images

bool sameFileContent(const QString& a, const QString& b)
{
    QFile fa(a), fb(b);
    if (!fa.open(QIODevice::ReadOnly) || !fb.open(QIODevice::ReadOnly)) return false;
    return fa.size() == fb.size() && fa.readAll() == fb.readAll();
}

bool copyFileOver(const QString& source, const QString& target, QString* error)
{
    if (samePath(source, target) || sameFileContent(source, target)) return true;
    QFile in(source);
    if (!in.open(QIODevice::ReadOnly)) {
        *error = QCoreApplication::translate("MarkdownTools", "Cannot read %1").arg(source);
        return false;
    }
    ImageInsert copy;
    copy.source = source;
    copy.target = target;
    return writeImageInsert(copy, in.readAll(), error);
}

// a destination of an image or a link: <path> or a path without spaces, then a title
static const char* const DESTINATION = R"(\(\s*(<[^>\n]*>|[^\s)]+)(?:\s+(?:"[^"]*"|'[^']*'))?\s*\))";
static const char* const IMAGE = R"(!\[((?:\\.|[^\\\]])*)\])";

static QString destination(const QString& written)
{
    if (written.startsWith('<') && written.endsWith('>')) return written.mid(1, written.size() - 2);
    return written;
}

// the code spans of a line as spaces, so no image is found in them and the columns stay
static QString withoutCodeSpans(const QString& line)
{
    static const QRegularExpression code("(`+)(.*?)\\1");
    QString masked = line;
    QRegularExpressionMatchIterator it = code.globalMatch(line);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        masked.replace(m.capturedStart(), m.capturedLength(), QString(m.capturedLength(), ' '));
    }
    return masked;
}

QList<MarkdownImage> markdownImages(const QString& text)
{
    static const QRegularExpression fence(R"(^ {0,3}(`{3,}|~{3,}))");
    static const QRegularExpression linked(QString(R"(\[\s*)") + IMAGE + DESTINATION + R"(\s*\])" + DESTINATION);
    static const QRegularExpression image(QString(IMAGE) + DESTINATION);
    QList<MarkdownImage> images;
    QStringList lines = text.split('\n');
    QString openFence;
    for (int n = 0; n < lines.size(); n++) {
        const QString& line = lines.at(n);
        QRegularExpressionMatch f = fence.match(line);
        if (f.hasMatch()) {
            if (openFence.isEmpty()) openFence = f.captured(1);
            else if (f.captured(1).startsWith(openFence.at(0)) && f.captured(1).size() >= openFence.size()) openFence.clear();
            continue;
        }
        if (!openFence.isEmpty()) continue;
        QString masked = withoutCodeSpans(line);
        QList<QPair<int, int>> taken;
        QRegularExpressionMatchIterator it = linked.globalMatch(masked);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            MarkdownImage i;
            i.line = n + 1;
            i.column = m.capturedStart() + 1;
            i.alt = m.captured(1);
            i.target = destination(m.captured(2));
            i.link = destination(m.captured(3));
            images << i;
            taken << qMakePair(m.capturedStart(), m.capturedEnd());
        }
        it = image.globalMatch(masked);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            bool inside = false;
            for (const QPair<int, int>& t : taken) inside = inside || (m.capturedStart() >= t.first && m.capturedEnd() <= t.second);
            if (inside) continue;
            MarkdownImage i;
            i.line = n + 1;
            i.column = m.capturedStart() + 1;
            i.alt = m.captured(1);
            i.target = destination(m.captured(2));
            images << i;
        }
    }
    std::sort(images.begin(), images.end(), [](const MarkdownImage& a, const MarkdownImage& b) {
        return a.line < b.line || (a.line == b.line && a.column < b.column);
    });
    return images;
}

static bool hasSuffix(const QString& path, const char* suffix)
{
    return QFileInfo(path).suffix().compare(QLatin1String(suffix), Qt::CaseInsensitive) == 0;
}

DiagramFileCache::Stamp DiagramFileCache::stamp(const QString& path)
{
    QFileInfo info(path);
    Stamp s;
    if (info.exists()) {
        s.modified = info.lastModified();
        s.size = info.size();
    }
    return s;
}

SvgSource DiagramFileCache::source(const QString& svg)
{
    Stamp now = stamp(svg);
    auto it = m_sources.constFind(svg);
    if (it != m_sources.constEnd() && it.value().first == now) return it.value().second;
    SvgSource read = readSvgSource(svg);
    m_sources.insert(svg, qMakePair(now, read));
    return read;
}

QString DiagramFileCache::digest(const QString& graphml)
{
    Stamp now = stamp(graphml);
    auto it = m_digests.constFind(graphml);
    if (it != m_digests.constEnd() && it.value().first == now) return it.value().second;
    QString read = fileDigest(graphml);
    m_digests.insert(graphml, qMakePair(now, read));
    return read;
}

bool diagramOfImage(const MarkdownImage& image, const QString& baseDir, DiagramImage* diagram,
                    DiagramFileCache* cache)
{
    QString svg = markdownImagePath(image.target, baseDir);
    if (svg.isEmpty() || !isSvgFile(svg)) return false;
    DiagramImage d;
    d.image = image;
    d.svg = svg;
    d.recorded = cache ? cache->source(svg) : readSvgSource(svg);
    QString linked = image.link.isEmpty() ? QString() : markdownImagePath(image.link, baseDir);
    QFileInfo info(svg);
    QString sibling = info.absoluteDir().absoluteFilePath(info.completeBaseName() + ".graphml");
    if (!linked.isEmpty() && hasSuffix(linked, "graphml")) d.graphml = linked;
    else if (!d.recorded.graphml.isEmpty()) d.graphml = d.recorded.graphml;
    else if (QFileInfo::exists(sibling)) d.graphml = QDir::cleanPath(sibling);
    else return false;
    QString digest = cache ? cache->digest(d.graphml) : fileDigest(d.graphml);
    d.upToDate = !d.recorded.digest.isEmpty() && d.recorded.digest == digest;
    *diagram = d;
    return true;
}

QList<DiagramImage> markdownDiagramImages(const QString& text, const QString& baseDir)
{
    QList<DiagramImage> diagrams;
    for (const MarkdownImage& image : markdownImages(text)) {
        DiagramImage d;
        if (diagramOfImage(image, baseDir, &d)) diagrams << d;
    }
    return diagrams;
}

QString markdownDiagramLine(const QString& alt, const QString& svg, const QString& graphml,
                            const QString& baseDir)
{
    return "[" + markdownImageLine(alt, svg, baseDir) + "](" + markdownPath(graphml, baseDir) + ")";
}

QString diagramFileTitle(const QString& graphml)
{
    CyberiadaSMEditorDocument doc;
    QString error;
    if (doc.load(graphml, &error)) {
        if (!doc.documentName().isEmpty()) return doc.documentName();
        const Cyberiada::LocalDocument* root = doc.model()->rootDocument();
        if (root) {
            Cyberiada::ConstStateMachineList machines = root->get_state_machines();
            if (!machines.empty() && !machines.front()->get_name().empty()) {
                return QString::fromStdString(machines.front()->get_name());
            }
        }
    }
    return QFileInfo(graphml).completeBaseName();
}

bool renderDiagramFile(const QString& graphml, const QString& svg, const QString& font, QString* error)
{
    CyberiadaSMEditorDocument doc;
    if (!doc.load(graphml, error)) return false;
    if (!QDir().mkpath(QFileInfo(svg).absolutePath())) {
        *error = QCoreApplication::translate("MarkdownTools", "Cannot make the folder %1").arg(QFileInfo(svg).absolutePath());
        return false;
    }
    QString title = doc.documentName().isEmpty() ? QFileInfo(graphml).completeBaseName() : doc.documentName();
    QString family = font.isEmpty() ? FontManager::instance().bundledFamily() : font;
    return renderScene(doc.scene(), svg, error, RENDER_DEFAULT_DPI, family, QDir::cleanPath(QFileInfo(graphml).absoluteFilePath()), title);
}

QList<DiagramUpdate> updateDiagramImages(const QList<DiagramImage>& images, bool all, const QString& font)
{
    QList<DiagramUpdate> updates;
    QStringList done;
    for (const DiagramImage& image : images) {
        if ((!all && image.upToDate) || done.contains(image.svg)) continue;
        done << image.svg;
        DiagramUpdate update;
        update.image = image;
        if (!QFileInfo(image.graphml).isFile()) {
            update.error = QCoreApplication::translate("MarkdownTools", "No diagram file %1").arg(image.graphml);
        } else {
            QString family = image.recorded.font.isEmpty() ? font : image.recorded.font;
            update.updated = renderDiagramFile(image.graphml, image.svg, family, &update.error);
        }
        updates << update;
    }
    return updates;
}

// ---------------------------------------------------------------------------- links and headings

static const char* const FENCE = R"(^ {0,3}(`{3,}|~{3,}))";

// the lines outside the code fences, by their numbers from 0; the code spans as spaces when
// masked, so no link is found in them
static QList<QPair<int, QString>> proseLines(const QString& text, bool masked)
{
    static const QRegularExpression fence(FENCE);
    QList<QPair<int, QString>> lines;
    QStringList all = text.split('\n');
    QString openFence;
    for (int n = 0; n < all.size(); n++) {
        QRegularExpressionMatch f = fence.match(all.at(n));
        if (f.hasMatch()) {
            if (openFence.isEmpty()) openFence = f.captured(1);
            else if (f.captured(1).startsWith(openFence.at(0)) && f.captured(1).size() >= openFence.size()) openFence.clear();
            continue;
        }
        if (openFence.isEmpty()) lines << qMakePair(n, masked ? withoutCodeSpans(all.at(n)) : all.at(n));
    }
    return lines;
}

QList<MarkdownLink> markdownLinks(const QString& text)
{
    static const QRegularExpression image(QString(IMAGE) + DESTINATION);
    // a link text may hold one level of brackets: an image
    static const QRegularExpression link(QString(R"((?<!!)\[(?:\\.|[^\\\[\]]|\[(?:\\.|[^\\\[\]])*\])*\])") + DESTINATION);
    static const QRegularExpression definition(R"(^ {0,3}\[((?:\\.|[^\\\]])+)\]:\s*(<[^>\n]*>|\S+))");
    QList<MarkdownLink> links;
    for (const QPair<int, QString>& line : proseLines(text, true)) {
        auto add = [&](const QRegularExpressionMatch& m, int group, bool isImage) {
            MarkdownLink l;
            l.line = line.first + 1;
            l.column = m.capturedStart() + 1;
            l.target = destination(m.captured(group));
            l.image = isImage;
            links << l;
        };
        QRegularExpressionMatchIterator it = image.globalMatch(line.second);
        while (it.hasNext()) add(it.next(), 2, true);
        it = link.globalMatch(line.second);
        while (it.hasNext()) add(it.next(), 1, false);
        QRegularExpressionMatch d = definition.match(line.second);
        if (d.hasMatch()) add(d, 2, false);
    }
    std::sort(links.begin(), links.end(), [](const MarkdownLink& a, const MarkdownLink& b) {
        return a.line < b.line || (a.line == b.line && a.column < b.column);
    });
    return links;
}

// the text a heading shows: a link or an image by its text
static QString plainHeading(const QString& title)
{
    static const QRegularExpression link(R"(!?\[((?:\\.|[^\\\]])*)\]\([^)]*\))");
    QString plain = title;
    plain.replace(link, "\\1");
    return plain;
}

QList<MarkdownHeading> markdownHeadings(const QString& text)
{
    static const QRegularExpression atx(R"(^ {0,3}(#{1,6})\s+(.*?)(\s+#+)?\s*$)");
    static const QRegularExpression setext(R"(^ {0,3}(=+|-+)\s*$)");
    static const QRegularExpression block(R"(^( {4}|\s*([-*+>]|\d+[.)])\s|\s*\|))");
    QList<MarkdownHeading> headings;
    QHash<QString, int> seen;
    QList<QPair<int, QString>> lines = proseLines(text, false);
    for (int i = 0; i < lines.size(); i++) {
        const QString& line = lines.at(i).second;
        MarkdownHeading h;
        QRegularExpressionMatch m = atx.match(line);
        if (m.hasMatch()) {
            h.level = m.captured(1).size();
            h.title = m.captured(2);
        } else if (i + 1 < lines.size() && lines.at(i + 1).first == lines.at(i).first + 1 &&
                   !line.trimmed().isEmpty() && !block.match(line).hasMatch() &&
                   setext.match(lines.at(i + 1).second).hasMatch() &&
                   (i == 0 || lines.at(i - 1).first != lines.at(i).first - 1 ||
                    lines.at(i - 1).second.trimmed().isEmpty() || atx.match(lines.at(i - 1).second).hasMatch())) {
            // a one-line paragraph underlined; an ATX heading before it ends no paragraph
            h.level = lines.at(i + 1).second.trimmed().startsWith('=') ? 1 : 2;
            h.title = line.trimmed();
        } else {
            continue;
        }
        h.line = lines.at(i).first + 1;
        QString name = MarkdownView::headingAnchor(plainHeading(h.title));
        int n = seen.value(name, 0);
        seen.insert(name, n + 1);
        h.anchor = n > 0 ? name + "-" + QString::number(n) : name;
        headings << h;
    }
    return headings;
}

static bool isMarkdownFile(const QString& path)
{
    return hasSuffix(path, "md") || hasSuffix(path, "markdown");
}

static QString readText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return QString();
    return QString::fromUtf8(file.readAll());
}

QList<MarkdownFinding> markdownCheck(const QString& text, const QString& baseDir,
                                     const std::function<QString(const QString&)>& openText)
{
    QList<MarkdownFinding> findings;
    auto add = [&findings](int line, int column, const QString& message) {
        MarkdownFinding f;
        f.line = line;
        f.column = column;
        f.message = message;
        findings << f;
    };
    auto anchorsOf = [](const QString& markdown) {
        QSet<QString> anchors;
        for (const MarkdownHeading& h : markdownHeadings(markdown)) anchors << h.anchor;
        return anchors;
    };
    QSet<QString> own = anchorsOf(text);
    QHash<QString, QSet<QString>> others;
    for (const MarkdownLink& link : markdownLinks(text)) {
        QUrl url(link.target, QUrl::TolerantMode);
        // a site is never fetched; a one-letter scheme is a Windows drive
        if (url.scheme().size() > 1) continue;
        QString fragment = url.hasFragment() ? url.fragment(QUrl::FullyDecoded) : QString();
        if (url.path().isEmpty()) {
            // a bare # is the Markdown comment
            if (!fragment.isEmpty() && !own.contains(fragment.toLower())) {
                add(link.line, link.column, QCoreApplication::translate("MarkdownTools", "no heading for #%1").arg(fragment));
            }
            continue;
        }
        // a relative path of an untitled text names nothing
        QString file = markdownImagePath(link.target, baseDir);
        if (file.isEmpty()) continue;
        QString shown = url.path(QUrl::FullyDecoded);
        if (!QFileInfo::exists(file)) {
            add(link.line, link.column, link.image ? QCoreApplication::translate("MarkdownTools", "image not found: %1").arg(shown)
                                                   : QCoreApplication::translate("MarkdownTools", "file not found: %1").arg(shown));
            continue;
        }
        if (fragment.isEmpty() || !isMarkdownFile(file)) continue;
        if (!others.contains(file)) {
            QString open = openText ? openText(file) : QString();
            others.insert(file, anchorsOf(open.isNull() ? readText(file) : open));
        }
        if (!others.value(file).contains(fragment.toLower())) {
            add(link.line, link.column, QCoreApplication::translate("MarkdownTools", "no heading for %1#%2").arg(shown, fragment));
        }
    }
    for (const DiagramImage& d : markdownDiagramImages(text, baseDir)) {
        QString graphml = markdownPath(d.graphml, baseDir);
        bool linked = !d.image.link.isEmpty();
        if (!QFileInfo(d.graphml).isFile()) {
            // a missing link target is told above
            if (!linked) add(d.image.line, d.image.column, QCoreApplication::translate("MarkdownTools", "no diagram file %1").arg(graphml));
            continue;
        }
        if (d.recorded.digest.isEmpty()) {
            add(d.image.line, d.image.column,
                QCoreApplication::translate("MarkdownTools", "diagram image without a recorded source: Update Diagram Images records %1").arg(graphml));
        } else if (!d.upToDate) {
            add(d.image.line, d.image.column, QCoreApplication::translate("MarkdownTools", "diagram image out of date: %1 changed").arg(graphml));
        }
        if (linked && !d.recorded.graphml.isEmpty() && d.recorded.graphml != d.graphml) {
            add(d.image.line, d.image.column, QCoreApplication::translate("MarkdownTools", "linked to %1, its SVG names %2")
                .arg(graphml, markdownPath(d.recorded.graphml, baseDir)));
        }
    }
    std::sort(findings.begin(), findings.end(), [](const MarkdownFinding& a, const MarkdownFinding& b) {
        return a.line < b.line || (a.line == b.line && a.column < b.column);
    });
    return findings;
}

// ---------------------------------------------------------------------------- links

static void collectMarkdownFiles(const QDir& base, const QDir& dir, QStringList* files)
{
    // no QDir::Hidden: the hidden files and folders (.git) are left out
    for (const QFileInfo& info : dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (info.isDir()) {
            if (!info.isSymLink()) collectMarkdownFiles(base, QDir(info.absoluteFilePath()), files);
        } else if (isMarkdownFile(info.fileName())) {
            *files << base.relativeFilePath(info.absoluteFilePath());
        }
    }
}

QStringList markdownFilesUnder(const QString& dir)
{
    QStringList files;
    if (dir.isEmpty()) return files;
    collectMarkdownFiles(QDir(dir), QDir(dir), &files);
    files.sort();
    return files;
}

QString markdownLinkLine(const QString& text, const QString& target)
{
    bool wrap = target.contains(' ') || target.contains('(') || target.contains(')');
    return "[" + markdownLabel(text) + "](" + (wrap ? "<" + target + ">" : target) + ")";
}
