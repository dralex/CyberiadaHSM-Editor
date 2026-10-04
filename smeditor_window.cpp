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
#include <QMessageBox>
#include <QToolBar>
#include <QComboBox>
#include <QMenu>
#include <QSignalBlocker>
#include <QTimer>
#include <QShowEvent>
#include <QScrollBar>
#include <QUndoGroup>
#include <QStackedWidget>
#include <QTabBar>

#include "smeditor_window.h"
#include "cyberiadasm_editor_view.h"
#include "myassert.h"
#include "fontmanager.h"
#include "dialogs/preferences_dialog.h"
#include "dialogs/about_dialog.h"
#include "dialogs/open_file_dialog.h"
#include "dialogs/save_file_dialog.h"
#include "dialogs/export_image_dialog.h"
#include "settings_manager.h"
#include "cyberiadasm_render.h"
#include "gesture_log.h"


CyberiadaSMEditorWindow::CyberiadaSMEditorWindow(QWidget* parent):
	QMainWindow(parent)
{
	setupUi(this);

	// layout: the drawing scene expands, the tree/property panel keeps its width
	// on the right (the vertical tool toolbar stays docked on the left)
	vSplitter->setStretchFactor(0, 1);
	vSplitter->setStretchFactor(1, 0);

    undoGroup = new QUndoGroup(this);
    // the tab line: a plain bar over the document stack and the right column,
    // hidden while one document is open (EDIT-DOC-1)
    documentTabs = new QTabBar(centralwidget);
    documentTabs->setTabsClosable(true);
    documentTabs->setMovable(true);
    documentTabs->setAutoHide(true);
    documentTabs->setExpanding(false);
    documentTabs->setDocumentMode(true);
    centralLayout->insertWidget(0, documentTabs);
    connect(documentTabs, &QTabBar::currentChanged, this, &CyberiadaSMEditorWindow::activateDocument);
    connect(documentTabs, &QTabBar::tabCloseRequested, this, &CyberiadaSMEditorWindow::closeTab);
    connect(documentTabs, &QTabBar::tabMoved, this,
            [this](int from, int to) { documents.move(from, to); });
    actionNextDocument->setShortcut(QKeySequence::NextChild);
    actionPreviousDocument->setShortcut(QKeySequence::PreviousChild);
    connect(actionClose, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotFileClose);
    connect(actionNextDocument, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotNextDocument);
    connect(actionPreviousDocument, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotPreviousDocument);

    // the first, untitled document (the tools need an active scene)
    setCurrentDocument(newDocument());
    initializeTools();

    connect(actionUndo, &QAction::triggered, undoGroup, &QUndoGroup::undo);
    connect(actionRedo, &QAction::triggered, undoGroup, &QUndoGroup::redo);
    // the session log records the undo/redo the user triggered
    connect(actionUndo, &QAction::triggered, this, []() { GestureLog::instance().logAction("undo"); });
    connect(actionRedo, &QAction::triggered, this, []() { GestureLog::instance().logAction("redo"); });
    connect(actionFitContent, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotFitContent);
    connect(actionNew, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotFileNew);
    connect(actionCut, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotCut);
    connect(actionCopy, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotCopy);
    connect(actionPaste, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotPaste);
    connect(actionReconstructGeometry, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotReconstructGeometry);
    // the undo group speaks for the active document's stack
    connect(undoGroup, &QUndoGroup::canUndoChanged, actionUndo, &QAction::setEnabled);
    connect(undoGroup, &QUndoGroup::canRedoChanged, actionRedo, &QAction::setEnabled);
    connect(undoGroup, &QUndoGroup::undoTextChanged, this, &CyberiadaSMEditorWindow::slotUndoTextChanged);
    connect(undoGroup, &QUndoGroup::redoTextChanged, this, &CyberiadaSMEditorWindow::slotRedoTextChanged);
    connect(undoGroup, &QUndoGroup::cleanChanged, this, &CyberiadaSMEditorWindow::slotCleanChanged);
    actionUndo->setEnabled(false);
    actionRedo->setEnabled(false);
    updateEditActions();
    updateTitle();   // start as a clean "untitled" document
}

