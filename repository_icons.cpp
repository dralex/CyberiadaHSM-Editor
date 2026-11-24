/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The icons of the Repository dock: the kind of a file and its status marker
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

#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSet>

#include "repository_icons.h"

// the colors of the places of a change (EDIT-GIT-4)
static const QRgb MARKER_INDEX = 0x2e7d32;      // staged: green
static const QRgb MARKER_TREE = 0xef6c00;       // not staged: orange
static const QRgb MARKER_CONFLICT = 0xc62828;   // conflicted: red
static const QRgb MARKER_IGNORED = 0x9e9e9e;    // the Ignored group: grey
// the disk against the side of the icon: on a file picture, alone for a group
static const qreal MARKER_SIDE = 0.6;
static const qreal GROUP_MARKER_SIDE = 0.8;
// against the diameter of the disk: the white ring keeping it apart from the picture, the
// stroke of the glyph
static const qreal MARKER_RING = 0.1;
static const qreal GLYPH_WIDTH = 0.17;
// the half length of a glyph stroke against the radius of the disk
static const qreal GLYPH_REACH = 0.55;
static const qreal LEAST_GLYPH_WIDTH = 1.0;

static const QChar PATH_SEPARATOR('/');
static const QChar SUFFIX_SEPARATOR('.');

FileKind fileKind(const QString& path)
{
    // an ignored folder is one row, its path ending with '/'
    if (path.endsWith(PATH_SEPARATOR)) return FileKind::folder;
    static const QSet<QString> codeNames = {
        "Makefile", "makefile", "GNUmakefile", "CMakeLists.txt", "Dockerfile"
    };
    static const QSet<QString> codeSuffixes = {
        "c", "h", "cc", "cpp", "cxx", "hh", "hpp", "hxx", "ino", "py", "pyw", "js", "mjs", "ts",
        "tsx", "jsx", "cs", "java", "kt", "lua", "rs", "go", "rb", "pl", "php", "swift", "sh",
        "bash", "bat", "cmd", "ps1", "cmake", "qml", "json", "xml", "yaml", "yml", "toml", "sql"
    };
    QString name = path.mid(path.lastIndexOf(PATH_SEPARATOR) + 1);
    if (codeNames.contains(name)) return FileKind::code;
    int dot = name.lastIndexOf(SUFFIX_SEPARATOR);
    QString suffix = dot <= 0 ? QString() : name.mid(dot + 1).toLower();
    if (suffix == "graphml") return FileKind::diagram;
    if (suffix == "md" || suffix == "markdown") return FileKind::markdown;
    if (codeSuffixes.contains(suffix)) return FileKind::code;
    return FileKind::text;
}

int StatusMarker::key() const
{
    // the places take the low bits
    return places | (int(glyph) << 4);
}

StatusMarker StatusMarker::of(const StatusEntry& e)
{
    StatusMarker m;
    if (e.conflicted()) {
        m.places = placeConflict;
        m.glyph = glyphConflict;
        return m;
    }
    if (e.unchanged() || e.ignored()) return m;
    if (e.untracked()) {
        m.places = placeTree;
        m.glyph = glyphUntracked;
        return m;
    }
    if (e.index != STATUS_NONE) m.places |= placeIndex;
    if (e.tree != STATUS_NONE) m.places |= placeTree;
    if (e.index == QLatin1Char('D') || e.tree == QLatin1Char('D')) m.glyph = glyphDeleted;
    else if (e.index == QLatin1Char('A')) m.glyph = glyphAdded;
    else if (e.index == QLatin1Char('R') || e.index == QLatin1Char('C')) m.glyph = glyphRenamed;
    return m;
}

