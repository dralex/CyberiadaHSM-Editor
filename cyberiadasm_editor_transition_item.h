#ifndef CYBERIADASMEDITORTRANSITIONITEM_H
#define CYBERIADASMEDITORTRANSITIONITEM_H


#include <QGraphicsItem>
#include <QObject>
#include <QPainterPath>
#include <QVector>
#include <QPointF>
#include <QGraphicsTextItem>
#include <QString>

#include "cyberiadasm_editor_items.h"
#include "dotsignal.h"


/* -----------------------------------------------------------------------------
 *  Item
 * ----------------------------------------------------------------------------- */


class CyberiadaSMEditorTransitionItem : public QObject, public CyberiadaSMEditorAbstractItem
{
    Q_OBJECT

public:
    explicit CyberiadaSMEditorTransitionItem(QObject *parent_object,
                        CyberiadaSMModel *model,
                        Cyberiada::Element *element,
                        QGraphicsItem *parent,
                        QMap<Cyberiada::ID, QGraphicsItem*> *elementItem);
    ~CyberiadaSMEditorTransitionItem();


    virtual int type() const { return TransitionItem; }

    CyberiadaSMEditorAbstractItem *source() const;
    // void setSource(Rectangle *source);
    QPointF sourcePoint() const;
    // void setSourcePoint(const QPointF& point);
    QPointF sourceCenter() const;

    CyberiadaSMEditorAbstractItem *target() const;
    // void setTarget(Rectangle *target);
    QPointF targetPoint() const;
    // void setTargetPoint(const QPointF& point);
    QPointF targetCenter() const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr);
    QPainterPath shape() const override; // для обнаружения столкновений

    QPainterPath path() const;
    // void setPath(const QPainterPath &path);
    void updatePath();

    void drawArrow(QPainter* painter);

    QVector<QPointF> points() const;
    // void setPoints(const QVector<QPointF>& points);

    QString text() const;
    void setText(const QString& text);
    void setTextPosition(const QPointF& pos);
    void updateTextPosition();

    // QPointF findIntersectionWithRect(const State* state);

signals:
    void pathChanged();
    //    void rectChanged();
    //    void pointsChanged();
    void clicked(CyberiadaSMEditorTransitionItem *rect);
    void signalMove(QGraphicsItem *item, qreal dx, qreal dy);

private slots:
    // void slotMoveSource(QGraphicsItem *item, qreal dx, qreal dy);
    // void slotMoveTarget(QGraphicsItem *item, qreal dx, qreal dy);
    // void slotMove();
    // void slotStateResized(State *rect, State::CornerFlags side);
    // void slotMove(QGraphicsItem *signalOwner, qreal dx, qreal dy);

    //protected:
    //    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    //    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    //    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    // void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    // void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    // void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    // void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;

private:
    // void updateCoordinates(State *state, State::CornerFlags side, QPointF *point, QPointF* previousCenterPos);

    const Cyberiada::Transition* m_transition;

    QMap<Cyberiada::ID, QGraphicsItem*> *m_elementItem;

    QPointF m_previousSourceCenterPos;
    QPointF m_previousTargetCenterPos;

    QGraphicsTextItem *m_actionItem = nullptr;
    QPointF m_textPosition;

    QPointF m_previousPosition;
    QPainterPath m_path;
    QVector<QPointF> m_points;

    QRectF m_boundingRect;

    QList<DotSignal *> m_listDotes;

    bool m_leftMouseButtonPressed;
    bool m_mouseTraking;

    // void updateDots();
};

#endif // CYBERIADASMEDITORTRANSITIONITEM_H
