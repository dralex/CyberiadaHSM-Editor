/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Editor Window
 *
 * Copyright (C) 2024 Alexey Fedoseev <aleksey@fedoseev.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see https://www.gnu.org/licenses/
 *
 * ----------------------------------------------------------------------------- */

#include <QCloseEvent>
#include <QFileDialog>
#include <QDebug>
#include <QDir>
#include <QFontDialog>
#include <QFont>
#include <QMessageBox>

#include "smeditor_window.h"
#include "myassert.h"
#include "fontmanager.h"
#include "dialogs/preferences_dialog.h"
#include "dialogs/open_file_dialog.h"
#include "settings_manager.h"


CyberiadaSMEditorWindow::CyberiadaSMEditorWindow(QWidget* parent):
	QMainWindow(parent)
{
	setupUi(this);
	
	model = new CyberiadaSMModel(this);
	SMView->setModel(model);
	SMView->setRootIndex(model->rootIndex());
	propertiesWidget->setModel(model);
    scene = new CyberiadaSMEditorScene(model, this);
	sceneView->setScene(scene);

    openFileName = QString();
    initializeTools();

    connect(SMView, SIGNAL(currentIndexActivated(QModelIndex)),
            scene, SLOT(slotElementSelected(QModelIndex)));
}

void CyberiadaSMEditorWindow::slotFileOpen()
{
    OpenFileDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) { return; }

    QString fileName = dlg.selectedFile();
    bool inspector = dlg.inspectorModeEnabled();

    if (!fileName.isEmpty()) {
        actionInspectorMode->setChecked(inspector);
        SettingsManager::instance().setInspectorMode(inspector);

        model->loadDocument(fileName);
        SMView->setRootIndex(model->rootIndex());
        SMView->expandToDepth(2);
        QModelIndex sm = model->firstSMIndex();
        if (sm.isValid()) {
            scene->loadScene();
            SMView->select(sm);
        }

       QFileInfo fileInfo(fileName);
       openFileName = fileInfo.fileName();

       if (!openFileName.isEmpty()) {
           if (SettingsManager::instance().getInspectorMode()) {
               setWindowTitle(openFileName + " (inspector mode)");
           } else {
               setWindowTitle(openFileName);
           }
       }
    }
}

void CyberiadaSMEditorWindow::slotFileSave()
{
    if (model->rootDocument() && !model->rootDocument()->get_file_path().empty()) {
        qDebug() << model->rootDocument()->get_file_path().empty();
        model->saveDocument();
    } else {
        slotFileSaveAs();
    }
}

void CyberiadaSMEditorWindow::slotFileSaveAs()
{
    QString selectedFilter;
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save State Machile GraphML file as",
        QDir::currentPath(),
        tr("CyberiadaML graph (*.graphml)"),
        &selectedFilter
        );
    if (fileName.isEmpty()) {
        return;
    }
    if (QFileInfo(fileName).suffix().isEmpty()) {
        if (selectedFilter.contains("CyberiadaML graph (*.graphml)")) fileName += ".graphml";
    }
    model->saveAsDocument(fileName, Cyberiada::DocumentFormat::formatCyberiada10);

    QFileInfo fileInfo(fileName);
    openFileName = fileInfo.fileName();

    if (!openFileName.isEmpty()) {
        if (SettingsManager::instance().getInspectorMode()) {
            setWindowTitle(openFileName + " (inspector mode)");
        } else {
            setWindowTitle(openFileName);
        }
    }
}

void CyberiadaSMEditorWindow::slotFileExport()
{
    QRectF sceneRect = scene->sceneRect();

    if (sceneRect.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Сцена пуста, нечего экспортировать.");
        return;
    }

    QImage image(sceneRect.size().toSize(), QImage::Format_ARGB32);
    image.fill(Qt::white); // или прозрачный фон: Qt::transparent

    QPainter painter(&image);
    scene->render(&painter);
    painter.end();

    QString selectedFilter;
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Экспорт сцены как изображение",
        "",
        "PNG (*.png);;JPEG (*.jpg *.jpeg);;BMP (*.bmp);;TIFF (*.tiff);;Все файлы (*)",
        &selectedFilter
        );

    if (!fileName.isEmpty()) {
        if (QFileInfo(fileName).suffix().isEmpty()) {
            if (selectedFilter.contains("PNG")) fileName += ".png";
            else if (selectedFilter.contains("JPEG")) fileName += ".jpg";
            else if (selectedFilter.contains("BMP")) fileName += ".bmp";
            else if (selectedFilter.contains("TIFF")) fileName += ".tiff";
        }
        if (!image.save(fileName)) {
            QMessageBox::critical(this, "Ошибка", "Не удалось сохранить изображение.");
        }
    }
}

