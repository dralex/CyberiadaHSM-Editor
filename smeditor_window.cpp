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

#include <QCloseEvent>
#include <QFileDialog>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegExp>
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
#include <QActionGroup>
#include <QEventLoop>

#include "smeditor_window.h"
#include "code_editor.h"
#include "cyberiadasm_editor_view.h"
#include "cyberiadasm_editor_subject_item.h"
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
#include "code_highlighter.h"
#include "meta_choice_menu.h"
#include "plugin_registry.h"
#include "plugins_menu.h"
#include "plugin_dock.h"
#include "plugin_session.h"
#include "plugin_session_view.h"
#include "dialogs/plugin_manager_dialog.h"
#include "dialogs/plugin_parameter_dialog.h"


// the initial width of the tree and the properties docks
static const int PANE_DOCK_WIDTH = 300;

// a bold dock title; the contents keep the inherited weight (EDIT-UI-6)
static void setBoldDockTitle(QDockWidget* dock)
{
    QFont contents;
    contents.setWeight(dock->font().weight());
    dock->widget()->setFont(contents);
    QFont title;
    title.setBold(true);
    dock->setFont(title);
}

CyberiadaSMEditorWindow::CyberiadaSMEditorWindow(QWidget* parent):
	QMainWindow(parent)
{
	setupUi(this);

    // the structure tree and the properties are docks at the right (EDIT-UI-6);
    // the plugin dock keeps the full width at the bottom
    setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);
    resizeDocks(QList<QDockWidget*>() << treeDock << propertiesDock,
                QList<int>() << PANE_DOCK_WIDTH << PANE_DOCK_WIDTH, Qt::Horizontal);
    initPaneDocks();

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

    initChoiceMenus();
    initDocumentMenu();
    // the first, untitled document (the tools need an active scene)
    setCurrentDocument(newDocument());
    initializeTools();

    // the plugins installed now (EDIT-PLUG-1); the search path comes from main
    pluginMenu = new PluginsMenu(this, menuHelp, zoomOptionsToolBar, menuView, actionPreferences);
    connect(pluginMenu, &PluginsMenu::actionTriggered, this, &CyberiadaSMEditorWindow::slotPluginTriggered);
    connect(pluginMenu, &PluginsMenu::settingsTriggered, this, &CyberiadaSMEditorWindow::slotPluginSettings);
    connect(&PluginRegistry::instance(), &PluginRegistry::scanned,
            this, &CyberiadaSMEditorWindow::slotPluginsScanned);
    pluginDockWidget = new PluginDock(this);
    addDockWidget(Qt::BottomDockWidgetArea, pluginDockWidget);
    pluginDockWidget->hide();
    for (QDockWidget* dock : findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        setBoldDockTitle(dock);
    }
    QAction* outputPanel = pluginDockWidget->toggleViewAction();
    outputPanel->setText(tr("Output panel"));
    QAction* manager = new QAction(tr("Plugin manager..."), this);
    connect(manager, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotPluginManager);
    initSessionMenu();
    pluginMenu->setTailActions(QList<QAction*>() << sessionMenu->menuAction() << outputPanel << manager);
    connect(this, &CyberiadaSMEditorWindow::pluginRunStarted, pluginDockWidget, &PluginDock::runStarted);
    connect(pluginDockWidget, &PluginDock::diagnosticActivated,
            this, &CyberiadaSMEditorWindow::slotDiagnosticActivated);
    connect(&SettingsManager::instance(), &SettingsManager::pluginsChanged,
            this, &CyberiadaSMEditorWindow::slotPluginSettingsChanged);
    connect(&SettingsManager::instance(), &SettingsManager::pluginSwitchesChanged,
            this, &CyberiadaSMEditorWindow::updatePluginActions);
    PluginRegistry::instance().setInterpreterPaths(SettingsManager::instance().getInterpreterPaths());
    PluginRegistry::instance().scan();

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
    // a tree selection refreshes the edit actions too: an element shown in the
    // tree but not on the scene (a component formal comment) still drives Delete
    connect(SMView, &CyberiadaSMView::currentIndexActivated, this, &CyberiadaSMEditorWindow::updateEditActions);
    connect(actionReconstructGeometry, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotReconstructGeometry);
    connect(actionUpdateGeometry, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotUpdateGeometry);
    connect(actionCenterHorizontally, &QAction::triggered, this, [this]() { slotCenterElement(Qt::Horizontal); });
    connect(actionCenterVertically, &QAction::triggered, this, [this]() { slotCenterElement(Qt::Vertical); });
    // the undo group speaks for the active document's stack
    connect(undoGroup, &QUndoGroup::canUndoChanged, this, &CyberiadaSMEditorWindow::updateUndoActions);
    connect(undoGroup, &QUndoGroup::canRedoChanged, this, &CyberiadaSMEditorWindow::updateUndoActions);
    connect(undoGroup, &QUndoGroup::undoTextChanged, this, &CyberiadaSMEditorWindow::slotUndoTextChanged);
    connect(undoGroup, &QUndoGroup::redoTextChanged, this, &CyberiadaSMEditorWindow::slotRedoTextChanged);
    connect(undoGroup, &QUndoGroup::cleanChanged, this, &CyberiadaSMEditorWindow::slotCleanChanged);
    // the when tokens follow every edit, undo and redo (EDIT-PLUG-52)
    connect(undoGroup, &QUndoGroup::indexChanged, this, &CyberiadaSMEditorWindow::updatePluginActions);
    actionUndo->setEnabled(false);
    actionRedo->setEnabled(false);
    updateEditActions();
    updateTitle();   // start as a clean "untitled" document
}

void CyberiadaSMEditorWindow::addTab(EditorDocument* doc)
{
    documents.append(doc);
    documentStack->addWidget(doc->page());
    undoGroup->addStack(doc->undoStack());
    {
        // the bar would activate the new tab itself; setCurrentDocument decides
        QSignalBlocker block(documentTabs);
        documentTabs->addTab(doc->title());
    }
    updateTabTitle(doc);
    connect(doc->undoStack(), &QUndoStack::cleanChanged, this,
            [this, doc](bool) { updateTabTitle(doc); });
    connect(doc, &EditorDocument::titleChanged, this,
            [this, doc]() {
                updateTabTitle(doc);
                // a Save As of an unmodified document changes the file, not the clean state
                if (doc == active) { updateTitle(); updatePluginActions(); }
            });
    connect(doc, &EditorDocument::modeChanged, this, [this, doc]() { documentStateChanged(doc); });
    connect(doc, &EditorDocument::readOnlyChanged, this, [this, doc]() { documentStateChanged(doc); });
}

CyberiadaSMEditorDocument* CyberiadaSMEditorWindow::newDocument()
{
    CyberiadaSMEditorDocument* doc = new CyberiadaSMEditorDocument(this);
    addTab(doc);
    // the per-document signals reach the window from the active document only
    connect(doc->scene(), &QGraphicsScene::selectionChanged, this,
            [this, doc]() { if (doc == current) updateEditActions(); });
    connect(doc->scene(), &CyberiadaSMEditorScene::toolChanged, this,
            [this, doc](ToolType tool) { if (doc == current) slotSceneToolChanged(tool); });
    connect(doc->view(), &CyberiadaSMGraphicsView::scaleChanged, this,
            [this, doc](qreal scale) { if (doc == current) slotZoomScaleChanged(scale); });
    connect(doc->scene(), &CyberiadaSMEditorScene::codeLanguageChanged, this,
            [this, doc]() { if (doc == current) syncChoiceMenus(); });
    connect(doc->scene(), &CyberiadaSMEditorScene::markupLanguageChanged, this,
            [this, doc]() { if (doc == current) syncChoiceMenus(); });
    return doc;
}

TextDocument* CyberiadaSMEditorWindow::newTextDocument(const QString& mode)
{
    TextDocument* doc = new TextDocument(isTextMode(mode) ? mode : QString(MODE_TEXT), this);
    addTab(doc);
    // the selection drives Cut and Copy
    connect(doc->editor(), &QPlainTextEdit::copyAvailable, this,
            [this, doc](bool) { if (doc == active) updateEditActions(); });
    return doc;
}

