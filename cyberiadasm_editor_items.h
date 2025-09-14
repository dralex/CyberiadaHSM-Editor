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
#include <QObject>
#include <QBrush>

#include "cyberiadasm_model.h"
#include "dotsignal.h"

/* -----------------------------------------------------------------------------
 * Abstract Item
 * ----------------------------------------------------------------------------- */

class CyberiadaSMEditorAbstractItem: public QObject, public QGraphicsItem {
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
    Q_PROPERTY(QPointF previousPosition READ getPreviousPosition WRITE setPreviousPosition NOTIFY previousPositionChanged)

public:
	CyberiadaSMEditorAbstractItem(CyberiadaSMModel* model,
                                  Cyberiada::Element* element,
								  QGraphicsItem* parent = NULL);

    virtual ~CyberiadaSMEditorAbstractItem() = default;

	enum {
        SMItem = UserType + 1,
		StateItem,
		CompositeStateItem,
		CommentItem,
		VertexItem,
		ChoiceItem,
		TransitionItem
	};

    enum CornerFlags {
        Top = 0x01,
        Bottom = 0x02,
        Left = 0x04,
        Right = 0x08,
        TopLeft = Top|Left,
        TopRight = Top|Right,
        BottomLeft = Bottom|Left,
        BottomRight = Bottom|Right
    };

    enum CornerGrabbers {
        GrabberTop = 0,
        GrabberBottom,
        GrabberLeft,
        GrabberRight,
        GrabberTopLeft,
        GrabberTopRight,
        GrabberBottomLeft,
        GrabberBottomRight
    };
	
	virtual int type() const = 0;
    Cyberiada::ID getId() { return element->get_id(); }
    QModelIndex getIndex() { return model->elementToIndex(element); }
    Cyberiada::Element* getElement() { return element; }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) = 0;

    virtual QRectF boundingRect() const = 0;
	virtual QVariant data(int key) const;

	static QRectF toQtRect(const Cyberiada::Rect& r) {
		return QRectF(r.x, r.y, r.width, r.height);
	}

    QPointF getPreviousPosition() const;
    void setPreviousPosition(const QPointF newPreviousPosition);

    bool hasGeometry();

    void setHighlighted(bool on);

    virtual void syncFromModel();
    virtual void updateSizeToFitChildren(CyberiadaSMEditorAbstractItem* child);

protected:
    CyberiadaSMModel* model;
    Cyberiada::Element* element;

    void onParentGeometryChanged();
    virtual void onParentSizeChanged(CornerFlags side, qreal d);
    void onChildGeometryChanged();

    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

signals:
    void geometryChanged();
    void sizeChanged(CornerFlags side, qreal d);
    void previousPositionChanged();

private slots:
    virtual void slotInspectorModeChanged(bool on);
    virtual void slotSelectionSettingsChanged();

protected:
    unsigned int cornerFlags;
    QPointF previousPosition;
    bool isLeftMouseButtonPressed;
    DotSignal *cornerGrabber[8];

    CyberiadaSMEditorAbstractItem* prevItemUnderCursor;
    bool isHighlighted;

    void resizeLeft( const QPointF &pt);
    void resizeRight( const QPointF &pt);
    void resizeBottom(const QPointF &pt);
    void resizeTop(const QPointF &pt);

    void updatePosGeometry();
    void updateSizeGeometry();

    virtual void initializeDots();
    virtual void setDotsPosition();
    virtual void showDots();
    virtual void hideDots();

    CyberiadaSMEditorAbstractItem* collectionUnderItem();

private:
    void handleParentChange();
};

#endif