CyberiadaSMEditorDocument* CyberiadaSMEditorWindow::newDocument()
{
    CyberiadaSMEditorDocument* doc = new CyberiadaSMEditorDocument(this);
    documents.append(doc);
    documentStack->addWidget(doc->view());
    undoGroup->addStack(doc->model()->undoStack());
    {
        // the bar would activate the new tab itself; setCurrentDocument decides
        QSignalBlocker block(documentTabs);
        documentTabs->addTab(doc->title());
    }
    updateTabTitle(doc);
    connect(doc->model()->undoStack(), &QUndoStack::cleanChanged, this,
            [this, doc](bool) { updateTabTitle(doc); });
    // the per-document signals reach the window from the active document only
    connect(doc->scene(), &QGraphicsScene::selectionChanged, this,
            [this, doc]() { if (doc == current) updateEditActions(); });
    connect(doc->scene(), &CyberiadaSMEditorScene::toolChanged, this,
            [this, doc](ToolType tool) { if (doc == current) slotSceneToolChanged(tool); });
    connect(doc->view(), &CyberiadaSMGraphicsView::scaleChanged, this,
            [this, doc](qreal scale) { if (doc == current) slotZoomScaleChanged(scale); });
    connect(doc, &CyberiadaSMEditorDocument::titleChanged, this,
            [this, doc]() { updateTabTitle(doc); if (doc == current) updateTitle(); });
    return doc;
}

void CyberiadaSMEditorWindow::updateTabTitle(CyberiadaSMEditorDocument* doc)
{
    int i = documents.indexOf(doc);
    if (i < 0) return;
    documentTabs->setTabText(i, doc->title() + (doc->isClean() ? "" : "*"));
    documentTabs->setTabToolTip(i, doc->filePath());
}

void CyberiadaSMEditorWindow::activateDocument(int index)
{
    setCurrentDocument(documents.value(index, nullptr));
}

void CyberiadaSMEditorWindow::closeTab(int index)
{
    closeDocument(documents.value(index, nullptr));
}

void CyberiadaSMEditorWindow::slotFileClose()
{
    closeDocument(current);
}

void CyberiadaSMEditorWindow::slotNextDocument()
{
    int n = documents.size();
    if (n > 1) activateDocument((documents.indexOf(current) + 1) % n);
}

void CyberiadaSMEditorWindow::slotPreviousDocument()
{
    int n = documents.size();
    if (n > 1) activateDocument((documents.indexOf(current) + n - 1) % n);
}

bool CyberiadaSMEditorWindow::closeDocument(CyberiadaSMEditorDocument* doc)
{
    if (!doc || !confirmDiscard(doc)) return false;
    // the window always holds a document: the last one gives way to a fresh untitled
    if (documents.size() == 1) setCurrentDocument(newDocument());
    int i = documents.indexOf(doc);
    if (doc == current) {
        // the right neighbour takes over, else the left one
        CyberiadaSMEditorDocument* next = documents.value(i + 1, nullptr);
        if (!next) next = documents.value(i - 1, nullptr);
        setCurrentDocument(next);
    }
    documents.removeAt(i);
    {
        QSignalBlocker block(documentTabs);
        documentTabs->removeTab(i);
    }
    documentStack->removeWidget(doc->view());
    undoGroup->removeStack(doc->model()->undoStack());
    delete doc;
    return true;
}

bool CyberiadaSMEditorWindow::openFile(const QString& fileName, QString* error,
                                       bool reconstruct, bool reconstruct_sm, bool strict)
{
    QString absolute = QFileInfo(fileName).absoluteFilePath();
    // an open file is activated, not loaded twice
    for (CyberiadaSMEditorDocument* doc : documents) {
        if (doc->hasFile() && QFileInfo(doc->filePath()).absoluteFilePath() == absolute) {
            setCurrentDocument(doc);
            return true;
        }
    }
    // a clean untitled active document takes the file, anything else gets a new tab
    CyberiadaSMEditorDocument* fresh = nullptr;
    if (!current->isUntitled()) {
        fresh = newDocument();
        setCurrentDocument(fresh);
    }
    if (!openDocument(fileName, error, reconstruct, reconstruct_sm, strict)) {
        if (fresh) closeDocument(fresh);   // the tab opened for the failed file goes
        return false;
    }
    return true;
}

bool CyberiadaSMEditorWindow::openFiles(const QStringList& fileNames, QString* error,
                                        bool reconstruct, bool reconstruct_sm, bool strict)
{
    bool ok = true;
    for (const QString& fileName : fileNames) {
        QString one;
        if (!openFile(fileName, &one, reconstruct, reconstruct_sm, strict)) {
            ok = false;
            if (error) *error += tr("Cannot open %1:\n%2\n").arg(fileName, one);
        }
    }
    return ok;
}

