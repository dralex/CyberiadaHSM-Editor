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
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QApplication>

#include "cyberiadasm_editor_subject_item.h"
#include "cyberiadasm_editor_transition_item.h"
#include "cyberiadasm_editor_scene.h"
#include "dotsignal.h"
#include "edge_geometry.h"
#include "editable_text_item.h"
#include "settings_manager.h"
#include "subject_fragment.h"
#include "dialogs/fragmentdialog.h"

// the comment point square (PNST 984 7.2.7) and the part of the end segments of a
// commented transition kept free of an unstored link foot (as libhtreegeom does)
static const qreal SUBJECT_POINT_SIZE = 6.0;
static const qreal SUBJECT_FOOT_MARGIN = 0.25;
static const qreal SUBJECT_HIT_WIDTH = 6.0;

static QPointF toQPoint(const Cyberiada::Point& p)
{
    return QPointF(p.x, p.y);
}

static Cyberiada::Point toPoint(const QPointF& p)
{
    return Cyberiada::Point(p.x(), p.y());
}

// the centre displacement a resize reports on its side (see EdgeGeometry::rebaseOnResize)
static QPointF centreShift(int side, qreal d)
{
    QPointF c;
    if (side & CyberiadaSMEditorAbstractItem::CornerFlags::Right)  c.rx() += d;
    if (side & CyberiadaSMEditorAbstractItem::CornerFlags::Left)   c.rx() -= d;
    if (side & CyberiadaSMEditorAbstractItem::CornerFlags::Bottom) c.ry() += d;
    if (side & CyberiadaSMEditorAbstractItem::CornerFlags::Top)    c.ry() -= d;
    return c;
}

CyberiadaSMEditorSubjectItem::CyberiadaSMEditorSubjectItem(CyberiadaSMModel* _model,
                                                           Cyberiada::Comment* _comment, int _index,
                                                           QMap<Cyberiada::ID, QGraphicsItem*>& elementItem):
    QGraphicsObject(nullptr), model(_model), comment(_comment), index(_index),
    elementIdToItemMap(elementItem)
{
    setFlags(ItemIsSelectable);
    for (int i = 0; i < 2; i++) {
        dots[i] = new DotSignal(this);
        dots[i]->setDotFlags(DotSignal::Movable);
        dots[i]->setVisible(false);
        connect(dots[i], &DotSignal::signalMove, this, &CyberiadaSMEditorSubjectItem::slotMoveDot);
        connect(dots[i], &DotSignal::signalMouseRelease, this, &CyberiadaSMEditorSubjectItem::slotReleaseDot);
    }
    connectEnds();
}

CyberiadaSMEditorSubjectItem::~CyberiadaSMEditorSubjectItem()
{
}

void CyberiadaSMEditorSubjectItem::connectEnds()
{
    for (const QPointer<CyberiadaSMEditorAbstractItem>& end : connectedEnds) {
        if (end) disconnect(end, nullptr, this, nullptr);
    }
    connectedEnds.clear();
    if (!hasSubject()) return;
    const Cyberiada::Element* target = subject().get_element();
    connectedTarget = target ? target->get_id() : Cyberiada::ID();

    CyberiadaSMEditorAbstractItem* c = commentItem();
    if (c) {
        connect(c, &CyberiadaSMEditorAbstractItem::geometryChanged, this, &CyberiadaSMEditorSubjectItem::updatePath);
        connect(c, &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorSubjectItem::onCommentSizeChanged);
        connectedEnds.append(c);
    }
    CyberiadaSMEditorAbstractItem* frame = targetNode();
    if (frame) {
        connect(frame, &CyberiadaSMEditorAbstractItem::geometryChanged, this, &CyberiadaSMEditorSubjectItem::updatePath);
        connect(frame, &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorSubjectItem::onTargetSizeChanged);
        connectedEnds.append(frame);
    }
    if (CyberiadaSMEditorTransitionItem* t = targetTransition()) {
        CyberiadaSMEditorAbstractItem* other = t->target();
        if (other && other != frame) {
            connect(other, &CyberiadaSMEditorAbstractItem::geometryChanged, this, &CyberiadaSMEditorSubjectItem::updatePath);
            connect(other, &CyberiadaSMEditorAbstractItem::sizeChanged, this, &CyberiadaSMEditorSubjectItem::updatePath);
            connectedEnds.append(other);
        }
    }
}

