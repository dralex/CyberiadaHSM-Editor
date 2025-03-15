/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The State Machine Editor Comment Item
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
#include <QFontDatabase>
#include "cyberiadasm_editor_comment_item.h"
#include "myassert.h"
#include "cyberiada_constants.h"
#include "fontmanager.h"

/* -----------------------------------------------------------------------------
 * Comment Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorCommentItem::CyberiadaSMEditorCommentItem(QObject *parent_object,
                                                           CyberiadaSMModel *model,
                                                           Cyberiada::Element *element,
                                                           QGraphicsItem *parent,
                                                           QMap<Cyberiada::ID, QGraphicsItem*>& elementItem) :
    CyberiadaSMEditorAbstractItem(model, element, parent),
    // QObject(parent_object),
    elementIdToItemMap(elementItem)
{
    comment = static_cast<const Cyberiada::Comment*>(element);

    if(comment->has_geometry()){
        Cyberiada::Rect r = comment->get_geometry_rect();
        setPos(QPointF(r.x, r.y));
    }

    text = new EditableTextItem(comment->get_body().c_str(), this);
    text->setPos(-boundingRect().width() / 2 + 15, - boundingRect().height() / 2);

    if (element->get_type() == Cyberiada::elementFormalComment) {
        int fontId = QFontDatabase::addApplicationFont(":/Fonts/fonts/courier.ttf");
        if (fontId != -1) {
            QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
            if (!fontFamilies.isEmpty()) {
                QFont customFont(fontFamilies.at(0), FontManager::instance().getFont().pointSize());
                text->setFont(customFont);
                text->setFontStyleChangeable(false);
            }
        }
    }

    commentBrush = QBrush(QColor(0xff, 0xcc, 0));

    setAcceptHoverEvents(true);
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);

    setPositionText();

    initializeDots();
    setDotsPosition();
    hideDots();
}


CyberiadaSMEditorCommentItem::~CyberiadaSMEditorCommentItem() {
}

QRectF CyberiadaSMEditorCommentItem::boundingRect() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    if (!comment->has_geometry()) {
        return text->boundingRect();
    }
    Cyberiada::Rect r = comment->get_geometry_rect();
    return QRectF(- r.width / 2,
                  - r.height / 2,
                  r.width,
                  r.height);
}

void CyberiadaSMEditorCommentItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    if (!comment->has_geometry()) return;

    QPen pen = QPen(Qt::black, 1, Qt::SolidLine);
    if (isSelected()) {
        pen.setColor(Qt::red);
        pen.setWidth(2);
    }

    painter->setPen(pen);
    painter->setBrush(commentBrush);

    QRectF r = boundingRect();

    const QPointF points[] = {
        QPointF(r.left(), r.top()),
        QPointF(r.right() - COMMENT_ANGLE_CORNER, r.top()),
        QPointF(r.right(), r.top() + COMMENT_ANGLE_CORNER),
        QPointF(r.right(), r.bottom()),
        QPointF(r.left(), r.bottom())
    };

    painter->drawConvexPolygon(points, 5);

    Cyberiada::ElementType type = element->get_type();
    if (type == Cyberiada::elementFormalComment) {
        painter->setBrush(QBrush(Qt::black));
    } else {
        painter->setBrush(commentBrush);
    }

    const QPointF triangle[] = {
        QPointF(r.right() - COMMENT_ANGLE_CORNER, r.top()),
        QPointF(r.right(), r.top() + COMMENT_ANGLE_CORNER),
        QPointF(r.right()- COMMENT_ANGLE_CORNER, r.top() + COMMENT_ANGLE_CORNER)
    };

    painter->drawConvexPolygon(triangle, 3);
}

void CyberiadaSMEditorCommentItem::setPositionText()
{
    QRectF oldRect = boundingRect();
    QRectF titleRect = text->boundingRect();
    text->setPos(oldRect.x() + (oldRect.width() - titleRect.width()) / 2 , oldRect.y());
}

