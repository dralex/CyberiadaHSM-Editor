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
#include <QFileSystemWatcher>
#include <QShowEvent>
#include <QScrollBar>
#include <QUndoGroup>
#include <QStackedWidget>
#include <QTabBar>
#include <QActionGroup>
#include <QEventLoop>
#include <QHeaderView>
#include <QInputDialog>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QCursor>
#include <QDesktopServices>
#include <QToolTip>
#include <QUrl>

#include "smeditor_window.h"
#include "text_diff_view.h"
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
#include "dialogs/insert_image_dialog.h"
#include "dialogs/insert_diagram_dialog.h"
#include "dialogs/insert_link_dialog.h"
#include "dialogs/insert_formula_dialog.h"
#include "markdown_formula.h"
#include "markdown_tools.h"
#include "markdown_objects.h"
#include "markdown_outline_model.h"
#include "markdown_outline_view.h"
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
#include "disk_bar.h"
#include "disk_state.h"
#include "git_process.h"
#include "repository_service.h"
#include "repository_view.h"
#include "history_view.h"
#include "dialogs/commit_dialog.h"
#include "dialogs/revision_dialog.h"
#include "dialogs/branch_dialog.h"
#include <QToolButton>
#include <QClipboard>
#include "shortcut_map.h"
#include "system_terminal.h"
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
    // the bar of a file changed on disk, between the tab line and the page (EDIT-DOC-15/16)
    m_diskBar = new DiskBar(centralwidget);
    centralLayout->insertWidget(1, m_diskBar);
    connect(m_diskBar, &DiskBar::reloadRequested, this, [this]() { answerDiskBar("reload", nullptr); });
    connect(m_diskBar, &DiskBar::keepRequested, this, [this]() { answerDiskBar("keep", nullptr); });
    connect(m_diskBar, &DiskBar::compareRequested, this, [this]() { answerDiskBar("compare", nullptr); });
    connect(m_diskBar, &DiskBar::closeRequested, this, [this]() { answerDiskBar("close", nullptr); });
    m_diskTimer = new QTimer(this);
    m_diskTimer->setSingleShot(true);
    m_diskTimer->setInterval(DISK_CHECK_DELAY_MS);
    connect(m_diskTimer, &QTimer::timeout, this, &CyberiadaSMEditorWindow::slotDiskPending);
    connect(documentTabs, &QTabBar::currentChanged, this, &CyberiadaSMEditorWindow::activateDocument);
    connect(documentTabs, &QTabBar::tabCloseRequested, this, &CyberiadaSMEditorWindow::closeTab);
    connect(documentTabs, &QTabBar::tabMoved, this,
            [this](int from, int to) { documents.move(from, to); });
    connect(actionClose, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotFileClose);
    connect(actionOpenTerminal, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotOpenTerminal);
    connect(actionNextDocument, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotNextDocument);
    connect(actionPreviousDocument, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotPreviousDocument);

    // the find bar under the documents, hidden until Find (EDIT-FIND-1, docs/FIND.md)
    m_findBar = new FindBar(centralwidget);
    centralLayout->addWidget(m_findBar);
    m_findBar->hide();
    connect(actionGoToLine, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotGoToLine);
    connect(actionInsertImage, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotInsertImage);
    connect(actionInsertDiagram, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotInsertDiagram);
    connect(actionInsertLink, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotInsertLink);
    connect(actionInsertFormula, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotInsertFormula);
    // View > Preview: a setting of every Markdown tab (EDIT-MD-11)
    actionPreview->setChecked(SettingsManager::instance().getMarkdownPreview());
    connect(actionPreview, &QAction::triggered, &SettingsManager::instance(), &SettingsManager::setMarkdownPreview);
    connect(&SettingsManager::instance(), &SettingsManager::markdownPreviewChanged, actionPreview, &QAction::setChecked);
    // View > Wrap Long Lines: a setting of every text editor (EDIT-MODE-30)
    actionWrapLines->setChecked(SettingsManager::instance().getEditorWrap());
    connect(actionWrapLines, &QAction::triggered, &SettingsManager::instance(), &SettingsManager::setEditorWrap);
    connect(&SettingsManager::instance(), &SettingsManager::editorWrapChanged, this,
            [this]() { actionWrapLines->setChecked(SettingsManager::instance().getEditorWrap()); });
    // View > Mode Line: a setting of every source editor (EDIT-MODE-31)
    actionModeLine->setChecked(SettingsManager::instance().getShowModeLine());
    connect(actionModeLine, &QAction::triggered, &SettingsManager::instance(), &SettingsManager::setShowModeLine);
    connect(&SettingsManager::instance(), &SettingsManager::modeLineChanged, this,
            [this]() { actionModeLine->setChecked(SettingsManager::instance().getShowModeLine()); });
    connect(actionUpdateDiagramImages, &QAction::triggered, this, &CyberiadaSMEditorWindow::slotUpdateDiagramImages);
    // Edit > Fill Paragraph, Unfill Paragraph and Fill as You Type, a setting (EDIT-MD-20..22)
    connect(actionFillParagraph, &QAction::triggered, this, [this]() { fillText(false); });
    connect(actionUnfillParagraph, &QAction::triggered, this, [this]() { fillText(true); });
    actionFillAsYouType->setChecked(SettingsManager::instance().getFillAsYouType());
    connect(actionFillAsYouType, &QAction::triggered, &SettingsManager::instance(), &SettingsManager::setFillAsYouType);
    connect(&SettingsManager::instance(), &SettingsManager::textFillChanged, this,
            [this]() { actionFillAsYouType->setChecked(SettingsManager::instance().getFillAsYouType()); });
    // the fill group of the text toolbar, shown in a Markdown or a Plain text tab (EDIT-MD-1)
    QAction* fillSeparator = new QAction(this);
    fillSeparator->setSeparator(true);
    fillToolActions << fillSeparator << actionFillParagraph;
    // the Markdown group of the text toolbar, shown in a Markdown tab (EDIT-MD-1)
    QAction* markdownSeparator = new QAction(this);
    markdownSeparator->setSeparator(true);
    markdownToolActions << markdownSeparator << actionInsertImage << actionInsertDiagram
                        << actionInsertLink << actionInsertFormula << actionUpdateDiagramImages;
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
    initGitMenu();
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
            [this, doc](bool clean) {
                updateTabTitle(doc);
                updateRepositoryMarks();
                // a save writes a file of the work tree (EDIT-GIT-6)
                if (clean) scheduleRepositoryRead();
            });
    connect(doc, &EditorDocument::findResultChanged, this,
            [this, doc](int current, int total, bool more) {
                if (doc == active) m_findBar->setResult(current, total, more);
            });
    connect(doc, &EditorDocument::titleChanged, this,
            [this, doc]() {
                updateTabTitle(doc);
                // a Save As of an unmodified document changes the file, not the clean state
                if (doc == active) { updateTitle(); updatePluginActions(); updateTerminalAction(); }
                updateDiskWatch();
                // a Save As may move the tab to another work tree (EDIT-GIT-2)
                if (doc == active) updateRepositoryDock(true);
            });
    connect(doc, &EditorDocument::modeChanged, this, [this, doc]() { documentStateChanged(doc); });
    connect(doc, &EditorDocument::readOnlyChanged, this, [this, doc]() { documentStateChanged(doc); });
    connect(doc, &EditorDocument::diskStatusChanged, this,
            [this, doc]() {
                documentStateChanged(doc);
                if (doc == active) updateDiskBar();
            });
    updateDiskWatch();
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
    // a file a link names opens as a recent one does, after the click (EDIT-MODE-28)
    connect(doc, &TextDocument::openFileRequested, this, &CyberiadaSMEditorWindow::openLinkedFile,
            Qt::QueuedConnection);
    connect(doc, &TextDocument::imageOffered, this,
            [this, doc](const QString& path, const QImage& image) { insertImage(doc, path, image); });
    // a Ctrl+clicked formula opens its dialog after the click (EDIT-MD-25)
    connect(doc, &TextDocument::editFormulaRequested, this,
            [this, doc](int position) { editFormulaAt(doc, position); }, Qt::QueuedConnection);
    // the outline follows the source's cursor, and its rows after each reading (EDIT-MD-15)
    connect(doc->editor(), &QPlainTextEdit::cursorPositionChanged, this,
            [this, doc]() { if (doc == active) followOutlineCursor(); });
    connect(doc, &TextDocument::outlineRead, this, [this, doc]() { if (doc == active) followOutlineCursor(); });
    return doc;
}

QString CyberiadaSMEditorWindow::openFileMode() const
{
    if (!active) return MODE_HSM;
    if (TextDiffDocument* diff = currentTextDiff()) return diff->language().isEmpty() ? QString(MODE_TEXT) : diff->language();
    return active->mode();
}

TextDocument* CyberiadaSMEditorWindow::findTextTab(const QString& path) const
{
    QString absolute = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    for (EditorDocument* doc : documents) {
        TextDocument* text = qobject_cast<TextDocument*>(doc);
        if (text && text->hasFile() && QDir::cleanPath(text->filePath()) == absolute) return text;
    }
    return nullptr;
}

bool CyberiadaSMEditorWindow::openTextFile(const QString& fileName, QString* error)
{
    QString absolute = QFileInfo(fileName).absoluteFilePath();
    // an open file is activated, not loaded twice (EDIT-DOC-2)
    if (TextDocument* open = findTextTab(absolute)) {
        setCurrentDocument(open);
        return true;
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
    applyDocks();
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
    updateDiskWatch();
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
    // the outline of a tab in the background is only marked out of date (EDIT-MD-14)
    if (TextDocument* left = currentText()) left->setOutlineShown(false);
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
    // a text tab has no canvas, no structure and no element tools (EDIT-MODE-15); the docks
    // are the tab type's (EDIT-UI-6)
    showDiagramParts(current != nullptr);
    applyDocks();
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
    updateDiskBar();
    updateRepositoryDock();
    QTimer::singleShot(0, this, &CyberiadaSMEditorWindow::restoreSavedView);
}

void CyberiadaSMEditorWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    // back from a terminal or another program, the files of the tabs may have changed
    if (event->type() == QEvent::ActivationChange && isActiveWindow() &&
        !qApp->property("batchMode").toBool()) {
        QMetaObject::invokeMethod(this, [this]() {
            checkDisk();
            // a work tree made or gone meanwhile: the folder is looked up again (EDIT-GIT-2)
            updateRepositoryDock(true);
        }, Qt::QueuedConnection);
    }
}

void CyberiadaSMEditorWindow::checkDisk(EditorDocument* doc)
{
    if (doc) {
        checkDiskTab(doc);
        return;
    }
    const QList<EditorDocument*> tabs = documents;
    for (EditorDocument* tab : tabs) {
        if (documents.contains(tab)) checkDiskTab(tab);
    }
}

void CyberiadaSMEditorWindow::checkDiskTab(EditorDocument* doc)
{
    if (!doc->hasFile() || qobject_cast<ComparisonDocument*>(doc)) return;
    DiskState now;
    switch (checkDiskState(doc->diskState(), doc->filePath(), &now)) {
    case DiskCheck::touched:
        doc->setDiskState(now);
        Q_FALLTHROUGH();    // the bytes are the ones the tab knows
    case DiskCheck::same:
        // the file is back as the tab read it: the bar goes (EDIT-DOC-16)
        doc->setDiskStatus(EditorDocument::diskOk);
        break;
    case DiskCheck::gone:
        if (doc->diskStatus() != EditorDocument::diskAbsent) doc->setDiskStatus(EditorDocument::diskAbsent);
        break;
    case DiskCheck::changed:
        // a clean tab follows its file; the edits and a running plugin keep the tab (EDIT-DOC-14/15)
        if (!doc->hasEdits() && !doc->pluginRun()) {
            QString error;
            if (reloadTab(doc, &error)) return;
            doc->setDiskStatus(EditorDocument::diskUnreadable);
        } else if (doc->diskStatus() != EditorDocument::diskChanged) {
            doc->setDiskStatus(EditorDocument::diskChanged);
        }
        break;
    }
}

bool CyberiadaSMEditorWindow::reloadTab(EditorDocument* doc, QString* error)
{
    bool finding = doc == active && m_findBar && !m_findBar->isHidden() && doc->canFind();
    if (finding) doc->clearFind(false);
    if (!doc->reload(error)) {
        if (finding) doc->setFindQuery(m_findBar->query(), false);
        return false;
    }
    if (doc == active) {
        // the model reset gave the tree its root back (slotModelReset)
        if (doc == current) {
            QModelIndex sm = model()->firstSMIndex();
            if (sm.isValid()) SMView->select(sm);
            QTimer::singleShot(0, this, &CyberiadaSMEditorWindow::restoreSavedView);
        }
        if (finding) doc->setFindQuery(m_findBar->query(), false);
        updateTitle();
        updateEditActions();
    }
    documentStateChanged(doc);
    return true;
}

void CyberiadaSMEditorWindow::updateDiskBar()
{
    if (!m_diskBar) return;
    if (active && active->diskBarShown()) {
        m_diskBar->setStatus(active->diskStatus(), active->title());
    } else {
        m_diskBar->setStatus(EditorDocument::diskOk, QString());
    }
}

