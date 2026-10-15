/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process plugin window test: the menu, the toolbars, the dialog, the
 * lock and the dock (see docs/TESTING.md, L4)
 *
 * Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
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

#include <QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QToolBar>
#include <QTreeWidget>
#include <sstream>

#include "smeditor_window.h"
#include "cyberiadasm_dump.h"
#include "code_highlighter.h"
#include "gesture_log.h"
#include "plugin_dock.h"
#include "plugin_registry.h"
#include "plugins_menu.h"
#include "settings_manager.h"
#include "dialogs/plugin_manager_dialog.h"
#include "dialogs/plugin_parameter_dialog.h"

#define RUN_WAIT_MS 20000

class TestPlugins: public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void test_menu_structure();
    void test_toolbars();
    void test_dialog_widgets();
    void test_dialog_widths();
    void test_click_runs_at_once();
    void test_replace_undo();
    void test_lock();
    void test_result_to_starting_document();
    void test_new_document_tab();
    void test_close_cancels();
    void test_inspection();
    void test_diagnostics_click();
    void test_document_values();
    void test_manager_switch();
    void test_preferences_interpreter();
    void test_secret_absent();
    void test_untitled_save_cancel();
    void test_session_log();
    void test_window_grab();
    void test_translated_texts();
    void test_meta_fill();
};

static QString documentDump(CyberiadaSMModel* model)
{
    std::ostringstream os;
    dumpDocument(model, os);
    return QString::fromStdString(os.str());
}

// the result of an asynchronous run, taken when the window applies it
static PluginResult waitFor(CyberiadaSMEditorWindow& win, PluginRunner* run)
{
    PluginResult result;
    bool done = false;
    QMetaObject::Connection c = QObject::connect(&win, &CyberiadaSMEditorWindow::pluginRunFinished,
        [&result, &done, run](PluginRunner* finished) {
            if (finished != run) return;
            result = finished->result();
            done = true;
        });
    QElapsedTimer clock;
    clock.start();
    while (!done && clock.elapsed() < RUN_WAIT_MS) QTest::qWait(20);
    QObject::disconnect(c);
    return result;
}

// a modal dialog the next event loop opens is handled by the callback
static void onModal(std::function<void(QWidget*)> handle)
{
    QTimer::singleShot(50, [handle]() {
        QWidget* modal = QApplication::activeModalWidget();
        if (modal) handle(modal);
    });
}

void TestPlugins::initTestCase()
{
    PluginRegistry::instance().setSearchPath(QStringList() << PLUGIN_TEST_DIR "/main");
    // a test run alone has no window that would scan first
    PluginRegistry::instance().scan();
    QSettings().remove("plugins");
    QSettings().remove("plugin-interpreters");
}

void TestPlugins::cleanup()
{
    SettingsManager::instance().setInspectorMode(false);
    QSettings().remove("plugins");
    QSettings().remove("plugin-interpreters");
    PluginRegistry::instance().setInterpreterPaths(QMap<QString, QString>());
}

// Plugins before Help, a submenu per plugin, the single entries (EDIT-PLUG-5)
void TestPlugins::test_menu_structure()
{
    CyberiadaSMEditorWindow win;
    PluginsMenu* pm = win.pluginsMenu();
    QVERIFY(pm);
    QList<QAction*> bar = win.QMainWindow::menuBar()->actions();
    int plugins = bar.indexOf(pm->menu()->menuAction());
    QVERIFY(plugins >= 0);
    QCOMPARE(bar.at(plugins + 1), win.menuHelp->menuAction());

    // echo: one action and no stored values, a single entry
    QAction* echo = pm->action("echo", "say");
    QVERIFY(echo);
    QVERIFY(pm->menu()->actions().contains(echo));
    // files: a submenu ending with Settings... for its document value
    QAction* files = pm->action("files", "make");
    QVERIFY(files && !pm->menu()->actions().contains(files));
    QVERIFY(pm->settingsAction("files"));
    QVERIFY(!pm->settingsAction("echo"));
    // the labels of the actions with a dialog end with ...
    QVERIFY(files->text().endsWith("..."));

    // the tail entries close the menu
    QList<QAction*> items = pm->menu()->actions();
    QCOMPARE(items.last()->text(), QString("Plugin manager..."));
    QCOMPARE(items.at(items.size() - 2), win.pluginDock()->toggleViewAction());

    // a missing interpreter disables the action with the reason (EDIT-PLUG-4)
    QAction* never = pm->action("nointerp", "run");
    QVERIFY(never && !never->isEnabled());
    QVERIFY(never->toolTip().contains("no-such-interpreter"));
}

