/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
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
#include "text_document.h"
#include "diff_document.h"
#include "plugin_runner.h"

class QToolBar;
class QComboBox;
class QMenu;
class QUndoGroup;
class QTabBar;
class QActionGroup;
class QTreeWidget;
class QTreeWidgetItem;
class MetaChoiceMenu;
class PluginsMenu;
class PluginDock;
class PluginSession;
class PluginSessionTab;

// The window holds the documents (docs/WINDOW.md): the menus, the toolbars,
// the clipboard and the armed tool are global; the structure tree, the
// properties panel and the actions target the active document.
class CyberiadaSMEditorWindow: public QMainWindow, public Ui_SMEditorWindow {
Q_OBJECT
public:
    CyberiadaSMEditorWindow(QWidget* parent = 0);

    ~CyberiadaSMEditorWindow();

    // load the file into the active document (the batch and test contract); an
    // inspected one is read as stored (EDIT-MODE-6)
    bool                    openDocument(const QString& fileName, QString* error = NULL,
                                         bool reconstruct = false, bool reconstruct_sm = false,
                                         bool strict = false, bool inspect = false);
    // the tab policy (EDIT-DOC-2): an open file activates its tab, a clean untitled
    // active document takes the file, anything else opens a new tab; the editing
    // and the inspection tab of a file are counted apart
    bool                    openFile(const QString& fileName, QString* error = NULL,
                                     bool reconstruct = false, bool reconstruct_sm = false,
                                     bool strict = false, bool inspect = false);
    // the command-line files, in order, the last one active; the failures are
    // collected in error and skipped
    bool                    openFiles(const QStringList& fileNames, QString* error = NULL,
                                      bool reconstruct = false, bool reconstruct_sm = false,
                                      bool strict = false, bool inspect = false);
    // MODES (docs/MODES.md): Document > Mode of a tab; the inspection opens a tab of
    // its own, HSM turns an inspection tab into an editing one, a text tab switches
    // between the text modes (EDIT-MODE-3/6/13)
    bool                    setDocumentMode(EditorDocument* doc, const QString& mode,
                                            QString* error = nullptr);
    // the inspection tab of a document: its file when saved, else its snapshot
    CyberiadaSMEditorDocument* inspectDocument(CyberiadaSMEditorDocument* source,
                                               QString* error = nullptr);
    // Document > Read-only of a tab (EDIT-MODE-4)
    void                    setDocumentReadOnly(EditorDocument* doc, bool on);
    // Document > Compare with...: a new diff tab of the diagram against the base (EDIT-MODE-10)
    DiffDocument*           compareDocument(CyberiadaSMEditorDocument* source, const DiffOrigin& base,
                                            QString* error = nullptr);
    // Document > Refresh: both sides again from their origins (EDIT-MODE-12)
    bool                    refreshDiff(DiffDocument* diff, QString* error = nullptr);
    // the change list of a diff tab, in the structure tree dock (EDIT-MODE-11)
    QTreeWidget*            changeList() const { return m_changeList; }
    // a new untitled diagram in a new tab
    CyberiadaSMEditorDocument* newDocument();
    // a new untitled text tab in a text mode (EDIT-MODE-13)
    TextDocument*           newTextDocument(const QString& mode);
    // a text file in a text tab by its extension; a clean untitled tab gives way
    bool                    openTextFile(const QString& fileName, QString* error = nullptr);
    void                    setCurrentDocument(EditorDocument* doc);
    // the close prompt first (skipped with discard); the last document is
    // replaced by a fresh untitled one
    bool                    closeDocument(EditorDocument* doc, bool discard = false);

    // the active tab; its diagram, null for a text tab, and the diagram's parts
    EditorDocument*         currentTab() const { return active; }
    CyberiadaSMEditorDocument* currentDocument() const { return current; }
    TextDocument*           currentText() const { return qobject_cast<TextDocument*>(active); }
    DiffDocument*           currentDiff() const { return qobject_cast<DiffDocument*>(active); }
    CyberiadaSMModel*       getModel() { return model(); }
    CyberiadaSMEditorScene* getScene() { return scene(); }
    int                     documentCount() const { return documents.size(); }
    EditorDocument*         tabAt(int i) const { return documents.value(i, nullptr); }
    // the diagram of a tab, null for a text tab
    CyberiadaSMEditorDocument* documentAt(int i) const {
        return qobject_cast<CyberiadaSMEditorDocument*>(documents.value(i, nullptr));
    }

    // the unsaved changes prompt: true when the document may go
    bool                    confirmDiscard();
    bool                    confirmDiscard(EditorDocument* doc);

    // the active document's canvas view (the tests address it by name)
    CyberiadaSMGraphicsView* sceneView = nullptr;
    QTabBar*                tabBar() const { return documentTabs; }

