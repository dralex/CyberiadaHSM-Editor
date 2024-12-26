#include <QDebug>
#include <QPainter>
#include <QColor>
#include "cyberiadasm_editor_comment_item.h"
#include "myassert.h"

/* -----------------------------------------------------------------------------
 * Comment Item
 * ----------------------------------------------------------------------------- */

CyberiadaSMEditorCommentItem::CyberiadaSMEditorCommentItem(QObject *parent_object,
                                                           CyberiadaSMModel *model,
                                                           Cyberiada::Element *element,
                                                           QGraphicsItem *parent,
                                                           QMap<Cyberiada::ID, QGraphicsItem*>* elementItem) :
    CyberiadaSMEditorAbstractItem(model, element, parent),
    QObject(parent_object),
    m_elementItem(elementItem)
{
    m_comment = static_cast<const Cyberiada::Comment*>(element);

    text = new EditableTextItem(m_comment->get_body().c_str(), this);

    m_commentBrush = QBrush(QColor(0xff, 0xcc, 0));
}


CyberiadaSMEditorCommentItem::~CyberiadaSMEditorCommentItem() {

}

QRectF CyberiadaSMEditorCommentItem::boundingRect() const
{
    Cyberiada::Rect r = m_comment->get_geometry_rect();
    return QRectF(- r.width / 2,
                  - r.height / 2,
                  r.width,
                  r.height);
}

void CyberiadaSMEditorCommentItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    setPositionText();

    painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));
    painter->setBrush(m_commentBrush);

    QRectF r = boundingRect();

    const QPointF points[] = {
        QPointF(r.left(), r.top()),
        QPointF(r.right() - COMMENT_ANGLE_CORNER, r.top()),
        QPointF(r.right(), r.top() + COMMENT_ANGLE_CORNER),
        QPointF(r.right(), r.bottom()),
        QPointF(r.left(), r.bottom())
    };

    painter->drawConvexPolygon(points, 5);

    Cyberiada::ElementType type = element->get_type();
    if (type == Cyberiada::elementFormalComment) {
        painter->setBrush(QBrush(Qt::black));
    } else {
        painter->setBrush(m_commentBrush);
    }

    const QPointF triangle[] = {
        QPointF(r.right() - COMMENT_ANGLE_CORNER, r.top()),
        QPointF(r.right(), r.top() + COMMENT_ANGLE_CORNER),
        QPointF(r.right()- COMMENT_ANGLE_CORNER, r.top() + COMMENT_ANGLE_CORNER)
    };

    painter->drawConvexPolygon(points, 3);
}

void CyberiadaSMEditorCommentItem::setPositionText()
{
    QRectF oldRect = boundingRect();
    QRectF titleRect = text->boundingRect();
    text->setPos(oldRect.x() + (oldRect.width() - titleRect.width()) / 2 , oldRect.y());
}