// one toolbar per plugin right of the main toolbar, hidden and kept (EDIT-PLUG-6)
void TestPlugins::test_toolbars()
{
    CyberiadaSMEditorWindow win;
    win.show();
    QVERIFY(QTest::qWaitForWindowExposed(&win));
    PluginsMenu* pm = win.pluginsMenu();
    QToolBar* checker = pm->toolbar("checker");
    QToolBar* echo = pm->toolbar("echo");
    QVERIFY(checker && echo);
    QVERIFY(!pm->toolbar("edit"));
    QCOMPARE(win.toolBarArea(checker), Qt::TopToolBarArea);
    QVERIFY(checker->actions().contains(pm->action("checker", "check")));
    QVERIFY(!checker->actions().contains(pm->action("checker", "bad")));
    QVERIFY(checker->geometry().x() > win.mainToolBar->geometry().x());

    QAction* toggle = checker->toggleViewAction();
    QVERIFY(pm->toolbarsMenu()->actions().contains(toggle));
    toggle->trigger();
    QVERIFY(!checker->isVisible());
    QVERIFY(!SettingsManager::instance().getPluginToolbarVisible("checker"));
    CyberiadaSMEditorWindow second;
    QVERIFY(second.pluginsMenu()->toolbar("checker")->isHidden());
}

// one widget per type, the text box highlighted, Run waits for the required (EDIT-PLUG-7)
void TestPlugins::test_dialog_widgets()
{
    const PluginEntry* vars = PluginRegistry::instance().find("vars");
    const PluginEntry* request = PluginRegistry::instance().find("request");
    QVERIFY(vars && request);
    QList<const PluginParameter*> params = vars->descriptor.actionParameters(*vars->descriptor.action("all"));
    params += request->descriptor.actionParameters(*request->descriptor.action("read"));
    PluginValues values = pluginDefaultValues(vars->descriptor, *vars->descriptor.action("all"),
                                              PluginDocumentPaths());
    PluginParameterDialog dlg("Variables - All", params, values, "Run");
    QVERIFY(qobject_cast<QCheckBox*>(dlg.editor("flag")));
    QSpinBox* count = qobject_cast<QSpinBox*>(dlg.editor("count"));
    QVERIFY(count);
    QCOMPARE(count->maximum(), 10);
    QCOMPARE(count->value(), 3);
    QVERIFY(qobject_cast<QComboBox*>(dlg.editor("mode")));
    QVERIFY(qobject_cast<QDoubleSpinBox*>(dlg.editor("ratio")));
    QLineEdit* key = qobject_cast<QLineEdit*>(dlg.editor("key"));
    QVERIFY(key && key->validator());
    QPlainTextEdit* prompt = qobject_cast<QPlainTextEdit*>(dlg.editor("prompt"));
    QVERIFY(prompt);
    QVERIFY(prompt->document()->findChild<CodeHighlighter*>());
    // the required prompt is empty
    QVERIFY(!dlg.acceptButton()->isEnabled());
    prompt->setPlainText("a traffic light");
    QVERIFY(dlg.acceptButton()->isEnabled());
    QCOMPARE(dlg.values().value("prompt"), QString("a traffic light"));
    QCOMPARE(dlg.values().value("count"), QString("3"));
}

