/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The SVG markers drawn as plain groups for QtSvg
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

#include <QHash>
#include <QList>
#include <QRectF>
#include <QRegularExpression>
#include <QStack>
#include <QXmlStreamReader>
#include <QtMath>
#include <algorithm>

#include "svg_markers.h"

namespace {

// the default size of a marker viewport (SVG 1.1, 11.6.2)
const qreal MARKER_DEFAULT_SIZE = 3.0;
// the CSS units in user units (px) at 96 dpi
const qreal PX_PER_IN = 96.0;
const qreal PX_PER_PT = PX_PER_IN / 72.0;
const qreal PX_PER_PC = PX_PER_IN / 6.0;
const qreal PX_PER_MM = PX_PER_IN / 25.4;
const qreal PX_PER_CM = PX_PER_IN / 2.54;
const int   NUMBER_PRECISION = 10;
const int   XML_DECLARATION_SIZE = 200;

// the numbers of the path data and the point lists with their shorthands
// (1.5.5, -1-2, 1e-3) and the arc flags written without separators
class PathScanner {
public:
    explicit PathScanner(const QString& text): s(text) {}

    bool atEnd() { skip(); return pos >= s.size(); }
    bool atCommand()
    {
        skip();
        if (pos >= s.size()) return false;
        QChar c = s.at(pos);
        return c.isLetter() && c != 'e' && c != 'E';
    }
    QChar command() { return s.at(pos++); }

    bool number(qreal* value)
    {
        skip();
        int start = pos;
        if (pos < s.size() && (s.at(pos) == '+' || s.at(pos) == '-')) pos++;
        bool digits = false;
        while (pos < s.size() && s.at(pos).isDigit()) { pos++; digits = true; }
        if (pos < s.size() && s.at(pos) == '.') {
            pos++;
            while (pos < s.size() && s.at(pos).isDigit()) { pos++; digits = true; }
        }
        if (!digits) {
            pos = start;
            return false;
        }
        if (pos < s.size() && (s.at(pos) == 'e' || s.at(pos) == 'E')) {
            int mark = pos++;
            if (pos < s.size() && (s.at(pos) == '+' || s.at(pos) == '-')) pos++;
            if (pos < s.size() && s.at(pos).isDigit()) {
                while (pos < s.size() && s.at(pos).isDigit()) pos++;
            } else {
                pos = mark;
            }
        }
        bool ok = false;
        *value = s.mid(start, pos - start).toDouble(&ok);
        return ok;
    }

    bool point(QPointF* p)
    {
        qreal x, y;
        if (!number(&x) || !number(&y)) return false;
        *p = QPointF(x, y);
        return true;
    }

    bool flag(bool* value)
    {
        skip();
        if (pos >= s.size() || (s.at(pos) != '0' && s.at(pos) != '1')) return false;
        *value = s.at(pos++) == '1';
        return true;
    }

private:
    void skip() { while (pos < s.size() && (s.at(pos).isSpace() || s.at(pos) == ',')) pos++; }

