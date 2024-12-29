#include "veworkplace.h"
#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QDebug>
#include <QVector>  // new
#include "verectangle.h"
#include "veselectionrect.h"
#include "vepolyline.h"
#include "transition.h"   // new

VEWorkplace::VEWorkplace(QObject *parent) :
    QGraphicsScene(parent),
    currentItem(nullptr),
    m_currentAction(DefaultType),
    m_previousAction(0),
    m_leftMouseButtonPressed(false)
{
    VERectangle *node001 = new VERectangle();
    node001->setRect(100, 25, 150, 150);
    addItem(node001);
    connect(node001, &VERectangle::clicked, this, &VEWorkplace::signalSelectItem);
    connect(node001, &VERectangle::signalMove, this, &VEWorkplace::slotMove);

    VERectangle *node002 = new VERectangle();
    node002->setRect(270, 200, 150, 150);
    addItem(node002);
    connect(node002, &VERectangle::clicked, this, &VEWorkplace::signalSelectItem);
    connect(node002, &VERectangle::signalMove, this, &VEWorkplace::slotMove);

    Transition *transition1 = new Transition();
    addItem(transition1);
    transition1->setSource(node001);
    transition1->setSourcePoint(QPointF(-75, 0));
    transition1->setTarget(node002);
    transition1->setTargetPoint(QPointF(-75, 0));
    QVector<QPointF> points = QVector<QPointF>{ QPointF(-175, 0), QPointF(-175, 100), QPointF(0, 100) };
    transition1->setPoints(points);
    transition1->setText(QString("action text"));
    transition1->setTextPosition(QPoint(150, 75));
}

VEWorkplace::~VEWorkplace()
{
    delete currentItem;
}

int VEWorkplace::currentAction() const
{
    return m_currentAction;
}

QPointF VEWorkplace::previousPosition() const
{
    return m_previousPosition;
}

void VEWorkplace::setCurrentAction(const int type)
{
    m_currentAction = type;
    emit currentActionChanged(m_currentAction);
}

void VEWorkplace::setPreviousPosition(const QPointF previousPosition)
{
    if (m_previousPosition == previousPosition)
        return;

    m_previousPosition = previousPosition;
    emit previousPositionChanged();
}

void VEWorkplace::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    qDebug() << "VEWorkplace::mousePressEvent";

    if (event->button() & Qt::LeftButton) {
        m_leftMouseButtonPressed = true;
        setPreviousPosition(event->scenePos());
        if(QApplication::keyboardModifiers() & Qt::ShiftModifier){
            m_previousAction = m_currentAction;
            setCurrentAction(SelectionType);
        }
    }

    switch (m_currentAction) {
    case TransitionType: {
        if (m_leftMouseButtonPressed && !(event->button() & Qt::RightButton) && !(event->button() & Qt::MiddleButton)) {
            deselectItems();

//            view->setDragMode(QGraphicsView::NoDrag);

            QGraphicsItem* itemUnderMouse = this->itemAt(event->scenePos(), QTransform());
            VERectangle* source = dynamic_cast<VERectangle*>(itemUnderMouse);

            if (!source) return;

            Transition *transition = new Transition();
            transition->setSource(source);
            transition->setSourcePoint(QPointF(0, 0));
            transition->setTargetPoint(event->scenePos());
            currentItem = transition;

            connect(transition, &Transition::clicked, this, &VEWorkplace::signalSelectItem);
            connect(transition, &Transition::signalMove, this, &VEWorkplace::slotMove);
            addItem(currentItem);

            emit signalNewSelectItem(transition);
        }
        break;
    }
    case LineType: {

    }
    case RectangleType: {
        if (m_leftMouseButtonPressed && !(event->button() & Qt::RightButton) && !(event->button() & Qt::MiddleButton)) {
            deselectItems();
            VERectangle *rectangle = new VERectangle();
            currentItem = rectangle;
            addItem(currentItem);
            connect(rectangle, &VERectangle::clicked, this, &VEWorkplace::signalSelectItem);
            connect(rectangle, &VERectangle::signalMove, this, &VEWorkplace::slotMove);
            emit signalNewSelectItem(rectangle);
        }
        break;
    }
    case SelectionType: {
        if (m_leftMouseButtonPressed && !(event->button() & Qt::RightButton) && !(event->button() & Qt::MiddleButton)) {
            deselectItems();
            VESelectionRect *selection = new VESelectionRect();
            currentItem = selection;
            addItem(currentItem);
        }
        break;
    }
    default: {
        QGraphicsScene::mousePressEvent(event);
        break;
    }
    }
}

