/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The State Machine Editor Transition Item
 *
 * Copyright (C) 2025 Anastasia Viktorova <viktorovaa.04@gmail.com>
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

#include <cstdio>
#include <limits>
#include <QPainter>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QtMath>
#include <QTextDocument>
#include <QTextOption>

// transition action includes
#include <QRegularExpression>

#include "myassert.h"
#include "cyberiadasm_editor_transition_item.h"
#include "cyberiada_constants.h"
#include "cyberiadasm_editor_scene.h"
#include "settings_manager.h"
#include "subject_fragment.h"
#include "edge_geometry.h"

using EdgeGeometry::lineIntersect;

/* -----------------------------------------------------------------------------
 * Transition Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorTransitionItem::CyberiadaSMEditorTransitionItem(QObject *parent_object,
                                                                 CyberiadaSMModel *model,
                                                                 Cyberiada::Element *element,
                                                                 QGraphicsItem *parent,
                                                                 QMap<Cyberiada::ID, QGraphicsItem*>& elementItem) :
    CyberiadaSMEditorAbstractItem(model, element, parent),
    // QObject(parent_object),
    elementIdToItemMap(elementItem)
{
    setAcceptHoverEvents(true);
    setFlags(ItemIsSelectable|ItemSendsGeometryChanges);

    isMouseTraking = false;
    isSourceTraking = false;
    isTargetTraking = false;

    transition = static_cast<const Cyberiada::Transition*>(element);

    actionItem = new TransitionAction(actionText(), this);
    setActionVisibility(SettingsManager::instance().getShowTransitionText());
    connect(&SettingsManager::instance(), &SettingsManager::showTransitionTextChanged, this, &CyberiadaSMEditorTransitionItem::setActionVisibility);
    // the label is centered on the polyline, so it follows its own size
    connect(actionItem, &EditableTextItem::sizeChanged, this, &CyberiadaSMEditorTransitionItem::updateActionPosition);
    // the corner handles follow the label box as it reflows
    connect(actionItem, &EditableTextItem::sizeChanged, this, &CyberiadaSMEditorTransitionItem::setLabelDotsPosition);

    connect(target(), &CyberiadaSMEditorAbstractItem::geometryChanged, this, &CyberiadaSMEditorTransitionItem::onTargetGeomertyChanged);
    connect(source(), &CyberiadaSMEditorAbstractItem::geometryChanged, this, &CyberiadaSMEditorTransitionItem::onSourceGeomertyChanged);
    connect(target(), &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorTransitionItem::onTargetSizeChanged);
    connect(source(), &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorTransitionItem::onSourceSizeChanged);

    updateActionPosition();
    initializeDots();
    initializeLabelDots();
    hideDots();
}

CyberiadaSMEditorTransitionItem::~CyberiadaSMEditorTransitionItem()
{
    if(actionItem) { delete actionItem; }

    foreach (DotSignal *dot, listDots) {
        retireDot(dot);
    }
    listDots.clear();

    foreach (DotSignal *dot, labelDots) {
        retireDot(dot);
    }
    labelDots.clear();
}

// a retired handle leaves the scene immediately (so a repaint cannot paint it) but its
// deletion is deferred - the rebuild that retires it may run from the dot's own event
// handler (see CyberiadaSMEditorStateItem::initializeActions for the same pattern)
void CyberiadaSMEditorTransitionItem::retireDot(DotSignal* dot)
{
    if (!dot) return;
    dot->disconnect(this);
    dot->hide();
    if (dot->scene()) dot->scene()->removeItem(dot);
    dot->deleteLater();
}

QRectF CyberiadaSMEditorTransitionItem::boundingRect() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    return path().boundingRect().adjusted(-10, -10, 10, 10); // enlarge the area for the arrowhead
}

void CyberiadaSMEditorTransitionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPen pen = QPen(Qt::black, 2, Qt::SolidLine);
    if (Cyberiada::element_has_color(element)) {
        pen.setColor(QColor(QString::fromStdString(Cyberiada::element_get_color(element))));
    }
    if (runMarkColor().isValid()) {
        pen.setColor(runMarkColor());
        pen.setWidth(RUN_MARK_WIDTH);
    }
    if (isSelected()) {
        SettingsManager& sm = SettingsManager::instance();
        pen.setColor(sm.getSelectionColor());
        pen.setWidth(sm.getSelectionBorderWidth());
    }

    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path());

    drawArrow(painter);
}

QPainterPath CyberiadaSMEditorTransitionItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(5);
    return stroker.createStroke(path());
}

CyberiadaSMEditorAbstractItem *CyberiadaSMEditorTransitionItem::source() const
{
    return static_cast<CyberiadaSMEditorAbstractItem*>(elementIdToItemMap.value(transition->source_element_id()));
}

bool CyberiadaSMEditorTransitionItem::crossesMachine(CyberiadaSMEditorAbstractItem* moving,
                                                     CyberiadaSMEditorAbstractItem* fixed) const
{
    // only a positively determined mismatch blocks the retarget; when either end or
    // its machine is unknown, let the model choke point be the backstop
    if (!moving || !fixed || !model || !model->rootDocument()) return false;
    Cyberiada::StateMachine* a = model->rootDocument()->get_parent_sm(moving->getElement());
    Cyberiada::StateMachine* b = model->rootDocument()->get_parent_sm(fixed->getElement());
    return a && b && a != b;
}

void CyberiadaSMEditorTransitionItem::setSource(CyberiadaSMEditorAbstractItem *newSource)
{
    if (source() == newSource) return;
    // EDIT-STRUCT-4: a retarget may not cross state machines; refuse before touching
    // the view so the dragged dot snaps back (the model would refuse it anyway)
    if (crossesMachine(newSource, target())) return;

    movePolyline(sourceCenter() - newSource->sceneBoundingRect().center());

    disconnect(source(), &CyberiadaSMEditorAbstractItem::geometryChanged, this, &CyberiadaSMEditorTransitionItem::onSourceGeomertyChanged);
    disconnect(source(), &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorTransitionItem::onSourceSizeChanged);

    connect(newSource, &CyberiadaSMEditorAbstractItem::geometryChanged, this, &CyberiadaSMEditorTransitionItem::onSourceGeomertyChanged);
    connect(newSource, &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorTransitionItem::onSourceSizeChanged);

    model->updateGeometry(model->elementToIndex(element), newSource->getId(), targetId());
    loopStartSet = false;
}


QPointF CyberiadaSMEditorTransitionItem::attachedPoint(const CyberiadaSMEditorAbstractItem* item,
                                                       const QPointF& toward) const
{
    return EdgeGeometry::attachedPoint(item, toward);
}

QPointF CyberiadaSMEditorTransitionItem::sourcePoint() const
{
    if (transition->has_geometry_source_point()) {
        return QPointF(transition->get_source_point().x, transition->get_source_point().y);
    }
    if (isBareLoop()) return bareLoopRoute().sp;
    QPointF toward;
    if (transition->has_polyline() && !transition->get_geometry_polyline().empty()) {
        const Cyberiada::Point& v = transition->get_geometry_polyline().front();
        toward = QPointF(v.x, v.y) + sourceCenter();
    } else if (transition->has_geometry_target_point()) {
        toward = QPointF(transition->get_target_point().x, transition->get_target_point().y) + targetCenter();
    } else {
        toward = targetCenter();
    }
    return attachedPoint(source(), toward);
}

// the other end keeps its stored point, or stays unplaced: the displayed
// attachment is never written back
void CyberiadaSMEditorTransitionItem::setSourcePoint(const QPointF &point)
{
    if(sourcePoint() == point) {
        return;
    }
    model->updateGeometry(model->elementToIndex(element), Cyberiada::Point(point.x(), point.y()),
                          transition->get_target_point());
}

void CyberiadaSMEditorTransitionItem::setSourcePoint(const Cyberiada::Point &point)
{
    model->updateGeometry(model->elementToIndex(element), point, transition->get_target_point());
}

QPointF CyberiadaSMEditorTransitionItem::sourceCenter() const
{
    Cyberiada::ID id = transition->source_element_id();

    if(!elementIdToItemMap.value(id)) return QPoint(); // workaround
    MY_ASSERT(elementIdToItemMap.value(id));
    return (elementIdToItemMap.value(id))->sceneBoundingRect().center();
}

CyberiadaSMEditorAbstractItem *CyberiadaSMEditorTransitionItem::target() const
{
    return static_cast<CyberiadaSMEditorAbstractItem*>(elementIdToItemMap.value(transition->target_element_id()));
}

void CyberiadaSMEditorTransitionItem::setTarget(CyberiadaSMEditorAbstractItem *newTarget)
{
    if (target() == newTarget) return;
    // EDIT-STRUCT-4: a retarget may not cross state machines (see setSource)
    if (crossesMachine(newTarget, source())) return;

    disconnect(target(), &CyberiadaSMEditorAbstractItem::geometryChanged, this, &CyberiadaSMEditorTransitionItem::onTargetGeomertyChanged);
    disconnect(target(), &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorTransitionItem::onTargetSizeChanged);

    connect(newTarget, &CyberiadaSMEditorAbstractItem::geometryChanged, this, &CyberiadaSMEditorTransitionItem::onTargetGeomertyChanged);
    connect(newTarget, &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorTransitionItem::onTargetSizeChanged);

    model->updateGeometry(model->elementToIndex(element), sourceId(), newTarget->getId());
    loopStartSet = false;

    isMouseTraking = false;
}

QPointF CyberiadaSMEditorTransitionItem::targetPoint() const
{
    if (transition->has_geometry_target_point()) {
        return QPointF(transition->get_target_point().x, transition->get_target_point().y);
    }
    if (isBareLoop()) return bareLoopRoute().tp;
    QPointF toward;
    if (transition->has_polyline() && !transition->get_geometry_polyline().empty()) {
        const Cyberiada::Point& v = transition->get_geometry_polyline().back();
        toward = QPointF(v.x, v.y) + sourceCenter();
    } else if (transition->has_geometry_source_point()) {
        toward = QPointF(transition->get_source_point().x, transition->get_source_point().y) + sourceCenter();
    } else {
        toward = sourceCenter();
    }
    return attachedPoint(target(), toward);
}

void CyberiadaSMEditorTransitionItem::setTargetPoint(const QPointF &point)
{
    if(targetPoint() == point) {
        return;
    }
    model->updateGeometry(model->elementToIndex(element), transition->get_source_point(),
                          Cyberiada::Point(point.x(), point.y()));
}

QPointF CyberiadaSMEditorTransitionItem::targetCenter() const
{
    if(!elementIdToItemMap.value(transition->target_element_id())) return QPoint(); // workaround
    MY_ASSERT(elementIdToItemMap.value(transition->target_element_id()));
    return (elementIdToItemMap.value(transition->target_element_id()))->sceneBoundingRect().center();
}

// a loop is drawn by its default route while it carries no polyline of its own
bool CyberiadaSMEditorTransitionItem::isBareLoop() const
{
    return source() && source() == target() &&
           (!transition->has_polyline() || transition->get_geometry_polyline().empty());
}

CyberiadaSMEditorTransitionItem::LoopSide
CyberiadaSMEditorTransitionItem::sideFromDir(const QPointF& dir)
{
    if (qAbs(dir.x()) >= qAbs(dir.y())) return dir.x() >= 0 ? LoopRight : LoopLeft;
    return dir.y() >= 0 ? LoopBottom : LoopTop;
}

CyberiadaSMEditorTransitionItem::LoopSide
CyberiadaSMEditorTransitionItem::nearestSide(const QPointF& p, qreal hw, qreal hh, bool* tie)
{
    // how far the point lies past each side line: the nearest side has the most
    qreal past[] = { 0.0, 0.0, 0.0, 0.0 };
    past[LoopTop]    = -p.y() - hh;
    past[LoopBottom] =  p.y() - hh;
    past[LoopLeft]   = -p.x() - hw;
    past[LoopRight]  =  p.x() - hw;
    const int count = sizeof(past) / sizeof(past[0]);
    int best = 0;
    for (int s = 1; s < count; s++) {
        if (past[s] > past[best]) best = s;
    }
    if (tie) {
        *tie = false;
        for (int s = 0; s < count; s++) {
            if (s != best && past[best] - past[s] < LOOP_SIDE_TIE) *tie = true;
        }
    }
    return LoopSide(best);
}

CyberiadaSMEditorTransitionItem::LoopSide
CyberiadaSMEditorTransitionItem::turnSide(LoopSide s, int steps)
{
    static const LoopSide clockwise[] = { LoopTop, LoopRight, LoopBottom, LoopLeft };
    const int count = sizeof(clockwise) / sizeof(clockwise[0]);
    int i = 0;
    while (clockwise[i] != s) i++;
    return clockwise[((i + steps) % count + count) % count];
}

QPointF CyberiadaSMEditorTransitionItem::sidePoint(LoopSide s, qreal along, qreal hw, qreal hh)
{
    switch (s) {
    case LoopTop:    return QPointF(along, -hh);
    case LoopBottom: return QPointF(along,  hh);
    case LoopLeft:   return QPointF(-hw, along);
    default:         return QPointF( hw, along);   // LoopRight
    }
}

qreal CyberiadaSMEditorTransitionItem::sideRoom(LoopSide s, qreal hw, qreal hh)
{
    // the straight part off the rounded corners, a short side whole, a point node unbounded
    qreal half = isTopOrBottom(s) ? hw : hh;
    if (half <= 0) return std::numeric_limits<qreal>::max();
    return half > ROUNDED_RECT_RADIUS ? half - ROUNDED_RECT_RADIUS : half;
}

QPointF CyberiadaSMEditorTransitionItem::bumpPartner(LoopSide s, const QPointF& e, qreal hw, qreal hh)
{
    qreal room = sideRoom(s, hw, hh);
    qreal along = isTopOrBottom(s) ? e.x() : e.y();
    qreal toward = along > 0 ? -1.0 : 1.0;
    return sidePoint(s, qBound(-room, along + toward * 2.0 * LOOP_LATERAL, room), hw, hh);
}

bool CyberiadaSMEditorTransitionItem::endsOnTips() const
{
    return source() && (source()->type() == CyberiadaSMEditorAbstractItem::ChoiceItem ||
                        source()->type() == CyberiadaSMEditorAbstractItem::VertexItem);
}

// the route leaves e1 straight out of side s1, runs parallel to the state and
// returns straight into e2 on side s2; equal sides make a bump, distinct ones a
// corner, opposite ones wrap around an adjacent side. With snap on the out-levels
// lie on grid lines, so the segments stay orthogonal (EDIT-EDGE-16)
CyberiadaSMEditorTransitionItem::LoopRoute
CyberiadaSMEditorTransitionItem::loopRoute(LoopSide s1, const QPointF& e1, LoopSide s2, const QPointF& e2,
                                           int nest, bool snap) const
{
    LoopRoute route;
    route.sp = e1;
    route.tp = e2;
    QRectF r = source()->sceneBoundingRect();
    QPointF c = r.center();
    qreal hw = r.width() / 2.0;
    qreal hh = r.height() / 2.0;
    qreal depth = LOOP_DEPTH + nest * LOOP_NEST_STEP;

    // the level depth past a border coordinate (sign: the outward side), moved to a
    // grid line still past the border when snapping
    auto level = [&](qreal edge, qreal sign, qreal origin) -> qreal {
        qreal v = edge + sign * depth;
        if (!snap) return v;
        qreal snapped = snapCoord(v + origin) - origin;
        if ((snapped - edge) * sign <= 0) snapped += sign * SettingsManager::instance().getGridSpacing();
        return snapped;
    };
    // the vertex the stub reaches straight out of side s from its end e
    auto outward = [&](LoopSide s, const QPointF& e) -> QPointF {
        switch (s) {
        case LoopTop:    return QPointF(e.x(), level(-hh, -1, c.y()));
        case LoopBottom: return QPointF(e.x(), level( hh,  1, c.y()));
        case LoopLeft:   return QPointF(level(-hw, -1, c.x()), e.y());
        default:         return QPointF(level( hw,  1, c.x()), e.y());
        }
    };

    QPointF v1 = outward(s1, e1), vn = outward(s2, e2);
    route.bends.append(v1);
    if (s1 != s2) {
        if (isTopOrBottom(s1) == isTopOrBottom(s2)) {
            // opposite sides: wrap around an adjacent side with two corners
            qreal wrap = isTopOrBottom(s1) ? level(hw, 1, c.x()) : level(hh, 1, c.y());
            route.bends.append(isTopOrBottom(s1) ? QPointF(wrap, v1.y()) : QPointF(v1.x(), wrap));
            route.bends.append(isTopOrBottom(s1) ? QPointF(wrap, vn.y()) : QPointF(vn.x(), wrap));
        } else {
            // adjacent sides: one corner sharing each side's out-level
            route.bends.append(isTopOrBottom(s1) ? QPointF(vn.x(), v1.y()) : QPointF(v1.x(), vn.y()));
        }
    }
    route.bends.append(vn);
    return route;
}

// EDIT-EDGE-22: the k-th loop of the state with no stored geometry takes the next
// side no other loop of the state has an end on, in the order right, bottom, left,
// top, and nests farther out once those sides are taken
void CyberiadaSMEditorTransitionItem::defaultSlot(LoopSide* side, int* nest) const
{
    static const LoopSide order[] = { LoopRight, LoopBottom, LoopLeft, LoopTop };
    const int count = sizeof(order) / sizeof(order[0]);
    *side = order[0];
    *nest = 0;
    if (!model || !model->rootDocument() || !source()) return;
    Cyberiada::StateMachine* sm = model->rootDocument()->get_parent_sm(source()->getElement());
    if (!sm) return;
    QRectF r = source()->sceneBoundingRect();
    qreal hw = r.width() / 2.0;
    qreal hh = r.height() / 2.0;
    bool taken[] = { false, false, false, false };   // by LoopSide
    const Cyberiada::ID& id = transition->source_element_id();
    int k = 0, mine = 0;
    std::vector<Cyberiada::Transition*> trans = sm->get_transitions();
    for (size_t i = 0; i < trans.size(); i++) {
        const Cyberiada::Transition* t = trans[i];
        if (t->source_element_id() != id || t->target_element_id() != id) continue;
        bool sp = t->has_geometry_source_point();
        bool tp = t->has_geometry_target_point();
        bool pl = t->has_polyline() && !t->get_geometry_polyline().empty();
        if (!sp && !tp && !pl) {
            if (t == transition) mine = k;
            k++;
            continue;
        }
        if (sp || pl) {
            Cyberiada::Point a = sp ? t->get_source_point() : t->get_geometry_polyline().front();
            taken[nearestSide(QPointF(a.x, a.y), hw, hh)] = true;
        }
        if (tp || pl) {
            Cyberiada::Point b = tp ? t->get_target_point() : t->get_geometry_polyline().back();
            taken[nearestSide(QPointF(b.x, b.y), hw, hh)] = true;
        }
    }
    LoopSide free[count];
    int nfree = 0;
    for (int i = 0; i < count; i++) {
        if (!taken[order[i]]) free[nfree++] = order[i];
    }
    if (nfree == 0) {
        for (int i = 0; i < count; i++) free[i] = order[i];
        nfree = count;
    }
    *side = free[mine % nfree];
    *nest = mine / nfree;
}

bool CyberiadaSMEditorTransitionItem::drawStartEnd(LoopSide* side, QPointF* end) const
{
    if (!loopStartSet || !source()) return false;
    QRectF r = source()->sceneBoundingRect();
    qreal hw = r.width() / 2.0;
    qreal hh = r.height() / 2.0;
    QPointF p = loopStart - r.center();
    bool tie = false;
    LoopSide s = nearestSide(p, hw, hh, &tie);
    if (tie && loopStartCaptured && !loopStartDir.isNull()) s = sideFromDir(loopStartDir);
    *side = s;
    if (endsOnTips()) {
        *end = sidePoint(s, 0, hw, hh);
        return true;
    }
    qreal room = sideRoom(s, hw, hh);
    *end = sidePoint(s, qBound(-room, isTopOrBottom(s) ? p.x() : p.y(), room), hw, hh);
    return true;
}

// the default route: stored ends keep their sides, a missing end pairs with the
// other into a bump (on a tip node: the next tip clockwise), and a loop with no
// geometry at all takes a centred clockwise bump on its default slot side
CyberiadaSMEditorTransitionItem::LoopRoute CyberiadaSMEditorTransitionItem::bareLoopRoute() const
{
    QRectF r = source()->sceneBoundingRect();
    qreal hw = r.width() / 2.0;
    qreal hh = r.height() / 2.0;
    bool tips = endsOnTips();
    LoopSide s1 = LoopRight, s2 = LoopRight;
    QPointF e1, e2;
    bool hasS = false;
    bool hasT = transition->has_geometry_target_point();
    if (transition->has_geometry_source_point()) {
        e1 = QPointF(transition->get_source_point().x, transition->get_source_point().y);
        s1 = nearestSide(e1, hw, hh);
        hasS = true;
    } else {
        hasS = drawStartEnd(&s1, &e1);
    }
    if (hasT) {
        e2 = QPointF(transition->get_target_point().x, transition->get_target_point().y);
        s2 = nearestSide(e2, hw, hh);
    }
    int nest = 0;
    if (!hasS && !hasT) {
        defaultSlot(&s1, &nest);
        if (tips) {
            s2 = turnSide(s1, 1);
            e1 = sidePoint(s1, 0, hw, hh);
            e2 = sidePoint(s2, 0, hw, hh);
        } else {
            s2 = s1;
            qreal span = qMin(qreal(LOOP_LATERAL + nest * LOOP_NEST_STEP), sideRoom(s1, hw, hh));
            // clockwise: rightward along the top, downward along the right
            qreal sign = (s1 == LoopTop || s1 == LoopRight) ? 1.0 : -1.0;
            e1 = sidePoint(s1, -sign * span, hw, hh);
            e2 = sidePoint(s1,  sign * span, hw, hh);
        }
    } else if (!hasT) {
        s2 = tips ? turnSide(s1, 1) : s1;
        e2 = tips ? sidePoint(s2, 0, hw, hh) : bumpPartner(s1, e1, hw, hh);
    } else if (!hasS) {
        s1 = tips ? turnSide(s2, -1) : s2;
        e1 = tips ? sidePoint(s1, 0, hw, hh) : bumpPartner(s2, e2, hw, hh);
    } else if (tips && s1 == s2) {
        s2 = turnSide(s1, 1);
        e2 = sidePoint(s2, 0, hw, hh);
    }
    return loopRoute(s1, e1, s2, e2, nest, false);
}

void CyberiadaSMEditorTransitionItem::setLoopStart(const QPointF& scenePress, const QPointF& tieDir)
{
    prepareGeometryChange();
    loopStart = scenePress;
    loopStartSet = true;
    loopStartDir = tieDir;
    loopStartCaptured = !tieDir.isNull();
    updateDots();
    setDotsPosition();
}

void CyberiadaSMEditorTransitionItem::seedDefaultLoop(const QPointF& endDir, Qt::KeyboardModifiers mods)
{
    if (!source() || source() != target()) return;
    if (!isBareLoop()) return;   // only a fresh loop

    QRectF r = source()->sceneBoundingRect();
    QPointF c = r.center();
    qreal hw = r.width() / 2.0;
    qreal hh = r.height() / 2.0;
    // with snap on the endpoints slide along the border (EDIT-EDGE-15/16)
    bool snap = snapActive(mods);
    bool tips = endsOnTips();
    auto onBorder = [&](const QPointF& e) -> QPointF {
        return snap ? snapAlongBorder(source(), e + c, mods, LockNone) - c : e;
    };

    LoopSide s1;
    LoopSide s2 = endDir.isNull() ? LoopRight : sideFromDir(endDir);
    QPointF e1, e2;
    if (drawStartEnd(&s1, &e1)) {
        // EDIT-EDGE-22: the loop starts where the draw did
        if (s1 != s2) {
            e2 = sidePoint(s2, 0, hw, hh);
        } else if (tips) {
            s2 = turnSide(s1, 1);
            e2 = sidePoint(s2, 0, hw, hh);
        } else {
            e2 = bumpPartner(s1, e1, hw, hh);
        }
    } else {
        // an endpoint drag released on its own state: the sides follow the drag
        s1 = loopStartDir.isNull() ? LoopTop : sideFromDir(loopStartDir);
        if (s1 == s2 && tips) s2 = turnSide(s1, 1);
        if (s1 == s2) {
            e1 = sidePoint(s1, -LOOP_LATERAL, hw, hh);
            e2 = sidePoint(s2,  LOOP_LATERAL, hw, hh);
        } else {
            e1 = sidePoint(s1, 0, hw, hh);
            e2 = sidePoint(s2, 0, hw, hh);
        }
    }
    LoopRoute route = loopRoute(s1, onBorder(e1), s2, onBorder(e2), 0, snap);
    loopStartSet = false;
    loopStartCaptured = false;

    Cyberiada::Polyline pl;
    for (int i = 0; i < route.bends.size(); i++) {
        pl.push_back(Cyberiada::Point(route.bends[i].x(), route.bends[i].y()));
    }
    QModelIndex index = model->elementToIndex(element);
    model->updateGeometry(index, Cyberiada::Point(route.sp.x(), route.sp.y()),
                                 Cyberiada::Point(route.tp.x(), route.tp.y()));
    model->updateGeometry(index, pl);   // sets has_polyline last
}

// the first edit of a bare loop starts from the route it shows (EDIT-EDGE-22)
void CyberiadaSMEditorTransitionItem::materialiseLoop()
{
    if (!isBareLoop()) return;
    LoopRoute route = bareLoopRoute();
    loopStartSet = false;
    Cyberiada::Polyline pl;
    for (int i = 0; i < route.bends.size(); i++) {
        pl.push_back(Cyberiada::Point(route.bends[i].x(), route.bends[i].y()));
    }
    QModelIndex index = model->elementToIndex(element);
    model->updateGeometry(index, Cyberiada::Point(route.sp.x(), route.sp.y()),
                                 Cyberiada::Point(route.tp.x(), route.tp.y()));
    model->updateGeometry(index, pl);
}

QVector<QPointF> CyberiadaSMEditorTransitionItem::routePoints() const
{
    QVector<QPointF> points;
    if (isBareLoop() && !(isSourceTraking || isTargetTraking)) {
        LoopRoute route = bareLoopRoute();
        QPointF c = sourceCenter();
        points.append(route.sp + c);
        for (int i = 0; i < route.bends.size(); i++) points.append(route.bends[i] + c);
        points.append(route.tp + c);
        return points;
    }
    points.append(isSourceTraking ? prevPosition : sourcePoint() + sourceCenter());
    if (transition->has_polyline()) {
        for (const auto& point : transition->get_geometry_polyline()) {
            points.append(QPointF(point.x, point.y) + sourceCenter());
        }
    }
    points.append(isTargetTraking ? prevPosition : targetPoint() + targetCenter());
    return points;
}

QVector<QPointF> CyberiadaSMEditorTransitionItem::shownBends() const
{
    QVector<QPointF> bends;
    QPointF c = sourceCenter();
    if (isBareLoop()) {
        LoopRoute route = bareLoopRoute();
        for (int i = 0; i < route.bends.size(); i++) bends.append(route.bends[i] + c);
    } else if (transition->has_polyline()) {
        for (const auto& point : transition->get_geometry_polyline()) {
            bends.append(QPointF(point.x, point.y) + c);
        }
    }
    return bends;
}

void CyberiadaSMEditorTransitionItem::refreshRoute()
{
    prepareGeometryChange();
    updateDots();
    setDotsPosition();
    updateActionPosition();
    update();
}

QRectF CyberiadaSMEditorTransitionItem::labelSceneRect() const
{
    if (!actionItem || !actionItem->isVisible() || actionText().isEmpty()) return QRectF();
    return actionItem->sceneBoundingRect();
}

QPainterPath CyberiadaSMEditorTransitionItem::path() const
{
    MY_ASSERT(model);
    QPainterPath path = QPainterPath();
    // a polyline point by point; a bare self-loop by its default route (EDIT-EDGE-22)
    QVector<QPointF> points = routePoints();
    path.moveTo(points.first());
    for (int i = 1; i < points.size(); i++) {
        path.lineTo(points[i]);
    }
    return path;
}

DotSignal* CyberiadaSMEditorTransitionItem::targetDot()
{
    return listDots.isEmpty() ? nullptr : listDots.last();
}

DotSignal *CyberiadaSMEditorTransitionItem::getDot(int index)
{
    if (index < 0) return nullptr;
    if (listDots.size() <= index) return nullptr;
    return listDots.at(index);
}

void CyberiadaSMEditorTransitionItem::drawArrow(QPainter* painter)
{
    SettingsManager& sm = SettingsManager::instance();

    QPen pen(Qt::black, 1);
    if (Cyberiada::element_has_color(element)) {
        pen.setColor(QColor(QString::fromStdString(Cyberiada::element_get_color(element))));
    }
    if (isSelected()) {
        pen.setColor(sm.getSelectionColor());
        pen.setWidth(sm.getSelectionBorderWidth());
    }
    painter->setPen(pen);

    // the tangent of the last segment, into the target
    QVector<QPointF> points = routePoints();
    QPointF p1 = points.at(points.size() - 2);
    QPointF p2 = points.last();
    QLineF line(p1, p2);
    double angle = std::atan2(-line.dy(), line.dx());

    qreal arrowSize = 10.0;
    QPointF arrowP1 = p2 + QPointF(-arrowSize * std::cos(angle - M_PI / 6),
                                   arrowSize * std::sin(angle - M_PI / 6));
    QPointF arrowP2 = p2 + QPointF(-arrowSize * std::cos(angle + M_PI / 6),
                                   arrowSize * std::sin(angle + M_PI / 6));

    QPolygonF arrowHead;
    arrowHead << p2 << arrowP1 << arrowP2;

    painter->setBrush(QBrush(pen.color()));
    painter->drawPolygon(arrowHead);
}

CyberiadaSMEditorAbstractItem *CyberiadaSMEditorTransitionItem::itemUnderCursor(const QPointF& cursor)
{
    QList<QGraphicsItem*> items = scene()->items(cursor);

    CyberiadaSMEditorAbstractItem* cItem = nullptr;
    for (QGraphicsItem* item : items) {
        cItem = dynamic_cast<CyberiadaSMEditorAbstractItem*>(item);
        if (!cItem) { continue; }

        if (cItem->type() == CyberiadaSMEditorAbstractItem::StateItem ||
            cItem->type() == CyberiadaSMEditorAbstractItem::CompositeStateItem ||
            cItem->type() == CyberiadaSMEditorAbstractItem::VertexItem ||
            cItem->type() == CyberiadaSMEditorAbstractItem::ChoiceItem) {
            return cItem;
        }
    }
    return nullptr;
}

QPointF CyberiadaSMEditorTransitionItem::findIntersectionWithItem(const CyberiadaSMEditorAbstractItem *item,
                                                                  const QPointF& start, const QPointF& end,
                                                                  bool* hasIntersections,
                                                                  bool forwardOnly) const
{
    return EdgeGeometry::findIntersectionWithItem(item, start, end, hasIntersections, forwardOnly);
}

void CyberiadaSMEditorTransitionItem::updateCoordinates(CornerFlags side, QPointF& point,
                                                        qreal d, const CyberiadaSMEditorAbstractItem* item)
{
    EdgeGeometry::rebaseOnResize(side, point, d, item);
}

void CyberiadaSMEditorTransitionItem::clampOntoBorder(QPointF& p, qreal hw, qreal hh)
{
    EdgeGeometry::clampOntoBorder(p, hw, hh);
}

void CyberiadaSMEditorTransitionItem::movePolyline(QPointF delta)
{
    if(!transition->has_polyline()) { return; }

    Cyberiada::Polyline pol = transition->get_geometry_polyline();
    for (size_t i = 0; i < pol.size(); ++i) {
        pol[i] = Cyberiada::Point(pol[i].x + delta.x(), pol[i].y + delta.y());
    }

    model->updateGeometry(model->elementToIndex(element), pol);
}

QString CyberiadaSMEditorTransitionItem::actionText() const
{
    // composed as the fragment places are (EDIT-EDGE-21)
    if (!transition->has_action()) return QString();
    return SubjectFragment::transitionLabel(transition->get_action()).text;
}

EditableTextItem* CyberiadaSMEditorTransitionItem::fieldText(const SubjectFragment::Field& field, int* start) const
{
    if (!actionItem || !transition->has_action()) return nullptr;
    *start = SubjectFragment::transitionLabel(transition->get_action()).start(field.kind);
    return *start >= 0 ? actionItem : nullptr;
}

QList<QGraphicsItem*> CyberiadaSMEditorTransitionItem::ownedChildren() const
{
    QList<QGraphicsItem*> owned = CyberiadaSMEditorAbstractItem::ownedChildren();
    if (actionItem) owned.append(actionItem);
    for (DotSignal* d : listDots) if (d) owned.append(d);
    for (DotSignal* d : labelDots) if (d) owned.append(d);
    return owned;
}

void CyberiadaSMEditorTransitionItem::updateAction()
{
    actionItem->setPlainText(actionText());
    actionItem->applyRichFormat();
    updateActionPosition();
}

void CyberiadaSMEditorTransitionItem::migrateLabelToRect()
{
    // only an explicitly placed point label migrates; auto-placed labels stay auto,
    // rects stay rects, and inspection keeps the original geometry
    if (!actionItem || SettingsManager::instance().getInspectorMode()) return;
    if (!transition->has_geometry_label_point()) return;

    Cyberiada::Point lp = transition->get_label_point();   // the label centre
    QRectF bb = actionItem->boundingRect();
    // a direct element write (not model->updateLabel): no undo step, no dirty flag
    static_cast<Cyberiada::Transition*>(element)->update_label(
        Cyberiada::Rect(lp.x, lp.y, bb.width(), bb.height()));
    updateActionPosition();
}

void CyberiadaSMEditorTransitionItem::updateActionPosition() {
    if (!actionItem) return;

    // while the user drags the label, leave it where the drag puts it: a reflow
    // triggered mid-drag must not snap it back to the auto midpoint. While the user
    // types, leave the top-left where it is so the box grows downward.
    if (actionItem->isDragging() || actionItem->isEditing() || labelHandleDragging) return;

    // a moved label keeps its stored box, its centre relative to the source centre;
    // the fixed width makes the text wrap and the height grow downward
    if (transition->has_geometry_label_rect()) {
        const Cyberiada::Rect& lr = transition->get_label_rect();
        actionItem->setTextWidth(lr.width);
        QPointF centre = QPointF(lr.x, lr.y) + sourceCenter();
        actionItem->setPos(centre - actionItem->boundingRect().center());
        update();
        return;
    }

    // a legacy/inspection point label keeps its stored centre, natural width
    if (transition->has_geometry_label_point()) {
        const Cyberiada::Point& lp = transition->get_label_point();
        actionItem->setTextWidth(-1);
        actionItem->setPos(QPointF(lp.x, lp.y) + sourceCenter() - actionItem->boundingRect().center());
        update();
        return;
    }

    actionItem->setTextWidth(-1);
    // EDIT-EDGE-22: a loop label lies outside, past the longest segment between bends
    QVector<QPointF> route = (source() && source() == target()) ? routePoints() : QVector<QPointF>();
    if (route.size() >= 4) {
        int outer = 1;
        for (int i = 2; i + 2 < route.size(); i++) {
            if (QLineF(route[i], route[i + 1]).length() > QLineF(route[outer], route[outer + 1]).length()) {
                outer = i;
            }
        }
        QPointF a = route[outer], b = route[outer + 1];
        QPointF mid = (a + b) / 2;
        QPointF c = sourceCenter();
        QRectF bb = actionItem->boundingRect();
        QPointF away;
        if (qAbs(a.y() - b.y()) <= qAbs(a.x() - b.x())) {
            away = QPointF(0, (mid.y() < c.y() ? -1 : 1) * (bb.height() / 2.0 + LOOP_LABEL_GAP));
        } else {
            away = QPointF((mid.x() < c.x() ? -1 : 1) * (bb.width() / 2.0 + LOOP_LABEL_GAP), 0);
        }
        actionItem->setPos(mid + away - bb.center());
        update();
        return;
    }
    QPointF lastPoint = sourcePoint();
    if(transition->has_polyline() && transition->get_geometry_polyline().size() > 0) {
        Cyberiada::Polyline polyline = transition->get_geometry_polyline();
        Cyberiada::Point lastPolylinePoint = *(std::next(polyline.begin(), polyline.size() - 1));
        lastPoint = QPointF(lastPolylinePoint.x, lastPolylinePoint.y);
    }
    QPointF textPos = (lastPoint + (targetPoint() + targetCenter() - sourceCenter())) / 2 + sourceCenter() - actionItem->boundingRect().center();

    actionItem->setPos(textPos);
    update();
}

void CyberiadaSMEditorTransitionItem::setActionVisibility(bool visible) {
    actionItem->setVisible(visible && SettingsManager::instance().getShowText());
}

void CyberiadaSMEditorTransitionItem::syncFromModel()
{
    prepareGeometryChange();
    if (transition->has_action()) {
        updateAction();
    } else {
        // no action: drop any stale label text so the empty box/dots disappear
        actionItem->setPlainText(QString());
    }
    updateDots();
    setDotsPosition();
    CyberiadaSMEditorAbstractItem::syncFromModel();
    // the default routes of the other bare loops of the state follow this one
    if (source() && source() == target()) {
        if (CyberiadaSMEditorScene* cScene = dynamic_cast<CyberiadaSMEditorScene*>(scene())) {
            cScene->refreshLoops(sourceId());
        }
    }
}

void CyberiadaSMEditorTransitionItem::onSourceGeomertyChanged()
{
    prepareGeometryChange();
    updateActionPosition();
    setDotsPosition();
}

void CyberiadaSMEditorTransitionItem::onTargetGeomertyChanged()
{
    prepareGeometryChange();
    updateActionPosition();
    setDotsPosition();
}

void CyberiadaSMEditorTransitionItem::onSourceSizeChanged(CyberiadaSMEditorAbstractItem::CornerFlags side, qreal d)
{
    prepareGeometryChange();
    // an auto-placed endpoint (no stored point) is recomputed every repaint by
    // sourcePoint(), so it needs no rebasing and must not be materialised here
    if (!transition->has_geometry_source_point()) return;
    QPointF newPosition = sourcePoint();
    updateCoordinates(side, newPosition, d, source());
    setSourcePoint(newPosition);
}

void CyberiadaSMEditorTransitionItem::onTargetSizeChanged(CyberiadaSMEditorAbstractItem::CornerFlags side, qreal d)
{
    prepareGeometryChange();
    if (!transition->has_geometry_target_point()) return;
    QPointF newPosition = targetPoint();
    updateCoordinates(side, newPosition, d, target());
    setTargetPoint(newPosition);
}

void CyberiadaSMEditorTransitionItem::slotMoveDot(QGraphicsItem *signalOwner, qreal dx, qreal dy, QPointF p)
{
    if (model->readOnly()) { return; }
    prepareGeometryChange();

    // with Ctrl held the endpoint locks to the adjacent point on the dominant
    // axis, so the segment next to it stays strictly horizontal or vertical
    DotSignal* movedDot = dynamic_cast<DotSignal*>(signalOwner);
    Qt::KeyboardModifiers mods = movedDot ? movedDot->modifiers() : Qt::NoModifier;

    // the cursor before the snap: an interior point follows it by its grab offset
    QPointF cursor = p;
    // snap the dragged line point to the grid (no-op when snap mode is off)
    p = snapToGrid(p, mods);
    prevPosition = p;

    // the first move of a self-loop being placed fixes its initial drag
    // direction: from the draw press it breaks a tie of two sides, from the
    // centre (an endpoint drag) it picks the source side of the default polyline
    if (isBareLoop() && !loopStartCaptured) {
        loopStartDir = p - (loopStartSet ? loopStart : sourceCenter());
        loopStartCaptured = true;
    }

    for(int i = 0; i < listDots.size(); i++){
        if(listDots.at(i) == signalOwner){
            // first point
            if(i == 0) {
                QPointF nextPoint = targetPoint() + targetCenter();
                if(transition->has_polyline() && transition->get_geometry_polyline().size() > 0) {
                    Cyberiada::Polyline polyline = transition->get_geometry_polyline();
                    nextPoint = QPointF(polyline.front().x, polyline.front().y) + sourceCenter();
                }
                LockedAxis locked = LockNone;
                if (mods & Qt::ControlModifier) {
                    if (qAbs(p.x() - nextPoint.x()) <= qAbs(p.y() - nextPoint.y())) {
                        p.setX(nextPoint.x());
                        locked = LockX;
                    } else {
                        p.setY(nextPoint.y());
                        locked = LockY;
                    }
                    prevPosition = p;
                }

                CyberiadaSMEditorAbstractItem* cItem = itemUnderCursor(cursor);
                if (cItem) {
                    QPointF newPoint;
                    bool hasIntersections = false;

                    // loop
                    if (cItem == target()) {
                        setSource(cItem);
                        hasIntersections = false;
                        QPointF newPoint = snappedIntersection(source(), p, sourceCenter(), &hasIntersections, mods, locked) -
                                           sourceCenter();
                        isSourceTraking = false;
                        setSourcePoint(newPoint);
                        return;
                    }

                    //
                    if (cItem == source()) {
                        hasIntersections = false;
                        QPointF newPoint = snappedIntersection(source(), p, nextPoint, &hasIntersections, mods, locked) -
                                           sourceCenter();
                        if (hasIntersections) {
                            isSourceTraking = false;
                            setSourcePoint(newPoint);
                            return;
                        }
                    }

                    newPoint = snappedIntersection(cItem, p, nextPoint, &hasIntersections, mods, locked) -
                               cItem->sceneBoundingRect().center();
                    // intersectoin with item under cursor
                    if (hasIntersections) {
                        isSourceTraking = false;
                        setSource(cItem);
                        setSourcePoint(newPoint);
                        return;
                    }

                    return;
                }
                bool hasIntersections = false;

                if (isBareLoop()) {
                    // loop
                    nextPoint = sourceCenter();
                }

                QPointF newPoint = snappedIntersection(source(), p, nextPoint, &hasIntersections, mods, locked) -
                           sourceCenter();
                // intersection with source in adjusted bounding rect
                if (hasIntersections) {
                    isSourceTraking = false;
                    setSourcePoint(newPoint);
                    return;
                }


                // mouse traking
                isSourceTraking = true;
                setDotsPosition();
                break;
            }
            // last point
            if(i == listDots.size() - 1) {
                QPointF nextPoint = sourcePoint() + sourceCenter();
                if(transition->has_polyline() && transition->get_geometry_polyline().size() > 0) {
                    Cyberiada::Polyline polyline = transition->get_geometry_polyline();
                    nextPoint = QPointF(polyline.back().x, polyline.back().y) + sourceCenter();
                }
                LockedAxis locked = LockNone;
                if (mods & Qt::ControlModifier) {
                    if (qAbs(p.x() - nextPoint.x()) <= qAbs(p.y() - nextPoint.y())) {
                        p.setX(nextPoint.x());
                        locked = LockX;
                    } else {
                        p.setY(nextPoint.y());
                        locked = LockY;
                    }
                    prevPosition = p;
                }

                CyberiadaSMEditorAbstractItem* cItem = itemUnderCursor(cursor);
                if (cItem) {
                    QPointF newPoint;
                    bool hasIntersections = false;
                    if (cItem == source()) {
                        // loop
                        setTarget(cItem);
                        hasIntersections = false;
                        QPointF newPoint = snappedIntersection(target(), p, targetCenter(), &hasIntersections, mods, locked) -
                                           targetCenter();
                        isTargetTraking = false;
                        setTargetPoint(newPoint);
                        return;
                    }

                    if (cItem == target()) {
                        hasIntersections = false;
                        QPointF newPoint = snappedIntersection(target(), p, nextPoint, &hasIntersections, mods, locked) -
                                           targetCenter();
                        if (hasIntersections) {
                            isTargetTraking = false;
                            setTargetPoint(newPoint);
                            return;
                        }
                    }

                    newPoint = snappedIntersection(cItem, p, nextPoint, &hasIntersections, mods, locked) -
                               cItem->sceneBoundingRect().center();
                    if (hasIntersections) {
                        isTargetTraking = false;
                        setTarget(cItem);
                        setTargetPoint(newPoint);
                        return;
                    }
                    return;
                }

                bool hasIntersections = false;

                if (isBareLoop()) {
                    // loop
                    nextPoint = sourceCenter();
                }

                QPointF newPoint = snappedIntersection(target(), p, nextPoint, &hasIntersections, mods, locked) -
                                   targetCenter();

                if (hasIntersections) {
                    isTargetTraking = false;
                    setTargetPoint(newPoint);
                    return;
                }

                isTargetTraking = true;
                setDotsPosition();
                break;
            }

            // polyline points; a bare loop stores its shown route first
            materialiseLoop();
            Cyberiada::Polyline pol = transition->get_geometry_polyline();
            if (i - 1 >= int(pol.size())) break;
            Cyberiada::Point pt = pol.at(i - 1);
            // the first move fixes the grab offset (dx, dy is the step from the
            // press); the point then follows the cursor and snaps in scene space,
            // so the small steps add up (EDIT-EDGE-16). The polyline is stored
            // relative to the source centre; Ctrl can still lock an axis
            if (vertexDragDot != signalOwner) {
                vertexDragDot = signalOwner;
                vertexGrab = QPointF(pt.x, pt.y) + sourceCenter() - (cursor - QPointF(dx, dy));
            }
            QPointF moved = snapToGrid(cursor + vertexGrab, mods) - sourceCenter();
            pt.x = moved.x();
            pt.y = moved.y();
            if (mods & Qt::ControlModifier) {
                // like the endpoints: snap the point onto the axis of its
                // previous neighbour so the incoming segment stays orthogonal
                // (the dots and the polyline share the source-centre origin)
                QPointF neigh = listDots.at(i - 1)->scenePos() - sourceCenter();
                if (qAbs(pt.x - neigh.x()) <= qAbs(pt.y - neigh.y())) pt.x = neigh.x();
                else                                                  pt.y = neigh.y();
            }
            pol.at(i - 1) = pt;
            model->updateGeometry(model->elementToIndex(element), pol);
            break;
        }
    }
}

QPointF CyberiadaSMEditorTransitionItem::snapAlongBorder(const CyberiadaSMEditorAbstractItem* item,
                                                         const QPointF& p, Qt::KeyboardModifiers mods,
                                                         LockedAxis locked) const
{
    if (!item || !snapActive(mods)) return p;
    // a pseudostate and a choice keep their own attachment (EDIT-EDGE-2/3/4)
    if (item->type() == CyberiadaSMEditorAbstractItem::VertexItem ||
        item->type() == CyberiadaSMEditorAbstractItem::ChoiceItem) {
        return p;
    }
    // a straight edge is exactly axis-aligned, a rounded corner is not; the
    // computed intersection lies on its edge up to the rounding error
    const qreal AXIS_EPSILON = 1e-6;
    const qreal ON_EDGE = 0.01;
    qreal g = SettingsManager::instance().getGridSpacing();
    QPolygonF polygon = item->sceneTransform().map(item->shape()).toFillPolygon();
    for (int i = 0; i < polygon.size(); ++i) {
        QPointF a = polygon[i];
        QPointF b = polygon[(i + 1) % polygon.size()];
        bool horizontal = qAbs(a.y() - b.y()) < AXIS_EPSILON && qAbs(a.x() - b.x()) > ON_EDGE;
        bool vertical   = qAbs(a.x() - b.x()) < AXIS_EPSILON && qAbs(a.y() - b.y()) > ON_EDGE;
        if (!horizontal && !vertical) continue;
        qreal lo     = horizontal ? qMin(a.x(), b.x()) : qMin(a.y(), b.y());
        qreal hi     = horizontal ? qMax(a.x(), b.x()) : qMax(a.y(), b.y());
        qreal along  = horizontal ? p.x() : p.y();
        qreal across = horizontal ? p.y() - a.y() : p.x() - a.x();
        if (qAbs(across) > ON_EDGE || along < lo - ON_EDGE || along > hi + ON_EDGE) continue;
        // the Ctrl-locked coordinate keeps the neighbour's (EDIT-TOOL-10)
        if ((horizontal && locked == LockX) || (vertical && locked == LockY)) return p;
        qreal first = std::ceil(lo / g) * g;
        qreal last = std::floor(hi / g) * g;
        if (first > last) return p;   // no grid line on this edge
        qreal snapped = qBound(first, snapCoord(along), last);
        return horizontal ? QPointF(snapped, a.y()) : QPointF(a.x(), snapped);
    }
    return p;
}

QPointF CyberiadaSMEditorTransitionItem::snappedIntersection(const CyberiadaSMEditorAbstractItem* item,
                                                             const QPointF& start, const QPointF& end,
                                                             bool* hasIntersections,
                                                             Qt::KeyboardModifiers mods,
                                                             LockedAxis locked) const
{
    QPointF p = findIntersectionWithItem(item, start, end, hasIntersections);
    return *hasIntersections ? snapAlongBorder(item, p, mods, locked) : p;
}

// true when anc is item's element or one of its ancestors in the model tree
bool CyberiadaSMEditorTransitionItem::isAncestorOf(CyberiadaSMEditorAbstractItem *anc,
                                                   CyberiadaSMEditorAbstractItem *item) const
{
    if (!anc || !item) return false;
    Cyberiada::Element* target = anc->getElement();
    for (Cyberiada::Element* e = item->getElement(); e; e = e->get_parent()) {
        if (e == target) return true;
    }
    return false;
}

void CyberiadaSMEditorTransitionItem::slotMouseReleaseDot(QGraphicsItem *signalOwner, QPointF p)
{
    isSourceTraking = false;
    isTargetTraking = false;
    vertexDragDot = nullptr;

    if (model->readOnly()) { return; }

    // a single-drag transition draw (or a quick drag-to-release) seeds a
    // self-loop and delivers no move to the grabbed endpoint dot, so the loop
    // survives. Bind the loose endpoint here if it was released over a
    // different item; released over the same one it stays a self-loop.
    if (source() != target()) { return; }
    if (listDots.isEmpty()) { return; }

    DotSignal* dot = dynamic_cast<DotSignal*>(signalOwner);
    int idx = listDots.indexOf(dot);
    if (idx != 0 && idx != listDots.size() - 1) { return; }

    Qt::KeyboardModifiers mods = dot ? dot->modifiers() : Qt::NoModifier;
    CyberiadaSMEditorAbstractItem* cItem = itemUnderCursor(p);
    p = snapToGrid(p, mods);
    prevPosition = p;
    prepareGeometryChange();
    if (!cItem) {
        // a loop released over the empty canvas stays as drawn (EDIT-EDGE-22)
        seedDefaultLoop(p - sourceCenter(), mods);
        setDotsPosition();
        return;
    }

    bool hasIntersections = false;
    if (idx == listDots.size() - 1) {
        // a small drag that ends over an ancestor of the source (the parent SM
        // it sits in) is a self-loop being placed, not a retarget to the parent
        if (cItem == target() || isAncestorOf(cItem, source())) {
            seedDefaultLoop(p - sourceCenter(), mods);
            return;
        }
        QPointF newPoint = snappedIntersection(cItem, p, sourceCenter(), &hasIntersections, mods, LockNone) -
                           cItem->sceneBoundingRect().center();
        if (!hasIntersections) { return; }
        isTargetTraking = false;
        setTarget(cItem);
        setTargetPoint(newPoint);
    } else {
        if (cItem == source() || isAncestorOf(cItem, target())) {
            seedDefaultLoop(p - sourceCenter(), mods);
            return;
        }
        QPointF newPoint = snappedIntersection(cItem, p, targetCenter(), &hasIntersections, mods, LockNone) -
                           cItem->sceneBoundingRect().center();
        if (!hasIntersections) { return; }
        isSourceTraking = false;
        setSource(cItem);
        setSourcePoint(newPoint);
    }
    setDotsPosition();
}

void CyberiadaSMEditorTransitionItem::slotDeleteDot(QGraphicsItem *signalOwner)
{
    if (model->readOnly()) { return; }
    // a bare loop stores no bend to remove
    if (isBareLoop()) { return; }

    for(int i = 0; i < listDots.size(); i++){
        if(listDots.at(i) == signalOwner){
            removePolylineVertex(i);
            break;
        }
    }
}

// interior dots map to polyline[dotIndex-1]; the endpoints have no vertex
void CyberiadaSMEditorTransitionItem::removePolylineVertex(int dotIndex)
{
    if (!transition->has_polyline()) { return; }
    if (dotIndex <= 0 || dotIndex >= listDots.size() - 1) { return; }

    Cyberiada::Polyline pol = transition->get_geometry_polyline();
    if (dotIndex - 1 >= int(pol.size())) { return; }
    pol.erase(pol.begin() + dotIndex - 1);
    model->updateGeometry(model->elementToIndex(element), pol);
}

// the two 10px diagonal probe crosses of a local point against each segment
int CyberiadaSMEditorTransitionItem::segmentAt(const QPointF& localPos) const
{
    QLineF checkLineFirst(localPos.x() - 5, localPos.y() - 5, localPos.x() + 5, localPos.y() + 5);
    QLineF checkLineSecond(localPos.x() + 5, localPos.y() - 5, localPos.x() - 5, localPos.y() + 5);
    QPainterPath oldPath = path();
    for(int i = 0; i < oldPath.elementCount() - 1; i++){
        QLineF checkableLine(oldPath.elementAt(i), oldPath.elementAt(i+1));
        if(lineIntersect(checkableLine, checkLineFirst, 0) == 1 ||
           lineIntersect(checkableLine, checkLineSecond, 0) == 1){
            return i;
        }
    }
    return -1;
}

void CyberiadaSMEditorTransitionItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (model->readOnly()) { return; }

    // a double click edits the transition label; a vertex is added by dragging
    // a segment instead. place the label where the line was clicked (item-local
    // space == scene space here), so the edit opens under the cursor
    actionItem->setVisible(true);
    actionItem->setPos(event->pos() - actionItem->boundingRect().center());
    actionItem->startEditing();
    QGraphicsItem::mouseDoubleClickEvent(event);
}

void CyberiadaSMEditorTransitionItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (model->readOnly()) { return; }

    // offer to remove a polyline vertex when the click is on an interior dot
    int vertexDot = -1;
    for (int i = 1; i < listDots.size() - 1; i++) {
        if (QLineF(listDots.at(i)->scenePos(), event->scenePos()).length() <= 6.0) {
            vertexDot = i;
            break;
        }
    }
    if (vertexDot < 0) { return; }

    QMenu menu;
    QAction* remove = menu.addAction(tr("Delete point"));
    if (menu.exec(event->screenPos()) == remove) {
        removePolylineVertex(vertexDot);
    }
    event->accept();
}

void CyberiadaSMEditorTransitionItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }
    if (model->readOnly()) {
        QGraphicsItem::mousePressEvent(event);
        return;
    }

    showDots();
    if (event->button() & Qt::LeftButton) {
        isLeftMouseButtonPressed = true;
        // a press on a segment arms a vertex insertion, sprung by the drag
        pressPos = event->pos();
        pendingSegment = segmentAt(pressPos);
        vertexDragArmed = (pendingSegment >= 0);
    }
    QGraphicsItem::mousePressEvent(event);
}

void CyberiadaSMEditorTransitionItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select ||
        model->readOnly()) {
        event->ignore();
        return;
    }

    // once the drag leaves the press spot, insert a vertex on the pressed
    // segment; the same gesture then carries it
    if (vertexDragArmed &&
        QLineF(pressPos, event->pos()).length() > 3.0) {
        vertexDragArmed = false;
        // a bare loop stores its shown route first (EDIT-EDGE-22)
        materialiseLoop();
        QPointF p = event->pos() - sourceCenter();
        Cyberiada::Point cybP(p.x(), p.y());
        Cyberiada::Polyline pol;
        if (transition->has_polyline()) {
            pol = transition->get_geometry_polyline();
            pol.insert(pol.begin() + pendingSegment, cybP);
        } else {
            pol.push_back(cybP);
        }
        model->updateGeometry(model->elementToIndex(element), pol);
        draggingVertex = pendingSegment;
    }
    // drag the inserted vertex with the rest of the gesture
    if (draggingVertex >= 0 && transition->has_polyline()) {
        Cyberiada::Polyline pol = transition->get_geometry_polyline();
        if (draggingVertex < int(pol.size())) {
            // snapped like a dragged vertex (EDIT-EDGE-16), from the move that
            // inserts it on
            QPointF p = snapToGrid(event->scenePos(), event->modifiers()) - sourceCenter();
            pol.at(draggingVertex) = Cyberiada::Point(p.x(), p.y());
            model->updateGeometry(model->elementToIndex(element), pol);
        }
    }
    QGraphicsItem::mouseMoveEvent(event);
}

void CyberiadaSMEditorTransitionItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
   if (event->button() & Qt::LeftButton) {
       isLeftMouseButtonPressed = false;
       vertexDragArmed = false;
       pendingSegment = -1;
       draggingVertex = -1;
   }
   QGraphicsItem::mouseReleaseEvent(event);
}

void CyberiadaSMEditorTransitionItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select ||
        model->readOnly()) {
        event->ignore();
        return;
    }

    if (isSelected()) {
        showDots();
        setDotsPosition();
    }
    QGraphicsItem::hoverEnterEvent(event);
}

void CyberiadaSMEditorTransitionItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }

    QGraphicsItem::hoverMoveEvent(event);
}

void CyberiadaSMEditorTransitionItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }

    hideDots();
    QGraphicsItem::hoverLeaveEvent(event);
}

void CyberiadaSMEditorTransitionItem::initializeDots()
{
    // a handle on every point of the shown route, the bends of a bare loop included
    QPainterPath linePath = path();
    for(int i = 0; i < linePath.elementCount(); i++) {
        QPointF point = linePath.elementAt(i);
        DotSignal *dot = new DotSignal(point, this);
        connect(dot, &DotSignal::signalMove, this, &CyberiadaSMEditorTransitionItem::slotMoveDot);
        connect(dot, &DotSignal::signalMouseRelease, this, &CyberiadaSMEditorTransitionItem::slotMouseReleaseDot);
        connect(dot, &DotSignal::signalDelete, this, &CyberiadaSMEditorTransitionItem::slotDeleteDot);
        dot->setDotFlags(DotSignal::Movable);
        dot->setDeleteable(true);
        listDots.append(dot);
    }
}

void CyberiadaSMEditorTransitionItem::updateDots()
{
    QVector<QPointF> bends = shownBends();
    int n = 2 + bends.size();

    if(listDots.size() == n) {
        return;
    }

    if (listDots.size() > 2) {
        for (int i = 1; i < listDots.size() - 1; ++i) {
            retireDot(listDots[i]);
        }

        DotSignal *first = listDots.first();
        DotSignal *last = listDots.last();

        listDots.clear();
        listDots.append(first);
        listDots.append(last);
    }

    for(int i = 0; i < n - 2; i++) {
        DotSignal *dot = new DotSignal(bends.at(i), this);
        connect(dot, &DotSignal::signalMove, this, &CyberiadaSMEditorTransitionItem::slotMoveDot);
        connect(dot, &DotSignal::signalMouseRelease, this, &CyberiadaSMEditorTransitionItem::slotMouseReleaseDot);
        connect(dot, &DotSignal::signalDelete, this, &CyberiadaSMEditorTransitionItem::slotDeleteDot);
        dot->setDotFlags(DotSignal::Movable);
        dot->setDeleteable(true);
        listDots.insert(i + 1, dot);
    }
}

void CyberiadaSMEditorTransitionItem::showDots()
{
    foreach( DotSignal* dot, listDots ) {
        dot->setVisible(true);
    }
    // the label handles appear only when the label carries visible text
    bool showLabel = actionItem && actionItem->isVisible() &&
                     !actionItem->toPlainText().trimmed().isEmpty();
    foreach( DotSignal* dot, labelDots ) {
        dot->setVisible(showLabel);
    }
    if (showLabel) setLabelDotsPosition();
}

void CyberiadaSMEditorTransitionItem::hideDots()
{
    foreach( DotSignal* dot, listDots ) {
        dot->setVisible(false);
    }
    foreach( DotSignal* dot, labelDots ) {
        dot->setVisible(false);
    }
}

void CyberiadaSMEditorTransitionItem::setDotsPosition()
{
    if (listDots.isEmpty()) return;
    QVector<QPointF> points = routePoints();
    if (points.size() == listDots.size()) {
        for (int i = 0; i < points.size(); i++) {
            listDots.at(i)->setPos(points.at(i));
        }
        return;
    }
    // a tracked bare loop draws no bends: its ends keep the end handles, the bend
    // handles stay on the default route
    QVector<QPointF> bends = shownBends();
    listDots.first()->setPos(points.first());
    listDots.last()->setPos(points.last());
    for (int i = 1; i < listDots.size() - 1 && i - 1 < bends.size(); i++) {
        listDots.at(i)->setPos(bends.at(i - 1));
    }
}

void CyberiadaSMEditorTransitionItem::initializeLabelDots()
{
    // four corner handles for the label box (order: TL, TR, BL, BR)
    for (int i = 0; i < 4; i++) {
        DotSignal* dot = new DotSignal(QPointF(0, 0), this);
        connect(dot, &DotSignal::signalMove, this, &CyberiadaSMEditorTransitionItem::slotMoveLabelHandle);
        connect(dot, &DotSignal::signalMouseRelease, this, &CyberiadaSMEditorTransitionItem::slotReleaseLabelHandle);
        dot->setDotFlags(DotSignal::Movable);
        dot->setVisible(false);
        labelDots.append(dot);
    }
}

void CyberiadaSMEditorTransitionItem::setLabelDotsPosition()
{
    if (labelDots.size() < 4 || !actionItem) return;
    // the label box in scene coordinates (the transition item sits at the origin)
    QRectF b = actionItem->boundingRect();
    QPointF tl = actionItem->pos();
    labelDots.at(0)->setPos(tl);
    labelDots.at(1)->setPos(tl + QPointF(b.width(), 0));
    labelDots.at(2)->setPos(tl + QPointF(0, b.height()));
    labelDots.at(3)->setPos(tl + QPointF(b.width(), b.height()));
}

void CyberiadaSMEditorTransitionItem::slotMoveLabelHandle(QGraphicsItem *signalOwner, qreal, qreal, QPointF p)
{
    if (model->readOnly() || !actionItem) return;
    int idx = labelDots.indexOf(dynamic_cast<DotSignal*>(signalOwner));
    if (idx < 0) return;

    // directional width resize: a left handle moves the left edge (right anchored),
    // a right handle moves the right edge (left anchored); the height is text-driven
    labelHandleDragging = true;
    const qreal MIN_W = 20.0;
    qreal left = actionItem->pos().x();
    qreal top = actionItem->pos().y();
    qreal right = left + actionItem->boundingRect().width();
    bool leftSide = (idx == 0 || idx == 2);
    qreal newLeft = left, newWidth;
    if (leftSide) {
        newLeft = qMin(p.x(), right - MIN_W);
        newWidth = right - newLeft;
    } else {
        newWidth = qMax(MIN_W, p.x() - left);
    }
    actionItem->setTextWidth(newWidth);      // reflow (height grows down)
    actionItem->setPos(newLeft, top);
    setLabelDotsPosition();
    update();
}

void CyberiadaSMEditorTransitionItem::slotReleaseLabelHandle(QGraphicsItem *, QPointF)
{
    labelHandleDragging = false;
    if (!actionItem || model->readOnly()) return;
    // commit the box as a rect: centre relative to the source centre, current width,
    // text-driven height (mirrors TransitionAction::persistLabelPosition)
    qreal w = (actionItem->textWidth() > 0) ? actionItem->textWidth() : actionItem->boundingRect().width();
    QPointF centre = actionItem->pos() + actionItem->boundingRect().center() - sourceCenter();
    model->updateLabel(model->elementToIndex(element),
                       Cyberiada::Rect(centre.x(), centre.y(), w, actionItem->boundingRect().height()));
}


/* -----------------------------------------------------------------------------
 * Transition Action Item
 * ----------------------------------------------------------------------------- */

