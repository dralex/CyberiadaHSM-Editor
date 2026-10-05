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
#include <QList>
#include "ui_smeditor_window.h"
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_editor_document.h"

class QToolBar;
class QComboBox;
class QMenu;
class QUndoGroup;
class QTabBar;
class QActionGroup;

// The window holds the documents (docs/WINDOW.md): the menus, the toolbars,
// the clipboard and the armed tool are global; the structure tree, the
// properties panel and the actions target the active document.
class CyberiadaSMEditorWindow: public QMainWindow, public Ui_SMEditorWindow {
Q_OBJECT
public:
    CyberiadaSMEditorWindow(QWidget* parent = 0);

    ~CyberiadaSMEditorWindow();

    // load the file into the active document (the batch and test contract)
    bool                    openDocument(const QString& fileName, QString* error = NULL,
                                         bool reconstruct = false, bool reconstruct_sm = false,
                                         bool strict = false);
    // the tab policy (EDIT-DOC-2): an open file activates its tab, a clean untitled
    // active document takes the file, anything else opens a new tab
    bool                    openFile(const QString& fileName, QString* error = NULL,
                                     bool reconstruct = false, bool reconstruct_sm = false,
                                     bool strict = false);
    // the command-line files, in order, the last one active; the failures are
    // collected in error and skipped
    bool                    openFiles(const QStringList& fileNames, QString* error = NULL,
                                      bool reconstruct = false, bool reconstruct_sm = false,
                                      bool strict = false);
    // a new untitled document in a new tab
    CyberiadaSMEditorDocument* newDocument();
    void                    setCurrentDocument(CyberiadaSMEditorDocument* doc);
    // the close prompt first (skipped with discard); the last document is
    // replaced by a fresh untitled one
    bool                    closeDocument(CyberiadaSMEditorDocument* doc, bool discard = false);

    // the active document and its parts
    CyberiadaSMEditorDocument* currentDocument() const { return current; }
    CyberiadaSMModel*       getModel() { return model(); }
    CyberiadaSMEditorScene* getScene() { return scene(); }
    int                     documentCount() const { return documents.size(); }
    CyberiadaSMEditorDocument* documentAt(int i) const { return documents.value(i, nullptr); }

    // the unsaved changes prompt: true when the document may go
    bool                    confirmDiscard();
    bool                    confirmDiscard(CyberiadaSMEditorDocument* doc);

    // the active document's canvas view (the tests address it by name)
    CyberiadaSMGraphicsView* sceneView = nullptr;
    QTabBar*                tabBar() const { return documentTabs; }

protected:
    void                    closeEvent(QCloseEvent* event) override;
    void                    showEvent(QShowEvent* event) override;

private:
    CyberiadaSMModel*       model() const { return current ? current->model() : nullptr; }
    CyberiadaSMEditorScene* scene() const { return current ? current->scene() : nullptr; }

    void                    initializeTools();
    void                    updateTitle();
    // on load: expand the tree fully and widen the right panel to fit its content
    void                    expandAndWidenTree();
    // the File > Open Recent submenu, rebuilt from the stored recent-files list
    void                    rebuildRecentMenu();
    void                    openRecentFile(const QString& path);

    // the tab text: the title with the modified marker, the path as the tooltip
    void                    updateTabTitle(CyberiadaSMEditorDocument* doc);
    // Edit > Language (EDIT-TEXT-9): the fixed entries, the check of the active
    // document's value, and the unknown values kept for the session
    void                    initLanguageMenu();
    QAction*                addLanguageAction(const QString& text, const QString& value);
    void                    syncLanguageMenu();
    void                    noteUnknownLanguage(const QString& value);
    // the selection links between the tree and the active scene
    void                    bindDocument(CyberiadaSMEditorDocument* doc);
    void                    unbindDocument(CyberiadaSMEditorDocument* doc);

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
    void                    activateDocument(int index);
    void                    closeTab(int index);
    void                    slotFileClose();
    void                    slotNextDocument();
    void                    slotPreviousDocument();
    void                    slotLanguageTriggered(QAction* action);
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
    void                    slotNewShallowHistory();
    void                    slotNewDeepHistory();
    void                    slotNewSubmachineState();
    void                    slotNewEntryPoint();
    void                    slotNewExitPoint();
    void                    slotNewComment();
    void                    slotNewFormalComment();
    void                    slotNewChoise();

    void                    slotDeleteElement();
    void                    slotReconstructGeometry();
    void                    slotCopy();
    void                    slotCut();
    void                    slotPaste();

private:
    QList<CyberiadaSMEditorDocument*> documents;
    CyberiadaSMEditorDocument* current = nullptr;
    QUndoGroup*             undoGroup = nullptr;
    // the tab line over the central area, shown from the second document on
    QTabBar*                documentTabs = nullptr;
    QActionGroup *toolGroup;
    QActionGroup *editGroup;
    ToolType currentTool = ToolType::Select;

    QMenu* recentMenu = nullptr;

    QActionGroup* languageGroup = nullptr;
    QAction* languageSeparator = nullptr;
    // the unknown values loaded or replaced in this session, then the transient one
    QStringList sessionLanguages;
    QList<QAction*> extraLanguageActions;

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