void CyberiadaSMEditorWindow::setCurrentDocument(CyberiadaSMEditorDocument* doc)
{
    if (!doc || doc == current) return;
    bool switched = (current != nullptr);
    if (current) unbindDocument(current);
    current = doc;
    sceneView = doc->view();
    documentStack->setCurrentWidget(sceneView);
    {
        QSignalBlocker block(documentTabs);
        documentTabs->setCurrentIndex(documents.indexOf(doc));
    }

    CyberiadaSMModel* m = doc->model();
    SMView->setModel(m);
    SMView->setRootIndex(m->rootIndex());
    propertiesWidget->setModel(m);
    propertiesWidget->setScene(doc->scene());
    undoGroup->setActiveStack(m->undoStack());
    bindDocument(doc);

    // the armed tool and the zoom follow; the tool goes to the scene directly,
    // so the session log gets no tool line
    doc->scene()->setCurrentTool(currentTool);
    sceneView->setCurrentTool(currentTool);
    slotZoomScaleChanged(sceneView->currentScale());
    expandAndWidenTree();
    updateTitle();
    updateEditActions();
    // a log session is bound to one document: a switch starts a new one
    if (switched && GestureLog::instance().isActive()) {
        GestureLog::instance().endSession();
        GestureLog::instance().startSession(m);
    }
    QTimer::singleShot(0, this, &CyberiadaSMEditorWindow::restoreSavedView);
}

void CyberiadaSMEditorWindow::bindDocument(CyberiadaSMEditorDocument* doc)
{
    // connected after the tree took the model: its own reset runs first and
    // drops the root index this slot restores
    connect(doc->model(), &CyberiadaSMModel::modelReset, this, &CyberiadaSMEditorWindow::slotModelReset);
    connect(SMView, SIGNAL(currentIndexActivated(QModelIndex)),
            doc->scene(), SLOT(slotElementSelected(QModelIndex)));
    connect(doc->scene(), &CyberiadaSMEditorScene::elementSelected, SMView, &CyberiadaSMView::select);
}

// a QModelIndex belongs to one model: a background document is never linked
void CyberiadaSMEditorWindow::unbindDocument(CyberiadaSMEditorDocument* doc)
{
    disconnect(doc->model(), &CyberiadaSMModel::modelReset, this, &CyberiadaSMEditorWindow::slotModelReset);
    disconnect(SMView, nullptr, doc->scene(), nullptr);
    disconnect(doc->scene(), nullptr, SMView, nullptr);
}

void CyberiadaSMEditorWindow::updateEditActions()
{
    if (!current) return;   // the documents are gone (destructor)
    Cyberiada::Element* el = nullptr;
    if (!scene()->selectedItems().isEmpty()) {
        if (CyberiadaSMEditorAbstractItem* item =
                dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene()->selectedItems().first())) {
            el = item->getElement();
        }
    }
    bool inspector = SettingsManager::instance().getInspectorMode();
    bool hasElement = (el != nullptr);
    bool copyable = hasElement && el->get_type() != Cyberiada::elementSM;   // an SM is not copyable
    actionCut->setEnabled(copyable && !inspector);
    actionCopy->setEnabled(copyable && !inspector);
    actionDeleteElement->setEnabled(hasElement && !inspector);
    actionPaste->setEnabled(clipboardElement != nullptr && !inspector);

    // an entry/exit point needs a container with a border to lie on: an explicit
    // state-machine border or a submachine state. A borderless machine cannot hold
    // one, so the tools are disabled until such a container exists
    bool hasContainer = false;
    if (model()->rootDocument()) {
        std::vector<Cyberiada::StateMachine*> sms = model()->rootDocument()->get_state_machines();
        for (size_t i = 0; i < sms.size() && !hasContainer; i++) {
            if (sms[i]->has_geometry() ||
                !sms[i]->find_elements_by_type(Cyberiada::elementSubmachineState).empty())
                hasContainer = true;
        }
    }
    actionNewEntryPoint->setEnabled(hasContainer && !inspector);
    actionNewExitPoint->setEnabled(hasContainer && !inspector);

    // the file actions follow the document and its modified state (the creation
    // tools stay enabled: the model creates a document lazily on the first edit)
    bool docOpen  = (model()->rootDocument() != nullptr);
    bool modified = docOpen && !model()->undoStack()->isClean();
    actionReconstructGeometry->setEnabled(docOpen && !inspector);
    actionSave->setEnabled(docOpen && modified && !inspector);
    actionSaveAs->setEnabled(docOpen);
    actionExport->setEnabled(docOpen);
}

// the documents go while the actions and the undo group are still alive; the
// per-document slots are guarded by the current pointer
CyberiadaSMEditorWindow::~CyberiadaSMEditorWindow()
{
    if (current) unbindDocument(current);
    current = nullptr;
    sceneView = nullptr;
    for (CyberiadaSMEditorDocument* doc : documents) {
        documentStack->removeWidget(doc->view());
        delete doc;
    }
    documents.clear();
    delete clipboardElement;
}

void CyberiadaSMEditorWindow::slotUndoTextChanged(const QString& text)
{
    actionUndo->setText(text.isEmpty() ? tr("Undo") : tr("Undo %1").arg(text));
}

void CyberiadaSMEditorWindow::slotRedoTextChanged(const QString& text)
{
    actionRedo->setText(text.isEmpty() ? tr("Redo") : tr("Redo %1").arg(text));
}

