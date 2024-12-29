#include "view.h"
#include <QWheelEvent>


View::View(Scene *scene, QWidget *parent)
    : QGraphicsView(scene, parent){
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

}


void View::wheelEvent(QWheelEvent *event){
    if (event->modifiers() & Qt::ControlModifier){
        double scaleFactor = 1.1;
        if (event->angleDelta().y() > 0) {
            scale(scaleFactor, scaleFactor);
        } else {
            scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        }
    }
    else {
        QGraphicsView::wheelEvent(event);
    }
}
