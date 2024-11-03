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

#ifndef CYBERIADA_SM_EDITOR_ITEMS_HEADER
#define CYBERIADA_SM_EDITOR_ITEMS_HEADER

#include <QGraphicsItem>
#include <QBrush>
#include "cyberiadasm_model.h"

/* -----------------------------------------------------------------------------
 * Abstract Item
 * ----------------------------------------------------------------------------- */

class CyberiadaSMEditorAbstractItem: public QGraphicsItem {
public:
	CyberiadaSMEditorAbstractItem(CyberiadaSMModel* model,
								  Cyberiada::Element* element,
								  QGraphicsItem* parent = NULL);

	enum {
		SMItem = UserType + 1,
		StateItem,
		CompositeStateItem,
		CommentItem,
		VertexItem,
		ChoiceItem,
		TransitionItem
	};
	
	virtual int type() const = 0;
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) = 0;

    virtual QRectF boundingRect() const;
	virtual QVariant data(int key) const;

	static QRectF toQtRect(const Cyberiada::Rect& r) {
		return QRectF(r.x, r.y, r.width, r.height);
	}
	
protected:
	CyberiadaSMModel* model;
	Cyberiada::Element* element;
};

/* -----------------------------------------------------------------------------
 * State Machine Item
 * ----------------------------------------------------------------------------- */

class CyberiadaSMEditorSMItem: public CyberiadaSMEditorAbstractItem {
public:
	CyberiadaSMEditorSMItem(CyberiadaSMModel* model,
							Cyberiada::Element* element,
							QGraphicsItem* parent = NULL);

	virtual int type() const { return SMItem; }
	
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
};

/* -----------------------------------------------------------------------------
 * State Item
 * ----------------------------------------------------------------------------- */

class CyberiadaSMEditorStateItem: public CyberiadaSMEditorAbstractItem {
public:
	CyberiadaSMEditorStateItem(CyberiadaSMModel* model,
							   Cyberiada::Element* element,
							   QGraphicsItem* parent = NULL);

	virtual int type() const { return StateItem; }
	
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
};

/* -----------------------------------------------------------------------------
 * Composite State Item
 * ----------------------------------------------------------------------------- */

class CyberiadaSMEditorCompositeStateItem: public CyberiadaSMEditorStateItem {
public:
	CyberiadaSMEditorCompositeStateItem(CyberiadaSMModel* model,
										Cyberiada::Element* element,
										QGraphicsItem* parent = NULL);

	virtual int type() const { return CompositeStateItem; }
};

/* -----------------------------------------------------------------------------
 * Vertex Item
 * ----------------------------------------------------------------------------- */

class CyberiadaSMEditorVertexItem: public CyberiadaSMEditorAbstractItem {
public:
	CyberiadaSMEditorVertexItem(CyberiadaSMModel* model,
								Cyberiada::Element* element,
								QGraphicsItem* parent = NULL);

	virtual int type() const { return VertexItem; }
	
	virtual QRectF boundingRect();
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private:
	QRectF fullCircle() const;
	QRectF partialCircle() const;
};

/* -----------------------------------------------------------------------------
 * Choice Item
 * ----------------------------------------------------------------------------- */

class CyberiadaSMEditorChoiceItem: public CyberiadaSMEditorAbstractItem {
public:
	CyberiadaSMEditorChoiceItem(CyberiadaSMModel* model,
								Cyberiada::Element* element,
								QGraphicsItem* parent = NULL);

	virtual int type() const { return ChoiceItem; }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
};

/* -----------------------------------------------------------------------------
 * Comment Item
 * ----------------------------------------------------------------------------- */

class CyberiadaSMEditorCommentItem: public CyberiadaSMEditorAbstractItem {
public:
	CyberiadaSMEditorCommentItem(CyberiadaSMModel* model,
								Cyberiada::Element* element,
								QGraphicsItem* parent = NULL);

	virtual int type() const { return CommentItem; }
	
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private:
	QBrush commentBrush;
};

/* -----------------------------------------------------------------------------
 * Transition Item
 * ----------------------------------------------------------------------------- */

class CyberiadaSMEditorTransitionItem: public CyberiadaSMEditorAbstractItem {
public:
	CyberiadaSMEditorTransitionItem(CyberiadaSMModel* model,
								Cyberiada::Element* element,
								QGraphicsItem* parent = NULL);

	virtual int type() const { return TransitionItem; }
	
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
};

#endif
