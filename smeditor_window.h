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

class QToolBar;
class QComboBox;

class CyberiadaSMEditorWindow: public QMainWindow, public Ui_SMEditorWindow {
Q_OBJECT
public:
    CyberiadaSMEditorWindow(QWidget* parent = 0);

    ~CyberiadaSMEditorWindow();

    bool                    openDocument(const QString& fileName, QString* error = NULL,
                                         bool reconstruct = false, bool reconstruct_sm = false,
                                         bool strict = false);

    CyberiadaSMModel*       getModel() { return model; }
    CyberiadaSMEditorScene* getScene() { return scene; }

    // the unsaved changes prompt: true when the document may go
    bool                    confirmDiscard();

protected:
    void                    closeEvent(QCloseEvent* event) override;
    void                    showEvent(QShowEvent* event) override;

private:
    void                    initializeTools();
    void                    updateTitle();
    // on load: expand the tree fully and widen the right panel to fit its content
    void                    expandAndWidenTree();

private slots:
    void                    slotModelReset();
    // restore the saved viewport once the window/splitter reach their final size
    void                    restoreSavedView();
    void                    slotUndoTextChanged(const QString& text);
    void                    slotRedoTextChanged(const QString& text);
    void                    slotCleanChanged(bool clean);
    void                    slotInspectorModeChanged(bool on);
    void                    slotServiceObjectsChanged(bool on);
    void                    slotLoggingChanged(bool on);
    // enable cut/copy/paste/delete only for a fitting selection (and not in inspector mode)
    void                    updateEditActions();

public slots:
	void                    slotFileNew();
	void                    slotFileOpen();
    void                    slotFileSave();
    void                    slotFileSaveAs();
    void                    slotFileExport();
    void                    slotInspectorModeTriggered(bool on);
    void                    slotShowTransitionActionTriggered(bool on);
    void                    slotSnapModeTriggered(bool on);
    void                    slotToolSelected(QAction *action);
    void                    slotSceneToolChanged(ToolType tool);
    void                    slotFitContent();
    void                    slotZoomToSM();
    void                    slotZoomScaleChanged(qreal scale);
    void                    slotZoomComboActivated();
    void                    slotPreferences();
    void                    slotAbout();
    void                    slotGridVisibilityTriggered(bool on);
    void                    slotServiceObjectsTriggered(bool on);
    void                    slotLogSessionTriggered(bool on);

    void                    slotNewSM();
    void                    slotNewState();
    void                    slotNewInitial();
    void                    slotNewFinal();
    void                    slotNewTerminate();
    void                    slotNewComment();
    void                    slotNewFormalComment();
    void                    slotNewChoise();

    void                    slotDeleteElement();
    void                    slotCopy();
    void                    slotCut();
    void                    slotPaste();

private:
	CyberiadaSMModel*       model;
	CyberiadaSMEditorScene* scene;
    QActionGroup *toolGroup;
    QActionGroup *editGroup;
    ToolType currentTool = ToolType::Select;

    QString openFileName;
    // the saved viewport waiting to be applied after the layout settles
    QString pendingViewState;

    // a detached deep clone of the last copied/cut element (nullptr when empty),
    // and the id of its original parent collection (the paste target level)
    Cyberiada::Element* clipboardElement = nullptr;
public:
    // the batch paste verb guards on this
    bool hasClipboard() const { return clipboardElement != nullptr; }
private:
    Cyberiada::ID clipboardParentId;

    QMap<ToolType, QAction*> toolActMap;
    // the contextual tool-options toolbars, shown only while their tool is active
    QMap<ToolType, QToolBar*> toolOptionBars;
    QToolBar* zoomOptionsToolBar = nullptr;
    QComboBox* zoomCombo = nullptr;
};

#endif