bool CyberiadaSMEditorWindow::openTextFile(const QString& fileName, QString* error)
{
    QString absolute = QFileInfo(fileName).absoluteFilePath();
    // an open file is activated, not loaded twice (EDIT-DOC-2)
    for (EditorDocument* doc : documents) {
        if (qobject_cast<TextDocument*>(doc) && doc->filePath() == absolute) {
            setCurrentDocument(doc);
            return true;
        }
    }
    // a clean untitled tab gives way to the file, as Open does
    EditorDocument* untitled = nullptr;
    if (current && current->isUntitled()) untitled = current;
    if (currentText() && currentText()->isUntitled()) untitled = currentText();
    TextDocument* doc = newTextDocument(modeForFile(fileName));
    if (!doc->load(fileName, error)) {
        closeDocument(doc, true);
        return false;
    }
    setCurrentDocument(doc);
    if (untitled) closeDocument(untitled, true);
    SettingsManager::instance().addRecentFile(absolute);
    return true;
}

void CyberiadaSMEditorWindow::updateTabTitle(EditorDocument* doc)
{
    int i = documents.indexOf(doc);
    if (i < 0) return;
    documentTabs->setTabText(i, doc->title() + doc->modeMarker() + (doc->isClean() ? "" : "*"));
    documentTabs->setTabToolTip(i, doc->filePath());
}

void CyberiadaSMEditorWindow::documentStateChanged(EditorDocument* doc)
{
    updateTabTitle(doc);
    if (doc != active) return;
    updateTitle();
    updateEditActions();
}

CyberiadaSMEditorDocument* CyberiadaSMEditorWindow::findDocument(const QString& fileName,
                                                                 bool inspection) const
{
    QString absolute = QFileInfo(fileName).absoluteFilePath();
    for (EditorDocument* tab : documents) {
        CyberiadaSMEditorDocument* doc = qobject_cast<CyberiadaSMEditorDocument*>(tab);
        if (doc && doc->hasFile() && QFileInfo(doc->filePath()).absoluteFilePath() == absolute &&
            (doc->mode() == MODE_INSPECTION) == inspection) {
            return doc;
        }
    }
    return nullptr;
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
    closeDocument(active);
}

void CyberiadaSMEditorWindow::slotNextDocument()
{
    int n = documents.size();
    if (n > 1) activateDocument((documents.indexOf(active) + 1) % n);
}

void CyberiadaSMEditorWindow::slotPreviousDocument()
{
    int n = documents.size();
    if (n > 1) activateDocument((documents.indexOf(active) + n - 1) % n);
}

