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
#include <QSignalBlocker>

#include "smeditor_window.h"
#include "cyberiadasm_editor_view.h"
#include "myassert.h"
#include "fontmanager.h"
#include "dialogs/preferences_dialog.h"
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
    connect(model, &CyberiadaSMModel::modelReset, this, &CyberiadaSMEditorWindow::slotModelReset);

    QUndoStack* stack = model->undoStack();
    connect(actionUndo, &QAction::triggered, stack, &QUndoStack::undo);
    connect(actionRedo, &QAction::triggered, stack, &QUndoStack::redo);
    // the session log records the undo/redo the user triggered
    connect(actionUndo, &QAction::triggered, this, []() { GestureLog::instance().logAction("undo"); });
    connect(actionRedo, &QAction::triggered, this, []() { GestureLog::instance().logAction("redo"); });
    connect(actionFitContent, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotFitContent);
    connect(actionNew, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotFileNew);
    connect(actionCut, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotCut);
    connect(actionCopy, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotCopy);
    connect(actionPaste, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotPaste);
    connect(stack, &QUndoStack::canUndoChanged, actionUndo, &QAction::setEnabled);
    connect(stack, &QUndoStack::canRedoChanged, actionRedo, &QAction::setEnabled);
    connect(stack, &QUndoStack::undoTextChanged, this, &CyberiadaSMEditorWindow::slotUndoTextChanged);
    connect(stack, &QUndoStack::redoTextChanged, this, &CyberiadaSMEditorWindow::slotRedoTextChanged);
    connect(stack, &QUndoStack::cleanChanged, this, &CyberiadaSMEditorWindow::slotCleanChanged);
    actionUndo->setEnabled(false);
    actionRedo->setEnabled(false);
    // cut/copy/paste/delete follow the selection and the clipboard
    connect(scene, &QGraphicsScene::selectionChanged, this, &CyberiadaSMEditorWindow::updateEditActions);
    updateEditActions();
}

void CyberiadaSMEditorWindow::updateEditActions()
{
    Cyberiada::Element* el = nullptr;
    if (!scene->selectedItems().isEmpty()) {
        if (CyberiadaSMEditorAbstractItem* item =
                dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene->selectedItems().first())) {
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
}

// the actions die before the model's stack, whose destructor still signals
CyberiadaSMEditorWindow::~CyberiadaSMEditorWindow()
{
    // the scene (a child) is torn down after this body: stop its selectionChanged
    // from reaching updateEditActions while its items are being destroyed
    if (scene) scene->disconnect(this);
    model->undoStack()->disconnect(this);
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
    // the marker only makes sense once a document title (with the [*] slot)
    // is set; before that the window has no placeholder
    if (!openFileName.isEmpty()) setWindowModified(!clean);
}

// the title carries the modified marker; the inspected document is read-only
void CyberiadaSMEditorWindow::updateTitle()
{
    if (openFileName.isEmpty()) return;
    QString title = openFileName + "[*]";
    if (SettingsManager::instance().getInspectorMode()) {
        title += " (inspector mode)";
    }
    setWindowTitle(title);
    setWindowModified(!model->undoStack()->isClean());
}

bool CyberiadaSMEditorWindow::confirmDiscard()
{
    if (model->undoStack()->isClean()) return true;
    QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Unsaved changes"),
        tr("The document has unsaved changes. Save them?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Save) {
        slotFileSave();
        return model->undoStack()->isClean();
    }
    return true;
}

void CyberiadaSMEditorWindow::closeEvent(QCloseEvent* event)
{
    if (confirmDiscard()) {
        GestureLog::instance().endSession();
        event->accept();
    } else {
        event->ignore();
    }
}

// the tree follows a restored document
void CyberiadaSMEditorWindow::slotModelReset()
{
    SMView->setRootIndex(model->rootIndex());
    SMView->expandToDepth(2);
}

void CyberiadaSMEditorWindow::slotFileNew()
{
    if (!confirmDiscard()) return;
    model->reset();
    model->undoStack()->clear();
    openFileName = QString();
    updateTitle();
}

void CyberiadaSMEditorWindow::slotFileOpen()
{
    if (!confirmDiscard()) return;
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
        if (!openDocument(fileName, &error, reconstruct, reconstruct_sm, strict)) {
            QMessageBox::critical(this, tr("Load State Machine"), error);
        }
    }
}

