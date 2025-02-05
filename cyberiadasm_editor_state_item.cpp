#include "cyberiadasm_editor_state_item.h"

#include <QPainter>
#include <QDebug>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <math.h>

#include "myassert.h"
// #include "grabber.h"
#include "cyberiada_constants.h"


/*
CyberiadaSMEditorStateItem::CyberiadaSMEditorStateItem(QObject *parent) :
    QObject(parent),
    m_cornerFlags(0)
*/

CyberiadaSMEditorStateItem::CyberiadaSMEditorStateItem(QObject *parent_object,
                     CyberiadaSMModel *model,
                     Cyberiada::Element *element,
                     QGraphicsItem *parent) :
    CyberiadaSMEditorAbstractItem(model, element, parent)
{
    // setAcceptHoverEvents(true);
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);

    /*
    for (int i = 0; i < 8; i++){
        cornerGrabber[i] = new Grabber(this);
    }
    setPositionGrabbers();
    */

    m_state = static_cast<const Cyberiada::State*>(element);

    setPos(QPointF(x(), y()));

    title = new EditableTextItem(m_state->get_name().c_str(), this, true);
    title->setFontBoldness(true);

    m_actions = m_state->get_actions();
    std::vector<Cyberiada::Action> actions = m_state->get_actions();
    for (std::vector<Cyberiada::Action>::const_iterator i = actions.begin(); i != actions.end(); i++) {
        Cyberiada::ActionType type = i->get_type();
        if (type == Cyberiada::actionEntry) {
            entry = new EditableTextItem(QString("entry() / ") + QString(i->get_behavior().c_str()), this);
            connect(entry, &EditableTextItem::sizeChanged, this, &CyberiadaSMEditorStateItem::onTextItemSizeChanged);
        } else if (type == Cyberiada::actionExit) {
            exit = new EditableTextItem(QString("exit() / ") + QString(i->get_behavior().c_str()), this);
            connect(exit, &EditableTextItem::sizeChanged, this, &CyberiadaSMEditorStateItem::onTextItemSizeChanged);
        }
    }

    if (m_state->is_composite_state()) {
        connect(title, &EditableTextItem::sizeChanged, this, &CyberiadaSMEditorStateItem::onTextItemSizeChanged);
        m_area = new StateArea(this);
        updateArea();
    }

    setPositionText();
}

CyberiadaSMEditorStateItem::~CyberiadaSMEditorStateItem()
{
    // for(int i = 0; i < 8; i++){
    //     delete cornerGrabber[i];
    // }
}

QPointF CyberiadaSMEditorStateItem::previousPosition() const
{
    return m_previousPosition;
}


void CyberiadaSMEditorStateItem::setPreviousPosition(const QPointF previousPosition)
{
    if (m_previousPosition == previousPosition)
        return;

    m_previousPosition = previousPosition;
    emit previousPositionChanged();
}

void CyberiadaSMEditorStateItem::setRect(qreal x, qreal y, qreal w, qreal h)
{
    setRect(QRectF(x, y, w, h));
}

void CyberiadaSMEditorStateItem::setRect(const QRectF &rect)
{
    prepareGeometryChange();
    /*
    if (rect.width() >= 320 && rect.height() >= 180){
        QGraphicsRectItem::setRect(rect);
    }
    QRectF r = rect;
    if (r.height() < 180) {
        r.setHeight(180);
    }
    if (r.width() < 360){
        r.setWidth(360);
    }
    if (r.width() >= 320 && r.height() >= 180) {

    QGraphicsRectItem::setRect(rect);
    }
*/
    m_rect = rect;
}

QRectF CyberiadaSMEditorStateItem::rect() const {
    Cyberiada::Rect model_rect = m_state->get_geometry_rect();
    QRectF rect = QRectF(-model_rect.width / 2, -model_rect.height / 2, model_rect.width, model_rect.height);
    return rect;
}