    const QString& s;
    int pos = 0;
};

// a drawn piece of a path with the directions at its two ends
struct Segment {
    QPointF p0, p1, t0, t1;
};

bool isZero(const QPointF& v)
{
    return qFuzzyIsNull(v.x()) && qFuzzyIsNull(v.y());
}

QPointF firstNonZero(const QPointF& a, const QPointF& b, const QPointF& c)
{
    return !isZero(a) ? a : (!isZero(b) ? b : c);
}

Segment lineSegment(const QPointF& p0, const QPointF& p1)
{
    Segment s;
    s.p0 = p0; s.p1 = p1;
    s.t0 = s.t1 = p1 - p0;
    return s;
}

Segment cubicSegment(const QPointF& p0, const QPointF& c1, const QPointF& c2, const QPointF& p1)
{
    Segment s;
    s.p0 = p0; s.p1 = p1;
    s.t0 = firstNonZero(c1 - p0, c2 - p0, p1 - p0);
    s.t1 = firstNonZero(p1 - c2, p1 - c1, p1 - p0);
    return s;
}

Segment quadSegment(const QPointF& p0, const QPointF& c, const QPointF& p1)
{
    return cubicSegment(p0, c, c, p1);
}

qreal vectorAngle(qreal ux, qreal uy, qreal vx, qreal vy)
{
    return qAtan2(ux * vy - uy * vx, ux * vx + uy * vy);
}

// the directions at the ends of an elliptical arc from its centre form
// (SVG 1.1, F.6.5 and F.6.6)
Segment arcSegment(const QPointF& p0, qreal rx, qreal ry, qreal degrees,
                   bool large, bool sweep, const QPointF& p1)
{
    rx = qAbs(rx);
    ry = qAbs(ry);
    if (qFuzzyIsNull(rx) || qFuzzyIsNull(ry) || p0 == p1) return lineSegment(p0, p1);
    qreal phi = qDegreesToRadians(degrees);
    qreal c = qCos(phi), s = qSin(phi);
    qreal dx = (p0.x() - p1.x()) / 2, dy = (p0.y() - p1.y()) / 2;
    qreal x1 = c * dx + s * dy, y1 = -s * dx + c * dy;
    qreal lambda = (x1 * x1) / (rx * rx) + (y1 * y1) / (ry * ry);
    if (lambda > 1) {
        rx *= qSqrt(lambda);
        ry *= qSqrt(lambda);
    }
    qreal num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
    qreal den = rx * rx * y1 * y1 + ry * ry * x1 * x1;
    qreal coef = (large != sweep ? 1 : -1) * qSqrt(qMax(qreal(0), num / den));
    qreal cx = coef * rx * y1 / ry, cy = -coef * ry * x1 / rx;
    qreal theta = vectorAngle(1, 0, (x1 - cx) / rx, (y1 - cy) / ry);
    qreal delta = vectorAngle((x1 - cx) / rx, (y1 - cy) / ry, (-x1 - cx) / rx, (-y1 - cy) / ry);
    if (!sweep && delta > 0) delta -= 2 * M_PI;
    if (sweep && delta < 0) delta += 2 * M_PI;
    qreal sign = delta > 0 ? 1 : -1;
    auto tangent = [&](qreal t) {
        qreal tx = -rx * qSin(t), ty = ry * qCos(t);
        return QPointF(c * tx - s * ty, s * tx + c * ty) * sign;
    };
    Segment seg;
    seg.p0 = p0; seg.p1 = p1;
    seg.t0 = tangent(theta);
    seg.t1 = tangent(theta + delta);
    return seg;
}

QList<Segment> pathSegments(const QString& d)
{
    QList<Segment> segments;
    PathScanner scan(d);
    QPointF current, start, control;
    QChar cmd, previous;
    bool active = false;
    while (!scan.atEnd()) {
        if (scan.atCommand()) {
            cmd = scan.command();
            active = true;
        } else if (!active) {
            break;
        }
        bool relative = cmd.isLower();
        QChar upper = cmd.toUpper();
        QPointF base = relative ? current : QPointF();
        QPointF p, c1, c2;
        qreal v;
        if (upper == 'Z') {
            if (current != start) segments.append(lineSegment(current, start));
            current = start;
            active = false;
        } else if (upper == 'M') {
            if (!scan.point(&p)) break;
            current = start = base + p;
            // the pairs after a moveto are linetos
            cmd = relative ? 'l' : 'L';
        } else if (upper == 'L') {
            if (!scan.point(&p)) break;
            segments.append(lineSegment(current, base + p));
            current = base + p;
        } else if (upper == 'H') {
            if (!scan.number(&v)) break;
            p = QPointF(relative ? current.x() + v : v, current.y());
            segments.append(lineSegment(current, p));
            current = p;
        } else if (upper == 'V') {
            if (!scan.number(&v)) break;
            p = QPointF(current.x(), relative ? current.y() + v : v);
            segments.append(lineSegment(current, p));
            current = p;
        } else if (upper == 'C' || upper == 'S') {
            if (upper == 'C') {
                if (!scan.point(&c1)) break;
                c1 += base;
            } else {
                c1 = (previous == 'C' || previous == 'S') ? 2 * current - control : current;
            }
            if (!scan.point(&c2) || !scan.point(&p)) break;
            c2 += base;
            p += base;
            segments.append(cubicSegment(current, c1, c2, p));
            control = c2;
            current = p;
        } else if (upper == 'Q' || upper == 'T') {
            if (upper == 'Q') {
                if (!scan.point(&c1)) break;
                c1 += base;
            } else {
                c1 = (previous == 'Q' || previous == 'T') ? 2 * current - control : current;
            }
            if (!scan.point(&p)) break;
            p += base;
            segments.append(quadSegment(current, c1, p));
            control = c1;
            current = p;
        } else if (upper == 'A') {
            qreal rx, ry, rotation;
            bool large, sweep;
            if (!scan.number(&rx) || !scan.number(&ry) || !scan.number(&rotation) ||
                !scan.flag(&large) || !scan.flag(&sweep) || !scan.point(&p)) break;
            p += base;
            segments.append(arcSegment(current, rx, ry, rotation, large, sweep, p));
            current = p;
        } else {
            break;
        }
        previous = upper;
    }
    return segments;
}

void segmentEnds(const QList<Segment>& segments, SvgPathEnd* start, SvgPathEnd* end)
{
    if (segments.isEmpty()) return;
    if (start) {
        start->point = segments.first().p0;
        for (int i = 0; i < segments.size() && isZero(start->direction); i++)
            start->direction = segments.at(i).t0;
        start->valid = true;
    }
    if (end) {
        end->point = segments.last().p1;
        for (int i = segments.size() - 1; i >= 0 && isZero(end->direction); i--)
            end->direction = segments.at(i).t1;
        end->valid = true;
    }
}

QList<QPointF> pointList(const QString& text)
{
    QList<QPointF> points;
    PathScanner scan(text);
    QPointF p;
    while (scan.point(&p)) points.append(p);
    return points;
}

qreal length(const QString& text, qreal fallback)
{
    static const QRegularExpression re("^\\s*([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)\\s*([a-z%]*)\\s*$");
    QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch()) return fallback;
    qreal v = m.captured(1).toDouble();
    QString unit = m.captured(2);
    if (unit == "pt") return v * PX_PER_PT;
    if (unit == "pc") return v * PX_PER_PC;
    if (unit == "mm") return v * PX_PER_MM;
    if (unit == "cm") return v * PX_PER_CM;
    if (unit == "in") return v * PX_PER_IN;
    return v;
}