bool CyberiadaSMEditorWindow::answerDiskBar(const QString& answer, QString* error)
{
    EditorDocument* doc = active;
    if (!doc || !doc->diskBarShown()) {
        if (error) *error = "the active tab has no disk bar";
        return false;
    }
    EditorDocument::DiskStatus status = doc->diskStatus();
    bool absent = status == EditorDocument::diskAbsent;
    QString reason;
    if (answer == "reload" && !absent) {
        if (reloadTab(doc, &reason)) return true;
        if (error) *error = reason;
        else QMessageBox::critical(this, tr("Reload"), reason);
        return false;
    }
    if (answer == "keep") {
        // a gone file keeps the tab modified; a changed one is taken as it is now (EDIT-DOC-15/16)
        if (absent) doc->dismissDiskBar();
        else doc->rememberDisk();
        return true;
    }
    if (answer == "compare" && status == EditorDocument::diskChanged) {
        DiffOrigin base;
        base.kind = DiffOrigin::originFile;
        base.path = doc->filePath();
        base.label = tr("on disk");
        bool ok = false;
        if (TextDocument* text = qobject_cast<TextDocument*>(doc)) {
            ok = compareText(text, base, &reason) != nullptr;
        } else {
            ok = compareDocument(qobject_cast<CyberiadaSMEditorDocument*>(doc), base, &reason) != nullptr;
        }
        if (!ok) {
            if (error) *error = reason;
            else QMessageBox::critical(this, tr("Compare"), reason);
        }
        return ok;
    }
    if (answer == "close" && absent) return closeDocument(doc, true);
    if (error) *error = QString("the disk bar of the tab offers no %1").arg(answer);
    return false;
}

bool CyberiadaSMEditorWindow::confirmDiskOverwrite(EditorDocument* doc, QString* error)
{
    if (!doc || !doc->hasFile()) return true;
    DiskState now;
    DiskCheck check = checkDiskState(doc->diskState(), doc->filePath(), &now);
    if (check == DiskCheck::touched) doc->setDiskState(now);
    // a gone file is written again; one changed since the tab read it is asked about
    if (check != DiskCheck::changed) return true;
    if (qApp->property("batchMode").toBool()) {
        if (error) *error = QString("%1 changed on disk: disk keep or disk reload first").arg(doc->title());
        return false;
    }
    QMessageBox box(QMessageBox::Question, tr("Save"), tr("%1 changed on disk. Overwrite it?").arg(doc->title()),
                    QMessageBox::NoButton, this);
    QPushButton* overwrite = box.addButton(tr("Overwrite"), QMessageBox::AcceptRole);
    QPushButton* cancel = box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(cancel);
    box.exec();
    return box.clickedButton() == overwrite;
}

void CyberiadaSMEditorWindow::updateDiskWatch()
{
    // the batch checks by its verbs, so a script runs the same on every machine
    if (qApp->property("batchMode").toBool()) return;
    if (!m_diskWatcher) {
        m_diskWatcher = new QFileSystemWatcher(this);
        connect(m_diskWatcher, &QFileSystemWatcher::fileChanged, this, &CyberiadaSMEditorWindow::slotDiskChanged);
    }
    QSet<QString> wanted;
    for (EditorDocument* doc : documents) {
        if (doc->hasFile() && !qobject_cast<ComparisonDocument*>(doc)) {
            wanted.insert(QFileInfo(doc->filePath()).absoluteFilePath());
        }
    }
    const QStringList watched = m_diskWatcher->files();
    for (const QString& path : watched) {
        if (!wanted.contains(path)) m_diskWatcher->removePath(path);
    }
    for (const QString& path : wanted) {
        if (!watched.contains(path) && QFileInfo::exists(path)) m_diskWatcher->addPath(path);
    }
}

QString CyberiadaSMEditorWindow::gitFilePath(EditorDocument* doc) const
{
    if (ComparisonDocument* diff = qobject_cast<ComparisonDocument*>(doc)) {
        EditorDocument* target = diff->target().tab.data();
        return target && target != doc ? target->filePath() : QString();
    }
    return doc ? doc->filePath() : QString();
}

RepositoryService* CyberiadaSMEditorWindow::repositoryOf(EditorDocument* doc, bool again) const
{
    QString path = gitFilePath(doc);
    if (path.isEmpty()) return nullptr;
    return RepositoryService::forRoot(RepositoryService::rootOf(QFileInfo(path).absolutePath(), again));
}

void CyberiadaSMEditorWindow::createRepositoryDock()
{
    SettingsManager& sm = SettingsManager::instance();
    m_repositoryView = new RepositoryView();
    m_repositoryView->setObjectName("repositoryView");
    // the batch starts from the defaults, so a script runs the same everywhere
    if (!qApp->property("batchMode").toBool()) {
        m_repositoryView->setLayoutMode(RepositoryModel::Layout(sm.getRepositoryLayout()));
        m_repositoryView->setChangedOnly(sm.getRepositoryChangedOnly());
        m_repositoryView->setIgnored(sm.getRepositoryIgnored());
    }
    connect(m_repositoryView, &RepositoryView::layoutModeChanged, this,
            [](RepositoryModel::Layout layout) { SettingsManager::instance().setRepositoryLayout(int(layout)); });
    connect(m_repositoryView, &RepositoryView::changedOnlyChanged, this,
            [](bool on) { SettingsManager::instance().setRepositoryChangedOnly(on); });
    connect(m_repositoryView, &RepositoryView::ignoredChanged, this, [this](bool on) {
        SettingsManager::instance().setRepositoryIgnored(on);
        if (!m_repository) return;
        m_repository->setIgnored(on);
        m_repository->refresh();
    });
    connect(m_repositoryView, &RepositoryView::refreshRequested, this, [this]() {
        if (m_repository) m_repository->refresh();
    });
    connect(m_repositoryView, &RepositoryView::openRequested, this, &CyberiadaSMEditorWindow::openRepositoryFile);

    repositoryDock = new QDockWidget(tr("Repository"), this);
    repositoryDock->setObjectName("repositoryDock");
    repositoryDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    repositoryDock->setWidget(m_repositoryView);
    addDockWidget(Qt::LeftDockWidgetArea, repositoryDock);
    resizeDocks(QList<QDockWidget*>() << repositoryDock, QList<int>() << PANE_DOCK_WIDTH, Qt::Horizontal);
    actionRepository = repositoryDock->toggleViewAction();
    actionRepository->setObjectName("actionRepositoryPane");
    actionRepository->setText(tr("Repository"));
    // the commits of a file or of the work tree, tabbed behind the Repository (EDIT-GIT-15)
    m_historyView = new HistoryView();
    m_historyView->setObjectName("historyView");
    historyDock = new QDockWidget(tr("History"), this);
    historyDock->setObjectName("historyDock");
    historyDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    historyDock->setWidget(m_historyView);
    addDockWidget(Qt::LeftDockWidgetArea, historyDock);
    tabifyDockWidget(repositoryDock, historyDock);
    repositoryDock->raise();
    actionHistory = historyDock->toggleViewAction();
    actionHistory->setObjectName("actionHistoryPane");
    actionHistory->setText(tr("History"));
    connect(historyDock, &QDockWidget::visibilityChanged, this, [this](bool shown) {
        if (shown) updateHistory();
    });
    connect(m_historyView, &HistoryView::moreRequested, this, [this]() { updateHistory(true); });
    connect(m_historyView, &HistoryView::scopeChanged, this, [this](bool repository) {
        m_historyRepository = repository;
        updateHistory();
    });
    connect(m_historyView, &HistoryView::compareRequested, this, [this](const QString& hash) {
        compareFile(m_historyRepository ? gitRelativePath(active) : m_historyPath, hash);
    });
    // the history follows the current file row (EDIT-GIT-15)
    connect(m_repositoryView->tree()->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current) {
                QModelIndex row = current.sibling(current.row(), RepositoryModel::columnName);
                if (!m_repositoryView->model()->entry(row)) return;
                QString path = m_repositoryView->model()->data(row, RepositoryModel::PathRole).toString();
                if (path == m_historyPath) return;
                m_historyPath = path;
                updateHistory();
            });
    // shown again, the dock reads what it missed (EDIT-GIT-6)
    connect(repositoryDock, &QDockWidget::visibilityChanged, this, [this](bool shown) {
        if (m_repository) m_repository->setWatching(shown && !qApp->property("batchMode").toBool());
        if (shown && m_repositoryStale) scheduleRepositoryRead();
    });
    m_repositoryTimer = new QTimer(this);
    m_repositoryTimer->setSingleShot(true);
    connect(m_repositoryTimer, &QTimer::timeout, this, [this]() {
        if (!m_repository) return;
        m_repositoryRead.start();
        m_repository->refresh();
    });
    // another program or no git: every work tree is looked up again (EDIT-GIT-1)
    connect(&sm, &SettingsManager::gitSettingsChanged, this, [this]() {
        if (m_repository) m_repository->disconnect(this);
        m_repository = nullptr;
        RepositoryService::reset();
        applyDocks();
        updateRepositoryDock(true);
    });
}

void CyberiadaSMEditorWindow::updateRepositoryDock(bool again)
{
    if (!m_repositoryView) return;
    RepositoryService* service = repositoryOf(active, again);
    bool switched = service != m_repository.data();
    if (switched) {
        if (m_repository) {
            m_repository->disconnect(this);
            m_repository->setWatching(false);
        }
        m_repository = service;
        if (service) {
            connect(service, &RepositoryService::statusChanged, this, &CyberiadaSMEditorWindow::repositoryStatusChanged);
            connect(service, &RepositoryService::gitChanged, this, &CyberiadaSMEditorWindow::scheduleRepositoryRead);
            service->setIgnored(m_repositoryView->ignored());
            service->setWatching(!repositoryDock->isHidden() && !qApp->property("batchMode").toBool());
        }
    }
    m_repositoryView->setRepository(service ? service->root() : QString(),
                                    service ? QString() : noRepositoryReason(active));
    if (service) {
        if (switched && service->hasSnapshot()) m_repositoryView->setSnapshot(service->snapshot());
        QString path = gitFilePath(active);
        m_repositoryView->setCurrentFile(QDir(service->root()).relativeFilePath(QFileInfo(path).absoluteFilePath()));
        updateRepositoryMarks();
        scheduleRepositoryRead();
    }
    updateRepositoryTitle();
    // the history of the file of the tab (EDIT-GIT-15)
    QString file = gitRelativePath(active);
    if (switched || !service || file != m_historyPath) {
        m_historyPath = file;
        updateHistory();
    }
    updateGitActions();
}

void CyberiadaSMEditorWindow::scheduleRepositoryRead()
{
    // the batch reads by its verbs only, so a script runs the same everywhere
    if (!m_repository || !m_repositoryTimer || qApp->property("batchMode").toBool()) return;
    // only while the dock is shown; shown again, it reads what it missed (EDIT-GIT-6)
    if (repositoryDock->isHidden()) {
        m_repositoryStale = true;
        return;
    }
    m_repositoryStale = false;
    if (m_repositoryTimer->isActive()) return;
    qint64 since = m_repositoryRead.isValid() ? m_repositoryRead.elapsed() : REPOSITORY_READ_INTERVAL_MS;
    m_repositoryTimer->start(int(qMax<qint64>(0, REPOSITORY_READ_INTERVAL_MS - since)));
}

void CyberiadaSMEditorWindow::repositoryStatusChanged()
{
    if (!m_repository || sender() != m_repository.data()) return;
    m_repositoryView->setSnapshot(m_repository->snapshot());
    updateRepositoryMarks();
    updateRepositoryTitle();
    updateGitActions();
    // a commit made elsewhere, a rename staged: the history read again (EDIT-GIT-15)
    updateHistory();
}

void CyberiadaSMEditorWindow::updateRepositoryMarks()
{
    if (!m_repositoryView || !m_repository) return;
    QHash<QString, bool> open;
    QDir root(m_repository->root());
    for (EditorDocument* doc : documents) {
        if (!doc->hasFile() || qobject_cast<ComparisonDocument*>(doc)) continue;
        QString relative = root.relativeFilePath(QFileInfo(doc->filePath()).absoluteFilePath());
        if (relative.startsWith("..")) continue;
        open[relative] = open.value(relative) || doc->hasEdits();
    }
    m_repositoryView->model()->setOpenFiles(open);
}

void CyberiadaSMEditorWindow::updateRepositoryTitle()
{
    // the dock title stays: a title sets the least width of its dock (EDIT-GIT-3)
    if (!m_repositoryView) return;
    if (!m_repository) {
        m_repositoryView->setHeading(QString());
        return;
    }
    m_repositoryView->setHeading(QFileInfo(m_repository->root()).fileName());
    QToolButton* branch = m_repositoryView->branchButton();
    branch->setText(m_repository->hasSnapshot() ? branchLabel(m_repository->snapshot()) : QString());
    branch->setToolTip(tr("The branch: switch, new, delete"));
}