void CyberiadaSMEditorSubjectItem::detach()
{
    // the bounding rect collapses with the comment gone
    if (scene()) prepareGeometryChange();
    for (const QPointer<CyberiadaSMEditorAbstractItem>& end : connectedEnds) {
        if (end) disconnect(end, nullptr, this, nullptr);
    }
    connectedEnds.clear();
    comment = nullptr;
}

const Cyberiada::CommentSubject& CyberiadaSMEditorSubjectItem::subject() const
{
    return comment->get_subjects().at(size_t(index));
}

bool CyberiadaSMEditorSubjectItem::hasSubject() const
{
    return comment && index >= 0 && size_t(index) < comment->get_subjects().size();
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
    if (!hasSubject()) return nullptr;
    const Cyberiada::Element* target = subject().get_element();
    if (!target || target->get_type() != Cyberiada::elementTransition) return nullptr;
    return dynamic_cast<CyberiadaSMEditorTransitionItem*>(elementIdToItemMap.value(target->get_id()));
}

CyberiadaSMEditorAbstractItem* CyberiadaSMEditorSubjectItem::targetNode() const
{
    if (CyberiadaSMEditorTransitionItem* t = targetTransition()) return t->source();
    if (!hasSubject()) return nullptr;
    const Cyberiada::Element* target = subject().get_element();
    if (!target) return nullptr;
    return dynamic_cast<CyberiadaSMEditorAbstractItem*>(elementIdToItemMap.value(target->get_id()));
}

CyberiadaSMEditorAbstractItem* CyberiadaSMEditorSubjectItem::targetItem() const
{
    if (!hasSubject()) return nullptr;
    const Cyberiada::Element* target = subject().get_element();
    if (!target) return nullptr;
    return dynamic_cast<CyberiadaSMEditorAbstractItem*>(elementIdToItemMap.value(target->get_id()));
}