// the dialogs are as wide as their content, within the screen
void TestPlugins::test_dialog_widths()
{
    const PluginEntry* files = PluginRegistry::instance().find("files");
    QVERIFY(files);
    QList<const PluginParameter*> params = files->descriptor.actionParameters(*files->descriptor.action("make"));
    PluginValues values;
    // a path the screen holds is shown whole in its field
    QString path = "/home/user/light/generated/sources";
    values.insert("dir", path);
    PluginParameterDialog dlg("Files - Make", params, values, "Run");
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));
    QLineEdit* dir = qobject_cast<QLineEdit*>(dlg.editor("dir"));
    QVERIFY(dir);
    QVERIFY(dir->width() > dir->fontMetrics().horizontalAdvance(path));
    // a longer one widens the dialog up to the screen share, not beyond it
    values.insert("dir", path + path + path);
    PluginParameterDialog wide("Files - Make", params, values, "Run");
    wide.show();
    QVERIFY(QTest::qWaitForWindowExposed(&wide));
    QCOMPARE(wide.width(), pluginDialogWidth(&wide, INT_MAX));
    // pictures for review only
    QVERIFY(dlg.grab().save(PLUGIN_TEST_DIR "/../plugins-dialog.png"));

    PluginManagerDialog manager;
    manager.show();
    QVERIFY(QTest::qWaitForWindowExposed(&manager));
    QTreeWidget* list = manager.list();
    int columns = 0;
    for (int c = 0; c < list->columnCount(); c++) {
        int needed = 0;
        for (int i = 0; i < list->topLevelItemCount(); i++) {
            needed = qMax(needed, list->fontMetrics().horizontalAdvance(list->topLevelItem(i)->text(c)));
        }
        // the column shows its longest text
        QVERIFY2(list->header()->sectionSize(c) >= needed, qPrintable(QString::number(c)));
        columns += list->header()->sectionSize(c);
    }
    QVERIFY(manager.width() >= qMin(columns, pluginDialogWidth(&manager, INT_MAX)));
    QVERIFY(manager.grab().save(PLUGIN_TEST_DIR "/../plugins-manager.png"));
}

// an action without run parameters runs at once; the dock comes with the run (EDIT-PLUG-32)
void TestPlugins::test_click_runs_at_once()
{
    CyberiadaSMEditorWindow win;
    win.show();
    QVERIFY(!win.pluginDock()->isVisible());
    QSignalSpy finished(&win, &CyberiadaSMEditorWindow::pluginRunFinished);
    win.slotPluginTriggered("echo", "say");
    QVERIFY(finished.wait(RUN_WAIT_MS) || finished.count() > 0);
    QVERIFY(win.pluginDock()->isVisible());
    QTRY_VERIFY(win.pluginDock()->output("echo")->toPlainText().contains("echo from main"));
    QVERIFY(win.pluginDock()->output("echo")->toPlainText().contains("echo to stderr"));
}

// a replace result is one undo step, its undo restores the document (EDIT-PLUG-12/13)
void TestPlugins::test_replace_undo()
{
    CyberiadaSMEditorWindow win;
    QVERIFY(win.openDocument("diagrams/hierarchy.graphml"));
    QString before = documentDump(win.getModel());
    PluginResult r = win.runPluginAndWait("edit", "rename", PluginValues());
    QCOMPARE(r.state, PluginResult::stateDone);
    QVERIFY(r.changed);
    QUndoStack* stack = win.getModel()->undoStack();
    QCOMPARE(stack->count(), 1);
    QCOMPARE(stack->undoText(), QString("Plugin: Rename"));
    QVERIFY(documentDump(win.getModel()).contains("Renamed"));
    stack->undo();
    QCOMPARE(documentDump(win.getModel()), before);

    r = win.runPluginAndWait("edit", "same", PluginValues());
    QCOMPARE(r.state, PluginResult::stateDone);
    QVERIFY(!r.changed);
    QCOMPARE(stack->count(), 1);
}