TransitionAction::TransitionAction(const QString &text, QGraphicsItem *parent) :
    EditableTextItem(text, parent)
{
    setFontRole(fontRoleTransition);
    setTextAlignment(Qt::AlignCenter);
    setTextMargin(0);
    setCodeRole(codeRoleTransition);
}

void TransitionAction::parseLabel(const QString& text, QString& trigger, QString& guard, QString& behaviour)
{
    QRegularExpressionMatch m;
    m = QRegularExpression(R"(^\s*([^\[/\]]+))").match(text);   // event before [ or /
    trigger = m.hasMatch() ? m.captured(1).trimmed() : QString();
    // all between [ and ] - DotMatchesEverything so a manually wrapped guard is kept whole
    m = QRegularExpression(R"(\[\s*(.*?)\s*\])",
                           QRegularExpression::DotMatchesEverythingOption).match(text);
    guard = m.hasMatch() ? m.captured(1).trimmed() : QString();
    // all after / - DotMatchesEverything so a multi-line behaviour is kept whole
    m = QRegularExpression(R"(\/\s*(.+)$)",
                           QRegularExpression::DotMatchesEverythingOption).match(text);
    behaviour = m.hasMatch() ? m.captured(1).trimmed() : QString();
}

QString TransitionAction::getTrigger() const
{
    QString trigger, guard, behaviour;
    parseLabel(toPlainText(), trigger, guard, behaviour);
    return trigger;
}

