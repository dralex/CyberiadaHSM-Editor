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
    // TODO copy from state
}

void CyberiadaSMEditorSMItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (!isEditable() || !element->has_geometry()) {
        event->ignore();
        return;
    }
    QMenu menu;
    QAction* removeBorder = menu.addAction(QObject::tr("Убрать границу автомата"));
    QAction* chosen = menu.exec(event->screenPos());
    if (chosen == removeBorder) {
        // an invalid rect clears the border: the machine returns to frameless
        model->updateGeometry(model->elementToIndex(element), Cyberiada::Rect());
    }
    event->accept();
}