bool CyberiadaSMEditorWindow::closeDocument(EditorDocument* doc, bool discard)
{
    if (!doc) return false;
    CyberiadaSMEditorDocument* diagram = qobject_cast<CyberiadaSMEditorDocument*>(doc);
    // a running plugin is cancelled with its document (EDIT-PLUG-23)
    if (diagram && diagram->pluginRun() && !discard && !qApp->property("batchMode").toBool() &&
        QMessageBox::question(this, tr("Close"),
                              tr("A plugin is running on %1. Cancel it and close?").arg(doc->title()),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return false;
    }
    if (!discard && !confirmDiscard(doc)) return false;
    // the window always holds a document: the last one gives way to a fresh untitled
    if (documents.size() == 1) setCurrentDocument(newDocument());
    int i = documents.indexOf(doc);
    if (doc == active) {
        // the right neighbour takes over, else the left one
        EditorDocument* next = documents.value(i + 1, nullptr);
        if (!next) next = documents.value(i - 1, nullptr);
        setCurrentDocument(next);
    }
    documents.removeAt(i);
    {
        QSignalBlocker block(documentTabs);
        documentTabs->removeTab(i);
    }
    documentStack->removeWidget(doc->page());
    undoGroup->removeStack(doc->undoStack());
    // the result of a closed document is never applied (EDIT-PLUG-23)
    if (diagram && diagram->pluginRun()) diagram->pluginRun()->abort();
    delete doc;
    return true;
}

bool CyberiadaSMEditorWindow::openFile(const QString& fileName, QString* error,
                                       bool reconstruct, bool reconstruct_sm, bool strict,
                                       bool inspect)
{
    // a text file opens in a text tab of the mode its extension names (EDIT-MODE-13)
    if (modeForFile(fileName) != MODE_HSM) return openTextFile(fileName, error);
    // an open file is activated, not loaded twice; its editing and its inspection
    // tab are counted apart (EDIT-DOC-2, EDIT-MODE-6)
    if (CyberiadaSMEditorDocument* open = findDocument(fileName, inspect)) {
        setCurrentDocument(open);
        return true;
    }
    // a clean untitled active document takes the file, anything else gets a new tab
    CyberiadaSMEditorDocument* fresh = nullptr;
    if (!current || !current->isUntitled()) {
        fresh = newDocument();
        setCurrentDocument(fresh);
    }
    if (!openDocument(fileName, error, reconstruct, reconstruct_sm, strict, inspect)) {
        if (fresh) closeDocument(fresh);   // the tab opened for the failed file goes
        return false;
    }
    return true;
}

bool CyberiadaSMEditorWindow::openFiles(const QStringList& fileNames, QString* error,
                                        bool reconstruct, bool reconstruct_sm, bool strict,
                                        bool inspect)
{
    bool ok = true;
    for (const QString& fileName : fileNames) {
        QString one;
        if (!openFile(fileName, &one, reconstruct, reconstruct_sm, strict, inspect)) {
            ok = false;
            if (error) *error += tr("Cannot open %1:\n%2\n").arg(fileName, one);
        }
    }
    return ok;
}

void CyberiadaSMEditorWindow::setCurrentDocument(EditorDocument* tab)
{
    if (!tab || tab == active) return;
    bool switched = (active != nullptr);
    if (current) unbindDocument(current);
    active = tab;
    current = qobject_cast<CyberiadaSMEditorDocument*>(tab);
    documentStack->setCurrentWidget(tab->page());
    {
        QSignalBlocker block(documentTabs);
        documentTabs->setCurrentIndex(documents.indexOf(tab));
    }
    undoGroup->setActiveStack(tab->undoStack());
    // a text tab has no canvas, no structure and no element tools (EDIT-MODE-15)
    showDiagramParts(current != nullptr);

    if (CyberiadaSMEditorDocument* doc = current) {
        sceneView = doc->view();
        CyberiadaSMModel* m = doc->model();
        SMView->setModel(m);
        SMView->setRootIndex(m->rootIndex());
        propertiesWidget->setModel(m);
        propertiesWidget->setScene(doc->scene());
        bindDocument(doc);
        // the armed tool and the zoom follow; the tool goes to the scene directly,
        // so the session log gets no tool line
        doc->scene()->setCurrentTool(currentTool);
        sceneView->setCurrentTool(currentTool);
        slotZoomScaleChanged(sceneView->currentScale());
        expandAndWidenTree();
    } else {
        // no index of a background model stays linked (docs/WINDOW.md)
        sceneView = nullptr;
        SMView->setModel(nullptr);
        propertiesWidget->detachModel();
    }
    updateTitle();
    updateEditActions();
    syncChoiceMenus();
    // a log session is bound to one diagram: a switch starts a new one
    if (switched && GestureLog::instance().isActive()) {
        GestureLog::instance().endSession();
        if (current) GestureLog::instance().startSession(current->model());
    }
    QTimer::singleShot(0, this, &CyberiadaSMEditorWindow::restoreSavedView);
}

void CyberiadaSMEditorWindow::showDiagramParts(bool on)
{
    if (!on) {
        if (elementToolBar->isVisible()) {
            elementToolBar->hide();
            elementBarHidden = true;
        }
        for (QToolBar* bar : toolOptionBars) bar->hide();
    } else {
        if (elementBarHidden) elementToolBar->show();
        elementBarHidden = false;
        for (auto i = toolOptionBars.constBegin(); i != toolOptionBars.constEnd(); i++) {
            i.value()->setVisible(i.key() == currentTool);
        }
    }
    const QList<QAction*> zooms = QList<QAction*>() << actionZoomIn << actionZoomOut
                                                    << actionFitContent << actionZoomToSM;
    for (QAction* zoom : zooms) zoom->setEnabled(on);
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

void CyberiadaSMEditorWindow::initChoiceMenus()
{
    QList<MetaChoiceMenu::Entry> languages;
    languages.append({tr("Undefined"), QString()});
    for (const QString& name : CodeStyle::instance().languages()) {
        languages.append({name, name});
    }
    // an alias checks its language
    languageChoice = new MetaChoiceMenu(menuLanguage, languages,
        [](const QString& value, bool* known) {
            const CodeLanguage* lang = CodeStyle::instance().find(value);
            *known = lang || value.trimmed().isEmpty();
            return lang ? lang->name : QString();
        }, this);
    connect(languageChoice, &MetaChoiceMenu::chosen, this, [this](const QString& value) {
        // refused (a read-only tab): the check goes back
        if (current && !model()->setPlatformLanguage(value)) syncChoiceMenus();
    });

    QList<MetaChoiceMenu::Entry> markups;
    markups.append({tr("Plain"), QString()});
    markups.append({tr("Markdown"), CodeStyle::markupValue(codeMarkupMarkdown)});
    markupChoice = new MetaChoiceMenu(menuMarkup, markups,
        [](const QString& value, bool* known) {
            return CodeStyle::markupValue(CodeStyle::findMarkup(value, known));
        }, this);
    connect(markupChoice, &MetaChoiceMenu::chosen, this, [this](const QString& value) {
        if (current && !model()->setMarkupLanguage(value)) syncChoiceMenus();
    });
}

void CyberiadaSMEditorWindow::syncChoiceMenus()
{
    if (!languageChoice || !current) return;
    languageChoice->sync(scene()->codeLanguageName());
    markupChoice->sync(scene()->markupLanguageName());
}

void CyberiadaSMEditorWindow::initDocumentMenu()
{
    // one mode per tab, the checked one the active tab's (EDIT-MODE-3)
    modeGroup = new QActionGroup(this);
    modeGroup->setExclusive(true);
    actionModeHSM->setData(QString(MODE_HSM));
    actionModeInspection->setData(QString(MODE_INSPECTION));
    actionModeFA->setData(QString(MODE_FA));
    modeGroup->addAction(actionModeHSM);
    modeGroup->addAction(actionModeInspection);
    modeGroup->addAction(actionModeFA);
    // the text modes: Plain text, Markdown, Code > the languages (EDIT-MODE-3/13)
    menuMode->addSeparator();
    QMenu* code = nullptr;
    QMenu* newText = menuNew->addMenu(tr("Text file"));
    newText->setObjectName("menuNewText");
    for (const QString& mode : textModes()) {
        bool language = mode != MODE_TEXT && mode != MODE_MARKDOWN;
        if (language && !code) {
            code = menuMode->addMenu(tr("Code"));
            code->setObjectName("menuModeCode");
        }
        QAction* action = (language ? code : menuMode)->addAction(modeName(mode));
        action->setCheckable(true);
        action->setData(mode);
        modeGroup->addAction(action);
        QAction* create = newText->addAction(modeName(mode));
        create->setData(mode);
        connect(create, &QAction::triggered, this, [this, mode]() { slotFileNewText(mode); });
    }
    connect(modeGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        QString error;
        if (active && !setDocumentMode(active, action->data().toString(), &error) &&
            !error.isEmpty() && !qApp->property("batchMode").toBool()) {
            QMessageBox::warning(this, tr("Mode"), error);
        }
        // a refused switch takes the check back
        updateModeActions();
    });
    connect(actionReadOnly, &QAction::triggered, this, [this](bool on) {
        if (active) setDocumentReadOnly(active, on);
        updateModeActions();
    });
    connect(actionDocumentProperties, &QAction::triggered,
            this, &CyberiadaSMEditorWindow::slotDocumentProperties);
    connect(actionCheck, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotCheck);
    connect(actionNewFA, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotFileNewFA);
    // View > FA symbols is a preference of every tab (EDIT-MODE-8)
    SettingsManager& sm = SettingsManager::instance();
    actionFASymbols->setChecked(sm.getShowFASymbols());
    connect(actionFASymbols, &QAction::triggered, &sm, &SettingsManager::setShowFASymbols);
    connect(&sm, &SettingsManager::faSymbolsChanged, actionFASymbols, &QAction::setChecked);
}

void CyberiadaSMEditorWindow::updateModeActions()
{
    if (!active || !modeGroup) return;
    QString mode = active->mode();
    bool text = active->traits().text;
    // a diagram reaches the diagram modes, a text the text ones (EDIT-MODE-3)
    for (QAction* action : modeGroup->actions()) {
        action->setChecked(action->data().toString() == mode);
        action->setEnabled(isTextMode(action->data().toString()) == text);
    }
    if (QMenu* code = menuMode->findChild<QMenu*>("menuModeCode")) code->setEnabled(text);
    // a read-only mode shows the flag checked and fixed (EDIT-MODE-4)
    actionReadOnly->setChecked(active->isReadOnly());
    actionReadOnly->setEnabled(!active->traits().readOnly);
    // the choices write the document: an editable diagram only (EDIT-TEXT-9/12)
    languageChoice->setEnabled(current && !current->isReadOnly());
    markupChoice->setEnabled(current && !current->isReadOnly());
}

bool CyberiadaSMEditorWindow::setDocumentMode(EditorDocument* tab, const QString& mode,
                                              QString* error)
{
    if (!tab) return false;
    if (tab->mode() == mode) return true;
    // a text switches between the text modes only: its text stays (EDIT-MODE-13)
    if (TextDocument* text = qobject_cast<TextDocument*>(tab)) {
        if (!isTextMode(mode)) {
            if (error) *error = tr("a text tab cannot become a %1").arg(modeName(mode));
            return false;
        }
        return text->setMode(mode);
    }
    CyberiadaSMEditorDocument* doc = static_cast<CyberiadaSMEditorDocument*>(tab);
    if (isTextMode(mode)) {
        if (error) *error = tr("a diagram tab cannot become a %1").arg(modeName(mode));
        return false;
    }
    if (mode == MODE_INSPECTION) return inspectDocument(doc, error) != nullptr;
    if (mode != MODE_HSM && mode != MODE_FA) {
        if (error) *error = tr("the mode %1 is not available").arg(mode);
        return false;
    }
    if (doc->mode() == MODE_INSPECTION) {
        // the file is edited in another tab already: that one takes the mode (EDIT-DOC-2)
        CyberiadaSMEditorDocument* editing =
            doc->hasFile() ? findDocument(doc->filePath(), false) : nullptr;
        if (editing) {
            setCurrentDocument(editing);
            return setDocumentMode(editing, mode, error);
        }
        // the model reset would pull the document from under a run
        if (doc->pluginRun()) {
            if (error) *error = tr("a plugin is running on the document");
            return false;
        }
        doc->leaveInspection();
        if (doc->mode() == mode) return true;
    }
    // HSM <-> FA writes the document: type/ fa, one undo step (EDIT-MODE-5)
    if (!doc->isEditable()) {
        if (error) *error = tr("the tab is read-only");
        return false;
    }
    if (!doc->model()->setFA(mode == MODE_FA)) {
        if (error) *error = tr("the document type is not changed");
        return false;
    }
    return true;
}

CyberiadaSMEditorDocument* CyberiadaSMEditorWindow::inspectDocument(CyberiadaSMEditorDocument* source,
                                                                    QString* error)
{
    if (!source) return nullptr;
    if (source->mode() == MODE_INSPECTION) {
        setCurrentDocument(source);
        return source;
    }
    // a saved document is read from its file, as stored (EDIT-MODE-6)
    if (source->hasFile() && source->isClean()) {
        return openFile(source->filePath(), error, false, false, false, true) ? current : nullptr;
    }
    // an unsaved one is inspected as it is now, titled as its source
    bool ok = true;
    std::string snapshot = source->model()->snapshot(&ok);
    if (!ok) {
        if (error) *error = tr("the document cannot be encoded");
        return nullptr;
    }
    QString view = source->view()->viewState();
    CyberiadaSMEditorDocument* doc = newDocument();
    setCurrentDocument(doc);
    doc->inspectSnapshot(snapshot, source->title(), view);
    QTimer::singleShot(0, this, &CyberiadaSMEditorWindow::restoreSavedView);
    return doc;
}

void CyberiadaSMEditorWindow::setDocumentReadOnly(EditorDocument* doc, bool on)
{
    if (doc) doc->setReadOnlyFlag(on);
}

void CyberiadaSMEditorWindow::slotFileNewText(const QString& mode)
{
    setCurrentDocument(newTextDocument(mode));
}

void CyberiadaSMEditorWindow::slotFileNewFA()
{
    CyberiadaSMEditorDocument* doc = newDocument();
    doc->model()->setNewDocumentFA(true);
    setCurrentDocument(doc);
}

void CyberiadaSMEditorWindow::slotCheck()
{
    if (!current || current->mode() != MODE_FA || !model()->rootDocument()) return;
    Cyberiada::FAFindingList findings;
    try {
        findings = model()->rootDocument()->check_fa();
    } catch (const Cyberiada::Exception& e) {
        QMessageBox::warning(this, tr("Check"), tr("The automaton is not checked:\n%1").arg(e.str().c_str()));
        return;
    }
    // the violations are findings of the document, as a plugin's (EDIT-PLUG-15)
    QList<PluginDiagnostic> diagnostics;
    for (const Cyberiada::FAFinding& f : findings) {
        PluginDiagnostic d;
        d.severity = "error";
        d.rule = QString::fromStdString(f.requirement);
        d.element = QString::fromStdString(f.element);
        d.message = QString::fromStdString(f.message);
        d.linked = !d.element.isEmpty() && model()->idToElement(d.element);
        diagnostics.append(d);
    }
    pluginDockWidget->showDiagnostics(diagnostics, current);
    pluginDockWidget->show();
    pluginDockWidget->raise();
    if (diagnostics.isEmpty() && !qApp->property("batchMode").toBool()) {
        QMessageBox::information(this, tr("Check"), tr("The automaton breaks no EXT-FA rule."));
    }
}

void CyberiadaSMEditorWindow::slotDocumentProperties()
{
    if (!current || !model()->rootDocument()) return;
    // the name and the metainformation of the Document element
    SettingsManager::instance().setShowProperties(true);
    SMView->select(model()->documentIndex());
}

void CyberiadaSMEditorWindow::updateTextActions()
{
    TextDocument* text = currentText();
    bool readOnly = text->isReadOnly();
    // the clipboard actions edit the text; the diagram ones have nothing to act on
    bool selection = text->editor()->textCursor().hasSelection();
    actionCut->setEnabled(selection && !readOnly);
    actionCopy->setEnabled(selection);
    actionPaste->setEnabled(!readOnly);
    const QList<QAction*> diagramOnly = QList<QAction*>()
        << actionDeleteElement << actionCenterHorizontally << actionCenterVertically
        << actionReconstructGeometry << actionUpdateGeometry << actionDocumentProperties
        << actionCheck << actionExport;
    for (QAction* off : diagramOnly) off->setEnabled(false);
    for (QAction* tool : toolActMap) tool->setEnabled(false);
    actionSave->setEnabled(!text->isClean() && !readOnly);
    actionSaveAs->setEnabled(true);
    updateModeActions();
    updateUndoActions();
    updatePluginActions();
}

void CyberiadaSMEditorWindow::updateEditActions()
{
    if (!active) return;   // the documents are gone (destructor)
    if (!current) {
        updateTextActions();
        return;
    }
    Cyberiada::Element* el = nullptr;
    if (!scene()->selectedItems().isEmpty()) {
        if (CyberiadaSMEditorAbstractItem* item =
                dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene()->selectedItems().first())) {
            el = item->getElement();
        }
    }
    // a read-only tab (its flag, the inspection) and a plugin lock keep the edits off
    // (EDIT-MODE-4, EDIT-PLUG-22)
    bool readOnly = model()->readOnly();
    bool hasElement = (el != nullptr);
    // the Delete action also reaches an element shown in the tree but not on the
    // scene (a component formal comment, possibly without geometry): fall back to
    // the tree's current element when the scene has no selection
    Cyberiada::Element* del = el;
    if (!del && SMView->currentIndex().isValid()) {
        del = model()->indexToElement(SMView->currentIndex());
    }
    bool deletable = del && dynamic_cast<Cyberiada::ElementCollection*>(del->get_parent());
    // a selected link is deleted on its own (its subject); it is not copied
    if (!scene()->selectedItems().isEmpty() &&
        dynamic_cast<CyberiadaSMEditorSubjectItem*>(scene()->selectedItems().first())) {
        deletable = true;
    }
    bool copyable = hasElement && el->get_type() != Cyberiada::elementSM;   // an SM is not copyable
    actionCut->setEnabled(copyable && !readOnly);
    // copying changes nothing: a read-only tab is copied from too
    actionCopy->setEnabled(copyable);
    actionDeleteElement->setEnabled(deletable && !readOnly);
    actionPaste->setEnabled(clipboardElement != nullptr && !readOnly);
    // centring needs a parent frame: never on the top level (EDIT-TOOL-18)
    bool centerable = model()->canCenterInParent(el);
    actionCenterHorizontally->setEnabled(centerable && !readOnly);
    actionCenterVertically->setEnabled(centerable && !readOnly);

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
    actionNewEntryPoint->setEnabled(hasContainer && !readOnly);
    actionNewExitPoint->setEnabled(hasContainer && !readOnly);
    // the order matters: the tool loop below turns off what the mode lacks
    // a read-only tab keeps its view: the creation tools wait, select, pan and zoom
    // stay (EDIT-INSPECT-1, EDIT-PLUG-41); an FA tab has the automaton tools only,
    // the others say why (EDIT-MODE-7)
    QString mode = current->mode();
    for (auto i = toolActMap.constBegin(); i != toolActMap.constEnd(); i++) {
        bool offered = modeHasTool(mode, i.key());
        i.value()->setToolTip(offered ? toolTips.value(i.value())
                                      : tr("%1: not an element of a finite automaton")
                                            .arg(toolTips.value(i.value())));
        if (!offered) {
            i.value()->setEnabled(false);
        } else if (i.key() != ToolType::Select && i.key() != ToolType::Pan && i.key() != ToolType::Zoom &&
                   i.key() != ToolType::NewEntryPoint && i.key() != ToolType::NewExitPoint) {
            i.value()->setEnabled(!readOnly);
        }
    }
    if ((readOnly && currentTool != ToolType::Select && currentTool != ToolType::Pan &&
         currentTool != ToolType::Zoom) || !modeHasTool(mode, currentTool)) {
        actionSelectTool->setChecked(true);
        slotToolSelected(actionSelectTool);
    }

    // the file actions follow the document and its modified state (the creation
    // tools stay enabled: the model creates a document lazily on the first edit)
    bool docOpen  = (model()->rootDocument() != nullptr);
    bool modified = docOpen && !model()->undoStack()->isClean();
    actionReconstructGeometry->setEnabled(docOpen && !readOnly);
    actionUpdateGeometry->setEnabled(docOpen && !readOnly);
    actionDocumentProperties->setEnabled(docOpen);
    actionCheck->setEnabled(docOpen && current->mode() == MODE_FA);
    actionSave->setEnabled(docOpen && modified && !readOnly);
    actionSaveAs->setEnabled(docOpen);
    actionExport->setEnabled(docOpen);
    updateModeActions();
    updateUndoActions();
    updatePluginActions();
}