// the id of url(#id) or url("#id"), empty for none
QString markerRef(const QString& value)
{
    static const QRegularExpression re("url\\(\\s*['\"]?#([^'\")\\s]+)['\"]?\\s*\\)");
    QRegularExpressionMatch m = re.match(value);
    return m.hasMatch() ? m.captured(1) : QString();
}

// the inherited properties the markers need
struct Props {
    qreal   strokeWidth = 1;
    QString markerStart;
    QString markerEnd;
    bool    hidden = false;
};

void applyProperty(const QString& name, const QString& value, Props* p)
{
    if (name == "stroke-width") p->strokeWidth = length(value, p->strokeWidth);
    else if (name == "marker-start") p->markerStart = markerRef(value);
    else if (name == "marker-end") p->markerEnd = markerRef(value);
    else if (name == "marker") p->markerStart = p->markerEnd = markerRef(value);
    else if (name == "display") p->hidden = value.trimmed() == "none";
}

// the presentation attributes first, the style declarations over them
void applyPresentation(const QXmlStreamAttributes& attrs, Props* p)
{
    static const char* const names[] = {"stroke-width", "marker", "marker-start", "marker-end", "display"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        QString name = QString::fromLatin1(names[i]);
        if (attrs.hasAttribute(name)) applyProperty(name, attrs.value(name).toString(), p);
    }
    const QStringList decls = attrs.value("style").toString().split(';');
    for (const QString& decl : decls) {
        int colon = decl.indexOf(':');
        if (colon > 0) applyProperty(decl.left(colon).trimmed(), decl.mid(colon + 1).trimmed(), p);
    }
}