QString CyberiadaSMEditorWindow::branchLabel(const RepositorySnapshot& s)
{
    QString label;
    if (s.detached()) {
        label = tr("detached at %1").arg(s.oid.left(GIT_SHORT_HASH));
    } else if (s.unborn()) {
        label = tr("%1 (no commits)").arg(s.branch);
    } else {
        label = s.branch;
        // the arrows of the upstream counts (EDIT-GIT-18)
        if (s.ahead > 0) label += QString(" %1%2").arg(QChar(0x2191)).arg(s.ahead);
        if (s.behind > 0) label += QString(" %1%2").arg(QChar(0x2193)).arg(s.behind);
    }
    if (s.state == "merging") label += tr(", merging");
    if (s.state == "rebasing") label += tr(", rebasing");
    return label;
}

void CyberiadaSMEditorWindow::fillBranchMenu()
{
    m_branchMenu->clear();
    RepositoryService* service = m_repository.data();
    if (!service) {
        QAction* none = m_branchMenu->addAction(noRepositoryReason(active));
        none->setEnabled(false);
        return;
    }
    service->refresh(true);
    const RepositorySnapshot& s = service->snapshot();
    // a merge or a rebase in progress keeps the branches (EDIT-GIT-19)
    bool busy = !s.state.isEmpty();
    for (const QString& branch : s.branches) {
        QAction* entry = m_branchMenu->addAction(branch);
        entry->setCheckable(true);
        entry->setChecked(branch == s.branch);
        entry->setEnabled(!busy);
        if (busy) entry->setToolTip(tr("finish the %1 in a terminal first").arg(s.state == "merging" ? tr("merge") : tr("rebase")));
        if (branch != s.branch) connect(entry, &QAction::triggered, this, [this, branch]() { switchBranch(branch); });
        else connect(entry, &QAction::triggered, this, [entry]() { entry->setChecked(true); });
    }
    if (!s.branches.isEmpty()) m_branchMenu->addSeparator();
    QAction* create = m_branchMenu->addAction(tr("New Branch..."));
    create->setEnabled(!busy && !s.unborn());
    connect(create, &QAction::triggered, this, &CyberiadaSMEditorWindow::newBranch);
    QAction* remove = m_branchMenu->addAction(tr("Delete Branch..."));
    remove->setEnabled(s.branches.size() > (s.detached() ? 0 : 1));
    connect(remove, &QAction::triggered, this, &CyberiadaSMEditorWindow::deleteBranch);
}

QStringList CyberiadaSMEditorWindow::unsavedTabs(RepositoryService* service) const
{
    QStringList tabs;
    QDir root(service->root());
    for (EditorDocument* doc : documents) {
        if (!doc->hasFile() || !doc->hasEdits() || qobject_cast<ComparisonDocument*>(doc)) continue;
        if (!root.relativeFilePath(QFileInfo(doc->filePath()).absoluteFilePath()).startsWith("..")) tabs << doc->title();
    }
    return tabs;
}

bool CyberiadaSMEditorWindow::saveRepositoryTabs(RepositoryService* service, QString* error)
{
    QDir root(service->root());
    const QList<EditorDocument*> tabs = documents;
    for (EditorDocument* doc : tabs) {
        if (!doc->hasFile() || !doc->hasEdits() || qobject_cast<ComparisonDocument*>(doc)) continue;
        if (root.relativeFilePath(QFileInfo(doc->filePath()).absoluteFilePath()).startsWith("..")) continue;
        if (!confirmDiskOverwrite(doc, error) || !doc->save(error)) return false;
    }
    return true;
}

bool CyberiadaSMEditorWindow::askSaveForSwitch(RepositoryService* service, const QString& branch)
{
    QStringList unsaved = unsavedTabs(service);
    if (unsaved.isEmpty()) return true;
    QMessageBox box(QMessageBox::Question, tr("Switch to %1").arg(branch),
                    tr("Save %1 documents before switching?").arg(unsaved.size()) + "\n\n" + unsaved.join('\n'),
                    QMessageBox::NoButton, this);
    QPushButton* save = box.addButton(tr("Save and Switch"), QMessageBox::AcceptRole);
    QPushButton* cancel = box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(cancel);
    box.exec();
    return box.clickedButton() == save;
}

void CyberiadaSMEditorWindow::switchBranch(const QString& branch)
{
    QString error;
    RepositoryService* service = gitService(&error);
    if (!service) {
        reportGitFailure(tr("Switch"), error);
        return;
    }
    if (!askSaveForSwitch(service, branch)) return;
    if (!gitSwitch(branch, &error)) reportGitFailure(tr("Switch to %1").arg(branch), error);
}

void CyberiadaSMEditorWindow::newBranch()
{
    QString error;
    RepositoryService* service = gitService(&error);
    if (!service) {
        reportGitFailure(tr("New Branch"), error);
        return;
    }
    BranchDialog dialog(service->snapshot().branches,
                        [this, service](const QString& name, const QString& start, bool switchTo, QString* reason) {
                            // a start of its own changes the files: the tabs are saved first (EDIT-GIT-20)
                            if (switchTo && !start.isEmpty() && start != service->snapshot().branch &&
                                !askSaveForSwitch(service, name)) {
                                *reason = tr("not switched: the documents are not saved");
                                return false;
                            }
                            return gitCreateBranch(name, start, switchTo, reason);
                        }, this);
    dialog.exec();
}

void CyberiadaSMEditorWindow::deleteBranch()
{
    QString error;
    RepositoryService* service = gitService(&error);
    if (!service) {
        reportGitFailure(tr("Delete Branch"), error);
        return;
    }
    QStringList others = service->snapshot().branches;
    others.removeAll(service->snapshot().branch);
    if (others.isEmpty()) return;
    bool ok = false;
    QString branch = QInputDialog::getItem(this, tr("Delete Branch"), tr("The branch to delete (merged only):"),
                                           others, 0, false, &ok);
    if (!ok || branch.isEmpty()) return;
    if (!gitDeleteBranch(branch, &error)) reportGitFailure(tr("Delete Branch"), error);
}

bool CyberiadaSMEditorWindow::gitSwitch(const QString& branch, QString* error)
{
    RepositoryService* service = gitService(error);
    if (!service) return false;
    service->refresh(true);
    if (!service->snapshot().state.isEmpty()) {
        if (error) *error = tr("a %1 is in progress: finish it in a terminal first").arg(
                                service->snapshot().state == "merging" ? tr("merge") : tr("rebase"));
        return false;
    }
    if (branch.isEmpty() || branch.startsWith('-')) {
        if (error) *error = tr("%1 is not a branch").arg(branch);
        return false;
    }
    // the tabs of the work tree are saved: git switches the files on disk (EDIT-GIT-19)
    if (!saveRepositoryTabs(service, error)) return false;
    GitResult r = service->command(QStringList() << "switch" << branch);
    afterGitCommand(service);
    updateRepositoryTitle();
    if (!r.ok() && error) *error = r.message();
    return r.ok();
}

bool CyberiadaSMEditorWindow::gitCreateBranch(const QString& name, const QString& start, bool switchTo,
                                              QString* error)
{
    RepositoryService* service = gitService(error);
    if (!service) return false;
    if (name.isEmpty() || name.startsWith('-') || start.startsWith('-')) {
        if (error) *error = tr("%1 is not a branch name").arg(name);
        return false;
    }
    bool moves = switchTo && !start.isEmpty() && start != service->snapshot().branch;
    if (moves && !saveRepositoryTabs(service, error)) return false;
    QStringList args = switchTo ? QStringList() << "switch" << "-c" << name : QStringList() << "branch" << name;
    if (!start.isEmpty()) args << start;
    GitResult r = service->command(args);
    afterGitCommand(service);
    updateRepositoryTitle();
    if (!r.ok() && error) *error = r.message();
    return r.ok();
}

bool CyberiadaSMEditorWindow::gitDeleteBranch(const QString& name, QString* error)
{
    RepositoryService* service = gitService(error);
    if (!service) return false;
    if (name.isEmpty() || name.startsWith('-')) {
        if (error) *error = tr("%1 is not a branch").arg(name);
        return false;
    }
    // an unmerged branch is refused by git itself (EDIT-GIT-20)
    GitResult r = service->command(QStringList() << "branch" << "-d" << name);
    afterGitCommand(service);
    if (!r.ok() && error) *error = r.message();
    return r.ok();
}

void CyberiadaSMEditorWindow::openRepositoryFile(const QString& path)
{
    if (!m_repository || path.endsWith('/')) return;
    QString file = QDir(m_repository->root()).filePath(path);
    // the editor opens a diagram and a text of a known mode, the system the rest (EDIT-GIT-7)
    if (isDocumentFile(file)) {
        QString error;
        if (!openFile(file, &error)) QMessageBox::warning(this, tr("Open"), error);
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(file));
}

QString CyberiadaSMEditorWindow::gitRelativePath(EditorDocument* doc) const
{
    RepositoryService* service = repositoryOf(doc);
    QString path = gitFilePath(doc);
    if (!service || path.isEmpty()) return QString();
    return QDir(service->root()).relativeFilePath(QFileInfo(path).absoluteFilePath());
}

void CyberiadaSMEditorWindow::initGitMenu()
{
    // Alt+G is Go to Line: the menu takes Alt+T
    m_gitMenu = new QMenu(tr("Gi&t"), this);
    m_gitMenu->setObjectName("menuGit");
    QMainWindow::menuBar()->insertMenu(pluginMenu->menu()->menuAction(), m_gitMenu);
    auto add = [this](const QString& name, const QString& text) {
        QAction* action = m_gitMenu->addAction(text);
        action->setObjectName(name);
        return action;
    };
    actionGitStage = add("actionGitStage", tr("Stage File"));
    actionGitUnstage = add("actionGitUnstage", tr("Unstage File"));
    actionGitDiscard = add("actionGitDiscard", tr("Discard Changes of the File..."));
    m_gitMenu->addSeparator();
    actionGitCompareHead = add("actionGitCompareHead", tr("Compare with HEAD"));
    actionGitCompareIndex = add("actionGitCompareIndex", tr("Compare with the Index"));
    actionGitCompareRevision = add("actionGitCompareRevision", tr("Compare with a Revision..."));
    actionGitHistory = add("actionGitHistory", tr("File History"));
    m_gitMenu->addSeparator();
    actionGitCommit = add("actionGitCommit", tr("Commit..."));
    // the branches: one menu for Git > Branch and the button of the dock (EDIT-GIT-18)
    m_branchMenu = new QMenu(tr("Branch"), this);
    m_branchMenu->setObjectName("menuGitBranch");
    m_gitMenu->addMenu(m_branchMenu);
    connect(m_branchMenu, &QMenu::aboutToShow, this, &CyberiadaSMEditorWindow::fillBranchMenu);
    m_repositoryView->branchButton()->setMenu(m_branchMenu);
    actionGitRefresh = add("actionGitRefresh", tr("Refresh"));
    connect(actionGitStage, &QAction::triggered, this, [this]() { stageFiles(QStringList() << gitRelativePath(active)); });
    connect(actionGitUnstage, &QAction::triggered, this, [this]() { unstageFiles(QStringList() << gitRelativePath(active)); });
    connect(actionGitDiscard, &QAction::triggered, this, [this]() { discardFiles(QStringList() << gitRelativePath(active)); });
    connect(actionGitCompareHead, &QAction::triggered, this, [this]() { compareFile(gitRelativePath(active), "HEAD"); });
    connect(actionGitCompareIndex, &QAction::triggered, this, [this]() { compareFile(gitRelativePath(active), QString()); });
    connect(actionGitCompareRevision, &QAction::triggered, this, [this]() { compareFileWithRevision(gitRelativePath(active)); });
    connect(actionGitHistory, &QAction::triggered, this, [this]() { gitHistory(gitRelativePath(active), false); });
    connect(actionGitCommit, &QAction::triggered, this, &CyberiadaSMEditorWindow::commitIndex);
    connect(actionGitRefresh, &QAction::triggered, this, [this]() {
        updateRepositoryDock(true);
        if (m_repository) m_repository->refresh();
    });

    // the dock acts on the selected rows: its toolbar, its menu, F2 and Del (EDIT-GIT-8)
    QToolBar* bar = m_repositoryView->toolBar();
    QAction* refresh = m_repositoryView->refreshAction();
    m_dockStage = new QAction(tr("Stage"), bar);
    m_dockStage->setObjectName("actionRepositoryStage");
    m_dockUnstage = new QAction(tr("Unstage"), bar);
    m_dockUnstage->setObjectName("actionRepositoryUnstage");
    bar->insertAction(refresh, m_dockStage);
    bar->insertAction(refresh, m_dockUnstage);
    bar->insertAction(refresh, actionGitCommit);
    bar->insertSeparator(refresh);
    connect(m_dockStage, &QAction::triggered, this, [this]() { stageFiles(m_repositoryView->selectedFiles()); });
    connect(m_dockUnstage, &QAction::triggered, this, [this]() { unstageFiles(m_repositoryView->selectedFiles()); });
    QTreeView* tree = m_repositoryView->tree();
    m_dockRename = new QAction(tr("Rename..."), tree);
    m_dockRename->setObjectName("actionRepositoryRename");
    m_dockRename->setShortcut(QKeySequence(Qt::Key_F2));
    m_dockRename->setShortcutContext(Qt::WidgetShortcut);
    m_dockDelete = new QAction(tr("Delete..."), tree);
    m_dockDelete->setObjectName("actionRepositoryDelete");
    m_dockDelete->setShortcut(QKeySequence(QKeySequence::Delete));
    m_dockDelete->setShortcutContext(Qt::WidgetShortcut);
    tree->addAction(m_dockRename);
    tree->addAction(m_dockDelete);
    connect(m_dockRename, &QAction::triggered, this, [this]() {
        QStringList files = m_repositoryView->selectedFiles();
        if (files.size() == 1) renameFile(files.first());
    });
    connect(m_dockDelete, &QAction::triggered, this, [this]() {
        QStringList files = m_repositoryView->selectedFiles();
        if (!files.isEmpty()) deleteFiles(files);
    });
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree, &QWidget::customContextMenuRequested, this, &CyberiadaSMEditorWindow::showRepositoryMenu);
    connect(tree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]() { updateGitActions(); });
    updateGitActions();
}

