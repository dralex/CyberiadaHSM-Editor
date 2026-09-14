/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The State Machine Editor State Machine Item
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
#include <QMenu>
#include <QGraphicsSceneContextMenuEvent>
#include <cmath>
#include "cyberiadasm_editor_sm_item.h"
#include "cyberiadasm_editor_state_item.h"
#include "cyberiadasm_model.h"
#include "myassert.h"
#include "settings_manager.h"

/* -----------------------------------------------------------------------------
 * State Machine Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorSMItem::CyberiadaSMEditorSMItem(CyberiadaSMModel* model,
                                                 Cyberiada::Element* element,
                                                 QGraphicsItem* parent):
    CyberiadaSMEditorAbstractItem(model, element, parent)
{
    QRectF rect = toQtRect(element->get_bound_rect(*(model->rootDocument())));

    if(element->has_geometry()) {
        setPos(rect.x(), rect.y());
    }
    setFlags(ItemIsSelectable);
    setAcceptHoverEvents(true);   // the resize cursor over the border

    isHighlighted = false;

    // an editable title, like a state, shown only when the border exists
    title = new StateTitle(QString(element->get_name().c_str()), this);
    title->setTextWidthEnabled(false);   // the header tab hugs the title text
    title->setTextAlignment(Qt::AlignLeft);
    title->setVisible(element->has_geometry() && SettingsManager::instance().getShowText());
    connect(title, &EditableTextItem::sizeChanged, this, [this]() { setTitlePosition(); update(); });
    setTitlePosition();
}

void CyberiadaSMEditorSMItem::setTitlePosition()
{
    if (!title || !element->has_geometry()) return;
    QRectF r = boundingRect();
    title->setPos(r.left() + 8, r.top() + 4);
}

qreal CyberiadaSMEditorSMItem::titleTabWidth() const
{
    if (!title || !title->isVisible()) return 0;
    return title->boundingRect().width() + 16;   // the tab padding
}

qreal CyberiadaSMEditorSMItem::minimumWidth() const
{
    // the floor is the children's extent (from the centre), not the current
    // border: ElementCollection::get_bound_rect would union the border in and
    // pin the minimum to the current size, so the SM could never be shrunk
    double hw, hh;
    model->childrenHalfExtent(static_cast<Cyberiada::ElementCollection*>(element), hw, hh);
    return std::max(std::max((qreal)ELEMENT_MIN_SIZE, titleTabWidth()), (qreal)(2.0 * hw));
}

qreal CyberiadaSMEditorSMItem::minimumHeight() const
{
    double hw, hh;
    model->childrenHalfExtent(static_cast<Cyberiada::ElementCollection*>(element), hw, hh);
    qreal titleH = (title && title->isVisible()) ? title->boundingRect().height() + 8 : 0;
    return std::max(std::max((qreal)ELEMENT_MIN_SIZE, titleH), (qreal)(2.0 * hh));
}

void CyberiadaSMEditorSMItem::syncFromModel()
{
    prepareGeometryChange();
    if (element->has_geometry()) {
        QRectF rect = toQtRect(element->get_bound_rect(*(model->rootDocument())));
        setPos(rect.x(), rect.y());
    }
    if (title) {
        QString name = QString(element->get_name().c_str());
        if (title->toPlainText() != name) title->setPlainText(name);
        title->setVisible(element->has_geometry() && SettingsManager::instance().getShowText());
        setTitlePosition();
        // a title longer than the border widens the machine (once)
        if (element->has_geometry() && !adjustingForTitle) {
            Cyberiada::Rect b = static_cast<Cyberiada::ElementCollection*>(element)->get_geometry_rect();
            qreal need = titleTabWidth();
            if (b.width + 0.5 < need) {
                adjustingForTitle = true;
                model->updateGeometry(getIndex(), Cyberiada::Rect(b.x, b.y, need, b.height));
                adjustingForTitle = false;
            }
        }
    }
    CyberiadaSMEditorAbstractItem::syncFromModel();
}

QRectF CyberiadaSMEditorSMItem::boundingRect() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    MY_ASSERT(element);

    Cyberiada::Rect r = element->get_bound_rect(*(model->rootDocument()));
    QRectF rect = toQtRect(r);

    if(!element->has_geometry()) {
        return QRectF(rect.x() - rect.width() / 2, rect.y() - rect.height() / 2, rect.width(), rect.height());
    }
    return QRectF(- rect.width() / 2, - rect.height() / 2, rect.width(), rect.height());
}


void CyberiadaSMEditorSMItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    if (!element->has_geometry()) return;

    QPen pen = QPen(Qt::black, 2, Qt::SolidLine);
    if (isSelected() || isHighlighted) {
        SettingsManager& sm = SettingsManager::instance();
        pen.setColor(sm.getSelectionColor());
        pen.setWidth(sm.getSelectionBorderWidth());
        QColor fillColor = sm.getSelectionColor();
        fillColor.setAlpha(50);
        painter->setBrush(QBrush(fillColor));
    }

    painter->setPen(pen);
    QRectF r = boundingRect();
    painter->drawRect(r);
    // the title tab (folded corner) fits the current title
    QRectF tb = (title && title->isVisible()) ? title->boundingRect() : QRectF(0, 0, 42, 22);
    qreal W = tb.width() + 16;
    qreal H = tb.height() + 8;
    qreal fold = 10;
    const QPointF name_frame[] = {
        QPointF(r.left(), r.top()),
        QPointF(r.left() + W, r.top()),
        QPointF(r.left() + W, r.top() + H - fold),
        QPointF(r.left() + W - fold, r.top() + H),
        QPointF(r.left(), r.top() + H)
    };
    painter->drawConvexPolygon(name_frame, 5);
}



void CyberiadaSMEditorSMItem::updateSizeToFitChildren(CyberiadaSMEditorAbstractItem *child)
{
    // the border grows to make room for a child moved toward or past its edge,
    // only on the crossed side, and the other children re-base so they stay put
    if (!child || !element->has_geometry()) return;
    Cyberiada::Rect border =
        static_cast<Cyberiada::ElementCollection*>(element)->get_geometry_rect();

    const qreal pad = 10.0;
    // the border rect is centre-based; the child in the border's own frame
    QRectF inner(-border.width / 2 + pad, -border.height / 2 + pad,
                 border.width - 2 * pad, border.height - 2 * pad);
    QRectF childRect = mapRectFromItem(child, child->boundingRect());

    qreal overLeft   = inner.left()       - childRect.left();
    qreal overRight  = childRect.right()  - inner.right();
    qreal overTop    = inner.top()        - childRect.top();
    qreal overBottom = childRect.bottom() - inner.bottom();

    // grow symmetrically about the centre so the other children keep their place
    // and the dragged child keeps tracking the cursor (a centre shift would fight
    // the live drag); the border stretches to swallow the overspill
    qreal addW = 2.0 * std::max(0.0, std::max(overLeft, overRight));
    qreal addH = 2.0 * std::max(0.0, std::max(overTop, overBottom));

    const qreal eps = 0.01;
    if (addW < eps && addH < eps) return;
    model->updateGeometry(getIndex(),
        Cyberiada::Rect(border.x, border.y, border.width + addW, border.height + addH));
}

