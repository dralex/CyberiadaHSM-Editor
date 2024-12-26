#include <QDebug>
#include <QPainter>
#include <QColor>

#include "cyberiadasm_editor_choice_item.h"
#include "myassert.h"

/* -----------------------------------------------------------------------------
 * Choice Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorChoiceItem::CyberiadaSMEditorChoiceItem(CyberiadaSMModel* model,
                                                         Cyberiada::Element* element,
                                                         QGraphicsItem* parent):
    CyberiadaSMEditorAbstractItem(model, element, parent)
{
}

void CyberiadaSMEditorChoiceItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));

    QRectF r = boundingRect();
    const QPointF points[] = {
        QPointF(r.left() + r.width() / 2.0, r.top()),
        QPointF(r.right(), r.top() + r.height() / 2.0),
        QPointF(r.left() + r.width() / 2.0, r.bottom()),
        QPointF(r.left(), r.top() + r.height() / 2.0)
    };

    painter->drawConvexPolygon(points, 4);
}
