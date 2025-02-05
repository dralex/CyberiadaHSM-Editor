#ifndef EDITABLETEXTITEM_H
#define EDITABLETEXTITEM_H

#include <QGraphicsTextItem>


class EditableTextItem : public QGraphicsTextItem {
    Q_OBJECT
public:
    explicit EditableTextItem(const QString &text, QGraphicsItem *parent = nullptr, bool align = false, bool parentHasGeometry = true);

    void setFontStyleChangeable(bool isChangeable);
    void setFontBoldness(bool isBold);

protected:
    void focusOutEvent(QFocusEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event);
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;


signals:
    void sizeChanged();

private slots:
    void onFontChanged(const QFont &newFont) ;

protected:
    void setAlign();
    bool isEdit;
    bool align;
    bool parentHasGeometry;

private:
    bool isFontStyleChangeable = true;
    bool isBold = false;
};




#endif // EDITABLETEXTITEM_H
