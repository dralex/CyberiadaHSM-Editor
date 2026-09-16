/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The State Machine Editor Vertex Item
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

#include <QDebug>
#include <QPainter>
#include <QColor>
#include <QTextCursor>
#include <math.h>

#include "myassert.h"
#include "cyberiada_constants.h"
#include "cyberiadasm_editor_vertex_item.h"
#include "cyberiadasm_editor_state_item.h"
#include "cyberiadasm_editor_scene.h"
#include "settings_manager.h"


/* -----------------------------------------------------------------------------
 * Vertex Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorVertexItem::CyberiadaSMEditorVertexItem(CyberiadaSMModel* model,
                                                         Cyberiada::Element* element,
                                                         QGraphicsItem* parent):
    CyberiadaSMEditorAbstractItem(model, element, parent)
{
    Cyberiada::Rect r = element->get_bound_rect(*(model->rootDocument()));
    setPos(r.x, r.y);

    setAcceptHoverEvents(true);
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);

    // the editable name, shown under the point when it is set
    title = new VertexTitle(QString(element->get_name().c_str()), this);
    title->setVisible(SettingsManager::instance().getShowText() && !element->get_name().empty());
    connect(title, &EditableTextItem::sizeChanged, this, [this]() { setTitlePosition(); });
    setTitlePosition();

    initializeDots();
    setDotsPosition();
    hideDots();
    // begin a transition from the four cardinal boxes (or a body drag)
    enableTransitionSourceDots(QList<int>() << GrabberTop << GrabberBottom
                               << GrabberLeft << GrabberRight);
}

void CyberiadaSMEditorVertexItem::setTitlePosition()
{
    if (!title) return;
    QRectF tb = title->boundingRect();
    // centred under the point circle
    title->setPos(-tb.width() / 2.0, VERTEX_POINT_RADIUS + 2);
}

void CyberiadaSMEditorVertexItem::syncFromModel()
{
    Cyberiada::Rect r = element->get_bound_rect(*(model->rootDocument()));
    setPos(r.x, r.y);
    if (title) {
        QString name = QString(element->get_name().c_str());
        if (title->toPlainText() != name) title->setPlainText(name);
        // keep it while editing even if empty, so the caret has somewhere to sit
        title->setVisible(SettingsManager::instance().getShowText() &&
                          (!name.isEmpty() || title->hasFocus()));
        setTitlePosition();
    }
    CyberiadaSMEditorAbstractItem::syncFromModel();
}

void CyberiadaSMEditorVertexItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !isEditable()) {
        QGraphicsItem::mouseDoubleClickEvent(event);
        return;
    }
    // a double click names the point (or edits the existing name), like a
    // transition label; the empty name is allowed and simply shows nothing
    event->accept();
    if (title) {
        title->setVisible(true);
        title->startEditing();
    }
}

QRectF CyberiadaSMEditorVertexItem::boundingRect() const
{
    return fullCircle();
}

QPainterPath CyberiadaSMEditorVertexItem::shape() const {
    QPainterPath path;
    // Cyberiada::ElementType type = element->get_type();
    // if (type == Cyberiada::elementInitial) {
    //     path.addEllipse(fullCircle());
    // } else if (type == Cyberiada::elementFinal) {
    //     path.addEllipse(partialCircle());
    // } else {
    //     MY_ASSERT(type == Cyberiada::elementTerminate);
    //     path.addEllipse(fullCircle());
    // }
    path.addEllipse(fullCircle());
    return path;
}

QRectF CyberiadaSMEditorVertexItem::fullCircle() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    return QRectF(- VERTEX_POINT_RADIUS,
                  - VERTEX_POINT_RADIUS,
                  VERTEX_POINT_RADIUS * 2,
                  VERTEX_POINT_RADIUS * 2);
}

QRectF CyberiadaSMEditorVertexItem::partialCircle() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    // Cyberiada::Rect r = element->get_bound_rect(*(model->rootDocument()));
    return QRectF(- VERTEX_POINT_RADIUS * 2.0 / 3.0,
                  - VERTEX_POINT_RADIUS * 2.0 / 3.0,
                  VERTEX_POINT_RADIUS * 4.0 / 3.0,
                  VERTEX_POINT_RADIUS * 4.0 / 3.0);
}

void CyberiadaSMEditorVertexItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    QColor color(Qt::black);
    if (Cyberiada::element_has_color(element)) {
        color = QColor(QString::fromStdString(Cyberiada::element_get_color(element)));
    }
    if (isSelected()) {
        SettingsManager& sm = SettingsManager::instance();
        color = sm.getSelectionColor();
    }
    painter->setPen(QPen(color, 1, Qt::SolidLine));
    Cyberiada::ElementType type = element->get_type();
    if (type == Cyberiada::elementInitial) {
        painter->setBrush(QBrush(color));
        painter->drawEllipse(fullCircle());
    } else if (type == Cyberiada::elementFinal) {
        painter->setBrush(painter->background());
        painter->drawEllipse(fullCircle());
        painter->setBrush(QBrush(color));
        painter->drawEllipse(partialCircle());
    } else if (type == Cyberiada::elementShallowHistory ||
               type == Cyberiada::elementDeepHistory) {
        // a circle marked with H (shallow) or H* (deep)
        QRectF r = fullCircle();
        painter->setBrush(painter->background());
        painter->drawEllipse(r);
        QFont font = painter->font();
        font.setBold(true);
        painter->setFont(font);
        QString glyph = (type == Cyberiada::elementDeepHistory) ? "H*" : "H";
        painter->drawText(r, Qt::AlignCenter, glyph);
    } else {
        MY_ASSERT(type == Cyberiada::elementTerminate);

        painter->setPen(QPen(color, 2, Qt::SolidLine));
        QRectF r = fullCircle();
        painter->drawEllipse(fullCircle());
        painter->drawLine(r.left(), r.top(), r.right(), r.bottom());
        painter->drawLine(r.right(), r.top(), r.left(), r.bottom());
    }
}

void CyberiadaSMEditorVertexItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
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

    if (isLeftMouseButtonPressed) {
        setFlag(ItemIsMovable);
    }

    QGraphicsItem::mouseMoveEvent(event);   // applies the move

    if (isLeftMouseButtonPressed) {
        // snap the moved point to the grid in scene space (no-op when snap is off)
        QPointF sp = snapToGrid(scenePos());
        setPos(parentItem() ? parentItem()->mapFromScene(sp) : sp);
        model->updateGeometry(model->elementToIndex(element),
                              Cyberiada::Point(pos().x(), pos().y()));
    }

    // a pseudostate dragged to the parent edge extends the parent, like a state
    if (parentItem()) {
        CyberiadaSMEditorAbstractItem* parent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem());
        if (parent) {
            parent->updateSizeToFitChildren(this);
        }
        StateRegion* stateArea = dynamic_cast<StateRegion*>(parentItem());
        if (stateArea) {
            parent = dynamic_cast<CyberiadaSMEditorAbstractItem*>(stateArea->parentItem());
            if (parent) {
                parent->updateSizeToFitChildren(this);
            }
        }
    }

    emit geometryChanged();
}

void CyberiadaSMEditorVertexItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (!isSelected() || !isEditable()) {
        event->ignore();
        return;
    }

    setCursor(QCursor(Qt::SizeAllCursor));
    QGraphicsItem::hoverMoveEvent(event);
}

/* -----------------------------------------------------------------------------
 * Vertex Name
 * ----------------------------------------------------------------------------- */