// a replace run locks its document only (EDIT-PLUG-21/22)
void TestPlugins::test_lock()
{
    CyberiadaSMEditorWindow win;
    QVERIFY(win.openDocument("diagrams/hierarchy.graphml"));
    CyberiadaSMEditorDocument* locked = win.currentDocument();
    PluginRunner* run = win.runPlugin("edit", "wait", PluginValues());
    QVERIFY(run);
    CyberiadaSMModel* model = locked->model();
    QVERIFY(model->isLocked() && model->readOnly());
    QVERIFY(!locked->view()->isInteractive());
    QVERIFY(!win.actionUndo->isEnabled());
    QModelIndex index = model->elementToIndex(model->idToElement("n0"));
    QVERIFY(!model->setData(index, "Changed"));
    // a second run on the document waits, another document runs
    QString error;
    QVERIFY(!win.runPlugin("echo", "say", PluginValues(), &error));
    QVERIFY(!error.isEmpty());
    CyberiadaSMEditorDocument* other = win.newDocument();
    PluginRunner* echo = win.runPlugin("echo", "say", PluginValues(), &error, other);
    QVERIFY2(echo, qPrintable(error));
    QCOMPARE(waitFor(win, echo).state, PluginResult::stateDone);

    QSignalSpy finished(&win, &CyberiadaSMEditorWindow::pluginRunFinished);
    QPushButton* cancel = win.pluginDock()->cancelButton(run);
    QVERIFY(cancel);
    cancel->click();
    QVERIFY(finished.wait(RUN_WAIT_MS));
    QVERIFY(!model->isLocked());
    QVERIFY(locked->view()->isInteractive());
    QVERIFY(model->setData(index, "Changed"));
}

// the result goes to the document the run started from (EDIT-PLUG-21)
void TestPlugins::test_result_to_starting_document()
{
    CyberiadaSMEditorWindow win;
    QVERIFY(win.openDocument("diagrams/hierarchy.graphml"));
    CyberiadaSMEditorDocument* start = win.currentDocument();
    PluginRunner* run = win.runPlugin("edit", "rename-late", PluginValues());
    QVERIFY(run);
    CyberiadaSMEditorDocument* other = win.newDocument();
    win.setCurrentDocument(other);
    PluginResult r = waitFor(win, run);
    QCOMPARE(r.state, PluginResult::stateDone);
    QCOMPARE(start->model()->undoStack()->count(), 1);
    QCOMPARE(other->model()->undoStack()->count(), 0);
    QCOMPARE(win.currentDocument(), other);
}

// a new document becomes the active modified untitled tab (EDIT-PLUG-14)
void TestPlugins::test_new_document_tab()
{
    CyberiadaSMEditorWindow win;
    QVERIFY(win.openDocument("diagrams/hierarchy.graphml"));
    CyberiadaSMEditorDocument* source = win.currentDocument();
    PluginResult r = win.runPluginAndWait("maker", "make", PluginValues());
    QCOMPARE(r.state, PluginResult::stateDone);
    QCOMPARE(win.documentCount(), 2);
    QVERIFY(win.currentDocument() != source);
    QVERIFY(!win.currentDocument()->hasFile());
    QVERIFY(!win.currentDocument()->isClean());
    QVERIFY(source->isClean());
}

// closing the tab cancels the run; the result is never applied (EDIT-PLUG-23)
void TestPlugins::test_close_cancels()
{
    CyberiadaSMEditorWindow win;
    QVERIFY(win.openDocument("diagrams/hierarchy.graphml"));
    win.newDocument();
    CyberiadaSMEditorDocument* doc = win.documentAt(0);
    QSignalSpy finished(&win, &CyberiadaSMEditorWindow::pluginRunFinished);
    QVERIFY(win.runPlugin("edit", "wait", PluginValues(), nullptr, doc));
    QVERIFY(win.closeDocument(doc, true));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(win.documentCount(), 1);
}

// the inspection mode disables replace and refuses a late result (EDIT-PLUG-24)
void TestPlugins::test_inspection()
{
    CyberiadaSMEditorWindow win;
    QVERIFY(win.openDocument("diagrams/hierarchy.graphml"));
    QString before = documentDump(win.getModel());
    PluginRunner* run = win.runPlugin("edit", "rename-late", PluginValues());
    QVERIFY(run);
    SettingsManager::instance().setInspectorMode(true);
    PluginResult r = waitFor(win, run);
    QCOMPARE(r.state, PluginResult::stateFailed);
    QCOMPARE(documentDump(win.getModel()), before);
    QVERIFY(!win.pluginsMenu()->action("edit", "rename")->isEnabled());
    QVERIFY(win.pluginsMenu()->action("checker", "check")->isEnabled());
    QVERIFY(!win.runPlugin("edit", "rename", PluginValues()));
}