QVector<QLineF> CyberiadaSMEditorSubjectItem::underline() const
{
    if (!hasSubject() || !subject().has_fragment()) return QVector<QLineF>();
    const Cyberiada::CommentSubject& s = subject();
    QString fragment = QString::fromStdString(s.get_fragment());
    // the first occurrence in the search order is the commented one (EDIT-STRUCT-12)
    SubjectFragment::Place place = SubjectFragment::locate(s.get_element(), s.get_type(), fragment);
    CyberiadaSMEditorAbstractItem* owner = targetItem();
    int start = -1;
    EditableTextItem* text = place.found() && owner ? owner->fieldText(place.field, &start) : nullptr;
    // a hidden text, or one being edited, leaves the end on the border
    if (!text || start < 0 || !text->isVisible() || text->isEditing()) return QVector<QLineF>();
    int from = start + place.offset;
    // the drawn text lags the model until its owner syncs
    if (text->toPlainText().mid(from, fragment.length()) != fragment) return QVector<QLineF>();
    return EdgeGeometry::textRangeLines(text, from, from + fragment.length());
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

QModelIndex CyberiadaSMEditorSubjectItem::commentIndex() const
{
    return model->elementToIndex(comment);
}

QPointF CyberiadaSMEditorSubjectItem::targetEnd() const
{
    if (dragging == 1) return dragPoint;
    if (!hasSubject()) return QPointF();
    const Cyberiada::CommentSubject& s = subject();
    if (s.has_fragment()) {
        // the end lies at the middle of the first underline segment (EDIT-EDGE-21)
        QVector<QLineF> lines = underline();
        if (!lines.isEmpty()) return lines.first().center();
    }
    const Cyberiada::Polyline& pl = s.get_geometry_polyline();
    QPointF toward = !pl.empty() ? toQPoint(pl.back()) + commentCentre() :
        (s.has_geometry_source_point() ? toQPoint(s.get_geometry_source_point()) + commentCentre() :
         commentCentre());
    if (CyberiadaSMEditorTransitionItem* t = targetTransition()) {
        // the foot lies on the path: the stored point is brought onto it after a
        // move of the transition, an unstored one keeps off the arrow and the label
        QPainterPath path = t->mapToScene(t->path());
        if (s.has_geometry_target_point()) {
            return EdgeGeometry::nearestOnPath(path, toQPoint(s.get_geometry_target_point()) + frameCentre());
        }
        return EdgeGeometry::nearestOnPath(path, toward, SUBJECT_FOOT_MARGIN, t->labelSceneRect());
    }
    if (s.has_geometry_target_point()) {
        return toQPoint(s.get_geometry_target_point()) + frameCentre();
    }
    return EdgeGeometry::attachedPoint(targetNode(), toward, true) + frameCentre();
}

QPointF CyberiadaSMEditorSubjectItem::sourceEnd() const
{
    if (dragging == 0) return dragPoint;
    if (!hasSubject()) return QPointF();
    const Cyberiada::CommentSubject& s = subject();
    if (s.has_geometry_source_point()) {
        return toQPoint(s.get_geometry_source_point()) + commentCentre();
    }
    const Cyberiada::Polyline& pl = s.get_geometry_polyline();
    QPointF toward = !pl.empty() ? toQPoint(pl.front()) + commentCentre() : targetEnd();
    return EdgeGeometry::attachedPoint(commentItem(), toward, true) + commentCentre();
}

QPainterPath CyberiadaSMEditorSubjectItem::path() const
{
    QPainterPath p;
    if (!hasSubject() || !commentItem() || !targetNode()) return p;
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
    QRectF r = path().boundingRect();
    for (const QLineF& l : underline()) r |= QRectF(l.p1(), l.p2()).normalized();
    return r.adjusted(-m, -m, m, m);
}

QPainterPath CyberiadaSMEditorSubjectItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(SUBJECT_HIT_WIDTH);
    return stroker.createStroke(path());
}

void CyberiadaSMEditorSubjectItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    if (!hasSubject()) return;
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
    if (subject().has_fragment()) {
        QPen under(pen.color(), pen.widthF(), Qt::SolidLine);
        painter->setPen(under);
        for (const QLineF& l : underline()) painter->drawLine(l);
    } else {
        QPointF end = targetEnd();
        qreal h = SUBJECT_POINT_SIZE / 2.0;
        painter->setPen(Qt::NoPen);
        painter->setBrush(point);
        painter->drawRect(QRectF(end.x() - h, end.y() - h, SUBJECT_POINT_SIZE, SUBJECT_POINT_SIZE));
    }
}

void CyberiadaSMEditorSubjectItem::updatePath()
{
    // a retarget keeps the item: it follows the signals of its new ends
    if (!hasSubject()) return;
    const Cyberiada::Element* target = subject().get_element();
    if (target && target->get_id() != connectedTarget) connectEnds();
    prepareGeometryChange();
    refreshDots();
    update();
}

bool CyberiadaSMEditorSubjectItem::editable() const
{
    CyberiadaSMEditorScene* sc = dynamic_cast<CyberiadaSMEditorScene*>(scene());
    return sc && sc->getCurrentTool() == ToolType::Select && model && !model->readOnly() &&
        !SettingsManager::instance().getInspectorMode();
}

void CyberiadaSMEditorSubjectItem::refreshDots()
{
    bool show = hasSubject() && isSelected() && editable();
    QPointF ends[2] = { sourceEnd(), targetEnd() };
    for (int i = 0; i < 2; i++) {
        if (i != dragging) dots[i]->setPos(ends[i]);
        dots[i]->setVisible(show);
    }
}

QVariant CyberiadaSMEditorSubjectItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemSelectedHasChanged) refreshDots();
    return QGraphicsObject::itemChange(change, value);
}