// the documents go while the actions and the undo group are still alive; the
// per-document slots are guarded by the current pointer
CyberiadaSMEditorWindow::~CyberiadaSMEditorWindow()
{
    // the runs end while the docks and the actions they report to are alive
    for (PluginRunner* run: pluginRuns.keys()) {
        if (run->session()) run->session()->disconnect(this);
        run->disconnect(this);
        delete run;
    }
    pluginRuns.clear();
    if (current) unbindDocument(current);
    current = nullptr;
    active = nullptr;
    sceneView = nullptr;
    for (EditorDocument* doc : documents) {
        documentStack->removeWidget(doc->page());
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

// the title carries the mode and the modified markers (EDIT-MODE-2)
void CyberiadaSMEditorWindow::updateTitle()
{
    if (!active) return;
    // the title always carries the [*] slot (an unsaved document is "untitled")
    setWindowTitle(active->title() + active->modeMarker(true) + "[*]");
    setWindowModified(!active->isClean());
}

bool CyberiadaSMEditorWindow::confirmDiscard()
{
    return confirmDiscard(active);
}

bool CyberiadaSMEditorWindow::confirmDiscard(EditorDocument* doc)
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
    if (!pluginRuns.isEmpty() &&
        QMessageBox::question(this, tr("Exit"), tr("Plugins are running. Cancel them and exit?"),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        event->ignore();
        return;
    }
    // every modified document asks in turn; a cancel keeps the editor open
    for (EditorDocument* doc : documents) {
        if (!confirmDiscard(doc)) {
            event->ignore();
            return;
        }
    }
    // every run ends with the editor (EDIT-PLUG-23)
    for (PluginRunner* run: pluginRuns.keys()) run->abort();
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
    // the whole structure is shown, and the tree dock grows to fit the deepest
    // node (grow-only: it never shrinks, and always leaves room for the scene)
    SMView->expandAll();
    SMView->resizeColumnToContents(0);
    if (treeDock->isHidden() || treeDock->isFloating()) return;
    int treeWidth = SMView->columnWidth(0) + 2 * SMView->frameWidth() +
                    SMView->verticalScrollBar()->sizeHint().width() + 8;
    int width = treeDock->width();
    int right = qMin(treeWidth, width + centralWidget()->width() - 100);
    if (right > width) {
        resizeDocks(QList<QDockWidget*>() << treeDock, QList<int>() << right, Qt::Horizontal);
    }
}

// the View entries of the tree and the properties docks; the shown/hidden
// choice is stored from the menu entry and the close button only: Qt unchecks
// the toggle actions on every hide, the main window's own included
void CyberiadaSMEditorWindow::initPaneDocks()
{
    SettingsManager& sm = SettingsManager::instance();
    actionStructureTree = treeDock->toggleViewAction();
    actionProperties = propertiesDock->toggleViewAction();
    menuView->insertAction(actionTransitionText, actionStructureTree);
    menuView->insertAction(actionTransitionText, actionProperties);
    menuView->insertSeparator(actionTransitionText);
    treeDock->setVisible(sm.getShowStructureTree());
    propertiesDock->setVisible(sm.getShowProperties());
    treeDock->installEventFilter(this);
    propertiesDock->installEventFilter(this);
    connect(actionStructureTree, &QAction::triggered, &sm, &SettingsManager::setShowStructureTree);
    connect(actionProperties, &QAction::triggered, &sm, &SettingsManager::setShowProperties);
    connect(&sm, &SettingsManager::structureTreeChanged, treeDock, &QDockWidget::setVisible);
    connect(&sm, &SettingsManager::propertiesChanged, propertiesDock, &QDockWidget::setVisible);
}

bool CyberiadaSMEditorWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Close) {
        if (watched == treeDock) SettingsManager::instance().setShowStructureTree(false);
        if (watched == propertiesDock) SettingsManager::instance().setShowProperties(false);
    }
    return QMainWindow::eventFilter(watched, event);
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
        // the inspection box opens an inspection tab (EDIT-MODE-6)
        QString error;
        if (!openFile(fileName, &error, reconstruct, reconstruct_sm, strict, inspector)) {
            QMessageBox::critical(this, tr("Load State Machine"), error);
        }
    }
}