static void paintMarker(QPainter* p, const QRectF& r, const StatusMarker& m)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);
    p->setBrush(Qt::white);
    p->drawEllipse(r);
    qreal ring = r.width() * MARKER_RING;
    QRectF disk = r.adjusted(ring, ring, -ring, -ring);
    // the angles of QPainter::drawPie are sixteenths of a degree
    const int quarter = 90 * 16, half = 180 * 16;
    if (m.places & StatusMarker::placeConflict) {
        p->setBrush(QColor(MARKER_CONFLICT));
        p->drawEllipse(disk);
    } else if (m.places & StatusMarker::placeIgnored) {
        p->setBrush(QColor(MARKER_IGNORED));
        p->drawEllipse(disk);
    } else if ((m.places & StatusMarker::placeIndex) && (m.places & StatusMarker::placeTree)) {
        // staged and changed again: the index at the left, the work tree at the right
        p->setBrush(QColor(MARKER_INDEX));
        p->drawPie(disk, quarter, half);
        p->setBrush(QColor(MARKER_TREE));
        p->drawPie(disk, -quarter, half);
    } else {
        p->setBrush(QColor(m.places & StatusMarker::placeIndex ? MARKER_INDEX : MARKER_TREE));
        p->drawEllipse(disk);
    }
    QPointF c = disk.center();
    qreal radius = disk.width() / 2;
    qreal reach = radius * GLYPH_REACH;
    p->setPen(QPen(Qt::white, qMax(LEAST_GLYPH_WIDTH, disk.width() * GLYPH_WIDTH), Qt::SolidLine,
                   Qt::RoundCap, Qt::RoundJoin));
    p->setBrush(Qt::NoBrush);
    auto at = [&c, radius](qreal x, qreal y) { return c + QPointF(x * radius, y * radius); };
    switch (m.glyph) {
    case StatusMarker::glyphAdded:
        p->drawLine(c - QPointF(reach, 0), c + QPointF(reach, 0));
        p->drawLine(c - QPointF(0, reach), c + QPointF(0, reach));
        break;
    case StatusMarker::glyphDeleted:
        p->drawLine(c - QPointF(reach, 0), c + QPointF(reach, 0));
        break;
    case StatusMarker::glyphRenamed:
        p->drawLine(c - QPointF(reach, 0), c + QPointF(reach, 0));
        p->drawLine(c + QPointF(reach, 0), at(0.1, -0.45));
        p->drawLine(c + QPointF(reach, 0), at(0.1, 0.45));
        break;
    case StatusMarker::glyphUntracked: {
        QPainterPath hook(at(-0.32, -0.22));
        hook.cubicTo(at(-0.32, -0.72), at(0.38, -0.72), at(0.34, -0.24));
        hook.cubicTo(at(0.3, 0.02), at(0, 0), at(0, 0.2));
        p->drawPath(hook);
        p->drawPoint(at(0, 0.52));
        break;
    }
    case StatusMarker::glyphConflict:
        p->drawLine(at(0, -0.55), at(0, 0.15));
        p->drawPoint(at(0, 0.5));
        break;
    default:
        break;
    }
    p->restore();
}

// Paints the picture and the marker at the size asked, so the marker stays sharp at any
// icon size and device pixel ratio
class MarkedIconEngine: public QIconEngine {
public:
    MarkedIconEngine(const QIcon& base, const StatusMarker& marker, bool greyed):
        m_base(base), m_marker(marker), m_greyed(greyed) {}

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        if (!m_base.isNull()) {
            m_base.paint(painter, rect, Qt::AlignCenter, m_greyed ? QIcon::Disabled : mode, state);
        }
        if (m_marker.isEmpty()) return;
        QRectF area(rect);
        qreal side = qMin(area.width(), area.height());
        if (m_base.isNull()) {
            qreal d = side * GROUP_MARKER_SIDE;
            paintMarker(painter, QRectF(area.center() - QPointF(d / 2, d / 2), QSizeF(d, d)), m_marker);
        } else {
            qreal d = side * MARKER_SIDE;
            paintMarker(painter, QRectF(area.right() - d, area.bottom() - d, d, d), m_marker);
        }
    }

    // the default leaves the pixmap uninitialized under the picture
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        QPixmap pixmap(size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        paint(&painter, QRect(QPoint(0, 0), size), mode, state);
        return pixmap;
    }

    QIconEngine* clone() const override
    {
        return new MarkedIconEngine(m_base, m_marker, m_greyed);
    }

private:
    QIcon        m_base;
    StatusMarker m_marker;
    bool         m_greyed;
};

static QIcon kindIcon(FileKind kind)
{
    switch (kind) {
    case FileKind::diagram:  return QIcon(":/Icons/images/document-new.png");
    case FileKind::markdown: return QIcon(":/Icons/images/document-new-md.png");
    case FileKind::code:     return QIcon(":/Icons/images/file-code.png");
    case FileKind::folder:   return QIcon(":/Icons/images/folder.png");
    default:                 return QIcon(":/Icons/images/file-text.png");
    }
}

QIcon markedIcon(FileKind kind, const StatusMarker& marker, bool greyed)
{
    QIcon base = kindIcon(kind);
    if (marker.isEmpty() && !greyed) return base;
    return QIcon(new MarkedIconEngine(base, marker, greyed));
}

QIcon markerIcon(const StatusMarker& marker)
{
    return QIcon(new MarkedIconEngine(QIcon(), marker, false));
}