// the clean state of the undo stack is the saved state of the document
void CyberiadaSMEditorWindow::slotCleanChanged(bool clean)
{
    // the title always carries the [*] slot (an unsaved document is "untitled")
    setWindowModified(!clean);
    updateEditActions();   // Save follows the modified state
}

// the title carries the modified marker; the inspected document is read-only
void CyberiadaSMEditorWindow::updateTitle()
{
    if (!current) return;
    // the title always carries the [*] slot (an unsaved document is "untitled")
    QString title = current->title() + "[*]";
    if (SettingsManager::instance().getInspectorMode()) {
        title += " (inspector mode)";
    }
    setWindowTitle(title);
    setWindowModified(!current->isClean());
}

bool CyberiadaSMEditorWindow::confirmDiscard()
{
    return confirmDiscard(current);
}

bool CyberiadaSMEditorWindow::confirmDiscard(CyberiadaSMEditorDocument* doc)
{
    if (!doc || doc->isClean()) return true;
    setCurrentDocument(doc);   // the user sees the document the prompt is about
    QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Unsaved changes"),
        tr("The document has unsaved changes. Save them?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Save) {
        slotFileSave();
        return doc->isClean();
    }
    return true;
}

void CyberiadaSMEditorWindow::closeEvent(QCloseEvent* event)
{
    // every modified document asks in turn; a cancel keeps the editor open
    for (CyberiadaSMEditorDocument* doc : documents) {
        if (!confirmDiscard(doc)) {
            event->ignore();
            return;
        }
    }
    GestureLog::instance().endSession();
    event->accept();
}

// the tree follows a restored document
void CyberiadaSMEditorWindow::slotModelReset()
{
    SMView->setRootIndex(model()->rootIndex());
    expandAndWidenTree();
    updateEditActions();   // a loaded/reset document refreshes the file actions
}

void CyberiadaSMEditorWindow::expandAndWidenTree()
{
    // the whole structure is shown, and the right panel grows to fit the deepest
    // node (grow-only: it never shrinks, and always leaves room for the scene)
    SMView->expandAll();
    SMView->resizeColumnToContents(0);
    int treeWidth = SMView->columnWidth(0) + 2 * SMView->frameWidth() +
                    SMView->verticalScrollBar()->sizeHint().width() + 8;
    QList<int> sizes = vSplitter->sizes();
    if (sizes.size() == 2 && treeWidth > sizes.at(1)) {
        int total = sizes.at(0) + sizes.at(1);
        int right = qMin(treeWidth, total - 100);
        if (right > sizes.at(1)) {
            vSplitter->setSizes(QList<int>() << (total - right) << right);
        }
    }
}

void CyberiadaSMEditorWindow::restoreSavedView()
{
    if (!current || !current->hasPendingView()) return;
    // wait until the scene view has a real (laid-out) size, so the scrollbar-based
    // pan restores against the final width; the show path re-arms this otherwise
    if (sceneView->viewport()->width() <= 0) return;
    // re-fit the panel to the now-final width, then restore zoom + pan
    expandAndWidenTree();
    current->restoreView();
}

void CyberiadaSMEditorWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    // the layout is final only once the window is shown: apply a pending viewport
    QTimer::singleShot(0, this, &CyberiadaSMEditorWindow::restoreSavedView);
}

void CyberiadaSMEditorWindow::slotFileNew()
{
    setCurrentDocument(newDocument());
}

void CyberiadaSMEditorWindow::slotFileOpen()
{
    OpenFileDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) { return; }

    QString fileName = dlg.selectedFile();
    bool inspector = dlg.inspectorModeEnabled();
    bool reconstruct = dlg.reconstructionEnabled();
    bool reconstruct_sm = dlg.reconstructionSMEnabled();
    bool strict = dlg.strictModeEnabled();

    if (!fileName.isEmpty()) {
        SettingsManager::instance().setInspectorMode(inspector);

        QString error;
        if (!openFile(fileName, &error, reconstruct, reconstruct_sm, strict)) {
            QMessageBox::critical(this, tr("Load State Machine"), error);
        }
    }
}

bool CyberiadaSMEditorWindow::openDocument(const QString& fileName, QString* error,
                                           bool reconstruct, bool reconstruct_sm, bool strict)
{
    if (!current->load(fileName, error, reconstruct, reconstruct_sm, strict)) {
        return false;
    }
    SMView->setRootIndex(model()->rootIndex());
    // show the full structure and widen the right panel to fit it
    expandAndWidenTree();
    QModelIndex sm = model()->firstSMIndex();
    if (sm.isValid()) {
        SMView->select(sm);
    }
    // restore the saved editor view only after the layout has settled, so the
    // scrollbar-based pan lands against the final scene-view size (the interim
    // fit from loadScene stands until then)
    QTimer::singleShot(0, this, &CyberiadaSMEditorWindow::restoreSavedView);

    updateTitle();
    SettingsManager::instance().addRecentFile(QFileInfo(fileName).absoluteFilePath());
    // a new document begins a new session so the start snapshot matches it
    if (GestureLog::instance().isActive()) {
        GestureLog::instance().endSession();
        GestureLog::instance().startSession(model());
    }
    return true;
}

