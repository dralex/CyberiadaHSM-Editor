#include <cstdio>
#include <QPainter>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QtMath>

#include "transition.h"

Transition::Transition(QObject *parent, VERectangle *source, VERectangle *target) :
    QObject(parent), QGraphicsItem(), m_sourcePoint(0, 0), m_targetPoint(0, 0)
{
    m_mouseTraking = true;
    setSource(source);
    setTarget(target);

    setAcceptHoverEvents(true);
    setFlags(ItemIsSelectable|ItemSendsGeometryChanges);
}

Transition::~Transition()
{
    if(m_text){
        delete m_text;
    }
}

QRectF Transition::boundingRect() const
{
    return m_path.boundingRect().adjusted(-10, -10, 10, 10); // Увеличиваем область для стрелки
}

void Transition::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPen pen(Qt::black, 1);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_path);

    drawArrow(painter);
}

QPainterPath Transition::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(10); // ширинf взаимодействия (например, 10px)
    return stroker.createStroke(m_path);
}

VERectangle *Transition::source() const
{
    return m_source;
}

void Transition::setSource(VERectangle *source)
{
    if (m_source == source) return;

    m_source = source;
    if (!m_source) return;

    m_previousSourceCenterPos = m_source->sceneBoundingRect().center();
    connect(m_source, &VERectangle::signalMove, this, &Transition::slotMoveSource);
    connect(m_source, &VERectangle::signalResized, this, &Transition::slotStateResized);
}

VERectangle *Transition::target() const
{
    return m_target;
}

void Transition::setTarget(VERectangle *target)
{
    if (m_target == target) return;

    m_target = target;
    if (!m_target) return;

    m_previousTargetCenterPos = m_target->sceneBoundingRect().center();
    connect(m_target, &VERectangle::signalMove, this, &Transition::slotMoveSource);
    connect(m_target, &VERectangle::signalResized, this, &Transition::slotStateResized);
    m_mouseTraking = false;
}

QPointF Transition::sourcePoint() const
{
    return m_sourcePoint;
}

void Transition::setSourcePoint(const QPointF &point)
{
    if(m_sourcePoint != point) {
        m_sourcePoint = point;
    }
    updatePath();
}

QPointF Transition::targetPoint() const
{
    return m_targetPoint;
}

void Transition::setTargetPoint(const QPointF &point)
{
    if(m_targetPoint != point) {
        m_targetPoint = point;
    }
    updatePath();
}

QPainterPath Transition::path() const
{
    return m_path;
}

void Transition::setPath(const QPainterPath &path)
{
    if (m_path != path) {
        m_path = path;
    }
    update();
    prepareGeometryChange();
}

void Transition::updatePath()
{
    if (!m_source) return;

    m_previousSourceCenterPos = m_source->sceneBoundingRect().center();

    if (m_target) {
        m_previousTargetCenterPos = m_target->sceneBoundingRect().center();
    }

    QPainterPath path = QPainterPath();
    path.moveTo(m_sourcePoint + m_previousSourceCenterPos);

    if (!m_points.isEmpty()) {
        for (int i = 0; i < m_points.size(); ++i) {
            path.lineTo(m_points[i] + m_previousSourceCenterPos);
        }
    }

    if (m_mouseTraking) {
        path.lineTo(m_targetPoint);
    }
    if (m_target) {
        path.lineTo(m_targetPoint + m_previousTargetCenterPos);
    }

    setPath(path);
    update();
    prepareGeometryChange();
}

void Transition::drawArrow(QPainter* painter)
{
    QPen pen(Qt::black, 1);
    painter->setPen(pen);

    QPointF p1 = m_sourcePoint + m_source->sceneBoundingRect().center();
    if (!m_points.isEmpty()) {
        p1 = m_points.last() + m_source->sceneBoundingRect().center();
    }
    QPointF p2;
    if (m_mouseTraking) {
        p2 = m_targetPoint;
    } else {
        p2 = m_targetPoint + m_target->sceneBoundingRect().center();
    }

    QLineF line(p1, p2);
    double angle = std::atan2(-line.dy(), line.dx());

    qreal arrowSize = 10.0;

    QPointF arrowP1 = p2 + QPointF(-arrowSize * std::cos(angle - M_PI / 6),
                                   arrowSize * std::sin(angle - M_PI / 6));
    QPointF arrowP2 = p2 + QPointF(-arrowSize * std::cos(angle + M_PI / 6),
                                   arrowSize * std::sin(angle + M_PI / 6));

    QPolygonF arrowHead;
    arrowHead << p2 << arrowP1 << arrowP2;

    painter->setBrush(Qt::black);
    painter->drawPolygon(arrowHead);
}

QVector<QPointF> Transition::points() const
{
    return m_points;
}