// a finding selects its element, an absent one has no link, both grey later (EDIT-PLUG-15)
void TestPlugins::test_diagnostics_click()
{
    CyberiadaSMEditorWindow win;
    QVERIFY(win.openDocument("diagrams/choice.graphml"));
    PluginResult r = win.runPluginAndWait("checker", "check", PluginValues());
    QCOMPARE(r.state, PluginResult::stateDone);
    QTreeWidget* list = win.pluginDock()->diagnostics();
    QCOMPARE(list->topLevelItemCount(), 3);
    QTreeWidgetItem* linked = list->topLevelItem(0);
    QTreeWidgetItem* absent = list->topLevelItem(1);
    QVERIFY(absent->text(2).contains("(absent)"));
    QVERIFY(absent->data(2, Qt::UserRole).toString().isEmpty());
    emit list->itemClicked(linked, 0);
    QModelIndexList rows = win.SMView->selectionModel()->selectedRows();
    QCOMPARE(rows.size(), 1);
    const Cyberiada::Element* selected = win.getModel()->indexToElement(rows.first());
    QVERIFY(selected);
    QCOMPARE(QString::fromStdString(selected->get_id()), QString("nMeta"));
    // the document changes: the findings turn grey
    QModelIndex n = win.getModel()->elementToIndex(win.getModel()->idToElement("n1"));
    QVERIFY(win.getModel()->setData(n, "Changed"));
    QCOMPARE(linked->foreground(4).color(), QColor(Qt::gray));
}

// a document value is asked once per document and edited in Settings... (EDIT-PLUG-7/11)
void TestPlugins::test_document_values()
{
    QTemporaryDir dir;
    QString path = dir.filePath("light.graphml");
    QVERIFY(QFile::copy("diagrams/hierarchy.graphml", path));
    CyberiadaSMEditorWindow win;
    QVERIFY(win.openDocument(path));
    QString out = dir.filePath("out");
    int dialogs = 0;
    onModal([&dialogs, out](QWidget* w) {
        PluginParameterDialog* d = qobject_cast<PluginParameterDialog*>(w);
        if (!d) return;
        dialogs++;
        qobject_cast<QLineEdit*>(d->editor("dir"))->setText(out);
        d->accept();
    });
    QSignalSpy finished(&win, &CyberiadaSMEditorWindow::pluginRunFinished);
    win.slotPluginTriggered("files", "make");
    QCOMPARE(dialogs, 1);
    QVERIFY(finished.wait(RUN_WAIT_MS) || finished.count() > 0);
    QVERIFY(QFile::exists(out + "/src/main.c"));
    // the value is kept for this document: the next click runs at once
    bool asked = false;
    PluginValues values = win.pluginValues(*PluginRegistry::instance().find("files"),
        PluginRegistry::instance().find("files")->descriptor.parameters.isEmpty() ?
        QList<const PluginParameter*>() :
        QList<const PluginParameter*>() << &PluginRegistry::instance().find("files")->descriptor.parameters.first(),
        win.currentDocument(), &asked);
    QVERIFY(!asked);
    QCOMPARE(values.value("dir"), out);
    QVERIFY(QSettings().childGroups().contains("plugins"));
}

