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

#ifndef CYBERIADA_SM_WINDOW
#define CYBERIADA_SM_WINDOW

#include <QMainWindow>
#include "ui_smeditor_window.h"
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"

class CyberiadaSMEditorWindow: public QMainWindow, public Ui_SMEditorWindow {
Q_OBJECT
public:
    CyberiadaSMEditorWindow(QWidget* parent = 0);

private:
    void                    initializeTools();

public slots:
	void                    slotFileOpen();
    void                    slotFileSave();
    void                    slotFileSaveAs();
    void                    slotFileExport();
    void                    slotInspectorModeTriggered(bool on);
    void                    slotShowTransitionActionTriggered(bool on);
    void                    slotSnapModeTriggered(bool on);
    void                    slotFontTriggered();
    void                    slotToolSelected(QAction *action);
    void                    slotFitContent();
    void                    slotPreferences();
    void                    slotGridVisibilityTriggered(bool on);

    void                    slotNewSM();
    void                    slotNewState();
    void                    slotNewInitial();
    void                    slotNewFinal();
    void                    slotNewTerminate();
    void                    slotNewComment();
    void                    slotNewFormalComment();
    void                    slotNewChoise();
    void                    slotNewTransition();

    void                    slotDeleteElement();

private:
	CyberiadaSMModel*       model;
	CyberiadaSMEditorScene* scene;
    QActionGroup *toolGroup;
    ToolType currentTool = ToolType::Select;

    QString openFileName;

    QMap<ToolType, QAction*> toolActMap;
};

#endif