CyberiadaSMEditorAbstractItem* CyberiadaSMEditorSubjectItem::retargetUnder(const QPointF& p) const
{
    if (!scene() || !hasSubject()) return nullptr;
    const Cyberiada::CommentSubject& s = subject();
    QString fragment = QString::fromStdString(s.get_fragment());
    for (QGraphicsItem* gi : scene()->items(p)) {
        if (gi == this || gi->parentItem() == this) continue;
        if (TransitionAction* label = dynamic_cast<TransitionAction*>(gi)) gi = label->parentItem();
        CyberiadaSMEditorAbstractItem* ci = dynamic_cast<CyberiadaSMEditorAbstractItem*>(gi);
        if (!ci || ci->type() == CyberiadaSMEditorAbstractItem::SMItem) continue;
        Cyberiada::Element* el = ci->getElement();
        if (el == comment || el == s.get_element()) return nullptr;
        if (el->get_type() != Cyberiada::elementTransition && !el->has_geometry()) return nullptr;
        return model->subjectAllowed(comment, el, s.get_type(), fragment) ? ci : nullptr;
    }
    return nullptr;
}

QPointF CyberiadaSMEditorSubjectItem::dragEnd(int end, const QPointF& p) const
{
    if (end == 0) return EdgeGeometry::borderNearest(commentItem(), p);
    if (retargetUnder(p)) return p;
    if (CyberiadaSMEditorTransitionItem* t = targetTransition()) {
        return EdgeGeometry::nearestOnPath(t->mapToScene(t->path()), p, 0.0, t->labelSceneRect());
    }
    return EdgeGeometry::borderNearest(targetNode(), p);
}

void CyberiadaSMEditorSubjectItem::slotMoveDot(QGraphicsItem* owner, qreal, qreal, QPointF p)
{
    int end = owner == dots[0] ? 0 : 1;
    prepareGeometryChange();
    dragging = end;
    dragPoint = dragEnd(end, p);
    dots[end]->setPos(dragPoint);
    update();
}

void CyberiadaSMEditorSubjectItem::slotReleaseDot(QGraphicsItem* owner, QPointF p)
{
    if (dragging < 0 || !hasSubject()) return;   // a click on a handle moves nothing
    int end = owner == dots[0] ? 0 : 1;
    prepareGeometryChange();
    dragging = -1;
    const Cyberiada::CommentSubject& s = subject();
    Cyberiada::Point sp = s.get_geometry_source_point();
    Cyberiada::Point tp = s.get_geometry_target_point();
    Cyberiada::Polyline pl = s.get_geometry_polyline();
    Cyberiada::CommentSubjectType type = s.get_type();
    QString fragment = QString::fromStdString(s.get_fragment());
    QModelIndex ci = commentIndex();
    if (end == 0) {
        sp = toPoint(EdgeGeometry::borderNearest(commentItem(), p) - commentCentre());
        model->updateSubjectGeometry(ci, index, sp, tp, pl);
        return;
    }
    if (CyberiadaSMEditorAbstractItem* other = retargetUnder(p)) {
        // dropped on another element: the subject points at it there (EDIT-EDGE-20)
        QPointF frame, foot;
        if (CyberiadaSMEditorTransitionItem* t = dynamic_cast<CyberiadaSMEditorTransitionItem*>(other)) {
            if (!t->source()) return;
            frame = t->source()->sceneBoundingRect().center();
            foot = EdgeGeometry::nearestOnPath(t->mapToScene(t->path()), p, 0.0, t->labelSceneRect());
        } else {
            frame = other->sceneBoundingRect().center();
            foot = EdgeGeometry::borderNearest(other, p);
        }
        if (model->updateCommentSubject(ci, index, other->getElement(), type, fragment) &&
            type == Cyberiada::commentSubjectElement) {
            model->updateSubjectGeometry(ci, index, sp, toPoint(foot - frame), pl);
        }
        return;
    }
    if (type != Cyberiada::commentSubjectElement) {
        // a fragment keeps no target point: only a retarget moves its end
        updatePath();
        return;
    }
    tp = toPoint(dragEnd(1, p) - frameCentre());
    model->updateSubjectGeometry(ci, index, sp, tp, pl);
}