void CyberiadaSMEditorWindow::updateGitActions()
{
    if (!m_gitMenu) return;
    RepositoryService* service = m_repository.data();
    QString reason = service ? QString() : noRepositoryReason(active);
    const QList<QAction*> fileActions = QList<QAction*>() << actionGitStage << actionGitUnstage << actionGitDiscard
        << actionGitCompareHead << actionGitCompareIndex << actionGitCompareRevision << actionGitHistory
        << actionGitCommit << actionGitRefresh;
    for (QAction* action : fileActions) {
        action->setEnabled(service != nullptr);
        action->setToolTip(reason);
    }
    if (!service) {
        for (QAction* action : {m_dockStage, m_dockUnstage, m_dockRename, m_dockDelete}) action->setEnabled(false);
        return;
    }
    QString file = gitRelativePath(active);
    const StatusEntry* entry = service->hasSnapshot() ? service->snapshot().entry(file) : nullptr;
    bool opens = !file.isEmpty() && isDocumentFile(file);
    for (QAction* action : {actionGitCompareHead, actionGitCompareRevision}) action->setEnabled(opens);
    // a conflicted file has no index base (EDIT-GIT-12)
    actionGitCompareIndex->setEnabled(opens && !(entry && entry->conflicted()));
    if (entry && entry->conflicted()) actionGitCompareIndex->setToolTip(tr("%1 is in conflict").arg(file));
    bool stage = false, unstage = false;
    const QStringList selected = m_repositoryView->selectedFiles();
    for (const QString& path : selected) {
        const StatusEntry* e = service->snapshot().entry(path);
        if (!e) continue;
        stage = stage || e->modified() || e->untracked() || e->conflicted();
        unstage = unstage || e->staged();
    }
    m_dockStage->setEnabled(stage);
    m_dockUnstage->setEnabled(unstage);
    m_dockRename->setEnabled(selected.size() == 1);
    m_dockDelete->setEnabled(!selected.isEmpty());
}