    // PLUGINS (docs/PLUGINS.md): start an action on the document, the active one by
    // default; nullptr with the reason when it cannot start; the result is applied
    // to the document the run started from (EDIT-PLUG-21)
    PluginRunner*           runPlugin(const QString& pluginId, const QString& actionId,
                                      const PluginValues& values, QString* error = nullptr,
                                      CyberiadaSMEditorDocument* doc = nullptr);
    // the batch and test form: run, wait in a local event loop, return the result
    PluginResult            runPluginAndWait(const QString& pluginId, const QString& actionId,
                                             const PluginValues& values,
                                             CyberiadaSMEditorDocument* doc = nullptr);
    // why the action cannot run on the document now; empty when it can
    QString                 pluginDisabledReason(const PluginEntry& plugin, const PluginAction& action,
                                                 CyberiadaSMEditorDocument* doc) const;
    // the values a GUI run starts from: the defaults, the stored persistent and
    // document values; needsDialog follows the click rule (EDIT-PLUG-7)
    PluginValues            pluginValues(const PluginEntry& plugin,
                                         const QList<const PluginParameter*>& parameters,
                                         CyberiadaSMEditorDocument* doc, bool* needsDialog = nullptr) const;
    void                    storePluginValues(const PluginEntry& plugin,
                                              const QList<const PluginParameter*>& parameters,
                                              CyberiadaSMEditorDocument* doc, const PluginValues& values);
    PluginsMenu*            pluginsMenu() const { return pluginMenu; }
    PluginDock*             pluginDock() const { return pluginDockWidget; }
    // Plugins > Session: the session of the active document (EDIT-PLUG-46)
    QMenu*                  pluginSessionMenu() const { return sessionMenu; }
    PluginSession*          currentSession() const;
    // View > Structure tree / Properties: the toggle actions of the docks (EDIT-UI-6)
    QAction*                actionStructureTree = nullptr;
    QAction*                actionProperties = nullptr;

signals:
    void                    pluginRunStarted(PluginRunner* run, CyberiadaSMEditorDocument* doc);
    // emitted after the result is applied; the runner is deleted later
    void                    pluginRunFinished(PluginRunner* run);

protected:
    void                    closeEvent(QCloseEvent* event) override;
    void                    showEvent(QShowEvent* event) override;
    bool                    eventFilter(QObject* watched, QEvent* event) override;

private:
    CyberiadaSMModel*       model() const { return current ? current->model() : nullptr; }
    CyberiadaSMEditorScene* scene() const { return current ? current->scene() : nullptr; }

    void                    initializeTools();
    void                    updateTitle();
    void                    initPaneDocks();
    // on load: expand the tree fully and widen the right panel to fit its content
    void                    expandAndWidenTree();
    // the File > Open Recent submenu, rebuilt from the stored recent-files list
    void                    rebuildRecentMenu();
    void                    openRecentFile(const QString& path);