void CyberiadaSMEditorSubjectItem::onCommentSizeChanged(CyberiadaSMEditorAbstractItem::CornerFlags side, qreal d)
{
    if (!hasSubject()) return;
    const Cyberiada::CommentSubject& s = subject();
    if (!s.has_geometry_source_point()) {
        updatePath();
        return;
    }
    QPointF sp = toQPoint(s.get_geometry_source_point());
    EdgeGeometry::rebaseOnResize(side, sp, d, commentItem());
    model->updateSubjectGeometry(commentIndex(), index, toPoint(sp), s.get_geometry_target_point(),
                                 s.get_geometry_polyline());
}

void CyberiadaSMEditorSubjectItem::onTargetSizeChanged(CyberiadaSMEditorAbstractItem::CornerFlags side, qreal d)
{
    if (!hasSubject()) return;
    const Cyberiada::CommentSubject& s = subject();
    if (!s.has_geometry_target_point()) {
        updatePath();
        return;
    }
    QPointF tp = toQPoint(s.get_geometry_target_point());
    if (targetTransition()) {
        // a foot on a transition keeps its place: no border to stay on
        tp -= centreShift(side, d);
    } else {
        EdgeGeometry::rebaseOnResize(side, tp, d, targetNode());
    }
    model->updateSubjectGeometry(commentIndex(), index, s.get_geometry_source_point(), toPoint(tp),
                                 s.get_geometry_polyline());
}

bool CyberiadaSMEditorSubjectItem::deleteSubject()
{
    return model->deleteCommentSubject(commentIndex(), index);
}

bool CyberiadaSMEditorSubjectItem::editFragment(QWidget* parent)
{
    // no dialogs in batch mode: the scripts use the subject verbs
    if (!hasSubject() || (qApp && qApp->property("batchMode").toBool())) return false;
    const Cyberiada::CommentSubject& s = subject();
    Cyberiada::Element* target = const_cast<Cyberiada::Element*>(s.get_element());
    if (!target) return false;
    QModelIndex ci = commentIndex();
    int i = index;
    FragmentDialog dialog(target, s.get_type(), QString::fromStdString(s.get_fragment()),
                          EditableTextItem::sceneCodeLanguage(scene()), parent);
    if (dialog.exec() != QDialog::Accepted) return false;
    // the item may be rebuilt by the change: nothing of it is read after the call
    return model->updateCommentSubject(ci, i, target, dialog.subjectType(), dialog.fragment());
}

void CyberiadaSMEditorSubjectItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    if (!hasSubject()) return;
    bool editable = model && !model->readOnly() && !SettingsManager::instance().getInspectorMode();
    const Cyberiada::CommentSubject& s = subject();
    QMenu menu;
    QAction* remove = menu.addAction(tr("Delete Link"));
    remove->setEnabled(editable);
    menu.addSeparator();
    // a fragment of the target's text, or the target whole (EDIT-TOOL-16)
    QAction* fragment = menu.addAction(tr("Comment a Fragment…"));
    fragment->setEnabled(editable &&
                         (!SubjectFragment::fields(s.get_element(), Cyberiada::commentSubjectName).isEmpty() ||
                          !SubjectFragment::fields(s.get_element(), Cyberiada::commentSubjectData).isEmpty()));
    QAction* whole = menu.addAction(tr("Comment the Element"));
    whole->setEnabled(editable && s.has_fragment());
    Cyberiada::Element* target = const_cast<Cyberiada::Element*>(s.get_element());
    QModelIndex ci = commentIndex();
    int i = index;
    QWidget* parent = event->widget() ? event->widget()->window() : nullptr;
    event->accept();
    // the subject goes, the comment stays (EDIT-EDGE-20); the item may be rebuilt
    // by the model change, so nothing of it is read after the call
    QAction* chosen = menu.exec(event->screenPos());
    if (chosen == remove) {
        deleteSubject();
    } else if (chosen == fragment) {
        editFragment(parent);
    } else if (chosen == whole) {
        model->updateCommentSubject(ci, i, target, Cyberiada::commentSubjectElement, QString());
    }
}
