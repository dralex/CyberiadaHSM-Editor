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

    setFlags(ItemIsSelectable);
}

QRectF CyberiadaSMEditorSMItem::boundingRect() const
{
    MY_ASSERT(model);
    MY_ASSERT(model->rootDocument());
    MY_ASSERT(element);
    Cyberiada::Rect r = element->get_bound_rect(*(model->rootDocument()));
    QRectF rect = toQtRect(r);
    return QRectF(rect.x(), rect.y(), rect.width(), rect.height());
}


void CyberiadaSMEditorSMItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    if (!element->has_geometry()) return;

    QColor color(Qt::black);
    if (isSelected()) {
        color.setRgb(255, 0, 0);
    }

    painter->setPen(QPen(color, 1, Qt::SolidLine));
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