void CyberiadaSMEditorWindow::rebuildRecentMenu()
{
    if (!recentMenu) return;
    recentMenu->clear();
    const QStringList recent = SettingsManager::instance().getRecentFiles();
    recentMenu->setEnabled(!recent.isEmpty());
    for (const QString& path : recent) {
        QAction* a = recentMenu->addAction(QFileInfo(path).fileName());
        a->setToolTip(path);
        connect(a, &QAction::triggered, this, [this, path]() { openRecentFile(path); });
    }
    if (!recent.isEmpty()) {
        recentMenu->addSeparator();
        connect(recentMenu->addAction(tr("Clear list")), &QAction::triggered,
                this, []() { SettingsManager::instance().clearRecentFiles(); });
    }
}

void CyberiadaSMEditorWindow::openRecentFile(const QString& path)
{
    QString error;
    if (!openFile(path, &error)) {
        QMessageBox::warning(this, tr("Open State Machine"),
                             tr("Cannot open the document:\n") + error);
    }
}

void CyberiadaSMEditorWindow::slotFileSave()
{
    if (!current->hasFile()) {
        slotFileSaveAs();
        return;
    }
    QString error;
    if (!current->save(&error)) {
        QMessageBox::critical(this, tr("Save State Machine"),
                              tr("Cannot save the document:\n") + error);
    }
}

void CyberiadaSMEditorWindow::slotFileSaveAs()
{
    SaveFileDialog dlg(this, model()->rootDocument());
    if (dlg.exec() != QDialog::Accepted) { return; }

    QString fileName = dlg.selectedFile();
    if (fileName.isEmpty()) {
        return;
    }
    QString error;
    if (!current->saveAs(fileName, dlg.selectedFormat(), dlg.roundEnabled(),
                         dlg.skipGeometryEnabled(), dlg.checkInitialEnabled(),
                         dlg.strictActionsEnabled(), dlg.skipEmptyBehaviorEnabled(), &error)) {
        QMessageBox::critical(this, tr("Save State Machine"),
                              tr("Cannot save the document:\n") + error);
        return;
    }
    updateTitle();
}

void CyberiadaSMEditorWindow::slotFileExport()
{
    ExportImageDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) { return; }

    QString fileName = dlg.selectedFile();
    if (!fileName.isEmpty()) {
        QString error;
        if (!renderScene(scene(), fileName, &error, dlg.dpi(), dlg.fontFamily())) {
            QMessageBox::critical(this, tr("Error"), error);
        }
    }
}