void CyberiadaSMEditorWindow::showRepositoryMenu(const QPoint& pos)
{
    if (!m_repository) return;
    QTreeView* tree = m_repositoryView->tree();
    QModelIndex at = tree->indexAt(pos);
    if (at.isValid() && !tree->selectionModel()->isSelected(at)) {
        tree->selectionModel()->select(at, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    const QStringList files = m_repositoryView->selectedFiles();
    if (files.isEmpty()) return;
    const RepositorySnapshot& s = m_repository->snapshot();
    bool stage = false, unstage = false, discard = false, conflicted = false;
    for (const QString& path : files) {
        const StatusEntry* e = s.entry(path);
        if (!e) continue;
        stage = stage || e->modified() || e->untracked() || e->conflicted();
        unstage = unstage || e->staged();
        discard = discard || e->modified() || e->untracked();
        conflicted = conflicted || e->conflicted();
    }
    bool one = files.size() == 1 && !files.first().endsWith('/');
    bool opens = one && isDocumentFile(files.first());
    QMenu menu(this);
    QAction* open = menu.addAction(tr("Open"));
    open->setEnabled(one);
    menu.addSeparator();
    QAction* stageEntry = menu.addAction(tr("Stage"));
    stageEntry->setEnabled(stage);
    QAction* unstageEntry = menu.addAction(tr("Unstage"));
    unstageEntry->setEnabled(unstage);
    QAction* discardEntry = menu.addAction(tr("Discard Changes..."));
    discardEntry->setEnabled(discard);
    menu.addSeparator();
    QAction* head = menu.addAction(tr("Compare with HEAD"));
    head->setEnabled(opens);
    QAction* index = menu.addAction(tr("Compare with the Index"));
    index->setEnabled(opens && !conflicted);
    QAction* revision = menu.addAction(tr("Compare with a Revision..."));
    revision->setEnabled(opens);
    QAction* history = menu.addAction(tr("History"));
    history->setEnabled(one);
    menu.addSeparator();
    menu.addAction(m_dockRename);
    menu.addAction(m_dockDelete);
    menu.addSeparator();
    QAction* copyPath = menu.addAction(tr("Copy Path"));
    QAction* copyRelative = menu.addAction(tr("Copy Relative Path"));
    QAction* chosen = menu.exec(tree->viewport()->mapToGlobal(pos));
    if (!chosen) return;
    const QString first = files.first();
    if (chosen == open) openRepositoryFile(first);
    else if (chosen == stageEntry) stageFiles(files);
    else if (chosen == unstageEntry) unstageFiles(files);
    else if (chosen == discardEntry) discardFiles(files);
    else if (chosen == head) compareFile(first, "HEAD");
    else if (chosen == index) compareFile(first, QString());
    else if (chosen == revision) compareFileWithRevision(first);
    else if (chosen == history) gitHistory(first, false);
    else if (chosen == copyPath || chosen == copyRelative) {
        QStringList lines;
        QDir root(m_repository->root());
        for (const QString& path : files) {
            lines << (chosen == copyPath ? QDir::toNativeSeparators(root.filePath(path)) : path);
        }
        QApplication::clipboard()->setText(lines.join('\n'));
    }
}

RepositoryService* CyberiadaSMEditorWindow::gitService(QString* error)
{
    RepositoryService* service = repositoryOf(active);
    if (!service) {
        if (error) *error = noRepositoryReason(active);
        return nullptr;
    }
    connect(service, &RepositoryService::commandRun, this, &CyberiadaSMEditorWindow::slotGitCommand,
            Qt::UniqueConnection);
    if (!service->hasSnapshot()) service->refresh(true);
    return service;
}

void CyberiadaSMEditorWindow::gitOutput(const QString& text, bool isError)
{
    pluginDockWidget->appendTool("git", "Git", text, isError);
}

void CyberiadaSMEditorWindow::slotGitCommand(const QStringList& args, const GitResult& result)
{
    // every command and its answer in the Git output (EDIT-GIT-17)
    RepositoryService* service = qobject_cast<RepositoryService*>(sender());
    gitOutput(QString("== git %1 - %2").arg(args.join(' '),
                                           service ? QDir::toNativeSeparators(service->root()) : QString()),
              false);
    QString out = QString::fromUtf8(result.out).trimmed();
    if (!out.isEmpty()) gitOutput(out, false);
    if (!result.ok()) gitOutput(result.message(), true);
    else if (!result.err.trimmed().isEmpty()) gitOutput(result.err.trimmed(), false);
}

void CyberiadaSMEditorWindow::afterGitCommand(RepositoryService* service)
{
    // a command may rewrite the files under the tabs (EDIT-DOC-13) and changes the status
    checkDisk();
    service->refresh(qApp->property("batchMode").toBool());
    updateRepositoryMarks();
    updateHistory();
}

void CyberiadaSMEditorWindow::reportGitFailure(const QString& title, const QString& error)
{
    if (error.isEmpty() || qApp->property("batchMode").toBool()) return;
    QMessageBox::warning(this, title, error);
}

bool CyberiadaSMEditorWindow::gitStage(const QStringList& paths, QString* error)
{
    RepositoryService* service = gitService(error);
    if (!service) return false;
    if (paths.isEmpty() || paths.contains(QString())) {
        if (error) *error = tr("no file to stage");
        return false;
    }
    GitResult r = service->command(QStringList() << "add" << "--" << paths);
    afterGitCommand(service);
    if (!r.ok() && error) *error = r.message();
    return r.ok();
}

bool CyberiadaSMEditorWindow::gitUnstage(const QStringList& paths, QString* error)
{
    RepositoryService* service = gitService(error);
    if (!service) return false;
    if (paths.isEmpty() || paths.contains(QString())) {
        if (error) *error = tr("no file to unstage");
        return false;
    }
    // before the first commit there is no HEAD to restore the index from (EDIT-GIT-9)
    QStringList args = service->snapshot().unborn() ? QStringList() << "rm" << "--cached" << "-q"
                                                    : QStringList() << "restore" << "--staged";
    GitResult r = service->command(args << "--" << paths);
    afterGitCommand(service);
    if (!r.ok() && error) *error = r.message();
    return r.ok();
}

bool CyberiadaSMEditorWindow::gitDiscard(const QStringList& paths, QString* error)
{
    RepositoryService* service = gitService(error);
    if (!service) return false;
    QStringList tracked, untracked;
    for (const QString& path : paths) {
        const StatusEntry* e = service->snapshot().entry(path);
        if (!e || e->conflicted()) continue;
        if (e->untracked()) untracked << path;
        else if (e->modified()) tracked << path;
    }
    if (tracked.isEmpty() && untracked.isEmpty()) {
        if (error) *error = tr("no change to discard");
        return false;
    }
    QDir root(service->root());
    if (!tracked.isEmpty()) {
        GitResult r = service->command(QStringList() << "restore" << "--" << tracked);
        if (!r.ok()) {
            afterGitCommand(service);
            if (error) *error = r.message();
            return false;
        }
    }
    for (const QString& path : untracked) {
        if (QFile::remove(root.filePath(path))) gitOutput(tr("removed %1").arg(path), false);
    }
    // the open tabs follow their files, their edits dropped: the question named them (EDIT-GIT-10)
    const QList<EditorDocument*> tabs = documents;
    for (EditorDocument* doc : tabs) {
        if (!doc->hasFile() || qobject_cast<ComparisonDocument*>(doc)) continue;
        QString relative = root.relativeFilePath(QFileInfo(doc->filePath()).absoluteFilePath());
        if (!tracked.contains(relative)) continue;
        QString reason;
        if (!reloadTab(doc, &reason)) doc->setDiskStatus(EditorDocument::diskUnreadable);
    }
    afterGitCommand(service);
    return true;
}

bool CyberiadaSMEditorWindow::gitRename(const QString& from, const QString& to, QString* error)
{
    RepositoryService* service = gitService(error);
    if (!service) return false;
    QDir root(service->root());
    QString target = QDir::cleanPath(to);
    if (target.isEmpty() || QDir::isAbsolutePath(target) || target.startsWith("..")) {
        if (error) *error = tr("%1 is not a path of the work tree").arg(to);
        return false;
    }
    if (QFileInfo::exists(root.filePath(target))) {
        if (error) *error = tr("%1 exists").arg(target);
        return false;
    }
    QString source = root.filePath(from);
    EditorDocument* tab = nullptr;
    for (EditorDocument* doc : documents) {
        if (doc->hasFile() && !qobject_cast<ComparisonDocument*>(doc) &&
            QFileInfo(doc->filePath()).absoluteFilePath() == QFileInfo(source).absoluteFilePath()) {
            tab = doc;
        }
    }
    // an edited diagram follows its file only saved (EDIT-GIT-11)
    if (tab && qobject_cast<CyberiadaSMEditorDocument*>(tab) && tab->hasEdits()) {
        if (error) *error = tr("%1 has unsaved edits: save it before renaming it").arg(tab->title());
        return false;
    }
    const StatusEntry* e = service->snapshot().entry(from);
    bool ok;
    QString reason;
    // git mv makes no folder
    root.mkpath(QFileInfo(root.filePath(target)).path());
    if (e && !e->untracked() && !e->ignored()) {
        GitResult r = service->command(QStringList() << "mv" << "--" << from << target);
        ok = r.ok();
        reason = r.message();
    } else {
        ok = QFile::rename(source, root.filePath(target));
        reason = tr("%1 is not renamed").arg(from);
        if (ok) gitOutput(tr("renamed %1 to %2").arg(from, target), false);
    }
    if (ok && tab) tab->followFile(root.filePath(target), nullptr);
    afterGitCommand(service);
    if (!ok && error) *error = reason;
    return ok;
}

bool CyberiadaSMEditorWindow::gitDelete(const QStringList& paths, QString* error)
{
    RepositoryService* service = gitService(error);
    if (!service) return false;
    QDir root(service->root());
    QStringList tracked, other;
    for (const QString& path : paths) {
        const StatusEntry* e = service->snapshot().entry(path);
        if (e && !e->untracked() && !e->ignored()) tracked << path;
        else other << path;
    }
    bool ok = true;
    QString reason;
    if (!tracked.isEmpty()) {
        // git refuses a file with changes not committed (EDIT-GIT-11)
        GitResult r = service->command(QStringList() << "rm" << "-q" << "--" << tracked);
        ok = r.ok();
        reason = r.message();
    }
    for (const QString& path : other) {
        if (QFile::remove(root.filePath(path))) gitOutput(tr("deleted %1").arg(path), false);
    }
    // the tabs of the deleted files close without a question: the question named them
    const QList<EditorDocument*> tabs = documents;
    for (EditorDocument* doc : tabs) {
        if (!doc->hasFile() || qobject_cast<ComparisonDocument*>(doc)) continue;
        QString relative = root.relativeFilePath(QFileInfo(doc->filePath()).absoluteFilePath());
        if (paths.contains(relative) && !QFileInfo::exists(doc->filePath())) closeDocument(doc, true);
    }
    afterGitCommand(service);
    if (!ok && error) *error = reason;
    return ok;
}

bool CyberiadaSMEditorWindow::gitCommit(const QString& message, bool amend, QString* error)
{
    RepositoryService* service = gitService(error);
    if (!service) return false;
    if (message.trimmed().isEmpty()) {
        if (error) *error = tr("the message is empty");
        return false;
    }
    QStringList args = QStringList() << "commit" << "-F" << "-";
    if (amend) args << "--amend";
    GitResult r = service->command(args, message.toUtf8());
    afterGitCommand(service);
    if (!r.ok()) {
        service->setPendingMessage(message);
        if (error) *error = r.message();
        return false;
    }
    service->setPendingMessage(QString());
    return true;
}

ComparisonDocument* CyberiadaSMEditorWindow::gitCompare(const QString& path, const QString& revision, QString* error)
{
    RepositoryService* service = gitService(error);
    if (!service) return nullptr;
    QString file = QDir(service->root()).filePath(path);
    if (path.isEmpty() || !isDocumentFile(file)) {
        if (error) *error = tr("%1 is not a diagram or a text the editor opens").arg(path);
        return nullptr;
    }
    const StatusEntry* e = service->snapshot().entry(path);
    if (revision.isEmpty() && e && e->conflicted()) {
        if (error) *error = tr("%1 is in conflict: the index has no file of it").arg(path);
        return nullptr;
    }
    // the tab of the file, opened first (EDIT-GIT-12)
    EditorDocument* tab = modeForFile(file) == MODE_HSM ? static_cast<EditorDocument*>(findDocument(file, false))
                                                        : static_cast<EditorDocument*>(findTextTab(file));
    if (!tab) {
        if (!openFile(file, error)) return nullptr;
        tab = active;
    }
    DiffOrigin base;
    base.kind = DiffOrigin::originGit;
    base.path = file;
    base.revision = revision;
    static const QRegularExpression fullHash("^[0-9a-f]{40}$");
    base.label = revision.isEmpty() ? tr("index") :
                 fullHash.match(revision).hasMatch() ? revision.left(GIT_SHORT_HASH) : revision;
    if (TextDocument* text = qobject_cast<TextDocument*>(tab)) return compareText(text, base, error);
    return compareDocument(qobject_cast<CyberiadaSMEditorDocument*>(tab), base, error);
}

void CyberiadaSMEditorWindow::gitHistory(const QString& path, bool repository)
{
    m_historyPath = path;
    m_historyRepository = repository;
    m_historyView->setRepositoryScope(repository);
    if (!qApp->property("batchMode").toBool()) {
        SettingsManager::instance().setShowHistory(true);
        historyDock->raise();
    }
    updateHistory();
}

void CyberiadaSMEditorWindow::updateHistory(bool more)
{
    if (!m_historyView) return;
    bool batch = qApp->property("batchMode").toBool();
    // only while shown; the batch reads as its verbs ask (EDIT-GIT-6/15)
    if (!batch && historyDock->isHidden()) return;
    RepositoryService* service = m_repository.data();
    if (!service) {
        m_historyView->setTitle(noRepositoryReason(active));
        m_historyView->model()->setEntries(QVector<LogEntry>(), false);
        return;
    }
    QString path = m_historyRepository ? QString() : m_historyPath;
    if (!m_historyRepository && path.isEmpty()) {
        m_historyView->setTitle(tr("No file"));
        m_historyView->model()->setEntries(QVector<LogEntry>(), false);
        return;
    }
    // the batch reads the status first: a staged rename is in it (EDIT-GIT-15)
    if (batch && !service->hasSnapshot()) service->refresh(true);
    QString what = m_historyRepository ? QFileInfo(service->root()).fileName() : path;
    // a staged rename is not committed yet: its commits are those of its old path
    const StatusEntry* entry = path.isEmpty() ? nullptr : service->snapshot().entry(path);
    if (entry && !entry->oldPath.isEmpty()) path = entry->oldPath;
    int skip = more ? m_historyView->model()->entries().size() : 0;
    QString error;
    QVector<LogEntry> rows = service->log(path, HISTORY_PAGE + 1, skip, &error);
    bool hasMore = rows.size() > HISTORY_PAGE;
    if (hasMore) rows.removeLast();
    if (more) m_historyView->model()->appendEntries(rows, hasMore);
    else m_historyView->model()->setEntries(rows, hasMore);
    if (!error.isEmpty()) m_historyView->setTitle(tr("History of %1: %2").arg(what, error));
    else if (m_historyView->model()->entries().isEmpty()) m_historyView->setTitle(tr("History of %1: no commits").arg(what));
    else m_historyView->setTitle(tr("History of %1").arg(what));
}

void CyberiadaSMEditorWindow::stageFiles(const QStringList& paths)
{
    QString error;
    if (!gitStage(paths, &error)) reportGitFailure(tr("Stage"), error);
}

void CyberiadaSMEditorWindow::unstageFiles(const QStringList& paths)
{
    QString error;
    if (!gitUnstage(paths, &error)) reportGitFailure(tr("Unstage"), error);
}

void CyberiadaSMEditorWindow::discardFiles(const QStringList& paths)
{
    RepositoryService* service = m_repository.data();
    if (!service || paths.isEmpty()) return;
    // the files and the tabs whose edits go too, No the default (EDIT-GIT-10)
    QStringList tabs;
    QDir root(service->root());
    for (EditorDocument* doc : documents) {
        if (!doc->hasFile() || !doc->hasEdits()) continue;
        if (paths.contains(root.relativeFilePath(QFileInfo(doc->filePath()).absoluteFilePath()))) tabs << doc->title();
    }
    QString text = tr("Discard the changes of these files? The untracked ones are deleted.") + "\n\n" +
                   paths.join('\n');
    if (!tabs.isEmpty()) text += "\n\n" + tr("The unsaved edits of these tabs are lost too:") + "\n" + tabs.join('\n');
    if (QMessageBox::question(this, tr("Discard Changes"), text, QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!gitDiscard(paths, &error)) reportGitFailure(tr("Discard Changes"), error);
}

void CyberiadaSMEditorWindow::renameFile(const QString& path)
{
    if (!m_repository || path.isEmpty()) return;
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Rename"), tr("The new name of %1:").arg(path),
                                         QLineEdit::Normal, QFileInfo(path).fileName(), &ok);
    if (!ok || name.trimmed().isEmpty() || name == QFileInfo(path).fileName()) return;
    // a name relative to the folder of the file (EDIT-GIT-11)
    QString folder = QFileInfo(path).path();
    QString target = QDir::cleanPath(folder == "." ? name.trimmed() : folder + "/" + name.trimmed());
    QString error;
    if (!gitRename(path, target, &error)) reportGitFailure(tr("Rename"), error);
}

void CyberiadaSMEditorWindow::deleteFiles(const QStringList& paths)
{
    if (!m_repository || paths.isEmpty()) return;
    QStringList tabs;
    QDir root(m_repository->root());
    for (EditorDocument* doc : documents) {
        if (doc->hasFile() && paths.contains(root.relativeFilePath(QFileInfo(doc->filePath()).absoluteFilePath()))) {
            tabs << doc->title();
        }
    }
    QString text = tr("Delete these files?") + "\n\n" + paths.join('\n');
    if (!tabs.isEmpty()) text += "\n\n" + tr("These tabs are closed:") + "\n" + tabs.join('\n');
    if (QMessageBox::question(this, tr("Delete"), text, QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!gitDelete(paths, &error)) reportGitFailure(tr("Delete"), error);
}

void CyberiadaSMEditorWindow::compareFile(const QString& path, const QString& revision)
{
    QString error;
    if (!gitCompare(path, revision, &error)) reportGitFailure(tr("Compare"), error);
}

void CyberiadaSMEditorWindow::compareFileWithRevision(const QString& path)
{
    QString error;
    RepositoryService* service = gitService(&error);
    if (!service) {
        reportGitFailure(tr("Compare with a Revision"), error);
        return;
    }
    QString root = service->root();
    RevisionDialog dialog(path, service->log(path, REVISION_CHOICES, 0, nullptr),
                          [root](const QString& revision, QString* reason) {
                              if (revision.startsWith('-')) {
                                  *reason = tr("a revision does not start with -");
                                  return false;
                              }
                              GitResult r = GitProcess::run(root, QStringList() << "rev-parse" << "--verify" << "-q"
                                                                               << revision + "^{commit}");
                              if (!r.ok()) *reason = tr("%1 is not a revision of the repository").arg(revision);
                              return r.ok();
                          }, this);
    if (dialog.exec() != QDialog::Accepted) return;
    compareFile(path, dialog.revision());
}

void CyberiadaSMEditorWindow::commitIndex()
{
    QString error;
    RepositoryService* service = gitService(&error);
    if (!service) {
        reportGitFailure(tr("Commit"), error);
        return;
    }
    // what the commit takes as it is now
    service->refresh(true);
    QStringList unsaved;
    QDir root(service->root());
    for (EditorDocument* doc : documents) {
        if (!doc->hasFile() || !doc->hasEdits() || qobject_cast<ComparisonDocument*>(doc)) continue;
        if (!root.relativeFilePath(QFileInfo(doc->filePath()).absoluteFilePath()).startsWith("..")) unsaved << doc->title();
    }
    CommitDialog dialog(service->snapshot(), unsaved, service->pendingMessage(),
                        [this](const QString& message, bool amend, QString* reason) {
                            return gitCommit(message, amend, reason);
                        },
                        [service]() { return service->lastMessage(); }, this);
    if (dialog.exec() != QDialog::Accepted) service->setPendingMessage(dialog.message());
}

QString CyberiadaSMEditorWindow::noRepositoryReason(EditorDocument* doc) const
{
    QString path = gitFilePath(doc);
    if (path.isEmpty()) return tr("The document has no file");
    QString reason = GitProcess::unavailableReason();
    if (!reason.isEmpty()) return reason;
    return tr("%1 is not in a git repository").arg(QFileInfo(path).fileName());
}

void CyberiadaSMEditorWindow::slotDiskChanged(const QString& path)
{
    m_diskPending.insert(path);
    m_diskTimer->start();
}

void CyberiadaSMEditorWindow::slotDiskPending()
{
    QSet<QString> paths;
    paths.swap(m_diskPending);
    const QList<EditorDocument*> tabs = documents;
    for (EditorDocument* doc : tabs) {
        if (documents.contains(doc) && doc->hasFile() &&
            paths.contains(QFileInfo(doc->filePath()).absoluteFilePath())) {
            checkDiskTab(doc);
        }
    }
    // a file replaced by a rename (git, a save) is watched no more: it is watched again
    updateDiskWatch();
}

void CyberiadaSMEditorWindow::showDiagramParts(bool on)
{
    // Elements for a diagram, Diagram also for a diff, Text for a text (EDIT-MD-1)
    showModeBar(elementToolBar, on);
    showModeBar(diagramToolBar, on || currentDiff());
    showModeBar(textToolBar, currentText() != nullptr);
    if (!on) {
        for (QToolBar* bar : toolOptionBars) bar->hide();
    } else {
        for (auto i = toolOptionBars.constBegin(); i != toolOptionBars.constEnd(); i++) {
            i.value()->setVisible(i.key() == currentTool);
        }
    }
    const QList<QAction*> zooms = QList<QAction*>() << actionZoomIn << actionZoomOut
                                                    << actionFitContent << actionZoomToSM;
    for (QAction* zoom : zooms) zoom->setEnabled(on);
}

void CyberiadaSMEditorWindow::showModeBar(QToolBar* bar, bool on)
{
    // isHidden, not isVisible: the window may not be shown yet; a bar the user closed stays closed
    if (!on) {
        if (!bar->isHidden()) {
            bar->hide();
            modeHiddenBars.insert(bar);
        }
    } else if (modeHiddenBars.remove(bar)) {
        bar->show();
    }
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
    actionModeTextDiff->setData(QString(MODE_TEXT_DIFF));
    modeGroup->addAction(actionModeTextDiff);
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
        if (ComparisonDocument* diff = currentComparison()) {
            if (!refreshDiff(diff, &error) && !qApp->property("batchMode").toBool()) {
                QMessageBox::warning(this, tr("Refresh"), error);
            }
        }
    });
    // the current row of the change list, the whitespace of a text diff (EDIT-MODE-22/23)
    connect(actionNextChange, &QAction::triggered, this, [this]() {
        if (ComparisonDocument* diff = currentComparison()) diff->moveChange(1);
    });
    connect(actionPreviousChange, &QAction::triggered, this, [this]() {
        if (ComparisonDocument* diff = currentComparison()) diff->moveChange(-1);
    });
    connect(actionIgnoreWhitespace, &QAction::triggered, this, [this](bool on) {
        if (TextDiffDocument* diff = currentTextDiff()) diff->setIgnoreWhitespace(on);
    });
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
    auto kind = [](const QString& id) { return isTextMode(id) ? 1 : id == MODE_DIFF || id == MODE_TEXT_DIFF ? 2 : 0; };
    for (QAction* action : modeGroup->actions()) {
        QString id = action->data().toString();
        action->setChecked(id == mode);
        // a diff reaches none: it is opened by Compare with... (EDIT-MODE-3)
        action->setEnabled(kind(mode) != 2 && kind(id) == kind(mode));
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
    if (qobject_cast<ComparisonDocument*>(tab) || mode == MODE_DIFF || mode == MODE_TEXT_DIFF) {
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

QList<PluginDiagnostic> CyberiadaSMEditorWindow::checkMarkdown(TextDocument* text)
{
    QList<PluginDiagnostic> diagnostics;
    if (!text || text->mode() != MODE_MARKDOWN) return diagnostics;
    // a Markdown file open in a tab is checked by its text there, unsaved headings too
    auto openText = [this](const QString& path) {
        TextDocument* tab = findTextTab(path);
        return tab ? tab->text() : QString();
    };
    for (const MarkdownFinding& f : markdownCheck(text->text(), text->baseDir(), openText)) {
        PluginDiagnostic d;
        d.severity = "warning";
        d.line = f.line;
        d.column = f.column;
        d.message = f.message;
        diagnostics << d;
    }
    // the findings link to the lines of the source (EDIT-PLUG-57)
    pluginDockWidget->showDiagnostics(diagnostics, text);
    pluginDockWidget->show();
    pluginDockWidget->raise();
    return diagnostics;
}

void CyberiadaSMEditorWindow::slotCheck()
{
    commitPendingEdit();
    // a Markdown text: its broken links (EDIT-MD-8)
    if (TextDocument* text = currentText()) {
        if (checkMarkdown(text).isEmpty() && text->mode() == MODE_MARKDOWN &&
            !qApp->property("batchMode").toBool()) {
            QMessageBox::information(this, tr("Check"), tr("The text has no broken link."));
        }
        return;
    }
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
        << actionCheck << actionExport << actionGeometry << actionRefresh
        << actionNextChange << actionPreviousChange << actionIgnoreWhitespace;
    for (QAction* off : diagramOnly) off->setEnabled(false);
    actionIgnoreWhitespace->setChecked(false);
    // a Markdown text checks its links, read-only too (EDIT-MD-8)
    actionCheck->setEnabled(text->mode() == MODE_MARKDOWN);
    // a text compares with a text (EDIT-MODE-19)
    actionCompare->setEnabled(true);
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
        << actionTransitionText << actionFASymbols << actionComponentComments
        << actionLogSession;
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

void CyberiadaSMEditorWindow::updateTerminalAction()
{
    actionOpenTerminal->setEnabled(active && active->hasFile());
}

void CyberiadaSMEditorWindow::slotOpenTerminal()
{
    if (!active || !active->hasFile()) return;
    QString error;
    if (!openSystemTerminal(QFileInfo(active->filePath()).absolutePath(), &error) &&
        !qApp->property("batchMode").toBool()) {
        QMessageBox::warning(this, tr("Open Terminal"), error);
    }
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

void CyberiadaSMEditorWindow::updateMarkdownActions()
{
    TextDocument* text = currentText();
    bool markdown = text && text->mode() == MODE_MARKDOWN;
    // the source of a writable Markdown text takes the inserts (EDIT-MD-1)
    bool inserts = markdown && text->isEditable() && !text->showsRendered();
    actionInsertImage->setEnabled(inserts);
    actionInsertDiagram->setEnabled(inserts);
    actionInsertLink->setEnabled(inserts);
    actionInsertFormula->setEnabled(inserts);
    // the images are other files: a read-only text updates them too
    actionUpdateDiagramImages->setEnabled(markdown && text->hasFile());
    actionPreview->setEnabled(markdown);
    actionWrapLines->setEnabled(text != nullptr);
    actionModeLine->setEnabled(text != nullptr);
    // the writable source of a text that fills (EDIT-MD-20/22)
    bool fills = text && text->fillKind() != textFillNone;
    actionFillParagraph->setEnabled(fills && text->canFill());
    actionUnfillParagraph->setEnabled(fills && text->canFill());
    actionFillAsYouType->setEnabled(fills);
    showTextTools(fills, markdown);
}

void CyberiadaSMEditorWindow::showTextTools(bool fill, bool markdown)
{
    bool fillShown = textToolBar->actions().contains(fillToolActions.first());
    bool markdownShown = textToolBar->actions().contains(markdownToolActions.first());
    if (fillShown == fill && markdownShown == markdown) return;
    for (QAction* action : fillToolActions + markdownToolActions) textToolBar->removeAction(action);
    if (fill) textToolBar->addActions(fillToolActions);
    if (markdown) textToolBar->addActions(markdownToolActions);
}

void CyberiadaSMEditorWindow::fillText(bool unfill)
{
    TextDocument* text = currentText();
    if (text && text->canFill()) text->fill(unfill);
}

bool CyberiadaSMEditorWindow::ensureTextFile(TextDocument* text, const QString& title)
{
    if (text->hasFile()) return true;
    if (qApp->property("batchMode").toBool()) return false;
    QMessageBox box(QMessageBox::Question, title,
                    tr("The text has no file yet. The inserted files are saved in its folder, "
                       "so the text is saved first."),
                    QMessageBox::Cancel, this);
    QPushButton* save = box.addButton(tr("Save and Insert"), QMessageBox::AcceptRole);
    box.setDefaultButton(save);
    box.exec();
    if (box.clickedButton() != save) return false;
    setCurrentDocument(text);
    slotFileSaveAs();
    return text->hasFile();
}

void CyberiadaSMEditorWindow::slotInsertImage()
{
    TextDocument* text = currentText();
    if (!text || text->mode() != MODE_MARKDOWN || !text->isEditable()) return;
    if (!ensureTextFile(text, tr("Insert Image"))) return;
    CyberiadaFileDialog dlg(this, tr("Insert Image"), imageFileFilter());
    dlg.setFileMode(QFileDialog::ExistingFile);
    if (dlg.exec() != QDialog::Accepted || dlg.selectedFile().isEmpty()) return;
    insertImage(text, dlg.selectedFile(), QImage());
}

void CyberiadaSMEditorWindow::insertImage(TextDocument* text, const QString& source, const QImage& pasted)
{
    if (!text || text->mode() != MODE_MARKDOWN || !text->isEditable()) return;
    if (!ensureTextFile(text, tr("Insert Image"))) return;
    SettingsManager& sm = SettingsManager::instance();
    InsertImageDialog dlg(source, pasted, text->baseDir(), sm.getMarkdownImageFolder(), this);
    // a refused name or a failed write goes back to the dialog as it was
    while (dlg.exec() == QDialog::Accepted) {
        ImageInsert insert = dlg.insert();
        QByteArray data;
        QString format, error;
        if (!imageInsertData(insert, &data, &format, &error)) {
            QMessageBox::warning(this, tr("Insert Image"), error);
            continue;
        }
        if (imageTargetClashes(insert, data)) {
            QMessageBox box(QMessageBox::Question, tr("Insert Image"),
                            tr("%1 exists with another content.").arg(markdownPath(insert.target, insert.baseDir)),
                            QMessageBox::Cancel, this);
            QPushButton* replace = box.addButton(tr("Replace"), QMessageBox::DestructiveRole);
            QPushButton* both = box.addButton(tr("Keep Both"), QMessageBox::AcceptRole);
            box.setDefaultButton(both);
            box.exec();
            if (box.clickedButton() == both) {
                QFileInfo target(insert.target);
                insert.target = numberedFileName(target.absolutePath(), target.completeBaseName(), target.suffix());
            } else if (box.clickedButton() != replace) {
                continue;
            }
        }
        if (!writeImageInsert(insert, data, &error)) {
            QMessageBox::warning(this, tr("Insert Image"), error);
            continue;
        }
        if (!dlg.folder().isEmpty()) sm.setMarkdownImageFolder(dlg.folder());
        text->insertBlock(markdownImageLine(insert.alt, insert.target, insert.baseDir), 0, tr("insert image"));
        return;
    }
}

void CyberiadaSMEditorWindow::slotInsertDiagram()
{
    TextDocument* text = currentText();
    if (!text || text->mode() != MODE_MARKDOWN || !text->isEditable()) return;
    if (!ensureTextFile(text, tr("Insert Diagram"))) return;
    // the diagram tabs with a file, their saved files drawn
    QList<DiagramTab> tabs;
    for (EditorDocument* doc : documents) {
        CyberiadaSMEditorDocument* diagram = qobject_cast<CyberiadaSMEditorDocument*>(doc);
        if (!diagram || !diagram->hasFile()) continue;
        DiagramTab tab;
        tab.title = diagram->title();
        tab.path = diagram->filePath();
        tab.modified = !diagram->isClean();
        tabs << tab;
    }
    SettingsManager& sm = SettingsManager::instance();
    QString font = sm.getExportFontFamily().isEmpty() ? FontManager::instance().bundledFamily()
                                                      : sm.getExportFontFamily();
    InsertDiagramDialog dlg(tabs, text->baseDir(), sm.getMarkdownDiagramFolder(), font, this);
    // a refused name or a failed write goes back to the dialog as it was
    while (dlg.exec() == QDialog::Accepted) {
        QString target = dlg.target();
        QString svg = dlg.svg();
        QString error;
        if (dlg.copies() && QFileInfo::exists(target) && !sameFileContent(dlg.source(), target)) {
            QMessageBox box(QMessageBox::Question, tr("Insert Diagram"),
                            tr("%1 exists with another content.").arg(markdownPath(target, text->baseDir())),
                            QMessageBox::Cancel, this);
            QPushButton* replace = box.addButton(tr("Replace"), QMessageBox::DestructiveRole);
            QPushButton* both = box.addButton(tr("Keep Both"), QMessageBox::AcceptRole);
            box.setDefaultButton(both);
            box.exec();
            if (box.clickedButton() == both) {
                QFileInfo info(target);
                target = numberedFileName(info.absolutePath(), info.completeBaseName(), info.suffix());
                svg = QDir(info.absolutePath()).filePath(QFileInfo(target).completeBaseName() + ".svg");
            } else if (box.clickedButton() != replace) {
                continue;
            }
        }
        if ((dlg.copies() && !copyFileOver(dlg.source(), target, &error)) ||
            !renderDiagramFile(target, svg, dlg.font(), &error)) {
            QMessageBox::warning(this, tr("Insert Diagram"), error);
            continue;
        }
        if (!dlg.folder().isEmpty()) sm.setMarkdownDiagramFolder(dlg.folder());
        text->insertBlock(markdownDiagramLine(dlg.alt(), svg, target, text->baseDir()), 0, tr("insert diagram"));
        return;
    }
}

void CyberiadaSMEditorWindow::slotInsertLink()
{
    TextDocument* text = currentText();
    if (!text || text->mode() != MODE_MARKDOWN || !text->isEditable()) return;
    if (!ensureTextFile(text, tr("Insert Link"))) return;
    // the selection is the link text, on one line
    QString selection = text->editor()->textCursor().selectedText();
    selection.replace(QChar::ParagraphSeparator, ' ');
    auto openText = [this](const QString& path) {
        TextDocument* tab = findTextTab(path);
        return tab ? tab->text() : QString();
    };
    InsertLinkDialog dlg(text->filePath(), text->text(), selection, openText, this);
    if (dlg.exec() == QDialog::Accepted && !dlg.insertedLine().isEmpty()) {
        text->insertInline(dlg.insertedLine(), tr("insert link"));
    }
}

void CyberiadaSMEditorWindow::slotInsertFormula()
{
    insertFormula(currentText());
}

// the dialog of a formula of the text: its place kept, the same formula changes nothing
static void editFormula(QWidget* parent, TextDocument* text, const MarkdownFormula& formula)
{
    QString source = text->text();
    FormulaPlace place;
    place.before = markdownCharBefore(source, formula.begin);
    place.after = markdownCharAfter(source, formula.end);
    InsertFormulaDialog dlg(formula.tex, formula.display, place, true, parent);
    if (dlg.exec() != QDialog::Accepted || dlg.insertedText().isEmpty()) return;
    if (dlg.tex() == formula.tex && dlg.display() == formula.display) return;
    QString written = dlg.insertedText();
    if (written == source.mid(formula.begin, formula.end - formula.begin)) return;
    text->replaceRange(formula.begin, formula.end, written, CyberiadaSMEditorWindow::tr("edit formula"));
}

void CyberiadaSMEditorWindow::insertFormula(TextDocument* text)
{
    if (!text || text->mode() != MODE_MARKDOWN || !text->isEditable() || text->showsRendered()) return;
    QString source = text->text();
    QTextCursor cursor = text->editor()->textCursor();
    // a formula is edited, never nested in another (EDIT-MD-24)
    for (int end : {cursor.position(), cursor.anchor()}) {
        MarkdownFormula formula = markdownFormulaAround(source, end);
        if (!formula.isNull()) {
            editFormula(this, text, formula);
            return;
        }
    }
    FormulaPlace place;
    place.before = markdownCharBefore(source, cursor.selectionStart());
    place.after = markdownCharAfter(source, cursor.selectionEnd());
    place.displayBlock = !cursor.hasSelection();
    QString selection = cursor.selectedText();
    selection.replace(QChar::ParagraphSeparator, '\n');
    InsertFormulaDialog dlg(selection, false, place, false, this);
    if (dlg.exec() != QDialog::Accepted || dlg.insertedText().isEmpty()) return;
    if (dlg.display() && place.displayBlock) text->insertBlock(dlg.insertedText(), 0, tr("insert formula"));
    else text->insertInline(dlg.insertedText(), tr("insert formula"));
}

void CyberiadaSMEditorWindow::editFormulaAt(TextDocument* text, int position)
{
    if (!text || text->mode() != MODE_MARKDOWN || !text->isEditable()) return;
    MarkdownFormula formula = markdownFormulaAt(text->text(), position);
    if (!formula.isNull()) editFormula(this, text, formula);
}

QList<PluginDiagnostic> CyberiadaSMEditorWindow::updateDiagramImages(TextDocument* text, bool all)
{
    QList<PluginDiagnostic> results;
    if (!text || text->mode() != MODE_MARKDOWN) return results;
    SettingsManager& sm = SettingsManager::instance();
    QString font = sm.getExportFontFamily().isEmpty() ? FontManager::instance().bundledFamily()
                                                      : sm.getExportFontFamily();
    QList<DiagramImage> images = markdownDiagramImages(text->text(), text->baseDir());
    for (const DiagramUpdate& u : ::updateDiagramImages(images, all, font)) {
        PluginDiagnostic d;
        d.severity = u.updated ? "info" : "error";
        d.line = u.image.image.line;
        QString svg = markdownPath(u.image.svg, text->baseDir());
        d.message = u.updated ? tr("%1 exported again from %2").arg(svg, markdownPath(u.image.graphml, text->baseDir()))
                              : tr("%1 is not exported: %2").arg(svg, u.error);
        results << d;
    }
    // the findings link to the lines of the text (EDIT-PLUG-57)
    pluginDockWidget->showDiagnostics(results, text);
    if (!results.isEmpty()) {
        pluginDockWidget->show();
        pluginDockWidget->raise();
    }
    return results;
}

void CyberiadaSMEditorWindow::slotUpdateDiagramImages()
{
    TextDocument* text = currentText();
    if (!text || text->mode() != MODE_MARKDOWN) return;
    QList<DiagramImage> images = markdownDiagramImages(text->text(), text->baseDir());
    if (images.isEmpty()) {
        QMessageBox::information(this, tr("Update Diagram Images"), tr("The text has no diagram images."));
        return;
    }
    bool stale = false;
    for (const DiagramImage& image : images) stale = stale || !image.upToDate;
    bool all = false;
    if (!stale) {
        QMessageBox box(QMessageBox::Information, tr("Update Diagram Images"),
                        tr("The diagram images of the text are up to date."), QMessageBox::Close, this);
        QPushButton* again = box.addButton(tr("Re-export All"), QMessageBox::AcceptRole);
        box.exec();
        if (box.clickedButton() != again) return;
        all = true;
    }
    updateDiagramImages(text, all);
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
    ComparisonDocument* comparison = currentComparison();
    DiffDocument* diff = currentDiff();
    TextDiffDocument* text = currentTextDiff();
    // nothing to edit, save or export; a diagram view zooms, the comparison refreshes
    const QList<QAction*> off = QList<QAction*>()
        << actionCut << actionCopy << actionPaste << actionDeleteElement
        << actionCenterHorizontally << actionCenterVertically << actionReconstructGeometry
        << actionUpdateGeometry << actionDocumentProperties << actionCheck << actionExport
        << actionSave << actionSaveAs << actionCompare << actionZoomToSM;
    for (QAction* action : off) action->setEnabled(false);
    for (QAction* tool : toolActMap) tool->setEnabled(false);
    // a text diff copies the selection of its focused side (EDIT-MODE-24)
    actionCopy->setEnabled(text && text->focusedPane()->textCursor().hasSelection());
    enableCanvasSwitches(diff != nullptr);
    actionZoomIn->setEnabled(diff != nullptr);
    actionZoomOut->setEnabled(diff != nullptr);
    actionFitContent->setEnabled(diff != nullptr);
    actionGeometry->setEnabled(diff != nullptr);
    actionGeometry->setChecked(diff && diff->geometry());
    actionIgnoreWhitespace->setEnabled(text != nullptr);
    actionIgnoreWhitespace->setChecked(text && text->ignoreWhitespace());
    actionRefresh->setEnabled(true);
    bool rows = comparison && !comparison->changes().isEmpty();
    actionNextChange->setEnabled(rows);
    actionPreviousChange->setEnabled(rows);
    updateModeActions();
    updateUndoActions();
    updatePluginActions();
}

void CyberiadaSMEditorWindow::fillChangeList()
{
    if (!m_changeList) return;
    QSignalBlocker block(m_changeList);
    m_changeList->clear();
    ComparisonDocument* diff = currentComparison();
    if (!diff) return;
    QTreeWidgetItem* group = nullptr;
    QString machine;
    const QList<ChangeEntry> entries = diff->changes();
    for (int i = 0; i < entries.size(); i++) {
        const ChangeEntry& entry = entries.at(i);
        QString change = entry.change == "added" ? tr("added") :
                         entry.change == "removed" ? tr("removed") : entry.change;
        QTreeWidgetItem* item = nullptr;
        if (entry.group.isEmpty()) {
            item = new QTreeWidgetItem(m_changeList);
        } else {
            if (!group || entry.group != machine) {
                machine = entry.group;
                group = new QTreeWidgetItem(m_changeList, QStringList() << machine);
                group->setExpanded(true);
            }
            // the machine's own row is its group
            item = entry.element == entry.group ? group : new QTreeWidgetItem(group);
        }
        item->setText(0, entry.element);
        item->setText(1, change);
        item->setToolTip(0, entry.element);
        item->setToolTip(1, change);
        item->setData(0, Qt::UserRole, i);
        QColor color = entry.kind == ChangeEntry::changeAdded ? QColor(DIFF_ADDED_COLOR) :
                       entry.kind == ChangeEntry::changeRemoved ? QColor(DIFF_REMOVED_COLOR)
                                                                : QColor(DIFF_CHANGED_COLOR);
        item->setForeground(1, color);
    }
    m_changeList->expandAll();
    fitChangeColumn();
    selectChange(diff->currentChange());
}

// what differs stays in view: the change column as wide as its texts, at most what the
// element column leaves at its least, elided past it (EDIT-MODE-11)
void CyberiadaSMEditorWindow::fitChangeColumn()
{
    QHeaderView* header = m_changeList->header();
    int available = m_changeList->viewport()->width() - header->minimumSectionSize();
    int content = qMax(header->sectionSizeHint(1),
                       static_cast<QAbstractItemView*>(m_changeList)->sizeHintForColumn(1));
    header->resizeSection(1, qMax(header->minimumSectionSize(), qMin(content, available)));
}

// the row of the current change selected in the list, none for -1
void CyberiadaSMEditorWindow::selectChange(int index)
{
    if (!m_changeList) return;
    QSignalBlocker block(m_changeList);
    m_changeList->clearSelection();
    for (QTreeWidgetItemIterator it(m_changeList); *it; ++it) {
        QVariant row = (*it)->data(0, Qt::UserRole);
        if (row.isValid() && row.toInt() == index) {
            m_changeList->setCurrentItem(*it);
            m_changeList->scrollToItem(*it);
            return;
        }
    }
}

void CyberiadaSMEditorWindow::slotChangeActivated(QTreeWidgetItem* item)
{
    ComparisonDocument* diff = currentComparison();
    if (!diff || !item) return;
    QVariant row = item->data(0, Qt::UserRole);
    // a group without a row of its own shows nothing
    if (row.isValid()) diff->setCurrentChange(row.toInt());
}

// a new comparison tab: its list follows its compares and its current row
void CyberiadaSMEditorWindow::openComparison(ComparisonDocument* diff)
{
    connect(diff, &ComparisonDocument::compared, this, [this, diff]() { if (diff == active) fillChangeList(); });
    connect(diff, &ComparisonDocument::currentChangeChanged, this, [this, diff](int index) {
        if (diff == active) selectChange(index);
    });
    setCurrentDocument(diff);
    QTimer::singleShot(0, diff, &ComparisonDocument::fit);
}

bool CyberiadaSMEditorWindow::originBuffer(const DiffOrigin& origin, const std::string& last,
                                           std::string* buffer, QString* error) const
{
    if (origin.kind == DiffOrigin::originTab) {
        // a text with its unsaved edits, in UTF-8 (EDIT-MODE-19)
        if (TextDocument* text = qobject_cast<TextDocument*>(origin.tab.data())) {
            QByteArray bytes = text->text().toUtf8();
            *buffer = std::string(bytes.constData(), size_t(bytes.size()));
            return true;
        }
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
    if (origin.kind == DiffOrigin::originGit) {
        // the file in the revision, HEAD resolved anew at every Refresh (EDIT-GIT-12)
        QString folder = QFileInfo(origin.path).absolutePath();
        RepositoryService* service = RepositoryService::forRoot(RepositoryService::rootOf(folder));
        if (!service) {
            if (error) *error = tr("%1 is not in a git repository").arg(QFileInfo(origin.path).fileName());
            return false;
        }
        QString relative = QDir(service->root()).relativeFilePath(QFileInfo(origin.path).absoluteFilePath());
        QByteArray bytes;
        if (!service->base(relative, origin.revision, &bytes, error)) return false;
        *buffer = std::string(bytes.constData(), size_t(bytes.size()));
        return true;
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

bool CyberiadaSMEditorWindow::refreshDiff(ComparisonDocument* diff, QString* error, bool wait)
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
    openComparison(diff);
    return diff;
}

TextDiffDocument* CyberiadaSMEditorWindow::compareText(TextDocument* source, const DiffOrigin& base,
                                                      QString* error)
{
    if (!source) {
        if (error) *error = tr("the active tab holds no text");
        return nullptr;
    }
    TextDiffDocument* diff = new TextDiffDocument(source->mode(), this);
    diff->base() = base;
    diff->target().kind = DiffOrigin::originTab;
    diff->target().tab = source;
    diff->target().label = source->title();
    if (!refreshDiff(diff, error)) {
        delete diff;
        return nullptr;
    }
    addTab(diff);
    connect(diff->basePane(), &QPlainTextEdit::copyAvailable, this,
            [this, diff](bool) { if (diff == active) updateEditActions(); });
    connect(diff->targetPane(), &QPlainTextEdit::copyAvailable, this,
            [this, diff](bool) { if (diff == active) updateEditActions(); });
    openComparison(diff);
    return diff;
}

// a text compares with its saved file, the other text tabs or a file (EDIT-MODE-19)
void CyberiadaSMEditorWindow::slotCompareText(TextDocument* text)
{
    QStringList labels;
    QList<DiffOrigin> origins;
    if (text->hasFile() && !text->isClean()) {
        DiffOrigin saved;
        saved.kind = DiffOrigin::originSaved;
        saved.tab = text;
        saved.path = text->filePath();
        saved.label = tr("saved");
        labels << tr("The saved file");
        origins << saved;
    }
    for (EditorDocument* doc : documents) {
        TextDocument* other = qobject_cast<TextDocument*>(doc);
        if (!other || other == text) continue;
        DiffOrigin tab;
        tab.kind = DiffOrigin::originTab;
        tab.tab = other;
        tab.label = other->title();
        labels << other->title() + other->modeMarker();
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
                                                    tr("All files (*)"), nullptr,
                                                    QFileDialog::DontUseNativeDialog);
        if (path.isEmpty()) return;
        base.kind = DiffOrigin::originFile;
        base.path = path;
        base.label = QFileInfo(path).fileName();
    }
    QString error;
    if (!compareText(text, base, &error)) {
        QMessageBox::warning(this, tr("Compare with"), error);
    }
}

void CyberiadaSMEditorWindow::slotCompare()
{
    commitPendingEdit();
    if (TextDocument* text = currentText()) {
        slotCompareText(text);
        return;
    }
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
    updateTerminalAction();
    updateMarkdownActions();
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
    actionNextChange->setEnabled(false);
    actionPreviousChange->setEnabled(false);
    actionIgnoreWhitespace->setEnabled(false);
    actionIgnoreWhitespace->setChecked(false);
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
    // the change list of a diff tab, a dock of its own under the other two
    m_changeList = new QTreeWidget();
    m_changeList->setObjectName("changeList");
    m_changeList->setHeaderLabels(QStringList() << tr("Element") << tr("Change"));
    // what differs stays in view: the element column takes the rest, elided (EDIT-MODE-11)
    m_changeList->header()->setStretchLastSection(false);
    m_changeList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_changeList->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_changeList->viewport()->installEventFilter(this);
    connect(m_changeList, &QTreeWidget::itemActivated, this, &CyberiadaSMEditorWindow::slotChangeActivated);
    connect(m_changeList, &QTreeWidget::itemClicked, this, &CyberiadaSMEditorWindow::slotChangeActivated);
    changesDock = new QDockWidget(tr("Changes"), this);
    changesDock->setObjectName("changesDock");
    changesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    changesDock->setWidget(m_changeList);
    addDockWidget(Qt::RightDockWidgetArea, changesDock);
    resizeDocks(QList<QDockWidget*>() << changesDock, QList<int>() << PANE_DOCK_WIDTH, Qt::Horizontal);
    // the outline of a Markdown tab, a dock of its own (EDIT-MD-13)
    m_outlineView = new MarkdownOutlineView();
    m_outlineView->setObjectName("outlineView");
    connect(m_outlineView, &MarkdownOutlineView::rowShown, this,
            [this](const QModelIndex& index) { showOutlineRow(index, false); });
    connect(m_outlineView, &MarkdownOutlineView::rowEntered, this,
            [this](const QModelIndex& index) { showOutlineRow(index, true); });
    connect(m_outlineView, &MarkdownOutlineView::diagramRequested, this, &CyberiadaSMEditorWindow::openOutlineDiagram);
    connect(m_outlineView, &MarkdownOutlineView::imageRequested, this, &CyberiadaSMEditorWindow::openOutlineImage);
    outlineDock = new QDockWidget(tr("Outline"), this);
    outlineDock->setObjectName("outlineDock");
    outlineDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    outlineDock->setWidget(m_outlineView);
    addDockWidget(Qt::RightDockWidgetArea, outlineDock);
    resizeDocks(QList<QDockWidget*>() << outlineDock, QList<int>() << PANE_DOCK_WIDTH, Qt::Horizontal);
    createRepositoryDock();

    actionStructureTree = treeDock->toggleViewAction();
    actionStructureTree->setObjectName("actionStructureTree");
    actionProperties = propertiesDock->toggleViewAction();
    actionProperties->setObjectName("actionPropertiesPane");
    actionChanges = changesDock->toggleViewAction();
    actionChanges->setObjectName("actionChangesPane");
    actionOutline = outlineDock->toggleViewAction();
    actionOutline->setObjectName("actionOutlinePane");
    menuView->insertAction(actionTransitionText, actionStructureTree);
    menuView->insertAction(actionTransitionText, actionProperties);
    menuView->insertAction(actionTransitionText, actionChanges);
    menuView->insertAction(actionTransitionText, actionOutline);
    menuView->insertAction(actionTransitionText, actionRepository);
    menuView->insertAction(actionTransitionText, actionHistory);
    menuView->insertSeparator(actionTransitionText);
    treeDock->installEventFilter(this);
    propertiesDock->installEventFilter(this);
    changesDock->installEventFilter(this);
    outlineDock->installEventFilter(this);
    repositoryDock->installEventFilter(this);
    historyDock->installEventFilter(this);
    // the choice changes by a View entry or a close button only, never by a tab switch
    connect(actionStructureTree, &QAction::triggered, &sm, &SettingsManager::setShowStructureTree);
    connect(actionProperties, &QAction::triggered, &sm, &SettingsManager::setShowProperties);
    connect(actionChanges, &QAction::triggered, &sm, &SettingsManager::setShowChanges);
    connect(actionOutline, &QAction::triggered, &sm, &SettingsManager::setShowOutline);
    connect(actionRepository, &QAction::triggered, &sm, &SettingsManager::setShowRepository);
    connect(actionHistory, &QAction::triggered, &sm, &SettingsManager::setShowHistory);
    connect(&sm, &SettingsManager::structureTreeChanged, this, &CyberiadaSMEditorWindow::applyDocks);
    connect(&sm, &SettingsManager::propertiesChanged, this, &CyberiadaSMEditorWindow::applyDocks);
    connect(&sm, &SettingsManager::changesChanged, this, &CyberiadaSMEditorWindow::applyDocks);
    connect(&sm, &SettingsManager::outlineChanged, this, &CyberiadaSMEditorWindow::applyDocks);
    connect(&sm, &SettingsManager::repositoryChanged, this, &CyberiadaSMEditorWindow::applyDocks);
    connect(&sm, &SettingsManager::historyChanged, this, &CyberiadaSMEditorWindow::applyDocks);
    applyDocks();
}

void CyberiadaSMEditorWindow::applyDocks()
{
    const SettingsManager& sm = SettingsManager::instance();
    // before the first tab: the docks of a diagram
    int docks = active ? modeTraits(active->mode()).docks : modeDockTree | modeDockProperties;
    const struct {
        QDockWidget* dock;
        QAction* entry;
        int flag;
        bool chosen;
    } rows[] = {
        { treeDock,       actionStructureTree, modeDockTree,       sm.getShowStructureTree() },
        { propertiesDock, actionProperties,    modeDockProperties, sm.getShowProperties() },
        { changesDock,    actionChanges,       modeDockChanges,    sm.getShowChanges() },
        { outlineDock,    actionOutline,       modeDockOutline,    sm.getShowOutline() }
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        bool has = (docks & rows[i].flag) != 0;
        rows[i].entry->setEnabled(has);
        rows[i].dock->setVisible(has && rows[i].chosen);
    }
    // the git dock is in every tab while git is in use (EDIT-UI-6, EDIT-GIT-1)
    bool git = !GitProcess::program().isEmpty();
    actionRepository->setEnabled(git);
    actionRepository->setToolTip(git ? QString() : GitProcess::unavailableReason());
    repositoryDock->setVisible(git && sm.getShowRepository());
    actionHistory->setEnabled(git);
    actionHistory->setToolTip(actionRepository->toolTip());
    historyDock->setVisible(git && sm.getShowHistory());
    bindOutline();
}

void CyberiadaSMEditorWindow::bindOutline()
{
    TextDocument* text = currentText();
    MarkdownOutlineModel* outline = text ? text->outline() : nullptr;
    // read before the tree takes it, so it shows the text as it is
    if (outline) text->setOutlineShown(!outlineDock->isHidden());
    m_outlineView->setOutlineModel(outline);
    followOutlineCursor();
}

void CyberiadaSMEditorWindow::followOutlineCursor()
{
    TextDocument* text = currentText();
    if (m_outlineShowing || !text || !text->outline() || text->outlineOutOfDate()) return;
    m_outlineView->follow(text->outlineRowAtCursor());
}

void CyberiadaSMEditorWindow::showOutlineRow(const QModelIndex& index, bool focus)
{
    TextDocument* text = currentText();
    const OutlineEntry* entry = text && text->outline() ? text->outline()->entry(index) : nullptr;
    if (!entry) return;
    QPointer<TextDocument> tab(text);
    QPersistentModelIndex row(index);
    OutlineEntry named = *entry;
    // after the event: a reading removes rows the tree may still be handling (EDIT-MD-16)
    QMetaObject::invokeMethod(this, [this, tab, row, named, focus]() {
        if (!tab || tab != currentText()) return;
        m_outlineMessage.clear();
        m_outlineShowing = true;
        QModelIndex shown = tab->showOutlineRow(row, named, focus);
        m_outlineShowing = false;
        if (shown.isValid()) {
            m_outlineView->follow(shown);
            return;
        }
        m_outlineMessage = named.isImage() ? tr("The image \"%1\" is no longer in the text").arg(named.target)
                                           : tr("The heading \"%1\" is no longer in the text").arg(named.title);
        followOutlineCursor();
        // at the pointer over the tree, else under the current row
        QWidget* area = m_outlineView->viewport();
        QPoint at = area->mapFromGlobal(QCursor::pos());
        if (!area->rect().contains(at)) at = m_outlineView->visualRect(m_outlineView->currentIndex()).bottomLeft();
        QToolTip::showText(area->mapToGlobal(at), m_outlineMessage, m_outlineView);
    }, Qt::QueuedConnection);
}

void CyberiadaSMEditorWindow::openOutlineDiagram(const QModelIndex& index)
{
    TextDocument* text = currentText();
    const OutlineEntry* entry = text && text->outline() ? text->outline()->entry(index) : nullptr;
    if (!entry || entry->kind != OutlineEntry::outlineDiagram) return;
    QString path = entry->graphml;
    // after the event: the new tab takes the tree from under it
    QMetaObject::invokeMethod(this, [this, path]() { openLinkedFile(path, QString()); }, Qt::QueuedConnection);
}

void CyberiadaSMEditorWindow::openOutlineImage(const QModelIndex& index)
{
    TextDocument* text = currentText();
    const OutlineEntry* entry = text && text->outline() ? text->outline()->entry(index) : nullptr;
    if (!entry || !entry->isImage()) return;
    QString path = entry->kind == OutlineEntry::outlineDiagram ? entry->svg : markdownImagePath(entry->target, text->baseDir());
    if (!path.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool CyberiadaSMEditorWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (m_changeList && watched == m_changeList->viewport() && event->type() == QEvent::Resize) {
        fitChangeColumn();
    }
    if (event->type() == QEvent::Close) {
        if (watched == treeDock) SettingsManager::instance().setShowStructureTree(false);
        if (watched == propertiesDock) SettingsManager::instance().setShowProperties(false);
        if (watched == changesDock) SettingsManager::instance().setShowChanges(false);
        if (watched == outlineDock) SettingsManager::instance().setShowOutline(false);
        if (watched == repositoryDock) SettingsManager::instance().setShowRepository(false);
        if (watched == historyDock) SettingsManager::instance().setShowHistory(false);
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
    // the type and the folder of the active tab first (EDIT-MODE-13)
    QString folder = active && active->hasFile() ? QFileInfo(active->filePath()).absolutePath() : QString();
    OpenFileDialog dlg(this, openFileMode(), folder);
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

void CyberiadaSMEditorWindow::openLinkedFile(const QString& path, const QString& fragment)
{
    openRecentFile(path);
    // an anchor no heading has only opens the tab
    TextDocument* text = findTextTab(path);
    if (!fragment.isEmpty() && text && text == currentText()) text->showHeading(fragment);
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
        if (!confirmDiskOverwrite(text, nullptr)) return;
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
    if (!confirmDiskOverwrite(current, nullptr)) return;
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
    // the SVG records the saved file it is drawn from (EDIT-IO-8)
    bool saved = current->hasFile() && current->isClean();
    ExportImageDialog dlg(this, saved ? QString() : tr("Save the diagram first: the metadata records its saved file"));
    // propose the diagram's base name; the dialog's default suffix / filter adds
    // the image extension (mirrors slotFileSaveAs)
    if (current && current->hasFile()) {
        dlg.selectFile(QFileInfo(current->filePath()).completeBaseName());
    }
    if (dlg.exec() != QDialog::Accepted) { return; }

    QString fileName = dlg.selectedFile();
    if (!fileName.isEmpty()) {
        QString error;
        QString source = dlg.recordSource() ? current->filePath() : QString();
        QString title = current->documentName().isEmpty() ? QFileInfo(current->filePath()).completeBaseName()
                                                          : current->documentName();
        if (!renderScene(scene(), fileName, &error, dlg.dpi(), dlg.fontFamily(), source, title)) {
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
    if (TextDiffDocument* diff = currentTextDiff()) {
        diff->focusedPane()->copy();
        return;
    }
    if (!current || scene()->selectedItems().isEmpty()) return;
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
    if (qobject_cast<ComparisonDocument*>(doc)) return tr("a diff tab gives a plugin no input");
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
    } else if (!confirmDiskOverwrite(doc, error) || !doc->save(error)) {
        // a file changed on disk is not written over unasked (EDIT-DOC-17)
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
                                                 EditorDocument* doc, ComparisonDocument* refresh)
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
    if (refresh) diffRefreshes.insert(run, QPointer<ComparisonDocument>(refresh));
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
                                                       EditorDocument* doc, ComparisonDocument* refresh)
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
    QPointer<ComparisonDocument> refresh = diffRefreshes.take(run);
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
        ComparisonDocument* diff = refresh.data();
        if (!diff) {
            // a text gets a text diff tab (EDIT-PLUG-58)
            if (text) diff = new TextDiffDocument(text->mode(), this);
            else diff = new DiffDocument(this);
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
            openComparison(diff);
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

void CyberiadaSMEditorWindow::slotLineActivated(EditorDocument* doc, int line, int column)
{
    TextDocument* text = qobject_cast<TextDocument*>(doc);
    if (!text || !documents.contains(doc)) return;
    setCurrentDocument(doc);
    text->goToLine(line, column);
}

void CyberiadaSMEditorWindow::slotFileActivated(const QString& path)
{
    // an open file activates its tab (EDIT-DOC-2)
    QString error;
    if (!openFile(path, &error) && !qApp->property("batchMode").toBool()) {
        QMessageBox::warning(this, tr("Open"), error);
    }
}