bool CyberiadaSMEditorWindow::openDocument(const QString& fileName, QString* error,
                                           bool reconstruct, bool reconstruct_sm, bool strict)
{
    if (!model->loadDocument(fileName, reconstruct, reconstruct_sm, strict)) {
        if (error) {
            *error = model->loadError();
        }
        return false;
    }
    SMView->setRootIndex(model->rootIndex());
    SMView->expandToDepth(2);
    QModelIndex sm = model->firstSMIndex();
    if (sm.isValid()) {
        scene->loadScene();
        SMView->select(sm);
    }
    // restore the saved editor view, if the document carries one, over the fit
    QString view = model->editorView();
    if (!view.isEmpty()) sceneView->applyViewState(view);

    QFileInfo fileInfo(fileName);
    openFileName = fileInfo.fileName();
    updateTitle();
    // a new document begins a new session so the start snapshot matches it
    if (GestureLog::instance().isActive()) {
        GestureLog::instance().endSession();
        GestureLog::instance().startSession(model);
    }
    return true;
}

void CyberiadaSMEditorWindow::slotFileSave()
{
    if (model->rootDocument() && !model->rootDocument()->get_file_path().empty()) {
        try {
            model->setEditorView(sceneView->viewState());   // persist the current view
            model->saveDocument();
        } catch (const Cyberiada::Exception& e) {
            QMessageBox::critical(this, tr("Save State Machine"),
                                  tr("Cannot save the document:\n") + QString(e.str().c_str()));
        }
    } else {
        slotFileSaveAs();
    }
}

void CyberiadaSMEditorWindow::slotFileSaveAs()
{
    SaveFileDialog dlg(this, model->rootDocument());
    if (dlg.exec() != QDialog::Accepted) { return; }

    QString fileName = dlg.selectedFile();
    if (fileName.isEmpty()) {
        return;
    }
    try {
        model->setEditorView(sceneView->viewState());   // persist the current view
        model->saveAsDocument(fileName, dlg.selectedFormat(), dlg.roundEnabled(),
                              dlg.skipGeometryEnabled(), dlg.checkInitialEnabled(),
                              dlg.strictActionsEnabled(), dlg.skipEmptyBehaviorEnabled());
    } catch (const Cyberiada::Exception& e) {
        QMessageBox::critical(this, tr("Save State Machine"),
                              tr("Cannot save the document:\n") + QString(e.str().c_str()));
        return;
    }

    QFileInfo fileInfo(fileName);
    openFileName = fileInfo.fileName();
    updateTitle();
}

void CyberiadaSMEditorWindow::slotFileExport()
{
    ExportImageDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) { return; }

    QString fileName = dlg.selectedFile();
    if (!fileName.isEmpty()) {
        QString error;
        if (!renderScene(scene, fileName, &error)) {
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
    connect(scene, &CyberiadaSMEditorScene::toolChanged, this, &CyberiadaSMEditorWindow::slotSceneToolChanged);

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

    connect(actionZoomIn, &QAction::triggered, sceneView, &CyberiadaSMGraphicsView::zoomIn);
    connect(actionZoomOut, &QAction::triggered, sceneView, &CyberiadaSMGraphicsView::zoomOut);
    connect(actionZoomToSM, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotZoomToSM);
    connect(sceneView, &CyberiadaSMGraphicsView::scaleChanged, this, &CyberiadaSMEditorWindow::slotZoomScaleChanged);
    connect(zoomCombo, &QComboBox::currentTextChanged, this, &CyberiadaSMEditorWindow::slotZoomComboActivated);

    emit toolGroup->triggered(actionSelectTool);

    // the document-mutating, non-tool actions are switched off while the
    // document is inspected. The creation tools live in the exclusive toolGroup
    // (an action belongs to one group only); inspector mode disables the whole
    // element toolbar (slotInspectorModeChanged), which covers them.
    editGroup = new QActionGroup(this);
    editGroup->setExclusive(false);
    editGroup->addAction(actionNew);
    editGroup->addAction(actionSave);
    // cut/copy/paste/delete are governed by updateEditActions (selection + inspector),
    // not the bulk editGroup, so they can react to the current selection
    editGroup->addAction(actionUndo);
    editGroup->addAction(actionRedo);

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
    scene->setCurrentTool(currentTool);
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
        {ToolType::NewComment, "new-comment"}, {ToolType::NewFormalComment, "new-formal-comment"},
    };
    QMap<ToolType, QString>::const_iterator v = verbs.find(currentTool);
    if (v != verbs.end()) GestureLog::instance().logGesture("tool " + v.value());
}