void Transition::setPoints(const QVector<QPointF> &points)
{
    if (m_points != points) {
        m_points = points;
    }
    updatePath();
}

QString Transition::text() const
{
    if (m_text) {
        return m_text->toPlainText();
    }
    return QString("");
}

void Transition::setText(const QString &text)
{
    if (!m_text) {
        m_text = new QGraphicsTextItem(this);
        m_text->setFlag(QGraphicsItem::ItemIsSelectable, true);
        m_text->setFlag(QGraphicsItem::ItemIsFocusable, true);
        m_text->setTextInteractionFlags(Qt::TextEditorInteraction);
//        connect(m_text, &QGraphicsTextItem::textChanged, this, &Edge::onTextChanged);
    }
    m_text->setPlainText(text);
    updateTextPosition();
}

void Transition::setTextPosition(const QPointF &pos)
{
    if (m_textPosition != pos) {
        m_textPosition = pos;
    }
    updateTextPosition();
}

void Transition::updateTextPosition() {
    if (!m_text) return;

    QPointF lastPoint = m_sourcePoint;
    if (!m_points.empty()) {
        lastPoint = m_points.last();
    }
    QPointF textPos = (lastPoint + (m_targetPoint + m_target->sceneBoundingRect().center() - m_source->sceneBoundingRect().center())) / 2 + m_source->sceneBoundingRect().center();

    m_text->setPos(textPos);
    update();
}

QPointF Transition::findIntersectionWithRect(const VERectangle *state)
{
    if (!state) return QPointF();

    QRectF rect = state->sceneBoundingRect();

    QLineF line(m_source->sceneBoundingRect().center(), m_target->sceneBoundingRect().center());

    QPointF intersection;
    QLineF edges[4] = {
        QLineF(rect.topLeft(), rect.topRight()),
        QLineF(rect.topRight(), rect.bottomRight()),
        QLineF(rect.bottomRight(), rect.bottomLeft()),
        QLineF(rect.bottomLeft(), rect.topLeft())
    };

    for (const QLineF& edge : edges) {
        if (line.intersects(edge, &intersection) == QLineF::BoundedIntersection) {
            return intersection - state->sceneBoundingRect().center();
        }
    }

    return QPointF();
}

void Transition::slotMoveSource(QGraphicsItem *item, qreal dx, qreal dy)
{
    updatePath();
    updateTextPosition();
}

void Transition::updateCoordinates(VERectangle *state, VERectangle::CornerFlags side, QPointF *point, QPointF* previousCenterPos)
{
    if (point->x() < 0 && (side & VERectangle::CornerFlags::Left) == VERectangle::CornerFlags::Left) {
        *point = *point + QPointF(state->sceneBoundingRect().center().x() - previousCenterPos->x(), 0);
    }
    if (point->x() > 0 && (side & VERectangle::CornerFlags::Left) == VERectangle::CornerFlags::Left) {
        *point = *point - QPointF(state->sceneBoundingRect().center().x() - previousCenterPos->x(), 0);
    }
    if (point->x() < 0 && (side & VERectangle::CornerFlags::Right) == VERectangle::CornerFlags::Right) {
        *point = *point - QPointF(state->sceneBoundingRect().center().x() - previousCenterPos->x(), 0);
    }
    if (point->x() > 0 && (side & VERectangle::CornerFlags::Right) == VERectangle::CornerFlags::Right) {
        *point = *point + QPointF(state->sceneBoundingRect().center().x() - previousCenterPos->x(), 0);
    }
    if (point->y() < 0 && (side & VERectangle::CornerFlags::Top) == VERectangle::CornerFlags::Top) {
        *point = *point + QPointF(0, state->sceneBoundingRect().center().y() - previousCenterPos->y());
    }
    if (point->y() > 0 && (side & VERectangle::CornerFlags::Top) == VERectangle::CornerFlags::Top) {
    }
    if (point->y() < 0 && (side & VERectangle::CornerFlags::Bottom) == VERectangle::CornerFlags::Bottom) {
        *point = *point - QPointF(0, state->sceneBoundingRect().center().y() - previousCenterPos->y());
    }
    if (point->y() > 0 && (side & VERectangle::CornerFlags::Bottom) == VERectangle::CornerFlags::Bottom) {
        *point = *point + QPointF(0, state->sceneBoundingRect().center().y() - previousCenterPos->y());
    }
}

void Transition::slotStateResized(VERectangle *state, VERectangle::CornerFlags side)
{
    if (state != m_source && state != m_target) return;

    if (state == m_source) {
        updateCoordinates(state, side, &m_sourcePoint, &m_previousSourceCenterPos);
        if (!m_points.isEmpty()) {
            for (int i = 0; i < m_points.size(); ++i) {
                updateCoordinates(state, side, &(m_points[i]), &m_previousSourceCenterPos);
            }
        }
//        updateCoordinates(state, side, &m_textPosition, &m_previousSourceCenterPos);
    }
    if (state == m_target) {
        updateCoordinates(state, side, &m_targetPoint, &m_previousTargetCenterPos);
//        updateCoordinates(state, side, &m_textPosition, &m_previousSourceCenterPos);
    }

    updatePath();
    updateTextPosition();
}

