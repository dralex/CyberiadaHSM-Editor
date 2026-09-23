/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Editor Scene Items
 *
 * Copyright (C) 2024 Alexey Fedoseev <aleksey@fedoseev.net>
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

#include <QDebug>
#include <QPainter>
#include <QColor>
#include <QCursor>

#include "cyberiadasm_editor_items.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_editor_state_item.h"
#include "cyberiadasm_editor_transition_item.h"
#include "settings_manager.h"
#include "myassert.h"
#include "cyberiada_constants.h"


/* -----------------------------------------------------------------------------
 * Abstract Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorAbstractItem::CyberiadaSMEditorAbstractItem(CyberiadaSMModel* _model,
                                                             Cyberiada::Element* _element,
                                                             QGraphicsItem* parent):
    // QObject(nullptr),
    QGraphicsItem(parent),
    model(_model),
    element(_element),
    cornerFlags(0)
{
    connect(&SettingsManager::instance(), &SettingsManager::inspectorModeChanged, this, &CyberiadaSMEditorAbstractItem::slotInspectorModeChanged);
    connect(&SettingsManager::instance(), &SettingsManager::serviceObjectsChanged, this, &CyberiadaSMEditorAbstractItem::slotServiceObjectsChanged);
    connect(&SettingsManager::instance(), &SettingsManager::selectionSettingsChanged, this, &CyberiadaSMEditorAbstractItem::slotSelectionSettingsChanged);

    prevItemUnderCursor = nullptr;
    isHighlighted = false;

    if(parent) {
        CyberiadaSMEditorAbstractItem* newParent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parent);
        if(newParent) {
            prevItemUnderCursor = newParent;
            // change in parent geometry
            // to change transition action position when parent of state/target changes
            connect(newParent, &CyberiadaSMEditorAbstractItem::geometryChanged,
                    this, &CyberiadaSMEditorAbstractItem::onParentGeometryChanged);
            // change in parent size
            // to change position of children
            connect(newParent, &CyberiadaSMEditorAbstractItem::sizeChanged,
                    this, &CyberiadaSMEditorAbstractItem::onParentSizeChanged);
            // change in this item geometry
            // to change size of parent when child moves inside
            // connect(this, &CyberiadaSMEditorAbstractItem::geometryChanged,
            //         newParent, &CyberiadaSMEditorAbstractItem::onChildGeometryChanged);
        }
        StateRegion* stateArea = dynamic_cast<StateRegion*>(parent);
        if(stateArea) {
            newParent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(stateArea->parentItem());
            if(newParent) {
                prevItemUnderCursor = newParent;
                // change in parent geometry
                connect(newParent, &CyberiadaSMEditorAbstractItem::geometryChanged,
                        this, &CyberiadaSMEditorAbstractItem::onParentGeometryChanged);
                // change in parent size
                // to change position of children
                connect(newParent, &CyberiadaSMEditorAbstractItem::sizeChanged,
                        this, &CyberiadaSMEditorAbstractItem::onParentSizeChanged);
                // change in this item geometry
                // connect(this, &CyberiadaSMEditorAbstractItem::geometryChanged,
                //         newParent, &CyberiadaSMEditorAbstractItem::onChildGeometryChanged);
            }
        }
    }
}

QVariant CyberiadaSMEditorAbstractItem::data(int key) const
{
	if (key == 0) {
		return QString(element->get_id().c_str());
	} else {
		return QVariant();
	}
}

QPointF CyberiadaSMEditorAbstractItem::getPreviousPosition() const
{
    return previousPosition;
}

void CyberiadaSMEditorAbstractItem::setPreviousPosition(const QPointF newPreviousPosition)
{
    if (previousPosition == newPreviousPosition)
        return;

    previousPosition = newPreviousPosition;
    emit previousPositionChanged();
}

bool CyberiadaSMEditorAbstractItem::hasGeometry()
{
    return element->has_geometry();
}

bool CyberiadaSMEditorAbstractItem::isEditable() const
{
    const CyberiadaSMEditorScene* s = dynamic_cast<const CyberiadaSMEditorScene*>(scene());
    return element->has_geometry() &&
           s && const_cast<CyberiadaSMEditorScene*>(s)->getCurrentTool() == ToolType::Select &&
           !SettingsManager::instance().getInspectorMode();
}

void CyberiadaSMEditorAbstractItem::setHighlighted(bool on)
{
    if (isHighlighted != on) {
        isHighlighted = on;
        update();
    }
}

void CyberiadaSMEditorAbstractItem::syncFromModel()
{
    setDotsPosition();
    update();
}

void CyberiadaSMEditorAbstractItem::onParentGeometryChanged() {
    update();
    emit geometryChanged();
}

void CyberiadaSMEditorAbstractItem::onParentSizeChanged(CornerFlags side, qreal delta)
{
    if(auto collection = dynamic_cast<Cyberiada::ElementCollection*>(element)) {
        Cyberiada::Rect r = collection->get_geometry_rect();
        QRectF tmpR = QRectF(r.x, r.y, r.width, r.height);
        switch (side) {
        case CornerFlags::Right:
            tmpR = QRectF(r.x - delta, r.y, r.width, r.height);
            break;
        case CornerFlags::Left:
            tmpR = QRectF(r.x + delta, r.y, r.width, r.height);
            break;
        case CornerFlags::Bottom:
            tmpR = QRectF(r.x, r.y - delta, r.width, r.height);
            break;
        case CornerFlags::Top:
            tmpR = QRectF(r.x, r.y + delta, r.width, r.height);
            break;
        default:
            break;
        }

        Cyberiada::Rect newR = Cyberiada::Rect(tmpR.x(),
                                            tmpR.y(),
                                            tmpR.width(),
                                            tmpR.height());
        model->updateGeometry(model->elementToIndex(element), newR);
    }
}

void CyberiadaSMEditorAbstractItem::onChildGeometryChanged()
{
    // update();
    // emit geometryChanged();
}

void CyberiadaSMEditorAbstractItem::updateSizeToFitChildren(CyberiadaSMEditorAbstractItem* child)
{

}

QVariant CyberiadaSMEditorAbstractItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged || change == ItemTransformHasChanged) {
        emit geometryChanged();
    }
    if (change == ItemParentHasChanged) {
        handleParentChange();
    }
    if (change == ItemSelectedHasChanged) {
        refreshTransitionDots();
    }
    return QGraphicsItem::itemChange(change, value);
}

void CyberiadaSMEditorAbstractItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    CyberiadaSMEditorScene* cScene = dynamic_cast<CyberiadaSMEditorScene*>(scene());

    // the transition tool works like selection but only starts transitions:
    // pressing an item selects it (its source boxes appear) and, for a valid
    // source, arms a body-drag that draws a transition
    if (cScene && cScene->getCurrentTool() == ToolType::Transition) {
        if (event->button() == Qt::LeftButton) {
            scene()->clearSelection();
            setSelected(true);
            if (transitionSourceEnabled && element->has_geometry() &&
                !SettingsManager::instance().getInspectorMode()) {
                creatingOfTrans = true;
            }
            event->accept();
        } else {
            event->ignore();
        }
        return;
    }

    if (!element->has_geometry() || (cScene && cScene->getCurrentTool() != ToolType::Select)) {
        event->ignore();
        return;
    }

    if (!isEditable()) {
        QGraphicsItem::mousePressEvent(event);
        return;
    }

    // the press decides the gesture: no prior hover or selection is needed
    cornerFlags = borderZone(event->pos());
    // remember where the cursor grabbed the item, to place it in scene space
    // during a body drag, and the start position for a Ctrl axis-locked move
    grabOffset = scenePos() - event->scenePos();
    dragStartScenePos = scenePos();

    if (event->button() & Qt::LeftButton) {
        isLeftMouseButtonPressed = true;
    }
    QGraphicsItem::mousePressEvent(event);
}

void CyberiadaSMEditorAbstractItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (creatingOfTrans) {
        creatingOfTrans = false;
        startTransition();
        return;
    }

    if (!isEditable()) {
        event->ignore();
        return;
    }

    if (!isLeftMouseButtonPressed) {
        return;
    }

    QPointF pt = event->pos();
    QPointF scenePt = event->scenePos();

    switch (cornerFlags) {
    case Top:
        updatePosGeometry();
        break;
    case Bottom:
        resizeBottom(scenePt);
        break;
    case Left:
        updatePosGeometry();
        break;
    case Right:
        resizeRight(scenePt);
        break;
    case TopLeft:
        updatePosGeometry();
        break;
    // case TopRight:
    //     resizeTop(pt);
    //     resizeRight(pt);
    //     break;
    // case BottomLeft:
    //     resizeBottom(pt);
    //     resizeLeft(pt);
    //     break;
    case BottomRight:
        resizeBottom(scenePt);
        resizeRight(scenePt);
        break;
    default:
        // if (isLeftMouseButtonPressed) {
        //     setCursor(Qt::ClosedHandCursor);
        //     setFlag(ItemIsMovable);
        // }
        break;
    }

    QGraphicsItem::mouseMoveEvent(event);

    if (parentItem()) {
        CyberiadaSMEditorAbstractItem* parent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem());
        if(parent) {
            parent->updateSizeToFitChildren(this);
        }
        StateRegion* stateArea = dynamic_cast<StateRegion*>(parentItem());
        if(stateArea) {
            parent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(stateArea->parentItem());
            if(parent) {
                parent->updateSizeToFitChildren(this);
            }
        }
    }
    // emit geometryChanged();
}

void CyberiadaSMEditorAbstractItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (!isEditable()) {
        event->ignore();
        return;
    }

    if (event->button() & Qt::LeftButton) {
        isLeftMouseButtonPressed = false;
        setFlag(ItemIsMovable, false);
    }
    QGraphicsItem::mouseReleaseEvent(event);
}

void CyberiadaSMEditorAbstractItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    if (!isEditable()) {
        event->ignore();
        return;
    }

    setDotsPosition();
    QGraphicsItem::hoverEnterEvent(event);
}

void CyberiadaSMEditorAbstractItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    if (!isEditable()) {
        event->ignore();
        return;
    }

    cornerFlags = 0;
    unsetCursor();
    QGraphicsItem::hoverLeaveEvent( event );
}

void CyberiadaSMEditorAbstractItem::slotInspectorModeChanged(bool on)
{
    update();
}

void CyberiadaSMEditorAbstractItem::slotServiceObjectsChanged(bool on)
{
    update();
}

void CyberiadaSMEditorAbstractItem::slotSelectionSettingsChanged()
{
    update();
}

int CyberiadaSMEditorAbstractItem::borderZone(const QPointF& pt) const
{
    // within 7 px of a side, inside or outside
    QRectF r = boundingRect();
    int flags = 0;
    if (qAbs(pt.y() - r.top()) < 7) flags |= Top;
    if (qAbs(pt.y() - r.bottom()) < 7) flags |= Bottom;
    if (qAbs(pt.x() - r.right()) < 7) flags |= Right;
    if (qAbs(pt.x() - r.left()) < 7) flags |= Left;
    return flags;
}

void CyberiadaSMEditorAbstractItem::applyZoneCursor(QGraphicsItem* item, int flags)
{
    switch (flags) {
    case Top:
    case Left:
    case TopLeft:
    case TopRight:
    case BottomLeft:
        item->setCursor(QCursor(Qt::SizeAllCursor));
        break;
    case Bottom:
        item->setCursor(QCursor(Qt::SizeVerCursor));
        break;
    case Right:
        item->setCursor(QCursor(Qt::SizeHorCursor));
        break;
    case BottomRight:
        item->setCursor(QCursor(Qt::SizeFDiagCursor));
        break;
    default:
        item->unsetCursor();
        break;
    }
}

void CyberiadaSMEditorAbstractItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (!isSelected() || !isEditable()) {
        event->ignore();
        return;
    }
    cornerFlags = borderZone(event->pos());
    applyZoneCursor(this, cornerFlags);
    QGraphicsItem::hoverMoveEvent(event);
}

// pt is the cursor in SCENE coordinates (the item's own coords are unstable while
// the centre moves during the resize)
void CyberiadaSMEditorAbstractItem::resizeRight(const QPointF &scenePt)
{
    // directional: keep the left edge, move the right edge to the cursor
    QRectF border = sceneBoundingRect();
    if (scenePt.x() <= border.left()) return;
    border.setRight(scenePt.x());
    applyBorderRect(border);
}

void CyberiadaSMEditorAbstractItem::resizeBottom(const QPointF &scenePt)
{
    QRectF border = sceneBoundingRect();
    if (scenePt.y() <= border.top()) return;
    border.setBottom(scenePt.y());
    applyBorderRect(border);
}

// the child elements' bounding box in scene coordinates (invalid if none)
QRectF CyberiadaSMEditorAbstractItem::contentBox() const
{
    QRectF box;
    Cyberiada::ElementCollection* coll = dynamic_cast<Cyberiada::ElementCollection*>(element);
    if (!coll) return box;
    const CyberiadaSMEditorScene* sc = dynamic_cast<const CyberiadaSMEditorScene*>(scene());
    if (!sc) return box;
    QMap<Cyberiada::ID, QGraphicsItem*>& map =
        const_cast<CyberiadaSMEditorScene*>(sc)->getMap();
    Cyberiada::ElementList kids = coll->get_children();
    for (Cyberiada::ElementList::const_iterator i = kids.begin(); i != kids.end(); i++) {
        // a comment does not hold the border open: it may lie outside it
        if (Cyberiada::element_is_comment(*i)) continue;
        QGraphicsItem* ci = map.value((*i)->get_id());
        if (ci) box = box.united(ci->sceneBoundingRect());
    }
    return box;
}

qreal CyberiadaSMEditorAbstractItem::minSpanWidth() const  { return ELEMENT_MIN_SIZE; }
qreal CyberiadaSMEditorAbstractItem::minSpanHeight() const { return ELEMENT_MIN_SIZE; }

void CyberiadaSMEditorAbstractItem::applyBorderRect(QRectF borderScene)
{
    Cyberiada::ElementCollection* coll = dynamic_cast<Cyberiada::ElementCollection*>(element);
    if (!coll) return;
    // clamp each edge so the content stays inside the inset region (children may
    // not enter the title / action bands)
    QRectF content = contentBox();
    QMarginsF ins = contentInset();
    if (content.isValid()) {
        borderScene.setLeft(qMin(borderScene.left(), content.left() - ins.left()));
        borderScene.setRight(qMax(borderScene.right(), content.right() + ins.right()));
        borderScene.setTop(qMin(borderScene.top(), content.top() - ins.top()));
        borderScene.setBottom(qMax(borderScene.bottom(), content.bottom() + ins.bottom()));
    }
    // the intrinsic floor (title tab / ELEMENT_MIN) when the content is small
    if (borderScene.width() < minSpanWidth()) {
        qreal c = borderScene.center().x();
        borderScene.setLeft(c - minSpanWidth() / 2.0);
        borderScene.setRight(c + minSpanWidth() / 2.0);
    }
    if (borderScene.height() < minSpanHeight()) {
        qreal c = borderScene.center().y();
        borderScene.setTop(c - minSpanHeight() / 2.0);
        borderScene.setBottom(c + minSpanHeight() / 2.0);
    }
    prepareGeometryChange();
    // the centre moves by the same delta in the model as in the scene (no scale),
    // so re-base the children by that delta to hold their absolute places
    Cyberiada::Rect old = coll->get_geometry_rect();
    QPointF delta = borderScene.center() - scenePos();
    model->updateGeometry(model->elementToIndex(element),
        Cyberiada::Rect(old.x + delta.x(), old.y + delta.y(), borderScene.width(), borderScene.height()));
    if (delta.x() != 0.0) emit sizeChanged(CornerFlags::Right, delta.x());
    if (delta.y() != 0.0) emit sizeChanged(CornerFlags::Bottom, delta.y());
}

qreal CyberiadaSMEditorAbstractItem::minimumWidth() const { return ELEMENT_MIN_SIZE; }
qreal CyberiadaSMEditorAbstractItem::minimumHeight() const { return ELEMENT_MIN_SIZE; }

void CyberiadaSMEditorAbstractItem::updatePosGeometry()
{
    // change the model data
    setFlag(ItemIsMovable);

    Cyberiada::Rect r = Cyberiada::Rect(pos().x(),
                                        pos().y(),
                                        boundingRect().width(),
                                        boundingRect().height());
    model->updateGeometry(model->elementToIndex(element), r);
}

void CyberiadaSMEditorAbstractItem::resizeToRect(const Cyberiada::Rect& req)
{
    Cyberiada::ElementCollection* coll = dynamic_cast<Cyberiada::ElementCollection*>(element);
    if (!coll || !element->has_geometry()) {
        // a leaf element (comment, choice, vertex) has no children to contain
        model->updateGeometry(model->elementToIndex(element), req);
        return;
    }
    // express the requested (centre-based) rect in scene coordinates and apply it
    // through the shared path: per-side content clamp + children re-base
    Cyberiada::Rect cur = coll->get_geometry_rect();
    QPointF centreScene = scenePos() + QPointF(req.x - cur.x, req.y - cur.y);
    QRectF border(centreScene.x() - req.width / 2.0, centreScene.y() - req.height / 2.0,
                  req.width, req.height);
    applyBorderRect(border);
}

void CyberiadaSMEditorAbstractItem::initializeDots()
{
    if (cornerGrabber[0] != nullptr) return;   // idempotent
    if (!element->has_geometry()) return;
    for (int i = 0; i < 8; i++){
        cornerGrabber[i] = new DotSignal(this);
    }
}

// change of parent
void CyberiadaSMEditorAbstractItem::handleParentChange() {
    // if (auto oldParent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem())) {
    //     disconnect(oldParent, &CyberiadaSMEditorAbstractItem::geometryChanged,
    //                this, &CyberiadaSMEditorAbstractItem::onParentGeometryChanged);
    // }

    // if (auto newParent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem())) {
    //     connect(newParent, &CyberiadaSMEditorAbstractItem::geometryChanged,
    //             this, &CyberiadaSMEditorAbstractItem::onParentGeometryChanged);
    // }
}

void CyberiadaSMEditorAbstractItem::setDotsPosition()
{    
    if(!element->has_geometry()) return;
    // an element may gain geometry after construction (a state machine border
    // added at runtime): create its handles the first time they are needed
    initializeDots();
    if (cornerGrabber[0] == nullptr) return;
    QRectF tmpRect = boundingRect();
    cornerGrabber[GrabberTop]->setPos(tmpRect.left() + tmpRect.width()/2, tmpRect.top());
    cornerGrabber[GrabberBottom]->setPos(tmpRect.left() + tmpRect.width()/2, tmpRect.bottom());
    cornerGrabber[GrabberLeft]->setPos(tmpRect.left(), tmpRect.top() + tmpRect.height()/2);
    cornerGrabber[GrabberRight]->setPos(tmpRect.right(), tmpRect.top() + tmpRect.height()/2);
    cornerGrabber[GrabberTopLeft]->setPos(tmpRect.topLeft().x(), tmpRect.topLeft().y());
    cornerGrabber[GrabberTopRight]->setPos(tmpRect.topRight().x(), tmpRect.topRight().y());
    cornerGrabber[GrabberBottomLeft]->setPos(tmpRect.bottomLeft().x(), tmpRect.bottomLeft().y());
    cornerGrabber[GrabberBottomRight]->setPos(tmpRect.bottomRight().x(), tmpRect.bottomRight().y());
}

void CyberiadaSMEditorAbstractItem::showDots()
{
    if(!isSelected()) return;
    if(!element->has_geometry()) return;
    if(cornerGrabber[0] == nullptr) return;
    for(int i = 0; i < 8; i++){
        cornerGrabber[i]->setVisible(true);
    }
}

void CyberiadaSMEditorAbstractItem::hideDots()
{
    if(cornerGrabber[0] == nullptr) return;
    for(int i = 0; i < 8; i++){
        cornerGrabber[i]->setVisible(false);
    }
}

void CyberiadaSMEditorAbstractItem::startTransition()
{
    // the generic source (a pseudostate or the choice) cannot always be its own
    // target (an initial has no incoming), so a self-loop seed would be
    // rejected: draw a rubber-band line and create the transition on release,
    // when a valid target is known. (States override this with the self-loop
    // seed, which they can retarget.)
    CyberiadaSMEditorScene* cScene = dynamic_cast<CyberiadaSMEditorScene*>(scene());
    if (cScene) cScene->beginTransitionDraw(this);
}

void CyberiadaSMEditorAbstractItem::slotTransitionFromBox()
{
    if (!element->has_geometry() || SettingsManager::instance().getInspectorMode()) return;
    startTransition();
}

void CyberiadaSMEditorAbstractItem::enableTransitionSourceDots(const QList<int>& indices)
{
    setDotsPosition();   // create/position the grabbers if needed
    for (int i = 0; i < indices.size(); i++) {
        int idx = indices.at(i);
        if (idx < 0 || idx >= 8 || cornerGrabber[idx] == nullptr) continue;
        cornerGrabber[idx]->setDotFlags(DotSignal::TransitionSource);
        connect(cornerGrabber[idx], &DotSignal::signalDragStarted,
                this, &CyberiadaSMEditorAbstractItem::slotTransitionFromBox);
    }
    transitionSourceDots = indices;
    transitionSourceEnabled = !indices.isEmpty();
    hideDots();
}

void CyberiadaSMEditorAbstractItem::refreshTransitionDots()
{
    if (cornerGrabber[0] == nullptr) return;
    CyberiadaSMEditorScene* cScene = dynamic_cast<CyberiadaSMEditorScene*>(scene());
    bool show = transitionSourceEnabled && cScene &&
                cScene->getCurrentTool() == ToolType::Transition &&
                isSelected() && element->has_geometry() &&
                !SettingsManager::instance().getInspectorMode();
    setDotsPosition();
    for (int i = 0; i < 8; i++) {
        cornerGrabber[i]->setVisible(show && transitionSourceDots.contains(i));
    }
}

CyberiadaSMEditorAbstractItem *CyberiadaSMEditorAbstractItem::collectionUnderItem()
{
    QPointF center = mapToScene(boundingRect().center());
    QList<QGraphicsItem*> items = scene()->items(center);

    CyberiadaSMEditorAbstractItem* cItem = nullptr;
    for (QGraphicsItem* item : items) {
        cItem = dynamic_cast<CyberiadaSMEditorAbstractItem*>(item);
        if (!cItem) { continue; }
        if (cItem == this) { continue; }

        if (cItem->type() == CyberiadaSMEditorAbstractItem::StateItem ||
            cItem->type() == CyberiadaSMEditorAbstractItem::CompositeStateItem ||
            cItem->type() == CyberiadaSMEditorAbstractItem::SMItem) {
            return cItem;
        }
    }
    return nullptr;
}