void CyberiadaSMEditorWindow::slotFitContent() {
    QRectF bounds = scene->visibleItemsBoundingRect();
    if (bounds.isNull()) return;
    sceneView->fitInView(bounds, Qt::KeepAspectRatio);
}

void CyberiadaSMEditorWindow::slotZoomToSM() {
    QRectF bounds = scene->recentlyModifiedSMRect();
    if (bounds.isNull()) { slotFitContent(); return; }
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
    // scene->enableGrid(on);
}

void CyberiadaSMEditorWindow::slotLogSessionTriggered(bool on)
{
    SettingsManager::instance().setLoggingEnabled(on);
}

void CyberiadaSMEditorWindow::slotLoggingChanged(bool on)
{
    actionLogSession->setChecked(on);
    if (on) {
        GestureLog::instance().startSession(model);
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
void CyberiadaSMEditorWindow::slotNewComment() {}
void CyberiadaSMEditorWindow::slotNewFormalComment() {}
void CyberiadaSMEditorWindow::slotNewChoise() {}

void CyberiadaSMEditorWindow::slotDeleteElement()
{
    if (scene->selectedItems().isEmpty()) return;
    CyberiadaSMEditorAbstractItem* itemToDelete = dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene->selectedItems().first());
    if (itemToDelete) {
        Cyberiada::Element* el = itemToDelete->getElement();
        // deleting a state machine removes only its border (the geometry); the
        // machine and its content stay
        if (el && el->get_type() == Cyberiada::elementSM) {
            model->updateGeometry(model->elementToIndex(el), Cyberiada::Rect());
            return;
        }
        model->deleteElement(model->elementToIndex(el));
        return;
    }
    DotSignal* dotToDelete = dynamic_cast<DotSignal*>(scene->focusItem());
    if(dotToDelete) {
        dotToDelete->deleteDot();
        return;
    }
}

void CyberiadaSMEditorWindow::slotCopy()
{
    if (scene->selectedItems().isEmpty()) return;
    CyberiadaSMEditorAbstractItem* item =
        dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene->selectedItems().first());
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
    if (scene->selectedItems().isEmpty()) return;
    CyberiadaSMEditorAbstractItem* item =
        dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene->selectedItems().first());
    if (!item) return;
    Cyberiada::Element* el = item->getElement();
    if (!el || el->get_type() == Cyberiada::elementSM) return;
    delete clipboardElement;
    clipboardElement = el->copy(nullptr);
    clipboardParentId = el->get_parent() ? el->get_parent()->get_id() : Cyberiada::ID();
    updateEditActions();
    model->deleteElement(model->elementToIndex(el));
}

void CyberiadaSMEditorWindow::slotPaste()
{
    if (!clipboardElement) return;
    // paste onto the copied element's original hierarchy level if it still exists,
    // else the first state machine
    Cyberiada::ElementCollection* target = dynamic_cast<Cyberiada::ElementCollection*>(
        model->idToElement(QString::fromStdString(clipboardParentId)));
    if (!target && model->rootDocument()) {
        std::vector<Cyberiada::StateMachine*> sms = model->rootDocument()->get_state_machines();
        if (!sms.empty()) target = sms.front();
    }
    if (!target) return;
    model->pasteElement(target, clipboardElement);
}

void CyberiadaSMEditorWindow::slotInspectorModeTriggered(bool on)
{
    SettingsManager::instance().setInspectorMode(on);
}

void CyberiadaSMEditorWindow::slotInspectorModeChanged(bool on)
{
    actionInspectorMode->setChecked(on);
    editGroup->setEnabled(!on);
    actionUndo->setEnabled(!on && model->undoStack()->canUndo());
    actionRedo->setEnabled(!on && model->undoStack()->canRedo());
    updateEditActions();   // cut/copy/paste/delete follow the inspector state too
    // disable the creation tools while inspecting, but keep select/pan/zoom so
    // the document can still be navigated
    for (auto i = toolActMap.constBegin(); i != toolActMap.constEnd(); i++) {
        if (i.key() != ToolType::Select && i.key() != ToolType::Pan && i.key() != ToolType::Zoom) {
            i.value()->setEnabled(!on);
        }
    }
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