struct MarkerDef {
    qreal   refX = 0;
    qreal   refY = 0;
    QString orient;
    bool    strokeUnits = true;
    QRectF  viewBox;
    qreal   width = MARKER_DEFAULT_SIZE;
    qreal   height = MARKER_DEFAULT_SIZE;
    QString content;
};

MarkerDef markerDef(const QXmlStreamAttributes& attrs)
{
    MarkerDef m;
    m.refX = length(attrs.value("refX").toString(), 0);
    m.refY = length(attrs.value("refY").toString(), 0);
    m.orient = attrs.value("orient").toString().trimmed();
    m.strokeUnits = attrs.value("markerUnits") != QLatin1String("userSpaceOnUse");
    m.width = length(attrs.value("markerWidth").toString(), MARKER_DEFAULT_SIZE);
    m.height = length(attrs.value("markerHeight").toString(), MARKER_DEFAULT_SIZE);
    QList<QPointF> vb = pointList(attrs.value("viewBox").toString());
    if (vb.size() == 2 && vb.at(1).x() > 0 && vb.at(1).y() > 0)
        m.viewBox = QRectF(vb.at(0), QSizeF(vb.at(1).x(), vb.at(1).y()));
    return m;
}

// a marked element: where its groups go and what they need
struct Insertion {
    int        offset = -1;
    QString    transform;
    qreal      strokeWidth = 1;
    QString    startId;
    QString    endId;
    SvgPathEnd start;
    SvgPathEnd end;
};

QString num(qreal v)
{
    return QString::number(v, 'g', NUMBER_PRECISION);
}

QString markerGroup(const MarkerDef& m, const SvgPathEnd& at, bool isStart,
                    qreal strokeWidth, const QString& transform)
{
    static const QRegularExpression ids("\\s(?:xml:)?id\\s*=\\s*(\"[^\"]*\"|'[^']*')");
    qreal angle;
    if (m.orient == "auto" || m.orient == "auto-start-reverse") {
        angle = qRadiansToDegrees(qAtan2(at.direction.y(), at.direction.x()));
        if (isStart && m.orient == "auto-start-reverse") angle += 180;
    } else {
        QString o = m.orient;
        if (o.endsWith("deg")) o.chop(3);
        angle = o.toDouble();
    }
    qreal scale = m.strokeUnits ? strokeWidth : 1;
    if (!m.viewBox.isEmpty())
        scale *= qMin(m.width / m.viewBox.width(), m.height / m.viewBox.height());
    QString t;
    if (!transform.isEmpty()) t = transform + " ";
    t += "translate(" + num(at.point.x()) + "," + num(at.point.y()) + ") rotate(" + num(angle) +
         ") scale(" + num(scale) + ") translate(" + num(-m.refX) + "," + num(-m.refY) + ")";
    // the copies must not repeat the ids of the marker
    QString content = m.content;
    content.remove(ids);
    return "<g transform=\"" + t + "\">" + content + "</g>";
}

bool markable(const QStringRef& name)
{
    return name == QLatin1String("path") || name == QLatin1String("line") ||
           name == QLatin1String("polyline") || name == QLatin1String("polygon");
}

void elementEnds(const QStringRef& name, const QXmlStreamAttributes& attrs,
                 SvgPathEnd* start, SvgPathEnd* end)
{
    if (name == QLatin1String("path")) {
        svgPathEnds(attrs.value("d").toString(), start, end);
        return;
    }
    QList<Segment> segments;
    if (name == QLatin1String("line")) {
        QPointF p0(length(attrs.value("x1").toString(), 0), length(attrs.value("y1").toString(), 0));
        QPointF p1(length(attrs.value("x2").toString(), 0), length(attrs.value("y2").toString(), 0));
        segments.append(lineSegment(p0, p1));
    } else {
        QList<QPointF> points = pointList(attrs.value("points").toString());
        for (int i = 1; i < points.size(); i++) segments.append(lineSegment(points.at(i - 1), points.at(i)));
        if (name == QLatin1String("polygon") && points.size() > 2 && points.last() != points.first())
            segments.append(lineSegment(points.last(), points.first()));
    }
    segmentEnds(segments, start, end);
}

} // namespace

