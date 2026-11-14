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
#include <QFile>
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
#include <QHeaderView>
#include <QInputDialog>
#include <QTreeWidget>

#include "smeditor_window.h"
#include <QTextBlock>
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
#include "find_bar.h"
#include "shortcut_map.h"
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
    connect(actionClose, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotFileClose);
    connect(actionNextDocument, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotNextDocument);
    connect(actionPreviousDocument, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotPreviousDocument);

    // the find bar under the documents, hidden until Find (EDIT-FIND-1, docs/FIND.md)
    m_findBar = new FindBar(centralwidget);
    centralLayout->addWidget(m_findBar);
    m_findBar->hide();
    connect(actionGoToLine, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotGoToLine);
    connect(actionFind, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotFind);
    connect(actionFindNext, &QAction::triggered, this, [this]() { slotFindStep(1); });
    connect(actionFindPrevious, &QAction::triggered, this, [this]() { slotFindStep(-1); });
    connect(m_findBar, &FindBar::queryChanged, this, &CyberiadaSMEditorWindow::slotFindQueryChanged);
    connect(m_findBar, &FindBar::nextRequested, this, [this]() { slotFindStep(1); });
    connect(m_findBar, &FindBar::previousRequested, this, [this]() { slotFindStep(-1); });
    connect(m_findBar, &FindBar::closed, this, &CyberiadaSMEditorWindow::slotFindClosed);

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
    outputPanel->setObjectName("actionOutputPanel");
    QAction* manager = new QAction(tr("Plugin manager..."), this);
    manager->setObjectName("actionPluginManager");
    connect(manager, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotPluginManager);
    initSessionMenu();
    pluginMenu->setTailActions(QList<QAction*>() << sessionMenu->menuAction() << outputPanel << manager);
    connect(this, &CyberiadaSMEditorWindow::pluginRunStarted, pluginDockWidget, &PluginDock::runStarted);
    connect(pluginDockWidget, &PluginDock::diagnosticActivated,
            this, &CyberiadaSMEditorWindow::slotDiagnosticActivated);
    connect(pluginDockWidget, &PluginDock::lineActivated, this, &CyberiadaSMEditorWindow::slotLineActivated);
    connect(pluginDockWidget, &PluginDock::fileActivated, this, &CyberiadaSMEditorWindow::slotFileActivated);
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
    initShortcuts();
    updateEditActions();
    updateTitle();   // start as a clean "untitled" document
}

// the text typed on the canvas goes into the document before an action reads or
// replaces it (EDIT-KEYS-10)
void CyberiadaSMEditorWindow::commitPendingEdit()
{
    if (current) scene()->commitTextEdit();
}

// the titles of the menus over an action, "Edit > Tools"
static QString menuPath(const QStringList& titles)
{
    QStringList path;
    for (QString title : titles) path << title.remove('&');
    return path.join(" > ");
}

void CyberiadaSMEditorWindow::registerMenuKeys(QMenu* menu, const QStringList& titles)
{
    QStringList path = titles;
    path << menu->title();
    for (QAction* action : menu->actions()) {
        if (action->isSeparator()) continue;
        if (action->menu()) registerMenuKeys(action->menu(), path);
        else registerKeys(action, menuPath(path));
    }
}

// an action of the window itself: its key acts while its toolbar or menu is hidden
// (EDIT-KEYS-6); the unnamed entries come and go and have no keys (EDIT-KEYS-1)
void CyberiadaSMEditorWindow::registerKeys(QAction* action, const QString& path)
{
    if (action->objectName().isEmpty()) return;
    addAction(action);
    m_shortcuts->add(action, path);
}