void VEWorkplace::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    switch (m_currentAction) {
    case LineType: {
        if (m_leftMouseButtonPressed) {
            VEPolyline * polyline = qgraphicsitem_cast<VEPolyline *>(currentItem);
            QPainterPath path;
            path.moveTo(m_previousPosition);
            path.lineTo(event->scenePos());
            polyline->setPath(path);
        }
        break;
    }
    case TransitionType: {
        if (m_leftMouseButtonPressed) {
            if (currentItem == nullptr) {
                return;
            }
            Transition *transition = qgraphicsitem_cast<Transition *>(currentItem);
            if(!transition) {
                return;
            }
            transition->setTargetPoint(event->scenePos());
        }
        break;
    }
    case RectangleType: {
        if (m_leftMouseButtonPressed) {
            auto dx = event->scenePos().x() - m_previousPosition.x();
            auto dy = event->scenePos().y() - m_previousPosition.y();
            VERectangle *rectangle = qgraphicsitem_cast<VERectangle *>(currentItem);
            rectangle->setRect((dx > 0) ? m_previousPosition.x() : event->scenePos().x(),
                                   (dy > 0) ? m_previousPosition.y() : event->scenePos().y(),
                                   qAbs(dx), qAbs(dy));
        }
        break;
    }
    case SelectionType: {
        if (m_leftMouseButtonPressed) {
            auto dx = event->scenePos().x() - m_previousPosition.x();
            auto dy = event->scenePos().y() - m_previousPosition.y();
            VESelectionRect * selection = qgraphicsitem_cast<VESelectionRect *>(currentItem);
            selection->setRect((dx > 0) ? m_previousPosition.x() : event->scenePos().x(),
                                   (dy > 0) ? m_previousPosition.y() : event->scenePos().y(),
                                   qAbs(dx), qAbs(dy));
        }
        break;
    }
    default: {
        QGraphicsScene::mouseMoveEvent(event);
        break;
    }
    }
}

void VEWorkplace::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() & Qt::LeftButton) m_leftMouseButtonPressed = false;
    switch (m_currentAction) {
    case TransitionType: {
        if (!m_leftMouseButtonPressed &&
            !(event->button() & Qt::RightButton) &&
            !(event->button() & Qt::MiddleButton)) {

            if(!currentItem) return;

            Transition *transition = qgraphicsitem_cast<Transition *>(currentItem);
            if(!transition) {
                qDebug() << "it is not transition";
            }
            transition->setTargetPoint(event->scenePos());

            QList<QGraphicsItem*> itemsUnderMouse = this->items(event->scenePos());

            VERectangle* target;
            for (QGraphicsItem* item : itemsUnderMouse) {
                if (target = qgraphicsitem_cast<VERectangle*>(item)) {
                    qDebug() << "State detected";
                    break; // Если нужен только первый найденный State
                }
            }

            if (!target) {
                delete transition;
                qDebug() << "transition deleted";
                currentItem = nullptr;
                return;
            }
            transition->setTarget(target);
            transition->setSourcePoint(transition->findIntersectionWithRect(transition->source()));
            transition->setTargetPoint(transition->findIntersectionWithRect(transition->target()));
            transition->setText(QString("action"));
            currentItem = nullptr;
        }
        break;
    }
    case RectangleType: {
        if (!m_leftMouseButtonPressed &&
                !(event->button() & Qt::RightButton) &&
                !(event->button() & Qt::MiddleButton)) {
            currentItem = nullptr;
        }
        break;
    }
    case SelectionType: {
        if (!m_leftMouseButtonPressed &&
                !(event->button() & Qt::RightButton) &&
                !(event->button() & Qt::MiddleButton)) {
            VESelectionRect * selection = qgraphicsitem_cast<VESelectionRect *>(currentItem);
            if(!selection->collidingItems().isEmpty()){
                foreach (QGraphicsItem *item, selection->collidingItems()) {
                    item->setSelected(true);
                }
            }
            selection->deleteLater();
            if(selectedItems().length() == 1){
                signalSelectItem(selectedItems().at(0));
            }
            setCurrentAction(m_previousAction);
            currentItem = nullptr;
        }
        break;
    }
    default: {
        QGraphicsScene::mouseReleaseEvent(event);
        break;
    }
    }
}

void VEWorkplace::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    switch (m_currentAction) {
    case LineType:
    case RectangleType:
    case SelectionType:
        break;
    default:
        QGraphicsScene::mouseDoubleClickEvent(event);
        break;
    }
}

void VEWorkplace::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Delete: {
        foreach (QGraphicsItem *item, selectedItems()) {
            removeItem(item);
            delete item;
        }
        deselectItems();
        break;
    }
    case Qt::Key_A: {
        if(QApplication::keyboardModifiers() & Qt::ControlModifier){
            foreach (QGraphicsItem *item, items()) {
                item->setSelected(true);
            }
            if(selectedItems().length() == 1) signalSelectItem(selectedItems().at(0));
        }
        break;
    }
    default:
        break;
    }
    QGraphicsScene::keyPressEvent(event);
}

void VEWorkplace::deselectItems()
{
    foreach (QGraphicsItem *item, selectedItems()) {
        item->setSelected(false);
    }
    selectedItems().clear();
}

void VEWorkplace::slotMove(QGraphicsItem *signalOwner, qreal dx, qreal dy)
{
    foreach (QGraphicsItem *item, selectedItems()) {
        if(item != signalOwner) item->moveBy(dx,dy);
    }
}
