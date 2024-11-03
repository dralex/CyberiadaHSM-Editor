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
#include "cyberiadasm_editor_items.h"
#include "myassert.h"

const int ROUNDED_RECT_RADIUS = 10;
const int VERTEX_POINT_RADIUS = 10;
const int COMMENT_ANGLE_CORNER = 10;

/* -----------------------------------------------------------------------------
 * Abstract Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorAbstractItem::CyberiadaSMEditorAbstractItem(CyberiadaSMModel* _model,
															 Cyberiada::Element* _element,
															 QGraphicsItem* parent):
	QGraphicsItem(parent), model(_model), element(_element)
{
}

QRectF CyberiadaSMEditorAbstractItem::boundingRect() const
{
	MY_ASSERT(model);
	MY_ASSERT(model->rootDocument());
	return toQtRect(element->get_bound_rect(*(model->rootDocument())));
}

QVariant CyberiadaSMEditorAbstractItem::data(int key) const
{
	if (key == 0) {
		return QString(element->get_id().c_str());
	} else {
		return QVariant();
	}
}

/* -----------------------------------------------------------------------------
 * State Machine Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorSMItem::CyberiadaSMEditorSMItem(CyberiadaSMModel* model,
												 Cyberiada::Element* element,
												 QGraphicsItem* parent):
	CyberiadaSMEditorAbstractItem(model, element, parent)
{
}
	
void CyberiadaSMEditorSMItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));
	QRectF r = boundingRect();
	qDebug() << "SM (" << r.left() << ", " << r.top() << ", " << r.right() << ", " << r.bottom() << ")";
	painter->drawRect(r);
	const QPointF name_frame[] = {
		QPointF(r.left(), r.top()),
		QPointF(r.left() + 50, r.top()),
		QPointF(r.left() + 50, r.top() + 20),
		QPointF(r.left() + 40, r.top() + 30),
		QPointF(r.left(), r.top() + 30)
	};
	painter->drawConvexPolygon(name_frame, 5);	
}

/* -----------------------------------------------------------------------------
 * State Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorStateItem::CyberiadaSMEditorStateItem(CyberiadaSMModel* model,
													   Cyberiada::Element* element,
													   QGraphicsItem* parent):
	CyberiadaSMEditorAbstractItem(model, element, parent)
{
}
	
void CyberiadaSMEditorStateItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));
	painter->drawRoundedRect(boundingRect(), ROUNDED_RECT_RADIUS, ROUNDED_RECT_RADIUS);
}

/* -----------------------------------------------------------------------------
 * Composite State Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorCompositeStateItem::CyberiadaSMEditorCompositeStateItem(CyberiadaSMModel* model,
																		 Cyberiada::Element* element,
																		 QGraphicsItem* parent):
	CyberiadaSMEditorStateItem(model, element, parent)
{
}

/* -----------------------------------------------------------------------------
 * Vertex Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorVertexItem::CyberiadaSMEditorVertexItem(CyberiadaSMModel* model,
														 Cyberiada::Element* element,
														 QGraphicsItem* parent):
	CyberiadaSMEditorAbstractItem(model, element, parent)
{
}

QRectF CyberiadaSMEditorVertexItem::boundingRect()
{
	return fullCircle();
}

QRectF CyberiadaSMEditorVertexItem::fullCircle() const
{
	MY_ASSERT(model);
	MY_ASSERT(model->rootDocument());
	Cyberiada::Rect r = element->get_bound_rect(*(model->rootDocument()));
	return QRectF(r.x - VERTEX_POINT_RADIUS,
				  r.y - VERTEX_POINT_RADIUS,
				  VERTEX_POINT_RADIUS * 2,
				  VERTEX_POINT_RADIUS * 2);
}

QRectF CyberiadaSMEditorVertexItem::partialCircle() const
{
	MY_ASSERT(model);
	MY_ASSERT(model->rootDocument());
	Cyberiada::Rect r = element->get_bound_rect(*(model->rootDocument()));
	return QRectF(r.x - (VERTEX_POINT_RADIUS * 2.0 / 3.0),
				  r.y - (VERTEX_POINT_RADIUS * 2.0 / 3.0),
				  VERTEX_POINT_RADIUS * 4.0 / 3.0,
				  VERTEX_POINT_RADIUS * 4.0 / 3.0);
}

void CyberiadaSMEditorVertexItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));
	Cyberiada::ElementType type = element->get_type();	
	if (type == Cyberiada::elementInitial) {
		painter->setBrush(QBrush(Qt::black));
		painter->drawEllipse(fullCircle());
	} else if (type == Cyberiada::elementFinal) {
		painter->setBrush(painter->background());
		painter->drawEllipse(fullCircle());
		painter->setBrush(QBrush(Qt::black));
		painter->drawEllipse(partialCircle());		
	} else {
		MY_ASSERT(type == Cyberiada::elementTerminate);
		painter->setPen(QPen(Qt::black, 2, Qt::SolidLine));
		QRectF r = fullCircle();
		painter->drawLine(r.left(), r.top(), r.right(), r.bottom());
	}
}

/* -----------------------------------------------------------------------------
 * Comment Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorCommentItem::CyberiadaSMEditorCommentItem(CyberiadaSMModel* model,
														   Cyberiada::Element* element,
														   QGraphicsItem* parent):
	CyberiadaSMEditorAbstractItem(model, element, parent)
{
	commentBrush = QBrush(QColor(0xff, 0xcc, 0));
}
	
void CyberiadaSMEditorCommentItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));
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

	painter->drawConvexPolygon(points, 3);	
}

/* -----------------------------------------------------------------------------
 * Choice Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorChoiceItem::CyberiadaSMEditorChoiceItem(CyberiadaSMModel* model,
														 Cyberiada::Element* element,
														 QGraphicsItem* parent):
	CyberiadaSMEditorAbstractItem(model, element, parent)
{
}
	
void CyberiadaSMEditorChoiceItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));
	
	QRectF r = boundingRect();
	const QPointF points[] = {
		QPointF(r.left() + r.width() / 2.0, r.top()),
		QPointF(r.right(), r.top() + r.height() / 2.0),
		QPointF(r.left() + r.width() / 2.0, r.bottom()),
		QPointF(r.left(), r.top() + r.height() / 2.0)
	};

	painter->drawConvexPolygon(points, 4);
}

/* -----------------------------------------------------------------------------
 * Transition Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorTransitionItem::CyberiadaSMEditorTransitionItem(CyberiadaSMModel* model,
																 Cyberiada::Element* element,
																 QGraphicsItem* parent):
	CyberiadaSMEditorAbstractItem(model, element, parent)
{
}
	
void CyberiadaSMEditorTransitionItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
//	painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));
}
