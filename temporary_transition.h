#ifndef TEMPORARY_TRANSITION_H
#define TEMPORARY_TRANSITION_H

#include <QGraphicsItem>
#include <QObject>
#include <QPainterPath>
#include <QVector>
#include <QPointF>
#include <QGraphicsTextItem>
#include <QString>
#include <QPainter>

#include "cyberiadasm_editor_items.h"
#include "dotsignal.h"



class TemporaryTransition : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    explicit TemporaryTransition(QObject *parent_object,
                                 CyberiadaSMModel *model,
                                 QGraphicsItem *parent,
                                 QMap<Cyberiada::ID, QGraphicsItem*> &elementItem,
                                 CyberiadaSMEditorAbstractItem* source,
                                 QPointF& targetPoint);
    ~TemporaryTransition();

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr);

    CyberiadaSMEditorAbstractItem *getSource() const;
    void setSource(CyberiadaSMEditorAbstractItem *newSource);
    Cyberiada::Element* getSourceElement() const;
    Cyberiada::Point getSourcePoint() const;
    void setSourcePoint(const QPointF& point);
    QPointF sourceCenter() const;

    CyberiadaSMEditorAbstractItem *getTarget() const;
    void setTarget(CyberiadaSMEditorAbstractItem *newTarget);
    Cyberiada::Element* getTargetElement() const;
    Cyberiada::Point getTargetPoint() const;
    void setTargetPoint(const QPointF& point);
    QPointF targetCenter() const;

    QPainterPath path() const;

    DotSignal* getDot(int index);

signals:
    void signalReady(TemporaryTransition* ttrans, bool valid);

private slots:
    void slotMoveDot(QGraphicsItem *signalOwner, qreal dx, qreal dy, QPointF p);
    void slotMouseReleaseDot();

private:
    void drawArrow(QPainter* painter);
    CyberiadaSMEditorAbstractItem* itemUnderCursor();
    QPointF findIntersectionWithItem(const CyberiadaSMEditorAbstractItem *item,
                                     const QPointF& start, const QPointF& end,
                                     bool* hasIntersections);

    CyberiadaSMModel* model;
    QMap<Cyberiada::ID, QGraphicsItem*>& elementIdToItemMap;

    CyberiadaSMEditorAbstractItem* source;
    CyberiadaSMEditorAbstractItem* target;
    QPointF sourcePoint;
    QPointF targetPoint;

    QPointF prevPosition;
    bool isSourceTraking;
    bool isTargetTraking;

    QList<DotSignal *> listDots;
    void initializeDots();
    void showDots();
    void hideDots();
    void setDotsPosition();
};

#endif // TEMPORARY_TRANSITION_H