VertexTitle::VertexTitle(const QString& text, CyberiadaSMEditorVertexItem* parent):
    EditableTextItem(text, parent)
{
    setFontRole(fontRoleTransition);   // a light label, like a transition label
    setTextAlignment(Qt::AlignCenter);
    setTextWidthEnabled(false);        // the label hugs its text
    setTextMargin(0);
}

void VertexTitle::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // while editing, place the caret; otherwise the press belongs to the point,
    // so ignore it and let the vertex drag (the name is edited by a double click)
    if (hasFocus()) { QGraphicsTextItem::mousePressEvent(event); return; }
    event->ignore();
}

void VertexTitle::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (hasFocus()) { QGraphicsTextItem::mouseMoveEvent(event); return; }
    event->ignore();
}

void VertexTitle::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (hasFocus()) { QGraphicsTextItem::mouseReleaseEvent(event); return; }
    event->ignore();
}

void VertexTitle::focusOutEvent(QFocusEvent *event)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    isEdit = false;
    QTextCursor cursor = textCursor();
    cursor.clearSelection();
    setTextCursor(cursor);

    CyberiadaSMEditorAbstractItem* owner = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem());
    QString newName = toPlainText().trimmed();
    QGraphicsTextItem::focusOutEvent(event);
    if (!owner) return;

    QString current = QString(owner->getElement()->get_name().c_str());
    if (newName != current) {
        // a vertex may be nameless, so the empty name is allowed (it clears it)
        owner->getModel()->updateTitle(owner->getIndex(), newName);
    }
}