    // the tab text: the title with the mode and the modified markers, the path as
    // the tooltip
    void                    updateTabTitle(EditorDocument* doc);
    // the tab line, the stack and the undo group take a new document
    void                    addTab(EditorDocument* doc);
    // the tab of a file, its editing or its inspection one
    CyberiadaSMEditorDocument* findDocument(const QString& fileName, bool inspection) const;
    // the actions of a text tab and of a diff tab (EDIT-MODE-15)
    void                    updateTextActions();
    void                    updateDiffActions();
    // the rows of the active diff tab
    void                    fillChangeList();
    // a side of a diff tab as a buffer; a closed source tab keeps the last one
    bool                    originBuffer(const DiffOrigin& origin, const std::string& last,
                                         std::string* buffer, QString* error) const;
    // the canvas tools and docks go with a text tab and come back with a diagram
    void                    showDiagramParts(bool on);
    // the Document menu: the modes, the flag (EDIT-MODE-3/4)
    void                    initDocumentMenu();
    void                    updateModeActions();
    // a mode or a flag changed: the tab, the title and the actions follow
    void                    documentStateChanged(EditorDocument* doc);
    // Edit > Language and Edit > Markup (EDIT-TEXT-9/12) follow the active document
    void                    initChoiceMenus();
    void                    syncChoiceMenus();
    // the selection links between the tree and the active scene
    void                    bindDocument(CyberiadaSMEditorDocument* doc);
    void                    unbindDocument(CyberiadaSMEditorDocument* doc);
    // the run context of a document (the snapshot, the meta, the selection)
    bool                    pluginContext(CyberiadaSMEditorDocument* doc, PluginRunContext* context,
                                          QString* error) const;
    // the declared metainformation fills the keys the document lacks (EDIT-PLUG-34)
    QList<PluginMetaNote>   fillPluginMeta(CyberiadaSMEditorDocument* doc, const PluginAction& action);
    // save, cwd: document need the file to be current (EDIT-PLUG-25)
    bool                    savePluginDocument(CyberiadaSMEditorDocument* doc, QString* error);
    // the document the result went to: the run's own, or the new one
    CyberiadaSMEditorDocument* applyPluginResult(PluginRunner* run, CyberiadaSMEditorDocument* doc);

private slots:
    void                    slotModelReset();
    // restore the saved viewport once the window/splitter reach their final size
    void                    restoreSavedView();
    void                    slotUndoTextChanged(const QString& text);
    void                    slotRedoTextChanged(const QString& text);
    void                    slotCleanChanged(bool clean);
    // Document > Properties...: the Document element in the tree and the properties
    void                    slotDocumentProperties();
    // File > New > FA diagram: an untitled tab that becomes a finite automaton (EDIT-MODE-5)
    void                    slotFileNewFA();
    // File > New > Text file > <mode> (EDIT-MODE-13)
    void                    slotFileNewText(const QString& mode);
    // Document > Compare with...: the base chosen among the tabs, a file, the saved file
    void                    slotCompare();
    void                    slotChangeActivated(QTreeWidgetItem* item);
    // Document > Check: the EXT-FA findings of an FA tab in Diagnostics (EDIT-MODE-9)
    void                    slotCheck();
    void                    slotServiceObjectsChanged(bool on);
    void                    slotComponentCommentsChanged(bool on);
    void                    slotLoggingChanged(bool on);
    void                    activateDocument(int index);
    void                    closeTab(int index);
    void                    slotFileClose();
    void                    slotNextDocument();
    void                    slotPreviousDocument();
    // enable cut/copy/paste/delete only for a fitting selection (and an editable tab)
    void                    updateEditActions();
    void                    slotPluginFinished();
    // the enabled state and the tooltips of the plugin actions (EDIT-PLUG-4)
    void                    updatePluginActions();
    // the Session menu follows the active document's session
    void                    updateSessionActions();
    void                    slotPluginsScanned();
    // the interpreters or the switches changed: the plugins are scanned again
    void                    slotPluginSettingsChanged();
    // undo and redo wait for a read-only tab and a plugin lock (EDIT-MODE-4, EDIT-PLUG-22)
    void                    updateUndoActions();

public slots:
    // a click on a plugin action: the dialog when needed, then the run
    void                    slotPluginTriggered(const QString& pluginId, const QString& actionId);
    void                    slotPluginSettings(const QString& pluginId);
    void                    slotPluginManager();
    // a finding was clicked: select and centre its element (EDIT-PLUG-15)
    void                    slotDiagnosticActivated(CyberiadaSMEditorDocument* doc, const QString& elementId);

public slots:
	void                    slotFileNew();
	void                    slotFileOpen();
    void                    slotFileSave();
    void                    slotFileSaveAs();
    void                    slotFileExport();
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
    void                    slotComponentCommentsTriggered(bool on);
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
    void                    slotUpdateGeometry();
    void                    slotCenterElement(Qt::Orientation orientation);
    void                    slotCopy();
    void                    slotCut();
    void                    slotPaste();

private:
    QList<EditorDocument*>  documents;
    // the active tab, and its diagram (null for a text tab)
    EditorDocument*         active = nullptr;
    CyberiadaSMEditorDocument* current = nullptr;
    // the element toolbar was hidden for a text tab, not by the user
    bool                    elementBarHidden = false;
    QTreeWidget*            m_changeList = nullptr;
    // the running plugins and the documents they started from
    QMap<PluginRunner*, QPointer<CyberiadaSMEditorDocument> > pluginRuns;
    QUndoGroup*             undoGroup = nullptr;
    // the tab line over the central area, shown from the second document on
    QTabBar*                documentTabs = nullptr;
    QActionGroup *toolGroup;
    QActionGroup* modeGroup = nullptr;
    ToolType currentTool = ToolType::Select;

    QMenu* recentMenu = nullptr;

    MetaChoiceMenu* languageChoice = nullptr;
    PluginsMenu* pluginMenu = nullptr;
    PluginDock* pluginDockWidget = nullptr;
    QMenu* sessionMenu = nullptr;
    QAction* actionSessionStep = nullptr;
    QAction* actionSessionRun = nullptr;
    QAction* actionSessionPause = nullptr;
    QAction* actionSessionRestart = nullptr;
    QAction* actionSessionStop = nullptr;
    QAction* actionSessionPost = nullptr;
    QAction* actionSessionLoad = nullptr;
    QAction* actionSessionEdit = nullptr;
    QAction* actionSessionSave = nullptr;
    void initSessionMenu();
    // a session's marks, findings and menu follow it (EDIT-PLUG-40/46/49)
    void connectSession(PluginRunner* run, CyberiadaSMEditorDocument* doc);
    // the session tab of the active document's session, else nullptr
    PluginSessionTab* currentSessionTab() const;
    MetaChoiceMenu* markupChoice = nullptr;

    // a detached deep clone of the last copied/cut element (nullptr when empty),
    // and the id of its original parent collection (the paste target level)
    Cyberiada::Element* clipboardElement = nullptr;
public:
    // the batch paste verb guards on this
    bool hasClipboard() const { return clipboardElement != nullptr; }
private:
    Cyberiada::ID clipboardParentId;

    QMap<ToolType, QAction*> toolActMap;
    // the tooltips of the tools, back once a mode offers them again
    QMap<QAction*, QString> toolTips;
    // the contextual tool-options toolbars, shown only while their tool is active
    QMap<ToolType, QToolBar*> toolOptionBars;
    QToolBar* zoomOptionsToolBar = nullptr;
    QComboBox* zoomCombo = nullptr;
};

#endif
