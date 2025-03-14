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

#include "smeditor_window.h"
#include "myassert.h"
#include "fontmanager.h"


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

    initializeTools();

    connect(SMView, SIGNAL(currentIndexActivated(QModelIndex)),
            scene, SLOT(slotElementSelected(QModelIndex)));
}

void CyberiadaSMEditorWindow::slotFileOpen()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open State Machile GraphML file"),
													QDir::currentPath(),
													tr("CyberiadaML graph (*.graphml)"));
	if (!fileName.isEmpty()) {
		model->loadDocument(fileName);
		SMView->setRootIndex(model->rootIndex());
		SMView->expandToDepth(2);
		QModelIndex sm = model->firstSMIndex();
		if (sm.isValid()) {
            scene->loadScene();
			SMView->select(sm);
        }
	}
}

void CyberiadaSMEditorWindow::initializeTools()
{
    toolGroup = new QActionGroup(this);
    toolGroup->addAction(selectToolAction);
    toolGroup->addAction(zoomInAction);
    toolGroup->addAction(zoomOutAction);
    toolGroup->addAction(panAction);

    toolGroup->setExclusive(true);
    selectToolAction->setChecked(true);

    connect(toolGroup, &QActionGroup::triggered, this, &CyberiadaSMEditorWindow::onToolSelected);

    emit toolGroup->triggered(selectToolAction);
}


void CyberiadaSMEditorWindow::on_actionFont_triggered()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, FontManager::instance().getFont());
    if (ok) {
        FontManager::instance().setFont(font);
    }
}

void CyberiadaSMEditorWindow::onToolSelected(QAction *action)
{
    if (action == selectToolAction) {
        currentTool = ToolType::Select;
    } else if (action == zoomInAction) {
        currentTool = ToolType::ZoomIn;
    } else if (action == zoomOutAction) {
        currentTool = ToolType::ZoomOut;
    } else if (action == panAction) {
        currentTool = ToolType::Pan;
    } else {
        currentTool = ToolType::Select;
    }
    sceneView->setCurrentTool(currentTool);
    scene->setCurrentTool(currentTool);
}

void CyberiadaSMEditorWindow::on_fitContentAction_triggered() {
    sceneView->fitInView(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
}
