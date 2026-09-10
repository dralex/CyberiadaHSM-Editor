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

#include "smeditor_window.h"
#include "myassert.h"
#include "fontmanager.h"
#include "dialogs/preferences_dialog.h"
#include "dialogs/open_file_dialog.h"
#include "dialogs/save_file_dialog.h"
#include "dialogs/export_image_dialog.h"
#include "settings_manager.h"
#include "cyberiadasm_render.h"


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
    connect(model, &CyberiadaSMModel::modelReset, this, &CyberiadaSMEditorWindow::slotModelReset);

    QUndoStack* stack = model->undoStack();
    connect(actionUndo, &QAction::triggered, stack, &QUndoStack::undo);
    connect(actionRedo, &QAction::triggered, stack, &QUndoStack::redo);
    connect(stack, &QUndoStack::canUndoChanged, actionUndo, &QAction::setEnabled);
    connect(stack, &QUndoStack::canRedoChanged, actionRedo, &QAction::setEnabled);
    connect(stack, &QUndoStack::undoTextChanged, this, &CyberiadaSMEditorWindow::slotUndoTextChanged);
    connect(stack, &QUndoStack::redoTextChanged, this, &CyberiadaSMEditorWindow::slotRedoTextChanged);
    connect(stack, &QUndoStack::cleanChanged, this, &CyberiadaSMEditorWindow::slotCleanChanged);
    actionUndo->setEnabled(false);
    actionRedo->setEnabled(false);
}

// the actions die before the model's stack, whose destructor still signals
CyberiadaSMEditorWindow::~CyberiadaSMEditorWindow()
{
    model->undoStack()->disconnect(this);
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

    QFileInfo fileInfo(fileName);
    openFileName = fileInfo.fileName();
    updateTitle();
    return true;
}

void CyberiadaSMEditorWindow::slotFileSave()
{
    if (model->rootDocument() && !model->rootDocument()->get_file_path().empty()) {
        try {
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
            QMessageBox::critical(this, tr("Ошибка"), error);
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
    toolGroup->addAction(actionNewTransition);

    toolGroup->setExclusive(true);
    actionSelectTool->setChecked(true);

    connect(toolGroup, &QActionGroup::triggered, this, &CyberiadaSMEditorWindow::slotToolSelected);
    connect(scene, &CyberiadaSMEditorScene::toolChanged, this, &CyberiadaSMEditorWindow::slotSceneToolChanged);
    emit toolGroup->triggered(actionSelectTool);

    // everything that modifies the document is switched off while it is inspected
    editGroup = new QActionGroup(this);
    editGroup->setExclusive(false);
    editGroup->addAction(actionNew);
    editGroup->addAction(actionSave);
    editGroup->addAction(actionNewStateMachine);
    editGroup->addAction(actionNewState);
    editGroup->addAction(actionNewInitial);
    editGroup->addAction(actionNewFinal);
    editGroup->addAction(actionNewTerminate);
    editGroup->addAction(actionNewChoise);
    editGroup->addAction(actionNewComment);
    editGroup->addAction(actionNewFormalComment);
    editGroup->addAction(actionNewTransition);
    editGroup->addAction(actionDeleteElement);
    editGroup->addAction(actionUndo);
    editGroup->addAction(actionRedo);

    connect(&SettingsManager::instance(), &SettingsManager::inspectorModeChanged,
            this, &CyberiadaSMEditorWindow::slotInspectorModeChanged);
    // the same setting is reachable from the preferences, so the action follows it
    connect(&SettingsManager::instance(), &SettingsManager::serviceObjectsChanged,
            this, &CyberiadaSMEditorWindow::slotServiceObjectsChanged);

    // TODO
    SettingsManager& sm = SettingsManager::instance();

    actionGridVisibility->setChecked(sm.getShowGrid());
    actionTransitionText->setChecked(sm.getShowTransitionText());
    slotInspectorModeChanged(sm.getInspectorMode());
    actionServiceObjects->setChecked(sm.getShowServiceObjects());
    actionSnapMode->setChecked(sm.getSnapMode());
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
    } else if (action == actionNewTransition) {
        currentTool = ToolType::Transition;
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

// the scene switched the tool itself (a drag from a border box): the
// toolbar follows without triggering the tool again
void CyberiadaSMEditorWindow::slotSceneToolChanged(ToolType tool)
{
    currentTool = tool;
    sceneView->setCurrentTool(tool);
    if (tool == ToolType::Transition) {
        actionNewTransition->setChecked(true);
    } else if (tool == ToolType::Select) {
        actionSelectTool->setChecked(true);
    }
}

void CyberiadaSMEditorWindow::slotNewSM()
{
    // when no machine has a border yet, the first one adopts the existing
    // elements: it gets an explicit border around them, rather than a new
    // empty machine appearing beside the content
    Cyberiada::LocalDocument* doc = model->rootDocument();
    if (doc) {
        std::vector<Cyberiada::StateMachine*> sms = doc->get_state_machines();
        bool anyBordered = false;
        for (std::vector<Cyberiada::StateMachine*>::iterator i = sms.begin(); i != sms.end(); i++) {
            if ((*i)->has_geometry()) { anyBordered = true; break; }
        }
        if (!anyBordered) {
            for (std::vector<Cyberiada::StateMachine*>::iterator i = sms.begin(); i != sms.end(); i++) {
                Cyberiada::Rect content = (*i)->get_bound_rect(*doc);
                if (content.valid) {
                    model->updateGeometry(model->elementToIndex(*i), content);
                    return;
                }
            }
        }
    }
    // otherwise add a new machine, placed clear of the others (distinct
    // geometry, like a new state)
    scene->addSMItem(Cyberiada::elementSM);
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

void CyberiadaSMEditorWindow::slotDeleteElement()
{
    if (scene->selectedItems().isEmpty()) return;
    CyberiadaSMEditorAbstractItem* itemToDelete = dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene->selectedItems().first());
    if (itemToDelete) {
        model->deleteElement(model->elementToIndex(itemToDelete->getElement()));
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
    SettingsManager::instance().setInspectorMode(on);
}

void CyberiadaSMEditorWindow::slotInspectorModeChanged(bool on)
{
    actionInspectorMode->setChecked(on);
    editGroup->setEnabled(!on);
    actionUndo->setEnabled(!on && model->undoStack()->canUndo());
    actionRedo->setEnabled(!on && model->undoStack()->canRedo());
    elementToolBar->setEnabled(!on);

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