QString TransitionAction::getGuard() const
{
    QString trigger, guard, behaviour;
    parseLabel(toPlainText(), trigger, guard, behaviour);
    return guard;
}

QString TransitionAction::getBehaviour() const
{
    QString trigger, guard, behaviour;
    parseLabel(toPlainText(), trigger, guard, behaviour);
    return behaviour;
}

void TransitionAction::applyRichFormat()
{
    // the event name is bold only when a guard or behaviour follows it
    bool more = !getGuard().isEmpty() || !getBehaviour().isEmpty();
    setBoldRange(getTrigger().length(),
                 SettingsManager::instance().getBoldTransitionEventName() && more);
}

void TransitionAction::paint(QPainter *painter, const QStyleOptionGraphicsItem *o, QWidget *w) {
    if (!toPlainText().isEmpty()) {
        QColor color = painter->background().color();
        color.setAlpha(230);
        painter->setBrush(color);
        // a thin border while the transition is selected, so the resizable box reads
        QGraphicsItem* p = parentItem();
        if (p && p->isSelected()) {
            painter->setPen(QPen(SettingsManager::instance().getSelectionColor(), 1, Qt::DashLine));
        } else {
            painter->setPen(Qt::NoPen);
        }
        painter->drawRect(boundingRect());
    }
    QGraphicsTextItem::paint(painter, o, w);
}