bool CyberiadaSMEditorWindow::openDocument(const QString& fileName, QString* error,
                                           bool reconstruct, bool reconstruct_sm, bool strict,
                                           bool inspect)
{
    // a text file goes to a text tab; a diagram needs the active tab to be one
    if (modeForFile(fileName) != MODE_HSM) return openTextFile(fileName, error);
    if (!current) {
        CyberiadaSMEditorDocument* doc = newDocument();
        setCurrentDocument(doc);
    }
    if (!current->load(fileName, error, reconstruct, reconstruct_sm, strict, inspect)) {
        return false;
    }
    // an unknown value of a file stays in Document > Language / Markup for the session
    languageChoice->noteUnknown(scene()->codeLanguageName());
    markupChoice->noteUnknown(scene()->markupLanguageName());
    syncChoiceMenus();
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
    if (TextDocument* text = currentText()) {
        if (!text->hasFile()) {
            slotFileSaveAs();
            return;
        }
        QString error;
        if (!text->save(&error)) QMessageBox::critical(this, tr("Save Text"), error);
        return;
    }
    if (!current) return;
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
    // a text proposes the extension of its mode (EDIT-MODE-13)
    if (TextDocument* text = currentText()) {
        QString extension = modeExtension(text->mode());
        QString proposed = text->hasFile() ? text->filePath() :
            QDir(SettingsManager::instance().getLastDirectory()).filePath(tr("untitled") + extension);
        QString path = QFileDialog::getSaveFileName(
            this, tr("Save Text"), proposed,
            tr("%1 (*%2)").arg(modeName(text->mode()), extension) + ";;" + tr("All files (*)"),
            nullptr, QFileDialog::DontUseNativeDialog);
        if (path.isEmpty()) return;
        if (QFileInfo(path).suffix().isEmpty()) path += extension;
        QString error;
        if (!text->saveAs(path, &error)) {
            QMessageBox::critical(this, tr("Save Text"), error);
            return;
        }
        SettingsManager::instance().addRecentFile(text->filePath());
        updateTitle();
        return;
    }
    if (!current) return;
    SaveFileDialog dlg(this, model()->rootDocument());
    if (!current->hasFile() && !current->documentName().isEmpty()) {
        // a named unsaved document proposes its name as the file (EDIT-DOC-10)
        QString name = current->documentName();
        name.replace(QRegExp("[/\\\\:]"), "_");
        dlg.selectFile(name + ".graphml");
    }
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
    // a text tab has no image (EDIT-MODE-15)
    if (!current) return;
    ExportImageDialog dlg(this);
    // propose the diagram's base name; the dialog's default suffix / filter adds
    // the image extension (mirrors slotFileSaveAs)
    if (current && current->hasFile()) {
        dlg.selectFile(QFileInfo(current->filePath()).completeBaseName());
    }
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
        toolTips.insert(a, a->toolTip());
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

    // the File > Open Recent submenu, kept in step with the stored list
    recentMenu = new QMenu(tr("Open Recent"), this);
    menuFile->insertMenu(actionSave, recentMenu);
    connect(&SettingsManager::instance(), &SettingsManager::recentFilesChanged,
            this, &CyberiadaSMEditorWindow::rebuildRecentMenu);
    rebuildRecentMenu();

    // the same setting is reachable from the preferences, so the action follows it
    connect(&SettingsManager::instance(), &SettingsManager::serviceObjectsChanged,
            this, &CyberiadaSMEditorWindow::slotServiceObjectsChanged);
    connect(&SettingsManager::instance(), &SettingsManager::componentCommentsChanged,
            this, &CyberiadaSMEditorWindow::slotComponentCommentsChanged);
    connect(&SettingsManager::instance(), &SettingsManager::loggingChanged,
            this, &CyberiadaSMEditorWindow::slotLoggingChanged);
    // the exit line is written on a clean quit; its absence marks a crash
    connect(qApp, &QCoreApplication::aboutToQuit, this, []() { GestureLog::instance().endSession(); });

    // TODO
    SettingsManager& sm = SettingsManager::instance();

    actionGridVisibility->setChecked(sm.getShowGrid());
    actionTransitionText->setChecked(sm.getShowTransitionText());
    actionServiceObjects->setChecked(sm.getShowServiceObjects());
    actionComponentComments->setChecked(sm.getShowComponentComments());
    actionSnapMode->setChecked(sm.getSnapMode());
    // seed the action and open the session if logging is on at launch
    slotLoggingChanged(sm.getLoggingEnabled());
}

void CyberiadaSMEditorWindow::slotToolSelected(QAction *action)
{
    if (!current) return;   // a text tab has no canvas
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
    if (!current) return;   // a text tab has no canvas
    QRectF bounds = scene()->visibleItemsBoundingRect();
    if (bounds.isNull()) return;
    // the target may sit outside the current scene rect (fitInView cannot scroll
    // past it); re-sync the rect to the content first
    scene()->updateSceneRect();
    sceneView->fitInView(bounds, Qt::KeepAspectRatio);
}