// the keys of the built-in actions, the default or the stored ones (docs/SHORTCUTS.md)
void CyberiadaSMEditorWindow::initShortcuts()
{
    m_shortcuts = new ShortcutMap(this);
    for (QAction* top : QMainWindow::menuBar()->actions()) {
        if (top->menu()) registerMenuKeys(top->menu(), QStringList());
    }
    QStringList plugins = QStringList() << pluginMenu->menu()->title();
    registerMenuKeys(sessionMenu, plugins);
    // the actions shown on a toolbar only
    QString tools = menuPath(QStringList() << menuEdit->title() << menuTools->title());
    registerKeys(actionSelectTool, tools);
    registerKeys(actionPan, tools);
    registerKeys(actionZoomTool, tools);
    registerKeys(actionZoomToSM, menuPath(QStringList() << menuView->title()));
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
    connect(doc, &EditorDocument::findResultChanged, this,
            [this, doc](int current, int total, bool more) {
                if (doc == active) m_findBar->setResult(current, total, more);
            });
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
    // Save is there for the text being typed (EDIT-IO-7): an edit began, or ended
    // with the focus
    connect(doc->scene(), &CyberiadaSMEditorScene::textEditChanged, this,
            [this, doc]() { if (doc == current) updateEditActions(); });
    connect(doc->scene(), &QGraphicsScene::focusItemChanged, this,
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
    commitPendingEdit();
    if (!doc) return false;
    // a running plugin is cancelled with its document (EDIT-PLUG-23)
    if (doc->pluginRun() && !discard && !qApp->property("batchMode").toBool() &&
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
    if (doc->pluginRun()) doc->pluginRun()->abort();
    // a stack with steps signals its clean state as it goes
    doc->undoStack()->disconnect(this);
    doc->disconnect(this);
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
    commitPendingEdit();
    if (!tab || tab == active) return;
    bool switched = (active != nullptr);
    if (current) unbindDocument(current);
    // the marks are the active tab's: the one left drops them (EDIT-FIND-9)
    bool finding = m_findBar && !m_findBar->isHidden();
    if (active && finding) active->clearFind(false);
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

    SMView->setVisible(!currentDiff());
    m_changeList->setVisible(currentDiff() != nullptr);
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
        fillChangeList();
    }
    updateTitle();
    updateEditActions();
    if (finding && active->canFind()) active->setFindQuery(m_findBar->query(), false);
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

QList<MetaChoiceMenu::Entry> CyberiadaSMEditorWindow::languageEntries() const
{
    QList<MetaChoiceMenu::Entry> languages;
    languages.append({tr("Undefined"), QString()});
    for (const QString& name : CodeStyle::instance().languages()) {
        languages.append({name, name});
    }
    return languages;
}

void CyberiadaSMEditorWindow::initChoiceMenus()
{
    // an alias checks its language
    languageChoice = new MetaChoiceMenu(menuLanguage, languageEntries(),
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
    actionModeDiff->setData(QString(MODE_DIFF));
    modeGroup->addAction(actionModeDiff);
    // the text modes: Plain text, Markdown, Code > the languages (EDIT-MODE-3/13)
    menuMode->addSeparator();
    menuNewText = menuNew->addMenu(tr("Text file"));
    menuNewText->setObjectName("menuNewText");
    rebuildTextModeMenus();
    // the languages of the plugins come and go with a scan (EDIT-PLUG-60)
    connect(&CodeStyle::instance(), &CodeStyle::languagesChanged, this, [this]() {
        rebuildTextModeMenus();
        if (languageChoice) languageChoice->setEntries(languageEntries());
        for (EditorDocument* doc : documents) {
            if (TextDocument* text = qobject_cast<TextDocument*>(doc)) text->refreshLanguage();
        }
        syncChoiceMenus();
        updateModeActions();
    });
    connect(modeGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        commitPendingEdit();
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
    // the diff tabs (EDIT-MODE-10..12)
    connect(actionCompare, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotCompare);
    connect(actionGeometry, &QAction::triggered, this, [this](bool on) {
        if (DiffDocument* diff = currentDiff()) {
            diff->setGeometry(on);
            fillChangeList();
        }
    });
    connect(actionRefresh, &QAction::triggered, this, [this]() {
        QString error;
        if (DiffDocument* diff = currentDiff()) {
            if (!refreshDiff(diff, &error) && !qApp->property("batchMode").toBool()) {
                QMessageBox::warning(this, tr("Refresh"), error);
            }
        }
    });
    // the change list shares the structure tree dock, shown for a diff tab
    m_changeList = new QTreeWidget(treeDockContents);
    m_changeList->setObjectName("changeList");
    m_changeList->setHeaderLabels(QStringList() << tr("Element") << tr("Change"));
    m_changeList->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_changeList->hide();
    treeDockLayout->addWidget(m_changeList);
    connect(m_changeList, &QTreeWidget::itemActivated, this, &CyberiadaSMEditorWindow::slotChangeActivated);
    connect(m_changeList, &QTreeWidget::itemClicked, this, &CyberiadaSMEditorWindow::slotChangeActivated);
    connect(actionNewFA, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotFileNewFA);
    // the toolbar button of a new Markdown text, also its entry in File > New > Text file
    actionNewMarkdown->setData(QString(MODE_MARKDOWN));
    connect(actionNewMarkdown, &QAction::triggered, this, [this]() { slotFileNewText(MODE_MARKDOWN); });
    // View > FA symbols is a preference of every tab (EDIT-MODE-8)
    SettingsManager& sm = SettingsManager::instance();
    actionFASymbols->setChecked(sm.getShowFASymbols());
    connect(actionFASymbols, &QAction::triggered, &sm, &SettingsManager::setShowFASymbols);
    connect(&sm, &SettingsManager::faSymbolsChanged, actionFASymbols, &QAction::setChecked);
}

void CyberiadaSMEditorWindow::rebuildTextModeMenus()
{
    for (QAction* action : textModeActions) {
        modeGroup->removeAction(action);
        delete action;
    }
    textModeActions.clear();
    delete menuMode->findChild<QMenu*>("menuModeCode");
    menuNewText->clear();
    QMenu* code = nullptr;
    for (const QString& mode : textModes()) {
        bool language = mode != MODE_TEXT && mode != MODE_MARKDOWN;
        if (language && !code) {
            code = menuMode->addMenu(tr("Code"));
            code->setObjectName("menuModeCode");
        }
        QAction* action = new QAction(modeName(mode), this);
        action->setCheckable(true);
        action->setData(mode);
        (language ? code : menuMode)->addAction(action);
        modeGroup->addAction(action);
        textModeActions.append(action);
        if (mode == MODE_MARKDOWN) {
            menuNewText->addAction(actionNewMarkdown);
            continue;
        }
        QAction* create = menuNewText->addAction(modeName(mode));
        create->setData(mode);
        connect(create, &QAction::triggered, this, [this, mode]() { slotFileNewText(mode); });
    }
}

void CyberiadaSMEditorWindow::updateModeActions()
{
    if (!active || !modeGroup) return;
    QString mode = active->mode();
    bool text = active->traits().text;
    // a diagram reaches the diagram modes, a text the text ones, a diff none (EDIT-MODE-3)
    auto kind = [](const QString& id) { return isTextMode(id) ? 1 : id == MODE_DIFF ? 2 : 0; };
    for (QAction* action : modeGroup->actions()) {
        QString id = action->data().toString();
        action->setChecked(id == mode);
        action->setEnabled(kind(id) == kind(mode));
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
    // a diff tab is opened by Compare with... and stays one
    if (tab->mode() == MODE_DIFF || mode == MODE_DIFF) {
        if (error) *error = tr("a diff tab is opened by Document > Compare with...");
        return false;
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
    commitPendingEdit();
    CyberiadaSMEditorDocument* doc = newDocument();
    doc->model()->setNewDocumentFA(true);
    setCurrentDocument(doc);
}

void CyberiadaSMEditorWindow::slotCheck()
{
    commitPendingEdit();
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
        << actionCheck << actionExport << actionCompare << actionGeometry << actionRefresh;
    for (QAction* off : diagramOnly) off->setEnabled(false);
    // a Markdown text exports its rendered pages (EDIT-MODE-18)
    actionExport->setEnabled(text->mode() == MODE_MARKDOWN);
    for (QAction* tool : toolActMap) tool->setEnabled(false);
    enableCanvasSwitches(false);
    actionSave->setEnabled(!text->isClean() && !readOnly);
    actionSaveAs->setEnabled(true);
    updateModeActions();
    updateUndoActions();
    updatePluginActions();
}

void CyberiadaSMEditorWindow::enableCanvasSwitches(bool on)
{
    const QList<QAction*> switches = QList<QAction*>()
        << actionGridVisibility << actionSnapMode << actionServiceObjects
        << actionTransitionText << actionFASymbols << actionComponentComments;
    for (QAction* action : switches) action->setEnabled(on);
}

void CyberiadaSMEditorWindow::updateFindActions()
{
    bool on = active && active->canFind();
    actionFind->setEnabled(on);
    actionFindNext->setEnabled(on);
    actionFindPrevious->setEnabled(on);
    if (m_findBar) m_findBar->setSearchable(on);
    // the lines of a source shown, not of a rendered page (EDIT-KEYS-7)
    TextDocument* text = currentText();
    actionGoToLine->setEnabled(text && !text->showsRendered());
}

void CyberiadaSMEditorWindow::slotGoToLine()
{
    TextDocument* text = currentText();
    if (!text || text->showsRendered()) return;
    bool ok = false;
    int line = QInputDialog::getInt(this, tr("Go to Line"), tr("Line number:"),
                                    text->currentLine(), 1, text->lineCount(), 1, &ok);
    if (ok) text->goToLine(line);
}

void CyberiadaSMEditorWindow::slotFind()
{
    if (!active || !active->canFind()) return;
    bool shown = !m_findBar->isHidden();
    QString before = m_findBar->query().text;
    m_findBar->start(active->findSeed());
    // an unchanged field emits nothing: a bar shown again searches here
    if (!shown && m_findBar->query().text == before) slotFindQueryChanged(m_findBar->query());
}

void CyberiadaSMEditorWindow::slotFindStep(int step)
{
    if (!active || !active->canFind()) return;
    // a hidden bar is shown first, its search making the match after the place current
    if (m_findBar->isHidden()) {
        slotFind();
        return;
    }
    active->moveFind(step);
}

void CyberiadaSMEditorWindow::slotFindQueryChanged(const FindQuery& query)
{
    if (active && active->canFind() && !m_findBar->isHidden()) active->setFindQuery(query, true);
}

void CyberiadaSMEditorWindow::slotFindClosed()
{
    if (active && active->canFind()) active->clearFind(true);
}

void CyberiadaSMEditorWindow::updateDiffActions()
{
    DiffDocument* diff = currentDiff();
    // nothing to edit, save or export; the view zooms, the comparison refreshes
    const QList<QAction*> off = QList<QAction*>()
        << actionCut << actionCopy << actionPaste << actionDeleteElement
        << actionCenterHorizontally << actionCenterVertically << actionReconstructGeometry
        << actionUpdateGeometry << actionDocumentProperties << actionCheck << actionExport
        << actionSave << actionSaveAs << actionCompare << actionZoomToSM;
    for (QAction* action : off) action->setEnabled(false);
    for (QAction* tool : toolActMap) tool->setEnabled(false);
    enableCanvasSwitches(true);
    actionZoomIn->setEnabled(true);
    actionZoomOut->setEnabled(true);
    actionFitContent->setEnabled(true);
    actionGeometry->setEnabled(true);
    actionGeometry->setChecked(diff->geometry());
    actionRefresh->setEnabled(true);
    updateModeActions();
    updateUndoActions();
    updatePluginActions();
}

void CyberiadaSMEditorWindow::fillChangeList()
{
    if (!m_changeList) return;
    m_changeList->clear();
    DiffDocument* diff = currentDiff();
    if (!diff) return;
    QTreeWidgetItem* group = nullptr;
    QString machine;
    for (const DiffRow& row : diff->rows()) {
        QString change = row.change == "added" ? tr("added") :
                         row.change == "removed" ? tr("removed") : row.change;
        QTreeWidgetItem* item = nullptr;
        if (row.machine.isEmpty()) {
            item = new QTreeWidgetItem(m_changeList);
        } else {
            if (!group || row.machine != machine) {
                machine = row.machine;
                group = new QTreeWidgetItem(m_changeList, QStringList() << machine);
                group->setExpanded(true);
            }
            // the machine's own row is its group
            item = row.element == row.machine ? group : new QTreeWidgetItem(group);
        }
        item->setText(0, row.element);
        item->setText(1, change);
        item->setData(0, Qt::UserRole, row.first);
        item->setData(1, Qt::UserRole, row.second);
        QColor color = row.change == "added" ? QColor(DIFF_ADDED_COLOR) :
                       row.change == "removed" ? QColor(DIFF_REMOVED_COLOR) : QColor(DIFF_CHANGED_COLOR);
        item->setForeground(1, color);
    }
    m_changeList->expandAll();
}

void CyberiadaSMEditorWindow::slotChangeActivated(QTreeWidgetItem* item)
{
    DiffDocument* diff = currentDiff();
    if (!diff || !item) return;
    diff->showElement(item->data(0, Qt::UserRole).toString(), item->data(1, Qt::UserRole).toString());
}

bool CyberiadaSMEditorWindow::originBuffer(const DiffOrigin& origin, const std::string& last,
                                           std::string* buffer, QString* error) const
{
    if (origin.kind == DiffOrigin::originTab) {
        CyberiadaSMEditorDocument* doc = qobject_cast<CyberiadaSMEditorDocument*>(origin.tab.data());
        // a closed source tab leaves the side as it was taken (EDIT-MODE-12)
        if (!doc) {
            *buffer = last;
            return true;
        }
        bool ok = true;
        *buffer = doc->model()->snapshot(&ok);
        if (!ok && error) *error = tr("the document %1 cannot be encoded").arg(doc->title());
        return ok;
    }
    QFile file(origin.path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = tr("Cannot read %1:\n%2").arg(origin.path, file.errorString());
        return false;
    }
    QByteArray bytes = file.readAll();
    *buffer = std::string(bytes.constData(), size_t(bytes.size()));
    return true;
}

bool CyberiadaSMEditorWindow::refreshDiff(DiffDocument* diff, QString* error, bool wait)
{
    if (!diff) return false;
    // a plugin base: the action again on the source tab, the same values (EDIT-PLUG-58)
    const DiffOrigin& origin = diff->base();
    if (origin.kind == DiffOrigin::originPlugin) {
        if (!origin.tab) {
            if (error) *error = tr("the tab the plugin ran on is closed");
            return false;
        }
        if (!wait) return runPlugin(origin.pluginId, origin.actionId, origin.values, error, origin.tab, diff);
        PluginResult result = runPluginAndWait(origin.pluginId, origin.actionId, origin.values, origin.tab, diff);
        if (result.state != PluginResult::stateDone) {
            if (error) *error = result.error;
            return false;
        }
        return true;
    }
    std::string base, target;
    if (!originBuffer(diff->base(), diff->baseBuffer(), &base, error)) return false;
    if (!originBuffer(diff->target(), diff->targetBuffer(), &target, error)) return false;
    diff->setSides(base, target);
    if (diff == active) {
        fillChangeList();
        updateEditActions();
    }
    return true;
}

DiffDocument* CyberiadaSMEditorWindow::compareDocument(CyberiadaSMEditorDocument* source,
                                                        const DiffOrigin& base, QString* error)
{
    if (!source) {
        if (error) *error = tr("the active tab holds no diagram");
        return nullptr;
    }
    DiffDocument* diff = new DiffDocument(this);
    diff->base() = base;
    diff->target().kind = DiffOrigin::originTab;
    diff->target().tab = source;
    diff->target().label = source->title();
    if (!refreshDiff(diff, error)) {
        delete diff;
        return nullptr;
    }
    addTab(diff);
    connect(diff, &DiffDocument::compared, this, [this, diff]() { if (diff == active) fillChangeList(); });
    setCurrentDocument(diff);
    QTimer::singleShot(0, diff, &DiffDocument::fit);
    return diff;
}

void CyberiadaSMEditorWindow::slotCompare()
{
    commitPendingEdit();
    if (!current) return;
    QStringList labels;
    QList<DiffOrigin> origins;
    // the saved file of a modified document, the other diagram tabs, a file
    if (current->hasFile() && !current->isClean()) {
        DiffOrigin saved;
        saved.kind = DiffOrigin::originSaved;
        saved.tab = current;
        saved.path = current->filePath();
        saved.label = tr("saved");
        labels << tr("The saved file");
        origins << saved;
    }
    for (int i = 0; i < documentCount(); i++) {
        CyberiadaSMEditorDocument* doc = documentAt(i);
        if (!doc || doc == current) continue;
        DiffOrigin tab;
        tab.kind = DiffOrigin::originTab;
        tab.tab = doc;
        tab.label = doc->title();
        labels << doc->title() + doc->modeMarker();
        origins << tab;
    }
    labels << tr("A file...");
    bool ok = false;
    QString choice = QInputDialog::getItem(this, tr("Compare with"), tr("The base of the comparison:"),
                                           labels, 0, false, &ok);
    if (!ok) return;
    int index = labels.indexOf(choice);
    DiffOrigin base;
    if (index >= 0 && index < origins.size()) {
        base = origins.at(index);
    } else {
        QString path = QFileDialog::getOpenFileName(this, tr("Compare with"),
                                                    SettingsManager::instance().getLastDirectory(),
                                                    tr("CyberiadaML graph (*.graphml)") + ";;" + tr("All files (*)"),
                                                    nullptr, QFileDialog::DontUseNativeDialog);
        if (path.isEmpty()) return;
        base.kind = DiffOrigin::originFile;
        base.path = path;
        base.label = QFileInfo(path).fileName();
    }
    QString error;
    if (!compareDocument(current, base, &error)) {
        QMessageBox::warning(this, tr("Compare with"), error);
    }
}

void CyberiadaSMEditorWindow::updateEditActions()
{
    if (!active) return;   // the documents are gone (destructor)
    updateFindActions();
    if (!current) {
        if (currentText()) updateTextActions();
        else updateDiffActions();
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
    // a diagram is compared; Geometry and Refresh belong to a diff tab (EDIT-MODE-15)
    actionCompare->setEnabled(true);
    actionGeometry->setEnabled(false);
    actionGeometry->setChecked(false);
    actionRefresh->setEnabled(false);
    actionSave->setEnabled(docOpen && (modified || scene()->textEdited()) && !readOnly);
    actionSaveAs->setEnabled(docOpen);
    actionExport->setEnabled(docOpen);
    enableCanvasSwitches(true);
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
        // a stack with steps signals its clean state as it goes, to a window half gone
        doc->undoStack()->disconnect(this);
        doc->disconnect(this);
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
    commitPendingEdit();
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
    // the contents, not the column: the last column is stretched over the viewport
    int contents = static_cast<QAbstractItemView*>(SMView)->sizeHintForColumn(0);
    int treeWidth = contents + 2 * SMView->frameWidth() +
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
    actionStructureTree->setObjectName("actionStructureTree");
    actionProperties = propertiesDock->toggleViewAction();
    actionProperties->setObjectName("actionPropertiesPane");
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
    commitPendingEdit();
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
    commitPendingEdit();
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
    // an edit that changed nothing leaves nothing to save
    if (current->isClean()) return;
    QString error;
    if (!current->save(&error)) {
        QMessageBox::critical(this, tr("Save State Machine"),
                              tr("Cannot save the document:\n") + error);
    }
}

void CyberiadaSMEditorWindow::slotFileSaveAs()
{
    commitPendingEdit();
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
    commitPendingEdit();
    // a Markdown tab exports its rendered text (EDIT-MODE-18), another text tab nothing
    if (TextDocument* text = currentText()) {
        if (text->mode() == MODE_MARKDOWN) exportMarkdown(text);
        return;
    }
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

void CyberiadaSMEditorWindow::exportMarkdown(TextDocument* text)
{
    // format names only, never translated, as in the image export
    CyberiadaFileDialog dlg(this, tr("Export the rendered text as PDF"), QString("PDF (*.pdf)"));
    dlg.setFileMode(QFileDialog::AnyFile);
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setLabelText(QFileDialog::Accept, tr("Export"));
    dlg.setDefaultSuffix("pdf");
    if (text->hasFile()) dlg.selectFile(QFileInfo(text->filePath()).completeBaseName());
    if (dlg.exec() != QDialog::Accepted) return;
    QString fileName = dlg.selectedFile();
    if (fileName.isEmpty()) return;
    QString error;
    if (!text->exportPdf(fileName, &error)) {
        QMessageBox::critical(this, tr("Error"), error);
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

    // a diff tab zooms both of its sides
    connect(actionZoomIn, &QAction::triggered, this, [this]() {
        if (sceneView) sceneView->zoomIn();
        else if (currentDiff()) currentDiff()->zoomIn();
    });
    connect(actionZoomOut, &QAction::triggered, this, [this]() {
        if (sceneView) sceneView->zoomOut();
        else if (currentDiff()) currentDiff()->zoomOut();
    });
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
    if (DiffDocument* diff = currentDiff()) {
        diff->fit();
        return;
    }
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
    PreferencesDialog dlg(this, m_shortcuts);
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
    // a focused point goes, not the transition selected with it (EDIT-KEYS-9)
    DotSignal* point = dynamic_cast<DotSignal*>(scene()->focusItem());
    if (point && point->isVisible() && point->isDeleteable()) {
        point->deleteDot();
        return;
    }
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
}

void CyberiadaSMEditorWindow::slotReconstructGeometry()
{
    commitPendingEdit();
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
    commitPendingEdit();
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
                                                      EditorDocument* doc) const
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
    // the mode before the when tokens (EDIT-PLUG-56)
    if (!action.modes.contains(doc->mode())) {
        QStringList names;
        for (const QString& mode: action.modes) {
            if (isKnownMode(mode)) names.append(modeName(mode));
        }
        return tr("the action works in: %1").arg(names.join(", "));
    }
    if (qobject_cast<DiffDocument*>(doc)) return tr("a diff tab gives a plugin no input");
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
    CyberiadaSMEditorDocument* diagram = qobject_cast<CyberiadaSMEditorDocument*>(doc);
    if (diagram && facts.machines > 0 && !diagram->model()->readOnly()) fills = action.meta;
    return PluginConditions::check(action.when, action.reason, facts, fills);
}

bool CyberiadaSMEditorWindow::pluginContext(EditorDocument* doc, PluginRunContext* context,
                                            QString* error) const
{
    context->documentPath = doc->hasFile() ? QFileInfo(doc->filePath()).absoluteFilePath() : QString();
    context->modified = !doc->isClean();
    // a text: input.<ext> in UTF-8, the format its mode (EDIT-PLUG-57)
    if (TextDocument* text = qobject_cast<TextDocument*>(doc)) {
        context->text = true;
        context->extension = modeExtension(text->mode());
        context->documentFormat = text->mode();
        context->input = text->text().toUtf8().toStdString();
        return true;
    }
    CyberiadaSMEditorDocument* diagram = qobject_cast<CyberiadaSMEditorDocument*>(doc);
    if (!diagram) {
        if (error) *error = tr("a diff tab gives a plugin no input");
        return false;
    }
    CyberiadaSMModel* m = diagram->model();
    bool ok = true;
    context->input = m->snapshot(&ok);
    if (!ok) {
        if (error) *error = tr("the document cannot be encoded");
        return false;
    }
    context->documentFormat = documentFormatName(m->rootDocument());
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
    context->selection = diagram->scene()->selectedIds();
    return true;
}

QList<PluginMetaNote> CyberiadaSMEditorWindow::fillPluginMeta(EditorDocument* doc,
                                                              const PluginAction& action)
{
    QList<PluginMetaNote> notes;
    if (action.meta.isEmpty()) return notes;
    // a text has no metainformation to fill
    CyberiadaSMEditorDocument* diagram = qobject_cast<CyberiadaSMEditorDocument*>(doc);
    CyberiadaSMModel* m = diagram ? diagram->model() : nullptr;
    const Cyberiada::LocalDocument* root = m ? m->rootDocument() : nullptr;
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

bool CyberiadaSMEditorWindow::savePluginDocument(EditorDocument* doc, QString* error)
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

void CyberiadaSMEditorWindow::setPluginLock(EditorDocument* doc, bool on, bool session)
{
    if (CyberiadaSMEditorDocument* diagram = qobject_cast<CyberiadaSMEditorDocument*>(doc)) {
        diagram->model()->setLocked(on);
        // a session keeps select and zoom (EDIT-PLUG-41)
        diagram->view()->setInteractive(!on || session);
    } else if (TextDocument* text = qobject_cast<TextDocument*>(doc)) {
        text->setLocked(on);
    }
}

QString CyberiadaSMEditorWindow::pluginBaseLabel(const PluginEntry& plugin, const PluginAction& action,
                                                 const PluginValues& values) const
{
    QStringList parts;
    for (const PluginParameter* p: plugin.descriptor.actionParameters(action)) {
        if (p->type != PluginParameter::typeString) continue;
        QString value = values.value(p->id).trimmed();
        if (!value.isEmpty()) parts.append(value);
    }
    if (!parts.isEmpty()) return parts.join(", ");
    QString label = action.label;
    if (label.endsWith("...")) label.chop(3);
    return label;
}

PluginRunner* CyberiadaSMEditorWindow::runPlugin(const QString& pluginId, const QString& actionId,
                                                 const PluginValues& values, QString* error,
                                                 EditorDocument* doc, DiffDocument* refresh)
{
    if (!doc) doc = active;
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
    pluginRuns.insert(run, QPointer<EditorDocument>(doc));
    if (refresh) diffRefreshes.insert(run, QPointer<DiffDocument>(refresh));
    if (action->changesDocument()) {
        // the document waits for the result (EDIT-PLUG-22)
        setPluginLock(doc, true, false);
        updateEditActions();
    } else if (action->session) {
        // a session reads the document as it was: select and zoom, no edits (EDIT-PLUG-41)
        setPluginLock(doc, true, true);
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
                                                       EditorDocument* doc, DiffDocument* refresh)
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
    run = runPlugin(pluginId, actionId, values, &error, doc, refresh);
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
    QPointer<EditorDocument> doc = pluginRuns.take(run);
    EditorDocument* target = applyPluginResult(run, doc.data());
    if (pluginDockWidget) pluginDockWidget->runFinished(run, target);
    emit pluginRunFinished(run);
    run->deleteLater();
}

EditorDocument* CyberiadaSMEditorWindow::applyPluginResult(PluginRunner* run, EditorDocument* doc)
{
    PluginResult* r = run->mutableResult();
    const PluginAction& action = run->action();
    QPointer<DiffDocument> refresh = diffRefreshes.take(run);
    if (doc) {
        doc->setPluginRun(nullptr);
        if (action.changesDocument() || action.session) {
            setPluginLock(doc, false, false);
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

    EditorDocument* target = doc;
    CyberiadaSMEditorDocument* diagram = qobject_cast<CyberiadaSMEditorDocument*>(doc);
    TextDocument* text = qobject_cast<TextDocument*>(doc);
    QString step = tr("Plugin: %1").arg(action.label);
    if (action.result == PluginAction::resultReplace && text) {
        // the whole text as one step (EDIT-PLUG-57)
        if (!text->isEditable()) {
            r->state = PluginResult::stateFailed;
            r->error = tr("the document is read-only");
            updateEditActions();
            return doc;
        }
        r->changed = text->replaceText(QString::fromUtf8(r->document.data(), int(r->document.size())), step);
    } else if (action.result == PluginAction::resultReplace) {
        // the mode may have been switched on during the run (EDIT-PLUG-24)
        bool changed = false;
        if (!diagram->model()->replaceDocument(r->document, step, &changed)) {
            r->state = PluginResult::stateFailed;
            r->error = tr("the document is read-only");
            updateEditActions();
            return doc;
        }
        r->changed = changed;
        if (changed && doc == current && GestureLog::instance().isActive()) {
            // a plugin is no script verb: the log goes on from the new document
            GestureLog::instance().endSession();
            GestureLog::instance().startSession(diagram->model());
        }
    } else if (action.result == PluginAction::resultNewDocument && text) {
        // a text of the same mode, in the active empty text tab or a new one
        TextDocument* untitled = qobject_cast<TextDocument*>(active);
        TextDocument* created = untitled && untitled->isUntitled() && !untitled->pluginRun() ?
            untitled : newTextDocument(text->mode());
        created->setMode(text->mode());
        created->setText(QString::fromUtf8(r->document.data(), int(r->document.size())));
        created->undoStack()->resetClean();
        setCurrentDocument(created);
        r->changed = true;
        r->tab = documents.indexOf(created);
        target = created;
    } else if (action.result == PluginAction::resultNewDocument) {
        // the active clean untitled tab takes it, as Open does (EDIT-PLUG-14)
        CyberiadaSMEditorDocument* created = current && current->isUntitled() && !current->pluginRun() ?
            current : newDocument();
        created->model()->restoreSnapshot(r->document);
        created->model()->undoStack()->resetClean();
        setCurrentDocument(created);
        slotFitContent();
        r->changed = true;
        r->tab = documents.indexOf(created);
        target = created;
    } else if (action.result == PluginAction::resultDiff) {
        // the output is the base, the run's input the target (EDIT-PLUG-58)
        DiffDocument* diff = refresh.data();
        if (!diff) {
            diff = new DiffDocument(this);
            diff->base().kind = DiffOrigin::originPlugin;
            diff->base().tab = doc;
            diff->base().pluginId = run->plugin().descriptor.id;
            diff->base().actionId = action.id;
            diff->base().values = run->values();
            diff->base().label = pluginBaseLabel(run->plugin(), action, run->values());
            diff->target().kind = DiffOrigin::originTab;
            diff->target().tab = doc;
            diff->target().label = doc->title();
            diff->setSides(r->document, run->runContext().input);
            addTab(diff);
            connect(diff, &DiffDocument::compared, this, [this, diff]() { if (diff == active) fillChangeList(); });
            setCurrentDocument(diff);
            QTimer::singleShot(0, diff, &DiffDocument::fit);
        } else {
            diff->setSides(r->document, run->runContext().input);
            if (diff == active) fillChangeList();
        }
        r->tab = documents.indexOf(diff);
        target = diff;
    }

    // a finding links to an element the document has (EDIT-PLUG-15)
    CyberiadaSMEditorDocument* linked = qobject_cast<CyberiadaSMEditorDocument*>(target);
    const Cyberiada::LocalDocument* root = linked ? linked->model()->rootDocument() : nullptr;
    for (PluginDiagnostic& d: r->diagnostics) {
        d.linked = !d.element.isEmpty() && root && linked->model()->idToElement(d.element);
    }
    updateEditActions();
    updateTitle();
    return target;
}

void CyberiadaSMEditorWindow::initSessionMenu()
{
    sessionMenu = new QMenu(tr("Session"), this);
    sessionMenu->setObjectName("menuPluginSession");
    // the names are the ids of the keys (docs/SHORTCUTS.md)
    actionSessionStep = sessionMenu->addAction(tr("Step"));
    actionSessionStep->setObjectName("actionSessionStep");
    actionSessionRun = sessionMenu->addAction(tr("Run"));
    actionSessionRun->setObjectName("actionSessionRun");
    actionSessionPause = sessionMenu->addAction(tr("Pause"));
    actionSessionPause->setObjectName("actionSessionPause");
    actionSessionRestart = sessionMenu->addAction(tr("Restart"));
    actionSessionRestart->setObjectName("actionSessionRestart");
    actionSessionStop = sessionMenu->addAction(tr("Stop"));
    actionSessionStop->setObjectName("actionSessionStop");
    sessionMenu->addSeparator();
    actionSessionPost = sessionMenu->addAction(tr("Post Event"));
    actionSessionPost->setObjectName("actionSessionPost");
    actionSessionLoad = sessionMenu->addAction(tr("Load Stream..."));
    actionSessionLoad->setObjectName("actionSessionLoad");
    actionSessionEdit = sessionMenu->addAction(tr("Edit Stream..."));
    actionSessionEdit->setObjectName("actionSessionEdit");
    actionSessionSave = sessionMenu->addAction(tr("Save Log..."));
    actionSessionSave->setObjectName("actionSessionSave");
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
    return active && active->pluginRun() ? active->pluginRun()->session() : nullptr;
}

PluginSessionTab* CyberiadaSMEditorWindow::currentSessionTab() const
{
    if (!currentSession() || !pluginDockWidget) return nullptr;
    return pluginDockWidget->sessionTab(active->pluginRun());
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

void CyberiadaSMEditorWindow::connectSession(PluginRunner* run, EditorDocument* doc)
{
    PluginSession* session = run->session();
    // the marks and the links need a diagram; a text session has neither
    QPointer<CyberiadaSMEditorDocument> target(qobject_cast<CyberiadaSMEditorDocument*>(doc));
    QPointer<EditorDocument> source(doc);
    // the marks go to the document's own scene, whichever tab is active (EDIT-PLUG-40)
    connect(session, &PluginSession::marksChanged, this, [session, target]() {
        if (target) target->scene()->setRunMarks(session->state().active, session->state().fired);
    });
    connect(session, &PluginSession::stateChanged, this, &CyberiadaSMEditorWindow::updateSessionActions);
    connect(session, &PluginSession::modelReceived, this, &CyberiadaSMEditorWindow::updateSessionActions);
    connect(session, &PluginSession::diagnosticsChanged, this, [this, session, target, source]() {
        QList<PluginDiagnostic> findings = session->diagnostics();
        for (PluginDiagnostic& d: findings) {
            d.linked = target && !d.element.isEmpty() && target->model()->idToElement(d.element);
        }
        if (pluginDockWidget) pluginDockWidget->showDiagnostics(findings, source);
    });
}

void CyberiadaSMEditorWindow::slotPluginsScanned()
{
    if (pluginMenu) pluginMenu->rebuild();
    // a .git directory made while the document is open counts after a scan (EDIT-PLUG-55)
    for (EditorDocument* doc : documents) doc->dropGitFact();
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
        QString reason = pluginDisabledReason(*entry, *action, active);
        act->setEnabled(reason.isEmpty());
        QString description = action->description.isEmpty() ? entry->descriptor.description
                                                           : action->description;
        act->setToolTip(reason.isEmpty() ? description : reason);
    }
}

PluginValues CyberiadaSMEditorWindow::pluginValues(const PluginEntry& plugin,
                                                   const QList<const PluginParameter*>& parameters,
                                                   EditorDocument* doc,
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
                                                EditorDocument* doc,
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
    commitPendingEdit();
    const PluginEntry* entry = PluginRegistry::instance().find(pluginId);
    const PluginAction* action = entry ? entry->descriptor.action(actionId) : nullptr;
    if (!action || !active) return;
    QList<const PluginParameter*> parameters = entry->descriptor.actionParameters(*action);
    bool dialog = false;
    PluginValues values = pluginValues(*entry, parameters, active, &dialog);
    if (dialog) {
        QString label = action->label;
        if (label.endsWith("...")) label.chop(3);
        PluginParameterDialog dlg(QString("%1 - %2").arg(entry->descriptor.name, label), parameters,
                                  values, tr("Run"), this);
        if (dlg.exec() != QDialog::Accepted) return;
        values = dlg.values();
        storePluginValues(*entry, parameters, active, values);
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
    PluginValues values = pluginValues(*entry, parameters, active);
    PluginParameterDialog dlg(tr("%1 - Settings").arg(entry->descriptor.name), parameters, values,
                              tr("Save"), this);
    if (dlg.exec() == QDialog::Accepted) storePluginValues(*entry, parameters, active, dlg.values());
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

void CyberiadaSMEditorWindow::slotDiagnosticActivated(EditorDocument* doc,
                                                      const QString& elementId)
{
    if (!doc || !documents.contains(doc)) return;
    setCurrentDocument(doc);
    if (!current || !model()->rootDocument()) return;
    const Cyberiada::Element* element = model()->idToElement(elementId);
    if (!element) return;
    SMView->select(model()->elementToIndex(element));
    QGraphicsItem* item = scene()->getMap().value(elementId.toStdString(), nullptr);
    if (item && sceneView) sceneView->centerOn(item);
}

void CyberiadaSMEditorWindow::slotLineActivated(EditorDocument* doc, int line)
{
    TextDocument* text = qobject_cast<TextDocument*>(doc);
    if (!text || !documents.contains(doc)) return;
    setCurrentDocument(doc);
    text->goToLine(line);
}

void CyberiadaSMEditorWindow::slotFileActivated(const QString& path)
{
    // an open file activates its tab (EDIT-DOC-2)
    QString error;
    if (!openFile(path, &error) && !qApp->property("batchMode").toBool()) {
        QMessageBox::warning(this, tr("Open"), error);
    }
}