qreal CyberiadaSMEditorStateItem::x() const
{
    Cyberiada::Rect model_rect = m_state->get_geometry_rect();
    return model_rect.x;
}

qreal CyberiadaSMEditorStateItem::y() const
{
    Cyberiada::Rect model_rect = m_state->get_geometry_rect();
    return model_rect.y;
}

qreal CyberiadaSMEditorStateItem::width() const
{
    Cyberiada::Rect model_rect = m_state->get_geometry_rect();
    return model_rect.width;
}

qreal CyberiadaSMEditorStateItem::height() const
{
    Cyberiada::Rect model_rect = m_state->get_geometry_rect();
    return model_rect.height;
}

StateArea *CyberiadaSMEditorStateItem::getArea()
{
    return m_area;
}

void CyberiadaSMEditorStateItem::updateArea()
{
    qreal top_delta = title->boundingRect().height();
    qreal bottom_delta = 0;

    if (entry) {
        top_delta += entry->boundingRect().height();
        m_area->setTopLine(true);
    }
    if (exit) {
        bottom_delta += entry->boundingRect().height();
        m_area->setBottomLine(true);
    }

    m_area->setRect(-width()/2, -(height() - top_delta - bottom_delta) / 2, width(), height() - top_delta - bottom_delta);
    m_area->setPos(0, (top_delta - bottom_delta)/2 );
}

QRectF CyberiadaSMEditorStateItem::boundingRect() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    return rect();
}


void CyberiadaSMEditorStateItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() & Qt::LeftButton) {
        m_leftMouseButtonPressed = true;
        setPreviousPosition(event->scenePos());
        emit clicked(this);
    }
    QGraphicsItem::mousePressEvent(event);
}

void CyberiadaSMEditorStateItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    /*
    QPointF pt = event->pos();
    switch (m_cornerFlags) {
    case Top:
        resizeTop(pt);
        break;
    case Bottom:
        resizeBottom(pt);
        break;
    case Left:
        resizeLeft(pt);
        break;
    case Right:
        resizeRight(pt);
        break;
    case TopLeft:
        resizeTop(pt);
        resizeLeft(pt);
        break;
    case TopRight:
        resizeTop(pt);
        resizeRight(pt);
        break;
    case BottomLeft:
        resizeBottom(pt);
        resizeLeft(pt);
        break;
    case BottomRight:
        resizeBottom(pt);
        resizeRight(pt);
        break;
    default:
    */
    if (m_leftMouseButtonPressed) {
        setCursor(Qt::ClosedHandCursor);
        // setFlag(ItemIsMovable);
    }
    // break;
    // }

    QGraphicsItem::mouseMoveEvent(event);
    emit geometryChanged();
}

void CyberiadaSMEditorStateItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() & Qt::LeftButton) {
        m_leftMouseButtonPressed = false;
        setFlag(ItemIsMovable, false);
    }
    QGraphicsItem::mouseReleaseEvent(event);
}


void CyberiadaSMEditorStateItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    // setPositionGrabbers();
    // showGrabbers();
    QGraphicsItem::hoverEnterEvent(event);
}

void CyberiadaSMEditorStateItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    // m_cornerFlags = 0;
    // hideGrabbers();
    setCursor(Qt::ArrowCursor);
    QGraphicsItem::hoverLeaveEvent( event );
}

void CyberiadaSMEditorStateItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    /*
    QPointF pt = event->pos();              // The current position of the mouse
    qreal drx = pt.x() - rect().right();    // Distance between the mouse and the right
    qreal dlx = pt.x() - rect().left();     // Distance between the mouse and the left

    qreal dby = pt.y() - rect().top();      // Distance between the mouse and the top
    qreal dty = pt.y() - rect().bottom();   // Distance between the mouse and the bottom

    If the mouse position is within a radius of 7
    to a certain side( top, left, bottom or right)
    we set the Flag in the Corner Flags Register

    m_cornerFlags = 0;
    if( dby < 7 && dby > -7 ) m_cornerFlags |= Top;       // Top side
    if( dty < 7 && dty > -7 ) m_cornerFlags |= Bottom;    // Bottom side
    if( drx < 7 && drx > -7 ) m_cornerFlags |= Right;     // Right side
    if( dlx < 7 && dlx > -7 ) m_cornerFlags |= Left;      // Left side

    switch (m_cornerFlags) {
    case Top:
    case Bottom:
        setCursor(QCursor(Qt::SizeVerCursor));
        break;

    case Left:
    case Right:
        setCursor(QCursor(Qt::SizeHorCursor));
        break;

    case TopRight:
    case BottomLeft:
        setCursor(QCursor(Qt::SizeBDiagCursor));
        break;

    case TopLeft:
    case BottomRight:
        setCursor(QCursor(Qt::SizeFDiagCursor));
        break;

    default:
*/
    setCursor(Qt::OpenHandCursor);
        // break;
    // }
    QGraphicsItem::hoverMoveEvent(event);
}

/*
void Rectangle::resizeLeft(const QPointF &pt)
{
    QRectF tmpRect = rect();
    // if the mouse is on the right side we return
    if( pt.x() > tmpRect.right() )
        return;
    qreal widthOffset =  ( pt.x() - tmpRect.right() );
    // limit the minimum width
    if( widthOffset > -10 )
        return;
    // if it's negative we set it to a positive width value
    if( widthOffset < 0 )
        tmpRect.setWidth( -widthOffset );
    else
        tmpRect.setWidth( widthOffset );
    // Since it's a left side , the rectange will increase in size
    // but keeps the topLeft as it was
    tmpRect.translate( rect().width() - tmpRect.width() , 0 );
    prepareGeometryChange();
    // Set the ne geometry
    setRect( tmpRect );
    // Update to see the result
    update();
    // setPositionGrabbers();
}
*/

/*
void Rectangle::resizeRight(const QPointF &pt)
{
    QRectF tmpRect = rect();
    if( pt.x() < tmpRect.left() )
        return;
    qreal widthOffset =  ( pt.x() - tmpRect.left() );
    if( widthOffset < 10 ) /// limit
        return;
    if( widthOffset < 10)
        tmpRect.setWidth( -widthOffset );
    else
        tmpRect.setWidth( widthOffset );
    prepareGeometryChange();
    setRect( tmpRect );
    update();
    // setPositionGrabbers();
}
*/

/*
void Rectangle::resizeBottom(const QPointF &pt)
{
    QRectF tmpRect = rect();
    if( pt.y() < tmpRect.top() )
        return;
    qreal heightOffset =  ( pt.y() - tmpRect.top() );
    if( heightOffset < 11 ) /// limit
        return;
    if( heightOffset < 0)
        tmpRect.setHeight( -heightOffset );
    else
        tmpRect.setHeight( heightOffset );
    prepareGeometryChange();
    setRect( tmpRect );
    update();
    // setPositionGrabbers();
}
*/

/*
void CyberiadaSMEditorStateItem::resizeTop(const QPointF &pt)
{
    QRectF tmpRect = rect();
    if( pt.y() > tmpRect.bottom() )
        return;
    qreal heightOffset =  ( pt.y() - tmpRect.bottom() );
    if( heightOffset > -11 ) /// limit
        return;
    if( heightOffset < 0)
        tmpRect.setHeight( -heightOffset );
    else
        tmpRect.setHeight( heightOffset );
    tmpRect.translate( 0 , rect().height() - tmpRect.height() );
    prepareGeometryChange();
    setRect( tmpRect );
    update();
    // setPositionGrabbers();
}
*/

