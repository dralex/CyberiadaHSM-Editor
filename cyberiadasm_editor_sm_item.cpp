#include <QDebug>
#include <QPainter>
#include <QColor>
#include "cyberiadasm_editor_sm_item.h"
#include "myassert.h"

/* -----------------------------------------------------------------------------
 * State Machine Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorSMItem::CyberiadaSMEditorSMItem(CyberiadaSMModel* model,
                                                 Cyberiada::Element* element,
                                                 QGraphicsItem* parent):
    CyberiadaSMEditorAbstractItem(model, element, parent)
{
    QRectF rect = toQtRect(element->get_bound_rect(*(model->rootDocument())));
}

QRectF CyberiadaSMEditorSMItem::boundingRect() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    QRectF rect = toQtRect(element->get_bound_rect(*(model->rootDocument())));
    return QRectF(rect.x(), rect.y(), rect.width(), rect.height());
}


void CyberiadaSMEditorSMItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));
    QRectF r = boundingRect();
    painter->drawRect(r);
    const QPointF name_frame[] = {
        QPointF(r.left(), r.top()),
        QPointF(r.left() + 50, r.top()),
        QPointF(r.left() + 50, r.top() + 20),
        QPointF(r.left() + 40, r.top() + 30),
        QPointF(r.left(), r.top() + 30)
    };
    painter->drawConvexPolygon(name_frame, 5);
}