void TransitionAction::focusOutEvent(QFocusEvent *event)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    isEdit = false;
    QGraphicsTextItem::focusOutEvent(event);

    CyberiadaSMEditorTransitionItem* transition = dynamic_cast<CyberiadaSMEditorTransitionItem*>(parentItem());
    if (!transition || CyberiadaSMEditorScene::discardsEdits(this)) return;

    QModelIndex idx = transition->model->elementToIndex(transition->element);
    // an emptied label removes the action and its stored box (a bare edge stays)
    if (toPlainText().trimmed().isEmpty()) {
        transition->model->deleteAction(idx, 0);
        transition->model->updateLabel(idx, Cyberiada::Rect());
        return;
    }
    // commit the action text first (it re-syncs the label from the model), then
    // store the label box sized from that text, so a freshly typed label is a rect
    transition->model->updateAction(idx, 0, getTrigger(), getGuard(), getBehaviour());
    persistLabelPosition(true);   // re-fit the box to the edited text
}

void TransitionAction::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (isEdit) { EditableTextItem::mousePressEvent(event); return; }
    if (event->button() == Qt::LeftButton) {
        // select the transition and arm a possible label drag
        if (scene()) scene()->clearSelection();
        if (parentItem()) parentItem()->setSelected(true);
        dragLast = event->scenePos();
        dragging = false;
        event->accept();
        return;
    }
    EditableTextItem::mousePressEvent(event);
}