void CyberiadaSMEditorStateItem::setPositionText()
{
    QRectF oldRect = rect();
    QRectF titleRect = title->boundingRect();
    title->setPos(oldRect.x() + (oldRect.width() - titleRect.width()) / 2 , oldRect.y());

    // simple state
    if (m_state->is_simple_state()) {
        if (entry != nullptr && exit != nullptr) {
            float delta = (entry->boundingRect().height() + exit->boundingRect().height());
            entry->setPos(oldRect.x() + 15, oldRect.y() + (height() + titleRect.height() - delta) / 2);
            exit->setPos(entry->pos() + QPointF(0, entry->boundingRect().height()));
            return;
        }
        if (entry != nullptr) {
            entry->setPos(oldRect.x() + 15, oldRect.y() + (height() + titleRect.height() - entry->boundingRect().height()) / 2);
        }
        if (exit != nullptr) {
            exit->setPos(oldRect.x() + 15, oldRect.y() + (height() + titleRect.height() - exit->boundingRect().height()) / 2);
        }
        return;
    }

    // composite state
    if (entry != nullptr) {
        entry->setPos(oldRect.x() + 15, oldRect.y() + titleRect.height());
    }
    if (exit != nullptr) {
        exit->setPos(oldRect.x() + 15, oldRect.bottom() - exit->boundingRect().height());
    }
    // setPositionGrabbers();
}

void CyberiadaSMEditorStateItem::onTextItemSizeChanged()
{
    if (m_state->is_composite_state()) updateArea();
    setPositionText();
}

/*
void Rectangle::setPositionGrabbers()
{
    QRectF tmpRect = rect();
    cornerGrabber[GrabberTop]->setPos(tmpRect.left() + tmpRect.width()/2, tmpRect.top());
    cornerGrabber[GrabberBottom]->setPos(tmpRect.left() + tmpRect.width()/2, tmpRect.bottom());
    cornerGrabber[GrabberLeft]->setPos(tmpRect.left(), tmpRect.top() + tmpRect.height()/2);
    cornerGrabber[GrabberRight]->setPos(tmpRect.right(), tmpRect.top() + tmpRect.height()/2);
    cornerGrabber[GrabberTopLeft]->setPos(tmpRect.topLeft().x(), tmpRect.topLeft().y());
    cornerGrabber[GrabberTopRight]->setPos(tmpRect.topRight().x(), tmpRect.topRight().y());
    cornerGrabber[GrabberBottomLeft]->setPos(tmpRect.bottomLeft().x(), tmpRect.bottomLeft().y());
    cornerGrabber[GrabberBottomRight]->setPos(tmpRect.bottomRight().x(), tmpRect.bottomRight().y());
}
*/

/*
void Rectangle::showGrabbers()
{
    for(int i = 0; i < 8; i++){
        cornerGrabber[i]->setVisible(true);
    }
}
*/

/*
void CyberiadaSMEditorStateItem::hideGrabbers()
{
    for(int i = 0; i < 8; i++){
        cornerGrabber[i]->setVisible(false);
    }
}
*/

void CyberiadaSMEditorStateItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget){
    Q_UNUSED(option)
    Q_UNUSED(widget)

    // setPositionText();------------------
    // QRectF oldRect = rect();
    // setRect(QRectF(oldRect.x(), oldRect.y(), oldRect.width(), title->boundingRect().height() ));
    qreal titleHeight = title->boundingRect().height();

    QColor color(Qt::black);
    if (isSelected()) {
        color.setRgb(255, 0, 0);
    }
    painter->setPen(QPen(color, 2, Qt::SolidLine));

    QPainterPath path;
    QRectF tmpRect = rect();
    path.addRoundedRect(tmpRect, ROUNDED_RECT_RADIUS, ROUNDED_RECT_RADIUS);
    painter->drawLine(QPointF(tmpRect.x(), tmpRect.y() + titleHeight), QPointF(tmpRect.right(), tmpRect.y() + titleHeight));
    painter->drawPath(path);

    painter->setBrush(Qt::red);
    painter->drawEllipse(QPointF(0, 0), 2, 2); // Центр системы координат
}
