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
#include "smeditor_window.h"
#include "myassert.h"

CyberiadaSMEditorWindow::CyberiadaSMEditorWindow(QWidget* parent):
	QMainWindow(parent)
{
	model = new CyberiadaSMModel(this);
	setupUi(this);
	SMView->setModel(model);
	SMView->setRootIndex(model->rootIndex());
}

void CyberiadaSMEditorWindow::slotFileOpen()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open State Machile GraphML file"),
													QDir::currentPath(),
													tr("CyberiadaML graph (*.graphml)"));
	model->loadGraph(fileName);
	SMView->setRootIndex(model->rootIndex());	
}
