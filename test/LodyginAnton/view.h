#ifndef VIEW_H
#define VIEW_H

#include <QGraphicsView>
#include <scene.h>

class View : public QGraphicsView {
    Q_OBJECT
public:
    View(Scene *scene, QWidget *parent = nullptr);
protected:
    void wheelEvent(QWheelEvent *event);

};

#endif // VIEW_H