void CyberiadaSMEditorWindow::slotZoomToSM() {
    if (!current) return;   // a text tab has no canvas
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
    if (!current) return;   // a text tab has no canvas
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

void CyberiadaSMEditorWindow::slotComponentCommentsTriggered(bool on)
{
    SettingsManager::instance().setShowComponentComments(on);
}

void CyberiadaSMEditorWindow::slotComponentCommentsChanged(bool on)
{
    actionComponentComments->setChecked(on);
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
        // a session records a diagram: a text tab has none to start from
        if (model()) GestureLog::instance().startSession(model());
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
    if (!current) return;   // a text tab has no canvas
    if (scene()->selectedItems().isEmpty()) {
        // an element shown in the tree but not on the scene (a component formal
        // comment, possibly without geometry) is deleted by its tree index, with
        // the same semantics as the scene path (an SM clears only its border)
        QModelIndex idx = SMView->currentIndex();
        Cyberiada::Element* el = idx.isValid() ? model()->indexToElement(idx) : nullptr;
        if (!el || !dynamic_cast<Cyberiada::ElementCollection*>(el->get_parent())) return;
        if (el->get_type() == Cyberiada::elementSM) {
            model()->clearStateMachineBorder(idx);
            return;
        }
        model()->deleteElement(idx);
        return;
    }
    // a selected comment link removes its subject, never the comment (EDIT-EDGE-20)
    if (CyberiadaSMEditorSubjectItem* link =
            dynamic_cast<CyberiadaSMEditorSubjectItem*>(scene()->selectedItems().first())) {
        link->deleteSubject();
        return;
    }
    CyberiadaSMEditorAbstractItem* itemToDelete = dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene()->selectedItems().first());
    if (itemToDelete) {
        Cyberiada::Element* el = itemToDelete->getElement();
        // deleting a state machine removes only its border (the geometry); the
        // machine and its content stay, re-anchored to the origin in place
        if (el && el->get_type() == Cyberiada::elementSM) {
            model()->clearStateMachineBorder(model()->elementToIndex(el));
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
    if (!current) return;   // a text tab has no canvas
    if (!model()->rootDocument()) return;
    QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Geometry reconstruction"),
        tr("Rebuild the whole diagram geometry from scratch? "
           "The current layout is replaced; use Undo to restore it."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    model()->reconstructGeometry();
}

void CyberiadaSMEditorWindow::slotUpdateGeometry()
{
    if (!current) return;   // a text tab has no canvas
    // the drawn elements stay, so no confirmation: one undo step restores it
    if (!model()->rootDocument()) return;
    model()->updateGeometry();
}

void CyberiadaSMEditorWindow::slotCenterElement(Qt::Orientation orientation)
{
    if (!current) return;   // a text tab has no canvas
    if (scene()->selectedItems().isEmpty()) return;
    CyberiadaSMEditorAbstractItem* item =
        dynamic_cast<CyberiadaSMEditorAbstractItem*>(scene()->selectedItems().first());
    if (!item || !item->getElement()) return;
    model()->centerInParent(model()->elementToIndex(item->getElement()), orientation);
}

void CyberiadaSMEditorWindow::slotCopy()
{
    // a text tab edits its text through the system clipboard
    if (TextDocument* text = currentText()) {
        text->editor()->copy();
        return;
    }
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
    // a text tab edits its text through the system clipboard
    if (TextDocument* text = currentText()) {
        text->editor()->cut();
        return;
    }
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
    // a text tab edits its text through the system clipboard
    if (TextDocument* text = currentText()) {
        text->editor()->paste();
        return;
    }
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

void CyberiadaSMEditorWindow::slotShowTransitionActionTriggered(bool on)
{
    SettingsManager::instance().setShowTransitionText(on);
}

void CyberiadaSMEditorWindow::slotSnapModeTriggered(bool on)
{
    SettingsManager::instance().setSnapMode(on);
}



// PLUGINS

static QString documentFormatName(const Cyberiada::LocalDocument* root)
{
    if (!root) return "cyberiada";
    switch (root->get_file_format()) {
    case Cyberiada::formatLegacyYEDOstranna:  return "yed-ostranna";
    case Cyberiada::formatLegacyYEDBerloga16: return "yed-berloga";
    case Cyberiada::formatLapki:              return "lapki";
    case Cyberiada::formatLegacyYED:
        return QString::fromStdString(root->get_file_format_str()).contains("Berloga") ?
            "yed-berloga" : "yed-ostranna";
    default:                                  return "cyberiada";
    }
}

QString CyberiadaSMEditorWindow::pluginDisabledReason(const PluginEntry& plugin,
                                                      const PluginAction& action,
                                                      CyberiadaSMEditorDocument* doc) const
{
    if (plugin.status == PluginEntry::statusDisabled) return plugin.reason;
    if (!plugin.usable()) return tr("the plugin is %1").arg(PluginEntry::statusText(plugin.status));
    // the batch mode ignores the switch of the plugin manager
    if (!qApp->property("batchMode").toBool() &&
        !SettingsManager::instance().getPluginEnabled(plugin.descriptor.id)) {
        return tr("the plugin is switched off");
    }
    QString unavailable = plugin.unavailable.value(action.id);
    if (!unavailable.isEmpty()) return unavailable;
    if (!doc) return tr("there is no document");
    if (doc->pluginRun()) return tr("a plugin is running on the document");
    // a replace needs an editable tab (EDIT-PLUG-24); the lock is the run's, checked above
    if (action.changesDocument() && doc->isReadOnly()) {
        return doc->mode() == MODE_INSPECTION ? tr("the inspection tab is read-only")
                                               : tr("the tab is read-only");
    }
    if (action.when.isEmpty()) return QString();
    PluginDocumentFacts facts = doc->pluginFacts();
    // the keys fillPluginMeta would write (EDIT-PLUG-34)
    QList<QPair<QString, QString> > fills;
    if (facts.machines > 0 && !doc->model()->readOnly()) fills = action.meta;
    return PluginConditions::check(action.when, action.reason, facts, fills);
}

bool CyberiadaSMEditorWindow::pluginContext(CyberiadaSMEditorDocument* doc, PluginRunContext* context,
                                            QString* error) const
{
    CyberiadaSMModel* m = doc->model();
    bool ok = true;
    context->input = m->snapshot(&ok);
    if (!ok) {
        if (error) *error = tr("the document cannot be encoded");
        return false;
    }
    context->documentPath = doc->hasFile() ? QFileInfo(doc->filePath()).absoluteFilePath() : QString();
    context->documentFormat = documentFormatName(m->rootDocument());
    context->modified = !doc->isClean();
    const Cyberiada::LocalDocument* root = m->rootDocument();
    if (root) {
        const Cyberiada::DocumentMetainformation& meta = root->meta();
        if (!meta.standard_version.empty()) {
            context->meta.append(qMakePair(QString("standardVersion"),
                                           QString::fromStdString(meta.standard_version)));
        }
        for (const std::pair<Cyberiada::String, Cyberiada::String>& item: meta.strings) {
            QString key = QString::fromStdString(item.first);
            // the view state is the editor's own (docs/PLUGIN-API.md, request.json)
            if (key == "cyberiadaEditorView") continue;
            context->meta.append(qMakePair(key, QString::fromStdString(item.second)));
        }
    }
    context->selection = doc->scene()->selectedIds();
    return true;
}

QList<PluginMetaNote> CyberiadaSMEditorWindow::fillPluginMeta(CyberiadaSMEditorDocument* doc,
                                                              const PluginAction& action)
{
    QList<PluginMetaNote> notes;
    if (action.meta.isEmpty()) return notes;
    CyberiadaSMModel* m = doc->model();
    const Cyberiada::LocalDocument* root = m->rootDocument();
    // nothing to describe, or nothing to change
    bool usable = root && !root->get_state_machines().empty() && !m->readOnly();
    QList<QPair<QString, QString> > fills;
    for (const QPair<QString, QString>& item: action.meta) {
        PluginMetaNote note;
        note.key = item.first;
        note.declared = item.second;
        if (!usable) {
            note.kind = PluginMetaNote::metaSkipped;
            notes.append(note);
            continue;
        }
        QString present = QString::fromStdString(root->meta().get_string(item.first.toStdString()));
        if (present.isEmpty()) {
            note.kind = PluginMetaNote::metaSet;
            note.value = item.second;
            fills.append(item);
            notes.append(note);
        } else if (!PluginConditions::sameMetaValue(item.first, present, item.second)) {
            // the document's own value wins
            note.kind = PluginMetaNote::metaKept;
            note.value = present;
            notes.append(note);
        }
    }
    if (!fills.isEmpty()) {
        m->beginUndoStep(tr("Plugin: %1").arg(action.label));
        for (const QPair<QString, QString>& item: fills) {
            m->updateMetainformation(m->documentIndex(), item.first, item.second);
        }
        m->endUndoStep();
    }
    return notes;
}

bool CyberiadaSMEditorWindow::savePluginDocument(CyberiadaSMEditorDocument* doc, QString* error)
{
    if (doc->hasFile() && doc->isClean()) return true;
    if (!doc->hasFile()) {
        if (qApp->property("batchMode").toBool()) {
            if (error) *error = tr("the document is not saved");
            return false;
        }
        setCurrentDocument(doc);
        slotFileSaveAs();
    } else if (!doc->save(error)) {
        return false;
    }
    if (!doc->hasFile() || !doc->isClean()) {
        if (error && error->isEmpty()) *error = tr("the document is not saved");
        return false;
    }
    return true;
}

PluginRunner* CyberiadaSMEditorWindow::runPlugin(const QString& pluginId, const QString& actionId,
                                                 const PluginValues& values, QString* error,
                                                 CyberiadaSMEditorDocument* doc)
{
    if (!doc) doc = current;
    const PluginEntry* entry = PluginRegistry::instance().find(pluginId);
    if (!entry) {
        if (error) *error = tr("the plugin '%1' is not found").arg(pluginId);
        return nullptr;
    }
    const PluginAction* action = entry->descriptor.action(actionId);
    if (!action) {
        if (error) *error = tr("the plugin '%1' has no action '%2'").arg(pluginId, actionId);
        return nullptr;
    }
    QString reason = pluginDisabledReason(*entry, *action, doc);
    if (!reason.isEmpty()) {
        if (error) *error = reason;
        return nullptr;
    }
    QList<PluginMetaNote> metaNotes = fillPluginMeta(doc, *action);
    QString saveError;
    if (action->needsSavedDocument() && !savePluginDocument(doc, &saveError)) {
        if (error) *error = saveError;
        return nullptr;
    }
    PluginRunContext context;
    if (!pluginContext(doc, &context, error)) return nullptr;

    PluginRunner* run = new PluginRunner(*entry, action->id, values, context, this);
    run->mutableResult()->metaNotes = metaNotes;
    doc->setPluginRun(run);
    pluginRuns.insert(run, QPointer<CyberiadaSMEditorDocument>(doc));
    if (action->changesDocument()) {
        // the document waits for the result (EDIT-PLUG-22)
        doc->model()->setLocked(true);
        doc->view()->setInteractive(false);
        updateEditActions();
    } else if (action->session) {
        // a session reads the document as it was: select and zoom, no edits (EDIT-PLUG-41)
        doc->model()->setLocked(true);
        updateEditActions();
        updateUndoActions();
        connectSession(run, doc);
    }
    connect(run, &PluginRunner::finished, this, &CyberiadaSMEditorWindow::slotPluginFinished);
    emit pluginRunStarted(run, doc);
    run->start();
    updatePluginActions();
    return run;
}

PluginResult CyberiadaSMEditorWindow::runPluginAndWait(const QString& pluginId, const QString& actionId,
                                                       const PluginValues& values,
                                                       CyberiadaSMEditorDocument* doc)
{
    PluginResult result;
    QString error;
    bool done = false;
    // the result is taken when it is applied; the runner is deleted later
    QMetaObject::Connection taken;
    PluginRunner* run = nullptr;
    taken = connect(this, &CyberiadaSMEditorWindow::pluginRunFinished, this,
                    [&result, &done, &run](PluginRunner* finished) {
                        if (finished != run) return;
                        result = finished->result();
                        done = true;
                    });
    run = runPlugin(pluginId, actionId, values, &error, doc);
    if (!run) {
        disconnect(taken);
        result.state = PluginResult::stateFailed;
        result.error = error;
        return result;
    }
    // a staging failure finishes inside runPlugin, before a loop could catch it
    if (run->isFinished() && !done) {
        result = run->result();
        done = true;
    }
    if (!done) {
        QEventLoop loop;
        QMetaObject::Connection quit = connect(this, &CyberiadaSMEditorWindow::pluginRunFinished, &loop,
                                               [&loop, run](PluginRunner* finished) {
                                                   if (finished == run) loop.quit();
                                               });
        loop.exec();
        disconnect(quit);
    }
    disconnect(taken);
    return result;
}

void CyberiadaSMEditorWindow::slotPluginFinished()
{
    PluginRunner* run = qobject_cast<PluginRunner*>(sender());
    if (!run || !pluginRuns.contains(run)) return;
    QPointer<CyberiadaSMEditorDocument> doc = pluginRuns.take(run);
    CyberiadaSMEditorDocument* target = applyPluginResult(run, doc.data());
    if (pluginDockWidget) pluginDockWidget->runFinished(run, target);
    emit pluginRunFinished(run);
    run->deleteLater();
}

CyberiadaSMEditorDocument* CyberiadaSMEditorWindow::applyPluginResult(PluginRunner* run,
                                                                     CyberiadaSMEditorDocument* doc)
{
    PluginResult* r = run->mutableResult();
    const PluginAction& action = run->action();
    if (doc) {
        doc->setPluginRun(nullptr);
        if (action.changesDocument() || action.session) {
            doc->model()->setLocked(false);
            doc->view()->setInteractive(true);
            updateUndoActions();
        }
    }
    if (r->state == PluginResult::stateDone && !doc) {
        r->state = PluginResult::stateFailed;
        r->error = tr("the document is closed");
    }
    if (r->state != PluginResult::stateDone) {
        updateEditActions();
        return doc;
    }
    QString logLine = QString("# plugin %1:%2").arg(run->plugin().descriptor.id, action.id);
    GestureLog::instance().logAction(logLine);

    CyberiadaSMEditorDocument* target = doc;
    if (action.result == PluginAction::resultReplace) {
        // the mode may have been switched on during the run (EDIT-PLUG-24)
        bool changed = false;
        if (!doc->model()->replaceDocument(r->document, tr("Plugin: %1").arg(action.label), &changed)) {
            r->state = PluginResult::stateFailed;
            r->error = tr("the document is read-only");
            updateEditActions();
            return doc;
        }
        r->changed = changed;
        if (changed && doc == current && GestureLog::instance().isActive()) {
            // a plugin is no script verb: the log goes on from the new document
            GestureLog::instance().endSession();
            GestureLog::instance().startSession(doc->model());
        }
    } else if (action.result == PluginAction::resultNewDocument) {
        // the active clean untitled tab takes it, as Open does (EDIT-PLUG-14)
        target = current && current->isUntitled() && !current->pluginRun() ? current : newDocument();
        target->model()->restoreSnapshot(r->document);
        target->model()->undoStack()->resetClean();
        setCurrentDocument(target);
        slotFitContent();
        r->changed = true;
        r->tab = documents.indexOf(target);
    }

    // a finding links to an element the document has (EDIT-PLUG-15)
    const Cyberiada::LocalDocument* root = target ? target->model()->rootDocument() : nullptr;
    for (PluginDiagnostic& d: r->diagnostics) {
        d.linked = !d.element.isEmpty() && root && target->model()->idToElement(d.element);
    }
    updateEditActions();
    updateTitle();
    return target;
}

void CyberiadaSMEditorWindow::initSessionMenu()
{
    sessionMenu = new QMenu(tr("Session"), this);
    sessionMenu->setObjectName("menuPluginSession");
    actionSessionStep = sessionMenu->addAction(tr("Step"));
    actionSessionStep->setShortcut(QKeySequence(Qt::Key_F10));
    actionSessionRun = sessionMenu->addAction(tr("Run"));
    actionSessionRun->setShortcut(QKeySequence(Qt::Key_F5));
    actionSessionPause = sessionMenu->addAction(tr("Pause"));
    actionSessionPause->setShortcut(QKeySequence(Qt::Key_F6));
    actionSessionRestart = sessionMenu->addAction(tr("Restart"));
    actionSessionStop = sessionMenu->addAction(tr("Stop"));
    actionSessionStop->setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_F5));
    sessionMenu->addSeparator();
    actionSessionPost = sessionMenu->addAction(tr("Post Event"));
    actionSessionLoad = sessionMenu->addAction(tr("Load Stream..."));
    actionSessionEdit = sessionMenu->addAction(tr("Edit Stream..."));
    actionSessionSave = sessionMenu->addAction(tr("Save Log..."));
    connect(actionSessionStep, &QAction::triggered, this, [this]() {
        if (PluginSession* s = currentSession()) { if (s->canStep()) s->step(); }
    });
    connect(actionSessionRun, &QAction::triggered, this, [this]() {
        if (PluginSession* s = currentSession()) { if (s->canRun()) s->run(s->lastSpeed()); }
    });
    connect(actionSessionPause, &QAction::triggered, this, [this]() {
        if (PluginSession* s = currentSession()) { if (s->canPause()) s->pause(); }
    });
    connect(actionSessionRestart, &QAction::triggered, this, [this]() {
        if (PluginSession* s = currentSession()) { if (s->canRestart()) s->restart(); }
    });
    connect(actionSessionStop, &QAction::triggered, this, [this]() {
        if (PluginSession* s = currentSession()) s->stop();
    });
    connect(actionSessionPost, &QAction::triggered, this, [this]() {
        if (PluginSessionTab* tab = currentSessionTab()) {
            pluginDockWidget->show();
            pluginDockWidget->tabs()->setCurrentWidget(tab);
            tab->focusEventLine();
        }
    });
    connect(actionSessionLoad, &QAction::triggered, this, [this]() {
        if (PluginSessionTab* tab = currentSessionTab()) tab->askLoadStream();
    });
    connect(actionSessionEdit, &QAction::triggered, this, [this]() {
        if (PluginSessionTab* tab = currentSessionTab()) tab->askEditStream();
    });
    connect(actionSessionSave, &QAction::triggered, this, [this]() {
        if (PluginSessionTab* tab = currentSessionTab()) tab->askSaveLog();
    });
    updateSessionActions();
}

PluginSession* CyberiadaSMEditorWindow::currentSession() const
{
    return current && current->pluginRun() ? current->pluginRun()->session() : nullptr;
}

PluginSessionTab* CyberiadaSMEditorWindow::currentSessionTab() const
{
    if (!currentSession() || !pluginDockWidget) return nullptr;
    return pluginDockWidget->sessionTab(current->pluginRun());
}

void CyberiadaSMEditorWindow::updateSessionActions()
{
    if (!sessionMenu) return;
    PluginSession* s = currentSession();
    actionSessionStep->setEnabled(s && s->canStep());
    actionSessionRun->setEnabled(s && s->canRun());
    actionSessionPause->setEnabled(s && s->canPause());
    actionSessionRestart->setEnabled(s && s->canRestart());
    actionSessionStop->setEnabled(s && (s->phase() == PluginSession::phaseStarting ||
                                        s->phase() == PluginSession::phaseRunning));
    actionSessionPost->setEnabled(s && s->canPost());
    actionSessionLoad->setEnabled(s && s->canStream());
    actionSessionEdit->setEnabled(s && s->canStream());
    actionSessionSave->setEnabled(s && s->canSave());
}

void CyberiadaSMEditorWindow::connectSession(PluginRunner* run, CyberiadaSMEditorDocument* doc)
{
    PluginSession* session = run->session();
    QPointer<CyberiadaSMEditorDocument> target(doc);
    // the marks go to the document's own scene, whichever tab is active (EDIT-PLUG-40)
    connect(session, &PluginSession::marksChanged, this, [session, target]() {
        if (target) target->scene()->setRunMarks(session->state().active, session->state().fired);
    });
    connect(session, &PluginSession::stateChanged, this, &CyberiadaSMEditorWindow::updateSessionActions);
    connect(session, &PluginSession::modelReceived, this, &CyberiadaSMEditorWindow::updateSessionActions);
    connect(session, &PluginSession::diagnosticsChanged, this, [this, session, target]() {
        QList<PluginDiagnostic> findings = session->diagnostics();
        for (PluginDiagnostic& d: findings) {
            d.linked = target && !d.element.isEmpty() && target->model()->idToElement(d.element);
        }
        if (pluginDockWidget) pluginDockWidget->showDiagnostics(findings, target);
    });
}

void CyberiadaSMEditorWindow::slotPluginsScanned()
{
    if (pluginMenu) pluginMenu->rebuild();
    // a .git directory made while the document is open counts after a scan (EDIT-PLUG-55)
    for (int i = 0; i < documentCount(); i++) {
        if (documentAt(i)) documentAt(i)->dropGitFact();
    }
    updatePluginActions();
}

void CyberiadaSMEditorWindow::updatePluginActions()
{
    updateSessionActions();
    if (!pluginMenu) return;
    for (const QPair<QString, QString>& ids: pluginMenu->actionIds()) {
        QAction* act = pluginMenu->action(ids.first, ids.second);
        const PluginEntry* entry = PluginRegistry::instance().find(ids.first);
        const PluginAction* action = entry ? entry->descriptor.action(ids.second) : nullptr;
        if (!act || !action) continue;
        // a plugin gets a diagram: a text tab offers none yet (docs/MODES.md, stage 5)
        QString reason = (active && !current) ? tr("the plugin works on the diagram tabs")
                                              : pluginDisabledReason(*entry, *action, current);
        act->setEnabled(reason.isEmpty());
        QString description = action->description.isEmpty() ? entry->descriptor.description
                                                           : action->description;
        act->setToolTip(reason.isEmpty() ? description : reason);
    }
}

PluginValues CyberiadaSMEditorWindow::pluginValues(const PluginEntry& plugin,
                                                   const QList<const PluginParameter*>& parameters,
                                                   CyberiadaSMEditorDocument* doc,
                                                   bool* needsDialog) const
{
    SettingsManager& sm = SettingsManager::instance();
    PluginDocumentPaths paths = pluginDocumentPaths(
        doc && doc->hasFile() ? QFileInfo(doc->filePath()).absoluteFilePath() : QString());
    QString key = paths.untitled() ? QString() : pluginDocumentKey(paths.path);
    const QString& id = plugin.descriptor.id;
    PluginValues values;
    bool dialog = false;
    for (const PluginParameter* p: parameters) {
        QString value = pluginDefaultValue(*p, paths);
        bool found = false;
        if (p->scope == PluginParameter::scopeRun) {
            dialog = true;
        } else if (p->scope == PluginParameter::scopePersistent) {
            QString stored = sm.getPluginValue(id, p->id, &found);
            if (found) value = stored;
        } else if (key.isEmpty()) {
            // an untitled document keeps its values for the session (EDIT-PLUG-11)
            QString sessionKey = id + "/" + p->id;
            found = doc && doc->sessionPluginValues().contains(sessionKey);
            if (found) value = doc->sessionPluginValues().value(sessionKey);
            else dialog = true;
        } else {
            QString stored = sm.getPluginDocumentValue(id, key, p->id, &found);
            if (found) value = stored;
            else dialog = true;
        }
        if (p->required && value.isEmpty()) dialog = true;
        values.insert(p->id, value);
    }
    if (needsDialog) *needsDialog = dialog;
    return values;
}

void CyberiadaSMEditorWindow::storePluginValues(const PluginEntry& plugin,
                                                const QList<const PluginParameter*>& parameters,
                                                CyberiadaSMEditorDocument* doc,
                                                const PluginValues& values)
{
    SettingsManager& sm = SettingsManager::instance();
    QString path = doc && doc->hasFile() ? QFileInfo(doc->filePath()).absoluteFilePath() : QString();
    const QString& id = plugin.descriptor.id;
    for (const PluginParameter* p: parameters) {
        if (p->scope == PluginParameter::scopePersistent) {
            sm.setPluginValue(id, p->id, values.value(p->id));
        } else if (p->scope == PluginParameter::scopeDocument) {
            if (path.isEmpty()) {
                if (doc) doc->sessionPluginValues().insert(id + "/" + p->id, values.value(p->id));
            } else {
                sm.setPluginDocumentValue(id, pluginDocumentKey(path), p->id, values.value(p->id));
            }
        }
    }
}

void CyberiadaSMEditorWindow::slotPluginTriggered(const QString& pluginId, const QString& actionId)
{
    const PluginEntry* entry = PluginRegistry::instance().find(pluginId);
    const PluginAction* action = entry ? entry->descriptor.action(actionId) : nullptr;
    if (!action || !current) return;
    QList<const PluginParameter*> parameters = entry->descriptor.actionParameters(*action);
    bool dialog = false;
    PluginValues values = pluginValues(*entry, parameters, current, &dialog);
    if (dialog) {
        QString label = action->label;
        if (label.endsWith("...")) label.chop(3);
        PluginParameterDialog dlg(QString("%1 - %2").arg(entry->descriptor.name, label), parameters,
                                  values, tr("Run"), this);
        if (dlg.exec() != QDialog::Accepted) return;
        values = dlg.values();
        storePluginValues(*entry, parameters, current, values);
    }
    QString error;
    if (!runPlugin(pluginId, actionId, values, &error) && !error.isEmpty()) {
        QMessageBox::warning(this, entry->descriptor.name, error);
    }
}

void CyberiadaSMEditorWindow::slotPluginSettings(const QString& pluginId)
{
    const PluginEntry* entry = PluginRegistry::instance().find(pluginId);
    if (!entry) return;
    QList<const PluginParameter*> parameters;
    for (const PluginParameter& p: entry->descriptor.parameters) {
        if (p.isStored()) parameters.append(&p);
    }
    PluginValues values = pluginValues(*entry, parameters, current);
    PluginParameterDialog dlg(tr("%1 - Settings").arg(entry->descriptor.name), parameters, values,
                              tr("Save"), this);
    if (dlg.exec() == QDialog::Accepted) storePluginValues(*entry, parameters, current, dlg.values());
}

void CyberiadaSMEditorWindow::updateUndoActions()
{
    bool blocked = current ? model()->readOnly() : (active && active->isReadOnly());
    actionUndo->setEnabled(!blocked && undoGroup->canUndo());
    actionRedo->setEnabled(!blocked && undoGroup->canRedo());
}

void CyberiadaSMEditorWindow::slotPluginSettingsChanged()
{
    PluginRegistry::instance().setInterpreterPaths(SettingsManager::instance().getInterpreterPaths());
    PluginRegistry::instance().scan();
}

void CyberiadaSMEditorWindow::slotPluginManager()
{
    PluginManagerDialog dlg(this);
    dlg.exec();
}

void CyberiadaSMEditorWindow::slotDiagnosticActivated(CyberiadaSMEditorDocument* doc,
                                                      const QString& elementId)
{
    if (!doc || !documents.contains(doc)) return;
    setCurrentDocument(doc);
    if (!model()->rootDocument()) return;
    const Cyberiada::Element* element = model()->idToElement(elementId);
    if (!element) return;
    SMView->select(model()->elementToIndex(element));
    QGraphicsItem* item = scene()->getMap().value(elementId.toStdString(), nullptr);
    if (item && sceneView) sceneView->centerOn(item);
}