void svgPathEnds(const QString& d, SvgPathEnd* start, SvgPathEnd* end)
{
    segmentEnds(pathSegments(d), start, end);
}

QByteArray expandSvgMarkers(const QByteArray& svg)
{
    if (!svg.contains("marker")) return svg;
    // the offsets below are those of the UTF-8 text; another encoding stays as it is
    static const QRegularExpression encoding("encoding\\s*=\\s*['\"]([^'\"]+)['\"]");
    QRegularExpressionMatch enc = encoding.match(QString::fromLatin1(svg.left(XML_DECLARATION_SIZE)));
    if (enc.hasMatch() && enc.captured(1).compare("utf-8", Qt::CaseInsensitive) != 0) return svg;

    QString src = QString::fromUtf8(svg);
    QXmlStreamReader xml(src);
    QStack<Props> props;
    props.push(Props());
    QStack<int> pending;
    QHash<QString, MarkerDef> markers;
    QList<Insertion> insertions;
    int markerDepth = 0;
    MarkerDef marker;
    QString markerId;
    int contentStart = 0;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            Props p = props.top();
            applyPresentation(xml.attributes(), &p);
            props.push(p);
            int index = -1;
            if (markerDepth > 0) {
                markerDepth++;
            } else if (xml.name() == QLatin1String("marker")) {
                markerDepth = 1;
                marker = markerDef(xml.attributes());
                markerId = xml.attributes().value("id").toString();
                contentStart = int(xml.characterOffset());
            } else if (!p.hidden && (!p.markerStart.isEmpty() || !p.markerEnd.isEmpty()) &&
                       markable(xml.name())) {
                Insertion ins;
                ins.transform = xml.attributes().value("transform").toString();
                ins.strokeWidth = p.strokeWidth;
                ins.startId = p.markerStart;
                ins.endId = p.markerEnd;
                elementEnds(xml.name(), xml.attributes(),
                            p.markerStart.isEmpty() ? nullptr : &ins.start,
                            p.markerEnd.isEmpty() ? nullptr : &ins.end);
                if (ins.start.valid || ins.end.valid) {
                    insertions.append(ins);
                    index = insertions.size() - 1;
                }
            }
            pending.push(index);
        } else if (xml.isEndElement()) {
            props.pop();
            int index = pending.pop();
            if (markerDepth > 0) {
                if (--markerDepth == 0) {
                    int end = src.lastIndexOf("</", int(xml.characterOffset()) - 1);
                    if (end > contentStart) marker.content = src.mid(contentStart, end - contentStart);
                    if (!markerId.isEmpty()) markers.insert(markerId, marker);
                }
            } else if (index >= 0) {
                insertions[index].offset = int(xml.characterOffset());
            }
        }
    }
    // a broken file goes to QtSvg as it is
    if (xml.hasError() || insertions.isEmpty()) return svg;

    std::sort(insertions.begin(), insertions.end(),
              [](const Insertion& a, const Insertion& b) { return a.offset > b.offset; });
    for (const Insertion& ins : insertions) {
        QString groups;
        if (ins.start.valid && markers.contains(ins.startId))
            groups += markerGroup(markers.value(ins.startId), ins.start, true, ins.strokeWidth, ins.transform);
        if (ins.end.valid && markers.contains(ins.endId))
            groups += markerGroup(markers.value(ins.endId), ins.end, false, ins.strokeWidth, ins.transform);
        if (!groups.isEmpty() && ins.offset >= 0) src.insert(ins.offset, groups);
    }
    return src.toUtf8();
}