void CyberiadaSMEditorWindow::initializeTools()
{
    // the modal tools, one exclusive group; the scene is the single source of
    // truth for the active tool, this map ties each tool to its toolbar action
    toolActMap[ToolType::Select]           = actionSelectTool;
    toolActMap[ToolType::Pan]              = actionPan;
    toolActMap[ToolType::Zoom]             = actionZoomTool;
    toolActMap[ToolType::Transition]       = actionNewTransition;
    toolActMap[ToolType::NewSM]            = actionNewStateMachine;
    toolActMap[ToolType::NewState]         = actionNewState;
    toolActMap[ToolType::NewInitial]       = actionNewInitial;
    toolActMap[ToolType::NewFinal]         = actionNewFinal;
    toolActMap[ToolType::NewChoice]        = actionNewChoise;
    toolActMap[ToolType::NewTerminate]     = actionNewTerminate;
    toolActMap[ToolType::NewShallowHistory]= actionNewShallowHistory;
    toolActMap[ToolType::NewDeepHistory]   = actionNewDeepHistory;
    toolActMap[ToolType::NewSubmachineState]= actionNewSubmachineState;
    toolActMap[ToolType::NewEntryPoint]    = actionNewEntryPoint;
    toolActMap[ToolType::NewExitPoint]     = actionNewExitPoint;
    toolActMap[ToolType::NewComment]       = actionNewComment;
    toolActMap[ToolType::NewFormalComment] = actionNewFormalComment;

    toolGroup = new QActionGroup(this);
    for (QAction* a : toolActMap.values()) {
        a->setCheckable(true);
        toolGroup->addAction(a);
    }
    toolGroup->setExclusive(true);
    actionSelectTool->setChecked(true);

    connect(toolGroup, &QActionGroup::triggered, this, &CyberiadaSMEditorWindow::slotToolSelected);

    // the zoom tool-options toolbar: actions (not tools), shown only when the
    // zoom tool is active
    zoomOptionsToolBar = addToolBar(tr("Zoom"));
    zoomOptionsToolBar->setObjectName("zoomOptionsToolBar");
    zoomOptionsToolBar->addAction(actionZoomIn);
    zoomOptionsToolBar->addAction(actionZoomOut);
    zoomOptionsToolBar->addAction(actionFitContent);   // already wired to slotFitContent in the constructor
    zoomOptionsToolBar->addAction(actionZoomToSM);
    zoomCombo = new QComboBox(zoomOptionsToolBar);
    zoomCombo->setEditable(true);
    zoomCombo->addItems(QStringList() << "25%" << "50%" << "75%" << "100%" << "150%" << "200%" << "400%");
    zoomCombo->setCurrentText("100%");
    zoomOptionsToolBar->addWidget(zoomCombo);
    zoomOptionsToolBar->setVisible(false);
    toolOptionBars[ToolType::Zoom] = zoomOptionsToolBar;

    connect(actionZoomIn, &QAction::triggered, this, [this]() { if (sceneView) sceneView->zoomIn(); });
    connect(actionZoomOut, &QAction::triggered, this, [this]() { if (sceneView) sceneView->zoomOut(); });
    connect(actionZoomToSM, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotZoomToSM);
    connect(zoomCombo, &QComboBox::currentTextChanged, this, &CyberiadaSMEditorWindow::slotZoomComboActivated);

    emit toolGroup->triggered(actionSelectTool);

    // the document-mutating, non-tool actions are switched off while the
    // document is inspected. The creation tools live in the exclusive toolGroup
    // (an action belongs to one group only); inspector mode disables the whole
    // element toolbar (slotInspectorModeChanged), which covers them.
    editGroup = new QActionGroup(this);
    editGroup->setExclusive(false);
    editGroup->addAction(actionNew);
    // save is governed solely by updateEditActions (document open + modified +
    // inspector); leaving it in editGroup would fight that per-action state
    // cut/copy/paste/delete are governed by updateEditActions (selection + inspector),
    // not the bulk editGroup, so they can react to the current selection
    editGroup->addAction(actionUndo);
    editGroup->addAction(actionRedo);

    // the File > Open Recent submenu, kept in step with the stored list
    recentMenu = new QMenu(tr("Open Recent"), this);
    menuFile->insertMenu(actionSave, recentMenu);
    connect(&SettingsManager::instance(), &SettingsManager::recentFilesChanged,
            this, &CyberiadaSMEditorWindow::rebuildRecentMenu);
    rebuildRecentMenu();

    connect(&SettingsManager::instance(), &SettingsManager::inspectorModeChanged,
            this, &CyberiadaSMEditorWindow::slotInspectorModeChanged);
    // the same setting is reachable from the preferences, so the action follows it
    connect(&SettingsManager::instance(), &SettingsManager::serviceObjectsChanged,
            this, &CyberiadaSMEditorWindow::slotServiceObjectsChanged);
    connect(&SettingsManager::instance(), &SettingsManager::loggingChanged,
            this, &CyberiadaSMEditorWindow::slotLoggingChanged);
    // the exit line is written on a clean quit; its absence marks a crash
    connect(qApp, &QCoreApplication::aboutToQuit, this, []() { GestureLog::instance().endSession(); });

    // TODO
    SettingsManager& sm = SettingsManager::instance();

    actionGridVisibility->setChecked(sm.getShowGrid());
    actionTransitionText->setChecked(sm.getShowTransitionText());
    slotInspectorModeChanged(sm.getInspectorMode());
    actionServiceObjects->setChecked(sm.getShowServiceObjects());
    actionSnapMode->setChecked(sm.getSnapMode());
    // seed the action and open the session if logging is on at launch
    slotLoggingChanged(sm.getLoggingEnabled());
}

void CyberiadaSMEditorWindow::slotToolSelected(QAction *action)
{
    currentTool = toolActMap.key(action, ToolType::Select);
    // the scene is the source of truth; setting it emits nothing, so drive the
    // view and the option bars here
    scene()->setCurrentTool(currentTool);
    sceneView->setCurrentTool(currentTool);
    // show the active tool's options toolbar, hide the others
    for (auto i = toolOptionBars.constBegin(); i != toolOptionBars.constEnd(); i++) {
        i.value()->setVisible(i.key() == currentTool);
    }
    // the log records the tools the batch language replays; pan/zoom are view
    // only and change nothing in the model
    static const QMap<ToolType, QString> verbs = {
        {ToolType::Select, "select"}, {ToolType::Transition, "transition"},
        {ToolType::NewSM, "new-sm"}, {ToolType::NewState, "new-state"},
        {ToolType::NewInitial, "new-initial"}, {ToolType::NewFinal, "new-final"},
        {ToolType::NewChoice, "new-choice"}, {ToolType::NewTerminate, "new-terminate"},
        {ToolType::NewShallowHistory, "new-shallow-history"}, {ToolType::NewDeepHistory, "new-deep-history"},
        {ToolType::NewSubmachineState, "new-submachine-state"},
        {ToolType::NewEntryPoint, "new-entry-point"}, {ToolType::NewExitPoint, "new-exit-point"},
        {ToolType::NewComment, "new-comment"}, {ToolType::NewFormalComment, "new-formal-comment"},
    };
    QMap<ToolType, QString>::const_iterator v = verbs.find(currentTool);
    if (v != verbs.end()) GestureLog::instance().logGesture("tool " + v.value());
}

