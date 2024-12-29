#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSvgGenerator>
#include <QFileDialog>
#include <QDebug>
#include <QGraphicsItem>
#include <QMessageBox>
#include "svgreader.h"
#include "veworkplace.h"
#include "vepolyline.h"
#include "verectangle.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    workplaceScene = new VEWorkplace(this);
    workplaceScene->setSceneRect(0,0,1000,700);
    ui->workplaceView->setScene(workplaceScene);
    ui->workplaceView->setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    ui->workplaceView->setDragMode(QGraphicsView::ScrollHandDrag);
    ui->workplaceView->setCursor(QCursor());
    ui->splitter->setStretchFactor(0,0);
    ui->splitter->setStretchFactor(1,1);
    ui->rectangleSettings->setVisible(false);
    ui->polylineSettings->setVisible(false);
    connect(ui->actionHelp, &QAction::triggered, this, &MainWindow::slotHelp);
    connect(ui->butLine, &QToolButton::clicked, [=](){workplaceScene->setCurrentAction(VEWorkplace::TransitionType);});
    connect(ui->butRectangle, &QToolButton::clicked, [=](){workplaceScene->setCurrentAction(VEWorkplace::RectangleType);});
    connect(ui->butDefault, &QToolButton::clicked, [=](){workplaceScene->setCurrentAction(VEWorkplace::DefaultType);});
    connect(workplaceScene, &VEWorkplace::selectionChanged, this, &MainWindow::checkSelection);
    connect(workplaceScene, &VEWorkplace::currentActionChanged, this, &MainWindow::checkActionStates);
    connect(workplaceScene, &VEWorkplace::signalSelectItem, this, &MainWindow::selectItem);
    connect(workplaceScene, &VEWorkplace::signalNewSelectItem, this, &MainWindow::selectNewItem);
}

MainWindow::~MainWindow()
{
    delete ui;
    delete workplaceScene;
}

void MainWindow::on_butSave_clicked()
{
    QString newPath = QFileDialog::getSaveFileName(this, trUtf8("Save SVG"),
        path, tr("SVG files (*.svg)"));

    if (newPath.isEmpty())
        return;

    path = newPath;

    QSvgGenerator generator;
    generator.setFileName(path);
    generator.setSize(QSize(workplaceScene->width(), workplaceScene->height()));
    generator.setViewBox(QRect(0, 0, workplaceScene->width(), workplaceScene->height()));
    generator.setTitle(trUtf8("Vector Editor"));
    generator.setDescription(trUtf8("File created by Vector Editor."));

    QPainter painter;
    painter.begin(&generator);
    workplaceScene->render(&painter);
    painter.end();
}

void MainWindow::on_butOpen_clicked()
{
    QString newPath = QFileDialog::getOpenFileName(this, trUtf8("Open SVG"),
                                                   path, tr("SVG files (*.svg)"));
    if (newPath.isEmpty())
        return;

    path = newPath;
    workplaceScene->clear();

    workplaceScene->setSceneRect(SvgReader::getSizes(path));

    foreach (QGraphicsItem *item, SvgReader::getElements(path)) {
        switch (item->type()) {
        case QGraphicsPathItem::Type: {
            VEPolyline *polyline = qgraphicsitem_cast<VEPolyline*>(item);
            workplaceScene->addItem(polyline);
            connect(polyline, &VEPolyline::clicked, workplaceScene, &VEWorkplace::signalSelectItem);
            connect(polyline, &VEPolyline::signalMove, workplaceScene, &VEWorkplace::slotMove);
            break;
        }
        case QGraphicsRectItem::Type: {
            VERectangle *rect = qgraphicsitem_cast<VERectangle*>(item);
            workplaceScene->addItem(rect);
            connect(rect, &VERectangle::clicked, workplaceScene, &VEWorkplace::signalSelectItem);
            connect(rect, &VERectangle::signalMove, workplaceScene, &VEWorkplace::slotMove);
            break;
        }
        default:
            break;
        }
    }
}

void MainWindow::checkSelection()
{
    checkActionStates();
    switch (workplaceScene->selectedItems().length()) {
    case 0:
        ui->rectangleSettings->deselect();
        ui->polylineSettings->deselect();
        checkActionStates();
        ui->toolsWidget->setEnabled(true);
        break;
    case 1:
        switch (workplaceScene->selectedItems().at(0)->type()) {
        case QGraphicsRectItem::Type:{
            ui->rectangleSettings->setVisible(true);
            ui->polylineSettings->deselect();
            ui->polylineSettings->setVisible(false);
            break;
        }
        default: {
            ui->rectangleSettings->deselect();
            ui->rectangleSettings->setVisible(false);
            ui->polylineSettings->setVisible(true);
            break;
        }
        }
        checkActionStates();
        ui->toolsWidget->setEnabled(true);
        break;
    default:
        ui->rectangleSettings->deselect();
        ui->polylineSettings->deselect();
        ui->toolsWidget->setEnabled(false);
        break;
    }
}

void MainWindow::checkActionStates()
{
    ui->workplaceView->setDragMode(QGraphicsView::NoDrag);
    ui->rectangleSettings->setVisible(false);
    ui->polylineSettings->setVisible(false);
    switch (workplaceScene->currentAction()){
    case VEWorkplace::LineType:
        ui->polylineSettings->setVisible(true);
        break;
    case VEWorkplace::RectangleType:
        ui->rectangleSettings->setVisible(true);
        break;
    case VEWorkplace::TransitionType:
        ui->polylineSettings->setVisible(true);
        break;
    case VEWorkplace::SelectionType:
        break;
    default:
        ui->workplaceView->setDragMode(QGraphicsView::ScrollHandDrag);
        break;
    }
}

void MainWindow::selectItem(QGraphicsItem *item)
{
    switch (item->type()) {
    case QGraphicsRectItem::Type: {
        VERectangle *rect = qgraphicsitem_cast<VERectangle *>(item);
        ui->rectangleSettings->loadRectangle(rect);
        break;
    }
    case QGraphicsPathItem::Type: {
        VEPolyline *polyline = qgraphicsitem_cast<VEPolyline *>(item);
        ui->polylineSettings->loadPolyline(polyline);
        break;
    }
    default:
        break;
    }
}

void MainWindow::selectNewItem(QGraphicsItem *item)
{
    switch (item->type()) {
    case QGraphicsRectItem::Type: {
        VERectangle *rect = qgraphicsitem_cast<VERectangle *>(item);
        ui->rectangleSettings->newRectangle(rect);
        break;
    }
    case QGraphicsPathItem::Type: {
        VEPolyline *polyline = qgraphicsitem_cast<VEPolyline *>(item);
        ui->polylineSettings->newPolyline(polyline);
        break;
    }
    default:
        break;
    }
}

void MainWindow::slotHelp()
{
    QMessageBox::information(this, trUtf8("Справка"),
                             trUtf8("Простой векторный редактор для работы с ломанными линиями и прямоугольниками.\n"
                                    "Частично поддерживает формат SVG - прямоугольники и полилинии.\n"
                                    "\n"
                                    "Горячие клавиши:\n"
                                    "Shift+ЛКМ+Перемещение Мыши - Выделение;\n"
                                    "Del - удаление выделенных объектов;\n"
                                    "Ctrl+A - выделение всех объектов.\n"
                                    "\n"
                                    "Двойной клик по прямоугольнику изменяет режим трансформаций.\n"
                                    "Прямоугольники поддерживают изменение размера и поворот."));
}
