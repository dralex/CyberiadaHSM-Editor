#ifndef CYBERIADASM_EDITOR_COMMENT_ITEM_H
#define CYBERIADASM_EDITOR_COMMENT_ITEM_H

#include <QObject>
#include <QBrush>

#include "cyberiadasm_editor_items.h"
#include "editable_text_item.h"

/* -----------------------------------------------------------------------------
 * Comment Item
 * ----------------------------------------------------------------------------- */


class CyberiadaSMEditorCommentItem : public QObject, public CyberiadaSMEditorAbstractItem
{
    Q_OBJECT

public:
    explicit CyberiadaSMEditorCommentItem(QObject *parent_object,
                         CyberiadaSMModel *model,
                         Cyberiada::Element *element,
                         QGraphicsItem *parent,
                         QMap<Cyberiada::ID, QGraphicsItem*> *elementItem);
    ~CyberiadaSMEditorCommentItem();

    virtual int type() const { return CommentItem; }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    QRectF boundingRect() const override;

    void setPositionText();

private:
    EditableTextItem* text;
    QBrush m_commentBrush;

    const Cyberiada::Comment* m_comment;
    QMap<Cyberiada::ID, QGraphicsItem*>* m_elementItem;
};

#endif // CYBERIADASM_EDITOR_COMMENT_ITEM_H