void CyberiadaSMEditorWindow::slotFitContent() {
    QRectF bounds = scene()->visibleItemsBoundingRect();
    if (bounds.isNull()) return;
    // the target may sit outside the current scene rect (fitInView cannot scroll
    // past it); re-sync the rect to the content first
    scene()->updateSceneRect();
    sceneView->fitInView(bounds, Qt::KeepAspectRatio);
}

void CyberiadaSMEditorWindow::slotZoomToSM() {
    QRectF bounds = scene()->recentlyModifiedSMRect();
    if (bounds.isNull()) { slotFitContent(); return; }
    scene()->updateSceneRect();
    sceneView->fitInView(bounds, Qt::KeepAspectRatio);
}

void CyberiadaSMEditorWindow::slotZoomScaleChanged(qreal scale) {
    // reflect the view scale in the combo without re-triggering a zoom
    QString text = QString("%1%").arg(qRound(scale * 100.0));
    if (zoomCombo && zoomCombo->currentText() != text) {
        QSignalBlocker block(zoomCombo);
        zoomCombo->setCurrentText(text);
    }
}

void CyberiadaSMEditorWindow::slotZoomComboActivated() {
    if (!zoomCombo) return;
    QString text = zoomCombo->currentText();
    text.remove('%').remove(' ');
    bool ok = false;
    double percent = text.toDouble(&ok);
    if (ok && percent > 0.0) sceneView->setScale(percent / 100.0);
}

void CyberiadaSMEditorWindow::slotPreferences()
{
    PreferencesDialog dlg(this);
    dlg.exec();
}

void CyberiadaSMEditorWindow::slotAbout()
{
    AboutDialog dlg(this);
    dlg.exec();
}

void CyberiadaSMEditorWindow::slotServiceObjectsTriggered(bool on)
{
    SettingsManager::instance().setShowServiceObjects(on);
}

void CyberiadaSMEditorWindow::slotServiceObjectsChanged(bool on)
{
    actionServiceObjects->setChecked(on);
}

void CyberiadaSMEditorWindow::slotGridVisibilityTriggered(bool on)
{
    SettingsManager& sm = SettingsManager::instance();
    sm.setShowGrid(on);
    // scene()->enableGrid(on);
}

void CyberiadaSMEditorWindow::slotLogSessionTriggered(bool on)
{
    SettingsManager::instance().setLoggingEnabled(on);
}

void CyberiadaSMEditorWindow::slotLoggingChanged(bool on)
{
    actionLogSession->setChecked(on);
    if (on) {
        GestureLog::instance().startSession(model());
    } else {
        GestureLog::instance().endSession();
    }
}

// the scene switched the tool itself (a drag from a border box): the
// toolbar follows without triggering the tool again
void CyberiadaSMEditorWindow::slotSceneToolChanged(ToolType tool)
{
    // the scene switched the tool itself (a transient transition, or an
    // auto-revert to Select after a creation): sync the view, the toolbar
    // check and the option bars, without pushing the tool back to the scene
    currentTool = tool;
    sceneView->setCurrentTool(tool);
    QAction* action = toolActMap.value(tool, nullptr);
    if (action) action->setChecked(true);
    for (auto i = toolOptionBars.constBegin(); i != toolOptionBars.constEnd(); i++) {
        i.value()->setVisible(i.key() == tool);
    }
}

// the element creation actions are modal tools now: they arm the tool through
// the exclusive tool group (slotToolSelected); the element is drawn/placed on
// the canvas. These slots stay for the .ui action connections but do no
// immediate creation.
void CyberiadaSMEditorWindow::slotNewSM() {}
void CyberiadaSMEditorWindow::slotNewState() {}
void CyberiadaSMEditorWindow::slotNewInitial() {}
void CyberiadaSMEditorWindow::slotNewFinal() {}
void CyberiadaSMEditorWindow::slotNewTerminate() {}
void CyberiadaSMEditorWindow::slotNewShallowHistory() {}
void CyberiadaSMEditorWindow::slotNewDeepHistory() {}
void CyberiadaSMEditorWindow::slotNewSubmachineState() {}
void CyberiadaSMEditorWindow::slotNewEntryPoint() {}
void CyberiadaSMEditorWindow::slotNewExitPoint() {}
void CyberiadaSMEditorWindow::slotNewComment() {}
void CyberiadaSMEditorWindow::slotNewFormalComment() {}
void CyberiadaSMEditorWindow::slotNewChoise() {}

