#ifndef CYBERIADASMEDITORSTATEITEM_H
#define CYBERIADASMEDITORSTATEITEM_H


#include <QObject>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <QPainter>
#include <QDebug>

// #include "grabber.h"
#include "editable_text_item.h"
#include "cyberiadasm_editor_items.h"


/* -----------------------------------------------------------------------------
 * State Item
 * ----------------------------------------------------------------------------- */

class StateArea : public QGraphicsRectItem
{
public:
    explicit StateArea(QGraphicsItem *parent = NULL):
        QGraphicsRectItem(parent) {}

    void setTopLine(bool topLine) {
        this->topLine = topLine;
    }

    void setBottomLine(bool bottomLine) {
        this->bottomLine = bottomLine;
    }

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        Q_UNUSED(option)
        Q_UNUSED(widget)

        if(topLine) painter->drawLine(boundingRect().topLeft(), boundingRect().topRight());
        if(bottomLine) painter->drawLine(boundingRect().bottomLeft(), boundingRect().bottomRight());

        painter->setBrush(Qt::blue);
        painter->drawEllipse(QPointF(0, 0), 2, 2); // Центр системы координат
    }
private:
    bool topLine = false;
    bool bottomLine = false;
};


class CyberiadaSMEditorStateItem : public QObject, public CyberiadaSMEditorAbstractItem
{
    Q_OBJECT
    //Q_PROPERTY(QPointF previousPosition READ previousPosition WRITE setPreviousPosition NOTIFY previousPositionChanged)

public:
    explicit CyberiadaSMEditorStateItem(QObject *parent_object,
                       CyberiadaSMModel *model,
                       Cyberiada::Element *element,
                       QGraphicsItem *parent = NULL);
    ~CyberiadaSMEditorStateItem();

    virtual int type() const { return StateItem; }

    /*
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
*/

    QPointF previousPosition() const;
    void setPreviousPosition(const QPointF previousPosition);

    void setRect(qreal x, qreal y, qreal w, qreal h);
    void setRect(const QRectF &rect);
    QRectF rect() const;
    qreal x() const;
    qreal y() const;
    qreal width() const;
    qreal height() const;

    StateArea* getArea();

    QRectF boundingRect() const override;

    void setPositionText() override;


signals:
    void rectChanged(CyberiadaSMEditorStateItem *rect);
    void previousPositionChanged();
    void clicked(CyberiadaSMEditorStateItem *rect);
    void signalMove(QGraphicsItem *item, qreal dx, qreal dy);

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    // unsigned int m_cornerFlags;
    QPointF m_previousPosition;
    bool m_leftMouseButtonPressed;
    // Grabber *cornerGrabber[8];

    EditableTextItem *title;
    EditableTextItem* entry = nullptr;
    EditableTextItem* exit = nullptr;

    QRectF m_rect;
    StateArea* m_area;

    const Cyberiada::State* m_state;
    std::vector<Cyberiada::Action> m_actions;
    // QMap<Cyberiada::ID, QGraphicsItem*> *m_elementItem;

    /*
    void resizeLeft( const QPointF &pt);
    void resizeRight( const QPointF &pt);
    void resizeBottom(const QPointF &pt);
    void resizeTop(const QPointF &pt);

    void setPositionGrabbers();
    void showGrabbers();
    void hideGrabbers();
*/


};


#endif // CYBERIADASMEDITORSTATEITEM_H