void CyberiadaSMEditorWindow::initializeTools()
{
    toolGroup = new QActionGroup(this);
    toolGroup->addAction(actionSelectTool);
    toolGroup->addAction(actionZoomIn);
    toolGroup->addAction(actionZoomOut);
    toolGroup->addAction(actionPan);

    toolGroup->setExclusive(true);
    actionSelectTool->setChecked(true);

    connect(toolGroup, &QActionGroup::triggered, this, &CyberiadaSMEditorWindow::slotToolSelected);
    emit toolGroup->triggered(actionSelectTool);

    // TODO
    SettingsManager& sm = SettingsManager::instance();

    actionGridVisibility->setChecked(sm.getShowGrid());
    actionTransitionText->setChecked(sm.getShowTransitionText());
    actionInspectorMode->setChecked(sm.getInspectorMode());
    actionSnapMode->setChecked(sm.getSnapMode());
}

void CyberiadaSMEditorWindow::slotFontTriggered()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, FontManager::instance().getFont());
    if (ok) {
        FontManager::instance().setFont(font);
    }
}

void CyberiadaSMEditorWindow::slotToolSelected(QAction *action)
{
    if (action == actionSelectTool) {
        currentTool = ToolType::Select;
    } else if (action == actionZoomIn) {
        currentTool = ToolType::ZoomIn;
    } else if (action == actionZoomOut) {
        currentTool = ToolType::ZoomOut;
    } else if (action == actionPan) {
        currentTool = ToolType::Pan;
    } else {
        currentTool = ToolType::Select;
    }
    sceneView->setCurrentTool(currentTool);
    scene->setCurrentTool(currentTool);
}

void CyberiadaSMEditorWindow::slotFitContent() {
    sceneView->fitInView(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
}

void CyberiadaSMEditorWindow::slotPreferences()
{
    PreferencesDialog dlg(this);
    dlg.exec();
}

void CyberiadaSMEditorWindow::slotGridVisibilityTriggered(bool on)
{
    SettingsManager& sm = SettingsManager::instance();
    sm.setShowGrid(on);
    // scene->enableGrid(on);
}

void CyberiadaSMEditorWindow::slotNewSM()
{
    QMessageBox::information(this, "Информация", QString("Пока не реализовано."));
}

void CyberiadaSMEditorWindow::slotNewState()
{
    scene->addSMItem(Cyberiada::ElementType::elementSimpleState);
    SMView->update();
}

void CyberiadaSMEditorWindow::slotNewInitial()
{
    scene->addSMItem(Cyberiada::ElementType::elementInitial);
}

void CyberiadaSMEditorWindow::slotNewFinal()
{
    scene->addSMItem(Cyberiada::ElementType::elementFinal);
}

void CyberiadaSMEditorWindow::slotNewTerminate()
{
    scene->addSMItem(Cyberiada::ElementType::elementTerminate);
}

void CyberiadaSMEditorWindow::slotNewComment()
{
    scene->addSMItem(Cyberiada::ElementType::elementComment);
}

void CyberiadaSMEditorWindow::slotNewFormalComment()
{
    scene->addSMItem(Cyberiada::ElementType::elementFormalComment);
}

void CyberiadaSMEditorWindow::slotNewChoise()
{
    QMessageBox::information(this, "Информация", QString("Пока не реализовано."));
}

void CyberiadaSMEditorWindow::slotNewTransition()
{
    QMessageBox::information(this, "Информация", QString("Для создания передода зажмите правую кнопку мыши на источкине и протяните к цели!"));
}

void CyberiadaSMEditorWindow::slotDeleteElement()
{
    if (scene->selectedItems().isEmpty()) return;
    CyberiadaSMEditorAbstractItem* itemToDelete = dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene->selectedItems().first());
    if (itemToDelete) {
        scene->deleteItemsRecursively(itemToDelete->getElement());
        return;
    }
    DotSignal* dotToDelete = dynamic_cast<DotSignal*>(scene->focusItem());
    if(dotToDelete) {
        dotToDelete->deleteDot();
        return;
    }
}

void CyberiadaSMEditorWindow::slotInspectorModeTriggered(bool on)
{
    if (on == SettingsManager::instance().getInspectorMode()) { return; }
    SettingsManager::instance().setInspectorMode(on);

    if (openFileName.isEmpty()) { return; }
    if (on) {
        setWindowTitle(openFileName + " (inspector mode)");
    } else {
        setWindowTitle(openFileName);
    }
    // TODO prohibit actions of editing
}

void CyberiadaSMEditorWindow::slotShowTransitionActionTriggered(bool on)
{
    SettingsManager::instance().setShowTransitionText(on);
}

void CyberiadaSMEditorWindow::slotSnapModeTriggered(bool on)
{
    SettingsManager::instance().setSnapMode(on);
}