// the switch of the plugin manager is kept, Rescan rebuilds the menu (EDIT-PLUG-30)
void TestPlugins::test_manager_switch()
{
    CyberiadaSMEditorWindow win;
    PluginManagerDialog dlg(&win);
    QTreeWidget* list = dlg.list();
    QTreeWidgetItem* echo = nullptr;
    for (int i = 0; i < list->topLevelItemCount(); i++) {
        if (list->topLevelItem(i)->data(0, Qt::UserRole).toString() == "echo") echo = list->topLevelItem(i);
    }
    QVERIFY(echo);
    echo->setCheckState(0, Qt::Unchecked);
    QVERIFY(!SettingsManager::instance().getPluginEnabled("echo"));
    QAction* say = win.pluginsMenu()->action("echo", "say");
    QVERIFY(say && !say->isEnabled());
    QVERIFY(say->toolTip().contains("switched off"));
    dlg.rescan();
    QVERIFY(win.pluginsMenu()->action("echo", "say"));
    SettingsManager::instance().setPluginEnabled("echo", true);
    QVERIFY(win.pluginsMenu()->action("echo", "say")->isEnabled());
}

// an interpreter set in the Preferences counts at the next scan (EDIT-PLUG-31)
void TestPlugins::test_preferences_interpreter()
{
    CyberiadaSMEditorWindow win;
    QVERIFY(!win.pluginsMenu()->action("interp", "run")->isEnabled());
    SettingsManager::instance().setInterpreterPath("cyb-test-helper", PLUGIN_HELPER);
    QVERIFY(win.pluginsMenu()->action("interp", "run")->isEnabled());
    SettingsManager::instance().setInterpreterPath("cyb-test-helper", QString());
    QVERIFY(!win.pluginsMenu()->action("interp", "run")->isEnabled());
}

// a secret reaches the plugin only (EDIT-PLUG-10)
void TestPlugins::test_secret_absent()
{
    qputenv("PLUGIN_TEST_SECRET", "s3cr3t");
    CyberiadaSMEditorWindow win;
    PluginValues values;
    values.insert("prompt", "a prompt");
    values.insert("key", "env:PLUGIN_TEST_SECRET");
    PluginResult r = win.runPluginAndWait("request", "read", values);
    QCOMPARE(r.state, PluginResult::stateDone);
    QVERIFY(r.stdoutText.contains("PLUGIN_TEST_KEY: ok"));
    QVERIFY(!r.stdoutText.contains("s3cr3t"));
    QVERIFY(!win.pluginDock()->output("request")->toPlainText().contains("s3cr3t"));
    const PluginEntry* request = PluginRegistry::instance().find("request");
    win.storePluginValues(*request, request->descriptor.actionParameters(*request->descriptor.action("read")),
                          win.currentDocument(), values);
    QSettings s;
    s.sync();
    QFile conf(s.fileName());
    QVERIFY(conf.open(QIODevice::ReadOnly));
    QVERIFY(!conf.readAll().contains("s3cr3t"));
    qunsetenv("PLUGIN_TEST_SECRET");
}

// a cwd: document action on an untitled document asks for a name (EDIT-PLUG-25)
void TestPlugins::test_untitled_save_cancel()
{
    CyberiadaSMEditorWindow win;
    onModal([](QWidget* w) { w->close(); });
    QString error;
    QVERIFY(!win.runPlugin("docdir", "look", PluginValues(), &error));
    QVERIFY(error.contains("not saved"));
}

