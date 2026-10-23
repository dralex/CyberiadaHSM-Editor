/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The State Machine Editor Comment Link Item
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

#include <QPainter>
#include <QPainterPathStroker>

#include "cyberiadasm_editor_subject_item.h"
#include "cyberiadasm_editor_transition_item.h"
#include "edge_geometry.h"
#include "settings_manager.h"

// the comment point square (PNST 984 7.2.7) and the part of the end segments of a
// commented transition kept free of an unstored link foot (as libhtreegeom does)
static const qreal SUBJECT_POINT_SIZE = 6.0;
static const qreal SUBJECT_FOOT_MARGIN = 0.25;
static const qreal SUBJECT_HIT_WIDTH = 6.0;

static QPointF toQPoint(const Cyberiada::Point& p)
{
    return QPointF(p.x, p.y);
}

CyberiadaSMEditorSubjectItem::CyberiadaSMEditorSubjectItem(CyberiadaSMModel* _model,
                                                           Cyberiada::Comment* _comment, int _index,
                                                           QMap<Cyberiada::ID, QGraphicsItem*>& elementItem):
    QGraphicsObject(nullptr), model(_model), comment(_comment), index(_index),
    elementIdToItemMap(elementItem)
{
    setFlags(ItemIsSelectable);
    connectEnd(commentItem());
    if (CyberiadaSMEditorTransitionItem* t = targetTransition()) {
        connectEnd(t->source());
        connectEnd(t->target());
    } else {
        connectEnd(targetNode());
    }
}

void CyberiadaSMEditorSubjectItem::connectEnd(QGraphicsItem* item)
{
    CyberiadaSMEditorAbstractItem* end = dynamic_cast<CyberiadaSMEditorAbstractItem*>(item);
    if (!end) return;
    connect(end, &CyberiadaSMEditorAbstractItem::geometryChanged, this, &CyberiadaSMEditorSubjectItem::updatePath);
    connect(end, &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorSubjectItem::updatePath);
}

const Cyberiada::CommentSubject& CyberiadaSMEditorSubjectItem::subject() const
{
    return comment->get_subjects().at(size_t(index));
}

bool CyberiadaSMEditorSubjectItem::drawable(const Cyberiada::Comment* c, const Cyberiada::CommentSubject& s,
                                            const QMap<Cyberiada::ID, QGraphicsItem*>& map)
{
    const Cyberiada::Element* target = s.get_element();
    if (!c || !target || !c->has_geometry() || !map.value(c->get_id())) return false;
    if (target->get_type() == Cyberiada::elementTransition) {
        return map.value(target->get_id()) != nullptr;
    }
    return target->has_geometry() && map.value(target->get_id()) != nullptr;
}

CyberiadaSMEditorAbstractItem* CyberiadaSMEditorSubjectItem::commentItem() const
{
    return dynamic_cast<CyberiadaSMEditorAbstractItem*>(elementIdToItemMap.value(comment->get_id()));
}

CyberiadaSMEditorTransitionItem* CyberiadaSMEditorSubjectItem::targetTransition() const
{
    const Cyberiada::Element* target = subject().get_element();
    if (!target || target->get_type() != Cyberiada::elementTransition) return nullptr;
    return dynamic_cast<CyberiadaSMEditorTransitionItem*>(elementIdToItemMap.value(target->get_id()));
}

CyberiadaSMEditorAbstractItem* CyberiadaSMEditorSubjectItem::targetNode() const
{
    if (CyberiadaSMEditorTransitionItem* t = targetTransition()) return t->source();
    const Cyberiada::Element* target = subject().get_element();
    if (!target) return nullptr;
    return dynamic_cast<CyberiadaSMEditorAbstractItem*>(elementIdToItemMap.value(target->get_id()));
}

QPointF CyberiadaSMEditorSubjectItem::commentCentre() const
{
    CyberiadaSMEditorAbstractItem* c = commentItem();
    return c ? c->sceneBoundingRect().center() : QPointF();
}

QPointF CyberiadaSMEditorSubjectItem::frameCentre() const
{
    CyberiadaSMEditorAbstractItem* n = targetNode();
    return n ? n->sceneBoundingRect().center() : QPointF();
}

QPointF CyberiadaSMEditorSubjectItem::targetEnd() const
{
    const Cyberiada::CommentSubject& s = subject();
    const Cyberiada::Polyline& pl = s.get_geometry_polyline();
    QPointF toward = !pl.empty() ? toQPoint(pl.back()) + commentCentre() :
        (s.has_geometry_source_point() ? toQPoint(s.get_geometry_source_point()) + commentCentre() :
         commentCentre());
    if (CyberiadaSMEditorTransitionItem* t = targetTransition()) {
        // the foot lies on the path: the stored point is brought onto it after a
        // move of the transition, an unstored one keeps off the arrow
        QPainterPath path = t->mapToScene(t->path());
        if (s.has_geometry_target_point()) {
            return EdgeGeometry::nearestOnPath(path, toQPoint(s.get_geometry_target_point()) + frameCentre());
        }
        return EdgeGeometry::nearestOnPath(path, toward, SUBJECT_FOOT_MARGIN, t->labelSceneRect());
    }
    if (s.has_geometry_target_point()) {
        return toQPoint(s.get_geometry_target_point()) + frameCentre();
    }
    return EdgeGeometry::attachedPoint(targetNode(), toward) + frameCentre();
}

QPointF CyberiadaSMEditorSubjectItem::sourceEnd() const
{
    const Cyberiada::CommentSubject& s = subject();
    if (s.has_geometry_source_point()) {
        return toQPoint(s.get_geometry_source_point()) + commentCentre();
    }
    const Cyberiada::Polyline& pl = s.get_geometry_polyline();
    QPointF toward = !pl.empty() ? toQPoint(pl.front()) + commentCentre() : targetEnd();
    return EdgeGeometry::attachedPoint(commentItem(), toward) + commentCentre();
}

QPainterPath CyberiadaSMEditorSubjectItem::path() const
{
    QPainterPath p;
    if (!commentItem() || !targetNode()) return p;
    p.moveTo(sourceEnd());
    for (const Cyberiada::Point& v : subject().get_geometry_polyline()) {
        p.lineTo(toQPoint(v) + commentCentre());
    }
    p.lineTo(targetEnd());
    return p;
}

QRectF CyberiadaSMEditorSubjectItem::boundingRect() const
{
    qreal m = SUBJECT_POINT_SIZE + SettingsManager::instance().getSelectionBorderWidth();
    return path().boundingRect().adjusted(-m, -m, m, m);
}

QPainterPath CyberiadaSMEditorSubjectItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(SUBJECT_HIT_WIDTH);
    return stroker.createStroke(path());
}

void CyberiadaSMEditorSubjectItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    QPen pen(Qt::black, 1, Qt::DashLine);
    QColor point(Qt::black);
    if (isSelected()) {
        SettingsManager& sm = SettingsManager::instance();
        pen.setColor(sm.getSelectionColor());
        pen.setWidth(sm.getSelectionBorderWidth());
        point = sm.getSelectionColor();
    }
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path());
    // a link to an element shows its comment point; a fragment is underlined instead
    if (!subject().has_fragment()) {
        QPointF end = targetEnd();
        qreal h = SUBJECT_POINT_SIZE / 2.0;
        painter->setPen(Qt::NoPen);
        painter->setBrush(point);
        painter->drawRect(QRectF(end.x() - h, end.y() - h, SUBJECT_POINT_SIZE, SUBJECT_POINT_SIZE));
    }
}

void CyberiadaSMEditorSubjectItem::updatePath()
{
    prepareGeometryChange();
    update();
}
