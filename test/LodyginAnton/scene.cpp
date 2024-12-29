#include "scene.h"
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneWheelEvent>
#include <QPointF>
#include <QColor>
#include <iostream>


Scene::Scene(QObject *parent)
    : QGraphicsScene(parent),
    activeItem(nullptr),
    start_pos(nullptr),
    color(nullptr){
    objectType = 0;
}


void Scene::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsScene::mousePressEvent(event);
    start_pos = new QPointF(event->scenePos());
    if (objectType == 0) {
        update();
        QGraphicsItem *item = itemAt(event->scenePos(), QTransform());
        if (item) {
            activeItem = item;
            activeItem->setFlags(QGraphicsItem::GraphicsItemFlag::ItemIsMovable | QGraphicsItem::GraphicsItemFlag::ItemIsSelectable | QGraphicsItem::GraphicsItemFlag::ItemIsFocusable);
        } else {
            std::cout << "noitem\n";
        }
    } else if (objectType == 1) {
        activeItem = new QGraphicsRectItem(start_pos->x(), start_pos->y(), 0, 0);
        int r, g, b;
        r = rand() % 256;
        g  = rand() % 256;
        b  = rand() % 256;
        color = new QColor(r, g, b, 180);
        addItem(activeItem);
    } else if (objectType == 2) {
        activeItem = new QGraphicsLineItem(start_pos->x(), start_pos->y(), start_pos->x(), start_pos->y());
        addItem(activeItem);
    }

}


void Scene::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsScene::mouseMoveEvent(event);

    if (start_pos == nullptr) {
        return;
    }

    if (objectType != 0) {
        removeItem(activeItem);
    }

    QPointF pos = event->scenePos();
    auto x1 = start_pos->x();
    auto y1 = start_pos->y();
    auto x2 = pos.x();
    auto y2 = pos.y();

    if (objectType == 1) {
        if (x2 >= x1 && y2 >= y1) {
            activeItem = new QGraphicsRectItem(x1, y1, x2 - x1, y2 - y1);
        } else if (x2 >= x1 && y2 <= y1) {
            activeItem = new QGraphicsRectItem(x1, y2, x2 - x1, y1 - y2);
        } else if (x2 <= x1 && y2 >= y1) {
            activeItem = new QGraphicsRectItem(x2, y1, x1 - x2, y2 - y1);
        } else if (x2 <= x1 && y2 <= y1) {
            activeItem = new QGraphicsRectItem(x2, y2, x1 - x2, y1 - y2);
        }
        static_cast<QGraphicsRectItem*>(activeItem)->setBrush(*color);
        addItem(activeItem);
    } else if (objectType == 2) {
        activeItem = new QGraphicsLineItem(x1, y1, x2, y2);
        addItem(activeItem);
    }
}


void Scene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsScene::mouseReleaseEvent(event);

    if (objectType != 0) {
        removeItem(activeItem);
    }

    QPointF pos = event->scenePos();
    auto x1 = start_pos->x();
    auto y1 = start_pos->y();
    auto x2 = pos.x();
    auto y2 = pos.y();

    if (objectType == 0) {
        QGraphicsItem *item = itemAt(event->scenePos(), QTransform());
        if (item) {
            activeItem->setFlags(item->flags() & ~QGraphicsItem::GraphicsItemFlag::ItemIsMovable);
        }
    } else if (objectType == 1) {
        if (x2 >= x1 && y2 >= y1) {
            activeItem = new QGraphicsRectItem(x1, y1, x2 - x1, y2 - y1);
        } else if (x2 >= x1 && y2 <= y1) {
            activeItem = new QGraphicsRectItem(x1, y2, x2 - x1, y1 - y2);
        } else if (x2 <= x1 && y2 >= y1) {
            activeItem = new QGraphicsRectItem(x2, y1, x1 - x2, y2 - y1);
        } else if (x2 <= x1 && y2 <= y1) {
            activeItem = new QGraphicsRectItem(x2, y2, x1 - x2, y1 - y2);
        }
        color->setAlpha(255);
        static_cast<QGraphicsRectItem*>(activeItem)->setBrush(*color);
        addItem(activeItem);
    } else if (objectType == 2){
        activeItem = new QGraphicsLineItem(x1, y1, x2, y2);
        addItem(activeItem);

    }
    activeItem = nullptr;
    start_pos = nullptr;
}



void Scene::wheelEvent(QGraphicsSceneWheelEvent *event) {
    activeItem = nullptr;

}


void Scene::cursorBtnClicked() {
    objectType = 0;

}


void Scene::rectBtnClicked() {
    objectType = 1;

}


void Scene::lineBtnClicked() {
    objectType = 2;
}
