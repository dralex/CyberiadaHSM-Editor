#include "scene.h"
#include "view.h"
#include <QApplication>
#include <QGraphicsView>
#include <QPushButton>
#include <iostream>
#include <QWheelEvent>
#include <QMainWindow>
#include <QWidget>
#include <toolbar.h>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QMainWindow window;
    window.setGeometry(250, 150, 1000, 500);

    QWidget widgetScene(&window);
    widgetScene.setGeometry(150, 0, 850, 500);

    Scene scene;
    scene.setSceneRect(0, 0, 850, 500);
    View view(&scene, &widgetScene);

    QWidget widgetToolBar(&window);
    widgetToolBar.setGeometry(0, 0, 150, 500);

    ToolBar toolbar(&widgetToolBar);

    QPushButton cursor_btn {"cursor"};
    toolbar.addWidget(&cursor_btn);
    QObject::connect(&cursor_btn, &QPushButton::clicked, &scene, &Scene::cursorBtnClicked);

    QPushButton rect_btn {"rect"};
    toolbar.addWidget(&rect_btn);
    QObject::connect(&rect_btn, &QPushButton::clicked, &scene, &Scene::rectBtnClicked);

    QPushButton line_btn {"line"};
    toolbar.addWidget(&line_btn);
    QObject::connect(&line_btn, &QPushButton::clicked, &scene, &Scene::lineBtnClicked);

    toolbar.addStretch();

    window.show();
    return a.exec();
}