// the log records a comment and goes on from the new document (EDIT-PLUG-29)
void TestPlugins::test_session_log()
{
    QTemporaryDir logRoot;
    qputenv("CYBERIADA_SESSION_LOG_DIR", logRoot.path().toUtf8());
    CyberiadaSMEditorWindow win;
    QVERIFY(win.openDocument("diagrams/hierarchy.graphml"));
    GestureLog::instance().startSession(win.getModel());
    QString first = GestureLog::instance().sessionDir();
    PluginResult r = win.runPluginAndWait("edit", "rename", PluginValues());
    QCOMPARE(r.state, PluginResult::stateDone);
    QString second = GestureLog::instance().sessionDir();
    GestureLog::instance().endSession();
    qunsetenv("CYBERIADA_SESSION_LOG_DIR");
    QVERIFY(first != second);
    QFile f(QDir(first).filePath("session.script"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.readAll().contains("# plugin edit:rename"));
    QVERIFY(QFile::exists(QDir(second).filePath("start.graphml")));
}

// a picture of the window with the menu, the toolbars and the dock, for review only
void TestPlugins::test_window_grab()
{
    CyberiadaSMEditorWindow win;
    win.resize(1200, 800);
    win.show();
    QVERIFY(win.openDocument("diagrams/choice.graphml"));
    QCOMPARE(win.runPluginAndWait("checker", "check", PluginValues()).state, PluginResult::stateDone);
    QVERIFY(QTest::qWaitForWindowExposed(&win));
    QTest::qWait(100);
    QVERIFY(win.grab().save(PLUGIN_TEST_DIR "/../plugins-window.png"));
}

// the plugin texts follow the language, the values and the run get it (EDIT-UI-4/PLUG-33)
void TestPlugins::test_translated_texts()
{
    PluginRegistry::instance().setLanguage("ru");
    CyberiadaSMEditorWindow win;
    QAction* all = win.pluginsMenu()->action("vars", "all");
    QVERIFY(all);
    QCOMPARE(all->text(), QString::fromUtf8("Все"));
    bool titled = false;
    for (QAction* a: win.pluginsMenu()->menu()->actions()) {
        if (a->menu() && a->text() == QString::fromUtf8("Переменные")) titled = true;
    }
    QVERIFY(titled);
    // an untranslated text stays as written
    QCOMPARE(win.pluginsMenu()->action("echo", "say")->text(), QString("Say"));

    const PluginEntry* vars = PluginRegistry::instance().find("vars");
    QList<const PluginParameter*> params = vars->descriptor.actionParameters(*vars->descriptor.action("all"));
    PluginValues values = pluginDefaultValues(vars->descriptor, *vars->descriptor.action("all"),
                                              PluginDocumentPaths());
    PluginParameterDialog dlg("vars", params, values, "Run");
    QComboBox* mode = qobject_cast<QComboBox*>(dlg.editor("mode"));
    QVERIFY(mode);
    QCOMPARE(mode->itemText(0), QString::fromUtf8("быстро"));
    mode->setCurrentIndex(1);
    QCOMPARE(dlg.values().value("mode"), QString("slow"));

    // the run and a ${language} default get the language
    const PluginAction& language = *vars->descriptor.action("language");
    PluginResult r = win.runPluginAndWait("vars", "language",
        pluginDefaultValues(vars->descriptor, language, PluginDocumentPaths()));
    QCOMPARE(r.state, PluginResult::stateDone);
    QVERIFY2(r.stdoutText.contains("arg1: ru\narg2: ru"), qPrintable(r.stdoutText));
    PluginRegistry::instance().setLanguage(QString());
    PluginRegistry::instance().scan();
}

// the declared platform language fills an absent one, one undo step (EDIT-PLUG-34)
void TestPlugins::test_meta_fill()
{
    CyberiadaSMEditorWindow win;
    QVERIFY(win.openDocument("diagrams/hierarchy.graphml"));
    QVERIFY(win.getScene()->codeLanguageName().isEmpty());
    PluginResult r = win.runPluginAndWait("vars", "fill", PluginValues());
    QCOMPARE(r.state, PluginResult::stateDone);
    QCOMPARE(r.metaNotes.size(), 1);
    QCOMPARE(r.metaNotes.first().kind, PluginMetaNote::metaSet);
    // the highlighting and Edit > Language follow the new value
    QCOMPARE(win.getScene()->codeLanguageName(), QString("Python"));
    QVERIFY(r.stdoutText.contains("arg1: Python"));
    QUndoStack* stack = win.getModel()->undoStack();
    QCOMPARE(stack->count(), 1);
    stack->undo();
    QVERIFY(win.getScene()->codeLanguageName().isEmpty());

    // another name of the same language is no conflict
    QVERIFY(win.getModel()->setPlatformLanguage("python3"));
    int steps = stack->count();
    r = win.runPluginAndWait("vars", "fill", PluginValues());
    QCOMPARE(r.state, PluginResult::stateDone);
    QVERIFY(r.metaNotes.isEmpty());
    QCOMPARE(stack->count(), steps);
}

QTEST_MAIN(TestPlugins)
#include "l4-plugins.moc"