void CyberiadaSMEditorWindow::slotDeleteElement()
{
    if (scene()->selectedItems().isEmpty()) return;
    CyberiadaSMEditorAbstractItem* itemToDelete = dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene()->selectedItems().first());
    if (itemToDelete) {
        Cyberiada::Element* el = itemToDelete->getElement();
        // deleting a state machine removes only its border (the geometry); the
        // machine and its content stay
        if (el && el->get_type() == Cyberiada::elementSM) {
            model()->updateGeometry(model()->elementToIndex(el), Cyberiada::Rect());
            return;
        }
        model()->deleteElement(model()->elementToIndex(el));
        return;
    }
    DotSignal* dotToDelete = dynamic_cast<DotSignal*>(scene()->focusItem());
    if(dotToDelete) {
        dotToDelete->deleteDot();
        return;
    }
}

void CyberiadaSMEditorWindow::slotReconstructGeometry()
{
    if (!model()->rootDocument()) return;
    QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Geometry reconstruction"),
        tr("Rebuild the whole diagram geometry from scratch? "
           "The current layout is replaced; use Undo to restore it."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    model()->reconstructGeometry();
}

void CyberiadaSMEditorWindow::slotCopy()
{
    if (scene()->selectedItems().isEmpty()) return;
    CyberiadaSMEditorAbstractItem* item =
        dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene()->selectedItems().first());
    if (!item) return;
    Cyberiada::Element* el = item->getElement();
    if (!el || el->get_type() == Cyberiada::elementSM) return;   // a State Machine is not copyable
    delete clipboardElement;
    clipboardElement = el->copy(nullptr);   // a detached template for later pastes
    clipboardParentId = el->get_parent() ? el->get_parent()->get_id() : Cyberiada::ID();
    updateEditActions();
}

void CyberiadaSMEditorWindow::slotCut()
{
    if (scene()->selectedItems().isEmpty()) return;
    CyberiadaSMEditorAbstractItem* item =
        dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene()->selectedItems().first());
    if (!item) return;
    Cyberiada::Element* el = item->getElement();
    if (!el || el->get_type() == Cyberiada::elementSM) return;
    delete clipboardElement;
    clipboardElement = el->copy(nullptr);
    clipboardParentId = el->get_parent() ? el->get_parent()->get_id() : Cyberiada::ID();
    updateEditActions();
    model()->deleteElement(model()->elementToIndex(el));
}

void CyberiadaSMEditorWindow::slotPaste()
{
    if (!clipboardElement) return;
    // paste onto the copied element's original hierarchy level if it still exists,
    // else the first state machine
    Cyberiada::ElementCollection* target = dynamic_cast<Cyberiada::ElementCollection*>(
        model()->idToElement(QString::fromStdString(clipboardParentId)));
    if (!target && model()->rootDocument()) {
        std::vector<Cyberiada::StateMachine*> sms = model()->rootDocument()->get_state_machines();
        if (!sms.empty()) target = sms.front();
    }
    if (!target) return;
    model()->pasteElement(target, clipboardElement);
}

void CyberiadaSMEditorWindow::slotInspectorModeTriggered(bool on)
{
    SettingsManager::instance().setInspectorMode(on);
}

void CyberiadaSMEditorWindow::slotInspectorModeChanged(bool on)
{
    actionInspectorMode->setChecked(on);
    editGroup->setEnabled(!on);
    actionUndo->setEnabled(!on && undoGroup->canUndo());
    actionRedo->setEnabled(!on && undoGroup->canRedo());
    // disable the creation tools while inspecting, but keep select/pan/zoom so
    // the document can still be navigated
    for (auto i = toolActMap.constBegin(); i != toolActMap.constEnd(); i++) {
        if (i.key() != ToolType::Select && i.key() != ToolType::Pan && i.key() != ToolType::Zoom) {
            i.value()->setEnabled(!on);
        }
    }
    // last, so the entry/exit-point container gating overrides the blanket enable
    updateEditActions();   // cut/copy/paste/delete follow the inspector state too
    if (on && currentTool != ToolType::Select && currentTool != ToolType::Pan &&
        currentTool != ToolType::Zoom) {
        actionSelectTool->setChecked(true);
        slotToolSelected(actionSelectTool);
    }

    updateTitle();
}

void CyberiadaSMEditorWindow::slotShowTransitionActionTriggered(bool on)
{
    SettingsManager::instance().setShowTransitionText(on);
}

void CyberiadaSMEditorWindow::slotSnapModeTriggered(bool on)
{
    SettingsManager::instance().setSnapMode(on);
}


