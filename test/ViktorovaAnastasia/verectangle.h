#ifndef RECTANGLE_H
#define RECTANGLE_H

#include <QObject>
#include <QGraphicsRectItem>
#include <QPen>
#include <QBrush>

class DotSignal;
class QGraphicsSceneMouseEvent;

class VERectangle : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
    Q_PROPERTY(QPointF previousPosition READ previousPosition WRITE setPreviousPosition NOTIFY previousPositionChanged)

public:
    explicit VERectangle(QObject * parent = 0);
    ~VERectangle();

    enum ActionStates {
        ResizeState = 0x01,
        RotationState = 0x02
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

    QPointF previousPosition() const;
    void setPreviousPosition(const QPointF previousPosition);

    void setRect(qreal x, qreal y, qreal w, qreal h);
    void setRect(const QRectF &rect);

    void setName(const QString &name);
    QString name() const;

    // Переопределяем boundingRect (обязательный метод для QGraphicsItem)
    QRectF boundingRect() const override;

    // Реализация отрисовки (имитация поведения QGraphicsRectItem)
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    QRectF rect() const;

    void setPen(const QPen &pen);
    void setBrush(const QBrush &brush);

    QPen pen();
    QBrush brush();

signals:
    void rectChanged(VERectangle *rect);
    void previousPositionChanged();
    void clicked(VERectangle *rect);
    void signalMove(QGraphicsItem *item, qreal dx, qreal dy);
    void signalResized(VERectangle *rect, VERectangle::CornerFlags side);

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    unsigned int m_cornerFlags;
    unsigned int m_actionFlags;
    QPointF m_previousPosition;
    bool m_leftMouseButtonPressed;
    DotSignal *cornerGrabber[8];
    QString m_name;

    QRectF m_rect;
    QPen m_pen;
    QBrush m_brush;

    void resizeLeft( const QPointF &pt);
    void resizeRight( const QPointF &pt);
    void resizeBottom(const QPointF &pt);
    void resizeTop(const QPointF &pt);

    void rotateItem(const QPointF &pt);
    void setPositionGrabbers();
    void setVisibilityGrabbers();
    void hideGrabbers();
};

#endif // RECTANGLE_H
