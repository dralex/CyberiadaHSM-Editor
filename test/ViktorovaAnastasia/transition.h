#ifndef TRANSITION_H
#define TRANSITION_H

#include <QGraphicsItem>
#include <QObject>
#include <QPainterPath>
#include <QVector>
#include <QPointF>
#include <QGraphicsTextItem>
#include <QString>

#include "verectangle.h"
#include "dotsignal.h"


class Transition : public QObject, public QGraphicsItem
{
    Q_OBJECT

public:
    explicit Transition(QObject *parent=0, VERectangle *source=0, VERectangle *target = 0);
    ~Transition();

    VERectangle *source() const;
    void setSource(VERectangle *source);
    VERectangle *target() const;
    void setTarget(VERectangle *target);
    QPointF sourcePoint() const;
    void setSourcePoint(const QPointF& point);
    QPointF targetPoint() const;
    void setTargetPoint(const QPointF& point);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr);
    QPainterPath shape() const override; // для обнаружения столкновений

    QPainterPath path() const;
    void setPath(const QPainterPath &path);
    void updatePath();
    void drawArrow(QPainter* painter);

    QVector<QPointF> points() const;
    void setPoints(const QVector<QPointF>& points);

    QString text() const;
    void setText(const QString& text);
    void setTextPosition(const QPointF& pos);
    void updateTextPosition();

    QPointF findIntersectionWithRect(const VERectangle* state);

signals:
    void pathChanged();
//    void rectChanged();
//    void pointsChanged();
    void clicked(Transition *rect);
    void signalMove(QGraphicsItem *item, qreal dx, qreal dy);

private slots:
    void slotMoveSource(QGraphicsItem *item, qreal dx, qreal dy);
//    void slotMoveTarget(QGraphicsItem *item, qreal dx, qreal dy);
//    void slotMove();
    void slotStateResized(VERectangle *rect, VERectangle::CornerFlags side);
    void slotMove(QGraphicsItem *signalOwner, qreal dx, qreal dy);

protected:
//    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
//    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
//    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;

private:
    void updateCoordinates(VERectangle *state, VERectangle::CornerFlags side, QPointF *point, QPointF* previousCenterPos);

    VERectangle *m_source;
    VERectangle *m_target;

    QPointF m_sourcePoint;
    QPointF m_targetPoint;

    QPointF m_previousSourceCenterPos;
    QPointF m_previousTargetCenterPos;

    QGraphicsTextItem *m_text = nullptr;
    QPointF m_textPosition;

    QPointF m_previousPosition;
    QPainterPath m_path;
    QVector<QPointF> m_points;

    QRectF m_boundingRect;

    QList<DotSignal *> m_listDotes;

    bool m_leftMouseButtonPressed;
    bool m_mouseTraking;

    void updateDots();
};

#endif // TRANSITION_H
