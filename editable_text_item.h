#ifndef EDITABLETEXTITEM_H
#define EDITABLETEXTITEM_H

#include <QGraphicsTextItem>


class EditableTextItem : public QGraphicsTextItem {
public:
    explicit EditableTextItem(const QString &text, QGraphicsItem *parent = nullptr, bool align = false, bool parentHasGeometry = true);

protected:
    void focusOutEvent(QFocusEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event);
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

signals:

protected:
    void setAlign();
    bool isEdit;
    bool align;
    bool parentHasGeometry;
};




#endif // EDITABLETEXTITEM_H