void Transition::slotMove(QGraphicsItem *signalOwner, qreal dx, qreal dy)
{
    QPainterPath linePath = path();
    for(int i = 0; i < linePath.elementCount(); i++){
        if(m_listDotes.at(i) == signalOwner){
            qDebug() << i << "/" << linePath.elementCount();
            if(i == 0) {
                qDebug() << "sourse DotPoint";
                break;
            }
            if(i == linePath.elementCount() - 1) {
                qDebug() << "target DotPoint";
                break;
            }
            m_points[i - 1] = QPointF(m_points[i - 1].x() + dx, m_points[i - 1].y() + dy);
//            m_pointForCheck = i;
        }
    }
    updatePath();
    updateTextPosition();
}

void Transition::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    QPointF clickPos = event->pos();
    QLineF checkLineFirst(clickPos.x() - 5, clickPos.y() - 5, clickPos.x() + 5, clickPos.y() + 5);
    QLineF checkLineSecond(clickPos.x() + 5, clickPos.y() - 5, clickPos.x() - 5, clickPos.y() + 5);
    QPainterPath oldPath = path();
    for(int i = 0; i < oldPath.elementCount() - 1; i++){
        QLineF checkableLine(oldPath.elementAt(i), oldPath.elementAt(i+1));
        if(checkableLine.intersect(checkLineFirst,0) == 1 || checkableLine.intersect(checkLineSecond,0) == 1){
            m_points.insert(i, clickPos - m_source->sceneBoundingRect().center());
        }
    }
    updatePath();
    updateTextPosition();
    updateDots();
    QGraphicsItem::mouseDoubleClickEvent(event);
}

//void Edge::mousePressEvent(QGraphicsSceneMouseEvent *event)
//{
//    if (event->button() & Qt::LeftButton) {
//        m_leftMouseButtonPressed = true;
//        qDebug() << "click on edge";
//        //        setPreviousPosition(event->scenePos());
//        emit clicked(this);
//    }
//    QGraphicsItem::mousePressEvent(event);
//}

//void Edge::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
//{
//    if (m_leftMouseButtonPressed) {
//        qDebug() << "move on edge";
////        auto dx = event->scenePos().x() - m_previousPosition.x();
////        auto dy = event->scenePos().y() - m_previousPosition.y();
////        moveBy(dx,dy);
////        //        setPreviousPosition(event->scenePos());
////        emit signalMove(this, dx, dy);
//    }
//    QGraphicsItem::mouseMoveEvent(event);
//}

//void Edge::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
//{
//    if (event->button() & Qt::LeftButton) {
//        m_leftMouseButtonPressed = false;
//        qDebug() << "release on edge";
//    }
//    QGraphicsItem::mouseReleaseEvent(event);
//}

void Transition::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    qDebug() << "Transition::hoverEnterEvent";
    QPainterPath linePath = path();
    for(int i = 0; i < linePath.elementCount(); i++){
        QPointF point = linePath.elementAt(i);
        DotSignal *dot = new DotSignal(point, this);
        connect(dot, &DotSignal::signalMove, this, &Transition::slotMove);
//        connect(dot, &DotSignal::signalMouseRelease, this, &Edge::checkForDeletePoints);
        dot->setDotFlags(DotSignal::Movable);
        m_listDotes.append(dot);
    }

    QGraphicsItem::hoverEnterEvent(event);
}

void Transition::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsItem::hoverMoveEvent(event);
}

void Transition::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    if(!m_listDotes.isEmpty()){
        foreach (DotSignal *dot, m_listDotes) {
            dot->deleteLater();
        }
        m_listDotes.clear();
    }
    QGraphicsItem::hoverLeaveEvent(event);
}

void Transition::updateDots()
{
    if(!m_listDotes.isEmpty()){
        foreach (DotSignal *dot, m_listDotes) {
            dot->deleteLater();
        }
        m_listDotes.clear();
    }

    QPainterPath linePath = path();
    for(int i = 0; i < linePath.elementCount(); i++){
        QPointF point = linePath.elementAt(i);
        DotSignal *dot = new DotSignal(point, this);
        connect(dot, &DotSignal::signalMove, this, &Transition::slotMove);
//        connect(dot, &DotSignal::signalMouseRelease, this, &Edge::checkForDeletePoints);
        dot->setDotFlags(DotSignal::Movable);
        m_listDotes.append(dot);
    }
}
