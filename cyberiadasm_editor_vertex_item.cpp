#include "cyberiadasm_editor_vertex_item.h"

#include <QDebug>
#include <QPainter>
#include <QColor>
#include "myassert.h"

/* -----------------------------------------------------------------------------
 * Vertex Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorVertexItem::CyberiadaSMEditorVertexItem(CyberiadaSMModel* model,
                                                         Cyberiada::Element* element,
                                                         QGraphicsItem* parent):
    CyberiadaSMEditorAbstractItem(model, element, parent)
{
    Cyberiada::Rect r = element->get_bound_rect(*(model->rootDocument()));
    setPos(r.x, r.y);
}

QRectF CyberiadaSMEditorVertexItem::boundingRect() const
{
    return fullCircle();
}

QRectF CyberiadaSMEditorVertexItem::fullCircle() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    return QRectF(- VERTEX_POINT_RADIUS,
                  - VERTEX_POINT_RADIUS,
                  VERTEX_POINT_RADIUS * 2,
                  VERTEX_POINT_RADIUS * 2);
}

QRectF CyberiadaSMEditorVertexItem::partialCircle() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    Cyberiada::Rect r = element->get_bound_rect(*(model->rootDocument()));
    return QRectF(r.x - (VERTEX_POINT_RADIUS * 2.0 / 3.0),
                  r.y - (VERTEX_POINT_RADIUS * 2.0 / 3.0),
                  VERTEX_POINT_RADIUS * 4.0 / 3.0,
                  VERTEX_POINT_RADIUS * 4.0 / 3.0);
}

void CyberiadaSMEditorVertexItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));
    Cyberiada::ElementType type = element->get_type();
    if (type == Cyberiada::elementInitial) {
        painter->setBrush(QBrush(Qt::black));
        painter->drawEllipse(fullCircle());
    } else if (type == Cyberiada::elementFinal) {
        painter->setBrush(painter->background());
        painter->drawEllipse(fullCircle());
        painter->setBrush(QBrush(Qt::black));
        painter->drawEllipse(partialCircle());
    } else {
        MY_ASSERT(type == Cyberiada::elementTerminate);
        painter->setPen(QPen(Qt::black, 2, Qt::SolidLine));
        QRectF r = fullCircle();
        painter->drawLine(r.left(), r.top(), r.right(), r.bottom());
    }
}

