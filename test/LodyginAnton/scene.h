#ifndef SCENE_H
#define SCENE_H

#include <QGraphicsScene>


class Scene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit Scene(QObject *parent = nullptr);
    void rectBtnClicked();
    void lineBtnClicked();
    void cursorBtnClicked();
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void wheelEvent(QGraphicsSceneWheelEvent *event) override;
    QGraphicsItem *activeItem;
    QPointF *start_pos;
    QColor *color;
private:
    int objectType;
};

#endif // SCENE_H