void TransitionAction::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!isEdit && (event->buttons() & Qt::LeftButton)) {
        setPos(pos() + event->scenePos() - dragLast);
        dragLast = event->scenePos();
        dragging = true;
        // a fragment underlined in the label moves with it (EDIT-EDGE-21)
        if (CyberiadaSMEditorScene* sc = dynamic_cast<CyberiadaSMEditorScene*>(scene())) sc->updateSubjectLinks();
        event->accept();
        return;
    }
    EditableTextItem::mouseMoveEvent(event);
}

void TransitionAction::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (!isEdit && dragging) {
        persistLabelPosition();
        dragging = false;
        event->accept();
        return;
    }
    EditableTextItem::mouseReleaseEvent(event);
}

void TransitionAction::persistLabelPosition(bool refitWidth)
{
    CyberiadaSMEditorTransitionItem* t = dynamic_cast<CyberiadaSMEditorTransitionItem*>(parentItem());
    if (!t || !t->model) return;
    QModelIndex index = t->model->elementToIndex(t->element);
    // an empty label carries no box: reset to auto-placement
    if (toPlainText().trimmed().isEmpty()) {
        t->model->updateLabel(index, Cyberiada::Rect());
        return;
    }
    // the label is a box: its centre relative to the source centre (the origin
    // updateActionPosition reads back), the width, and the wrapped height. A point
    // label becomes a rect the first time it is moved or edited.
    qreal w;
    if (refitWidth) {
        // an in-place text edit re-fits the box to the new text: measure the
        // natural (unwrapped) width so the box shrinks when the text shrinks and
        // grows when it grows. updateActionPosition re-pinned textWidth() to the
        // OLD stored width during the model sync, so textWidth() cannot be reused.
        setTextWidth(-1);
        w = boundingRect().width();
    } else {
        w = (textWidth() > 0) ? textWidth() : boundingRect().width();   // a drag keeps the width
    }
    QPointF centre = pos() + boundingRect().center() - t->sourceCenter();
    t->model->updateLabel(index, Cyberiada::Rect(centre.x(), centre.y(), w, boundingRect().height()));
}

void TransitionAction::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    // the text being edited has the edit menu (EDIT-TOOL-17)
    if (isEdit) { EditableTextItem::contextMenuEvent(event); return; }
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->readOnly()) { return; }
    QMenu menu;
    QAction* reset = menu.addAction(QObject::tr("Reset label position"));
    if (menu.exec(event->screenPos()) == reset) {
        CyberiadaSMEditorTransitionItem* t = dynamic_cast<CyberiadaSMEditorTransitionItem*>(parentItem());
        if (t && t->model) {
            // an invalid point clears the stored label, resuming auto-placement
            t->model->updateLabel(t->model->elementToIndex(t->element), Cyberiada::Point());
        }
    }
    event->accept();
}
