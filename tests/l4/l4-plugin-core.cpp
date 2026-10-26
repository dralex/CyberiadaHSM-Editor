/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process plugin core test: descriptors, values, variables and the runner
 * (see docs/TESTING.md, L4)
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
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "plugin_channel.h"
#include "plugin_descriptor.h"
#include "plugin_registry.h"
#include "plugin_runner.h"
#include "plugin_values.h"
#include "settings_manager.h"
#include "smeditor_window.h"

class TestPluginCore: public QObject {
    Q_OBJECT

private slots:
    void test_parse_defaults();
    void test_parse_rejections_data();
    void test_parse_rejections();
    void test_parse_warnings();
    void test_parse_session();
    void test_channel_lines();
    void test_default_folders();
    void test_config_data_home();
    void test_resolve_interpreter();
    void test_expand_arguments();
    void test_values();
    void test_settings_keys();
    void test_read_diagnostics();
    void test_runner_cancel();
    void test_run_dir();
    void test_output_decoding();
    void test_selected_ids_order();
    void test_translation();

private:
    // a plugin entry running the helper with the operations of one log action
    PluginEntry helperEntry(const QStringList& operations, const QString& folder) const;
    PluginResult runToEnd(PluginRunner* run) const;
};

static PluginDescriptor parsed(const QByteArray& json, QString* error = nullptr)
{
    PluginDescriptor d;
    QString e;
    bool ok = PluginDescriptor::parse(json, "/plugins/x", &d, &e);
    if (error) *error = ok ? QString() : e;
    return d;
}

PluginEntry TestPluginCore::helperEntry(const QStringList& operations, const QString& folder) const
{
    QJsonArray argv;
    argv.append(QString(PLUGIN_HELPER));
    for (const QString& op: operations) argv.append(op);
    QJsonObject action;
    action.insert("id", "run");
    action.insert("label", "Run");
    action.insert("result", "log");
    action.insert("argv", argv);
    QJsonObject root;
    root.insert("api", 1);
    root.insert("id", "helper");
    root.insert("name", "Helper");
    root.insert("actions", QJsonArray() << action);
    PluginEntry entry;
    QString error;
    bool ok = PluginDescriptor::parse(QJsonDocument(root).toJson(), folder, &entry.descriptor, &error);
    if (!ok) qWarning() << error;
    entry.folder = folder;
    entry.status = PluginEntry::statusEnabled;
    return entry;
}

PluginResult TestPluginCore::runToEnd(PluginRunner* run) const
{
    QSignalSpy spy(run, &PluginRunner::finished);
    run->start();
    if (!run->isFinished()) spy.wait(20000);
    return run->result();
}

// the defaults the contract promises (docs/PLUGIN-API.md, plugin.json)
void TestPluginCore::test_parse_defaults()
{
    QString error;
    PluginDescriptor d = parsed(R"({"api": 1, "id": "p", "name": "P",
        "parameters": [{"id": "b", "type": "bool"}, {"id": "e", "type": "enum", "choices": ["x", "y"]}],
        "actions": [{"id": "a", "label": "A", "result": "log", "argv": ["run"]}]})", &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const PluginAction* a = d.action("a");
    QVERIFY(a);
    QCOMPARE(a->exitOk, QList<int>() << 0);
    QCOMPARE(a->timeout, PLUGIN_TIMEOUT_DEFAULT);
    QVERIFY(!a->cwdDocument && !a->save && !a->toolbar && !a->loadStrict && !a->loadReconstruct);
    QCOMPARE(d.parameter("b")->defaultValue, QString("false"));
    QCOMPARE(d.parameter("b")->label, QString("b"));
    QCOMPARE(d.parameter("e")->defaultValue, QString("x"));
    QCOMPARE(d.parameter("e")->scope, PluginParameter::scopeRun);
    QCOMPARE(d.actionParameters(*a).size(), 2);
    QVERIFY(!d.hasStoredParameters());
}

void TestPluginCore::test_parse_rejections_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<QString>("reason");
    const QByteArray action = R"({"id": "a", "label": "A", "result": "log", "argv": ["run"]})";
    QTest::newRow("no api") << QByteArray(R"({"id": "p", "name": "P", "actions": [)" + action + "]}")
                            << "the api is not given";
    QTest::newRow("no name") << QByteArray(R"({"api": 1, "id": "p", "actions": [)" + action + "]}")
                             << "the name is not given";
    QTest::newRow("unknown type")
        << QByteArray(R"({"api": 1, "id": "p", "name": "P", "parameters": [{"id": "x", "type": "colour"}],
                         "actions": [)" + action + "]}")
        << "unknown type";
    QTest::newRow("unknown scope")
        << QByteArray(R"({"api": 1, "id": "p", "name": "P", "parameters": [{"id": "x", "type": "string",
                         "scope": "forever"}], "actions": [)" + action + "]}")
        << "unknown scope";
    QTest::newRow("enum default")
        << QByteArray(R"({"api": 1, "id": "p", "name": "P", "parameters": [{"id": "x", "type": "enum",
                         "choices": ["a"], "default": "b"}], "actions": [)" + action + "]}")
        << "is not a choice";
    QTest::newRow("secret export")
        << QByteArray(R"({"api": 1, "id": "p", "name": "P", "parameters": [{"id": "k", "type": "secret"}],
                         "actions": [)" + action + "]}")
        << "no valid export variable";
    QTest::newRow("outdir type")
        << QByteArray(R"({"api": 1, "id": "p", "name": "P", "parameters": [{"id": "x", "type": "string"}],
                         "actions": [{"id": "a", "label": "A", "result": "files", "outdir": "x",
                         "argv": ["run"]}]})")
        << "is not a dir or file parameter";
    QTest::newRow("condition")
        << QByteArray(R"({"api": 1, "id": "p", "name": "P", "actions": [{"id": "a", "label": "A",
                         "result": "log", "argv": ["run", {"if": "nope", "argv": ["-x"]}]}]})")
        << "names an unknown parameter 'nope'";
    QTest::newRow("timeout")
        << QByteArray(R"({"api": 1, "id": "p", "name": "P", "actions": [{"id": "a", "label": "A",
                         "result": "log", "timeout": -1, "argv": ["run"]}]})")
        << "non-negative integer";
    QTest::newRow("duplicate parameter")
        << QByteArray(R"({"api": 1, "id": "p", "name": "P", "parameters": [{"id": "x", "type": "string"},
                         {"id": "x", "type": "int"}], "actions": [)" + action + "]}")
        << "is duplicated";
    QTest::newRow("secret in a condition")
        << QByteArray(R"({"api": 1, "id": "p", "name": "P", "parameters": [{"id": "k", "type": "secret",
                         "export": "K"}], "actions": [{"id": "a", "label": "A", "result": "log",
                         "argv": ["run", {"if": "k", "argv": ["${param-file:k}"]}]}]})")
        << "is used in the argv";
}

// a descriptor breaking a rule is rejected with the reason (EDIT-PLUG-2)
void TestPluginCore::test_parse_rejections()
{
    QFETCH(QByteArray, json);
    QFETCH(QString, reason);
    QString error;
    parsed(json, &error);
    QVERIFY2(error.contains(reason), qPrintable(error));
}

// the unknown fields are listed, the plugin stays usable (EDIT-PLUG-3)
void TestPluginCore::test_parse_warnings()
{
    QString error;
    PluginDescriptor d = parsed(R"({"api": 1, "id": "p", "name": "P", "colour": "red",
        "session": {"version": 1, "actions": []},
        "parameters": [{"id": "x", "type": "string", "hint": "h"}],
        "actions": [{"id": "a", "label": "A", "result": "log", "argv": ["run"],
                     "load": {"strict": true, "fast": true}}]})", &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(d.warnings, QStringList() << "unknown field 'colour'" << "unknown field 'parameters/x/hint'"
                                       << "unknown field 'actions/a/load/fast'");
}

// the session actions follow the one-shot ones and hide those they replace; a
// broken session block costs only the session actions (EDIT-PLUG-36)
void TestPluginCore::test_parse_session()
{
    QString error;
    PluginDescriptor d = parsed(R"({"api": 1, "id": "p", "name": "P",
        "actions": [{"id": "run", "label": "Run", "result": "log", "argv": ["x"]},
                    {"id": "check", "label": "Check", "result": "log", "argv": ["x"]}],
        "session": {"version": 1, "actions": [{"id": "step", "label": "Step", "argv": ["x"],
                                               "replaces": ["run"]}]}})", &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(d.sessionVersion, SESSION_PROTOCOL_VERSION);
    QCOMPARE(d.actions.size(), 3);
    QVERIFY(d.action("run")->hidden);
    QVERIFY(!d.action("check")->hidden);
    const PluginAction* step = d.action("step");
    QVERIFY(step && step->session);
    QCOMPARE(step->result, PluginAction::resultLog);
    QCOMPARE(step->timeout, 0);
    QVERIFY(d.warnings.isEmpty());

    // each broken block keeps the plugin with its one-shot actions
    const char* const broken[][2] = {
        { R"({"version": 2, "actions": []})", "the session version 2 is not supported" },
        { R"({"actions": []})", "the session version is not given" },
        { R"({"version": 1, "actions": [{"id": "s", "label": "S", "argv": ["x"], "replaces": ["nope"]}]})",
          "replaces an unknown action 'nope'" },
        { R"({"version": 1, "actions": [{"id": "run", "label": "S", "argv": ["x"]}]})",
          "the action 'run' is duplicated" },
        { R"({"version": 1, "actions": [{"id": "s", "label": "S", "argv": ["x"], "result": "replace"}]})",
          "cannot have the result 'replace'" },
    };
    for (const auto& b: broken) {
        QByteArray json = QByteArray(R"({"api": 1, "id": "p", "name": "P", "actions": [{"id": "run",
            "label": "Run", "result": "log", "argv": ["x"]}], "session": )") + b[0] + "}";
        PluginDescriptor k = parsed(json, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(k.actions.size(), 1);
        QVERIFY(!k.actions.first().hidden);
        QCOMPARE(k.sessionVersion, 0);
        QCOMPARE(k.warnings.size(), 1);
        QVERIFY2(k.warnings.first().contains(b[1]), qPrintable(k.warnings.first()));
    }
    // the one-shot actions stay required, for the editors before the sessions
    parsed(R"({"api": 1, "id": "p", "name": "P", "actions": [],
        "session": {"version": 1, "actions": [{"id": "s", "label": "S", "argv": ["x"]}]}})", &error);
    QVERIFY(error.contains("the plugin has no actions"));
}

// one object per line: a message by its kind, any other line is text (EDIT-PLUG-38)
void TestPluginCore::test_channel_lines()
{
    PluginChannel channel;
    QStringList seen;
    connect(&channel, &PluginChannel::text, [&seen](const QString& line) { seen << "text " + line; });
    connect(&channel, &PluginChannel::notificationReceived, [&seen](const QString& method, const QJsonValue& p) {
        seen << "notification " + method + (p.isArray() ? " array" : "");
    });
    connect(&channel, &PluginChannel::requestReceived,
            [&seen](const QJsonValue& id, const QString& method, const QJsonValue&) {
                seen << QString("request %1 %2").arg(id.toInt()).arg(method);
            });
    connect(&channel, &PluginChannel::replyReceived, [&seen](int id, const QJsonValue&, const QJsonObject& e) {
        seen << QString("reply %1%2").arg(id).arg(e.isEmpty() ? "" : " error");
    });
    QList<QByteArray> sent;
    connect(&channel, &PluginChannel::send, [&sent](const QByteArray& line, const QJsonObject&) { sent << line; });
    channel.receive("hello from print()");
    channel.receive(R"({"jsonrpc":"2.0","method":"state","params":{"status":"paused"}})");
    channel.receive(R"({"jsonrpc":"2.0","method":"diagnostics","params":[]})");
    channel.receive(R"({"jsonrpc":"2.0","id":7,"method":"ask","params":{"text":"?"}})");
    channel.receive(R"({"jsonrpc":"2.0","id":1,"result":{}})");
    channel.receive(R"({"jsonrpc":"2.0","id":2,"error":{"code":-32602,"message":"bad"}})");
    channel.receive("{ not json");
    QCOMPARE(seen, QStringList() << "text hello from print()" << "notification state"
                                 << "notification diagnostics array" << "request 7 ask" << "reply 1"
                                 << "reply 2 error" << "text { not json");
    QCOMPARE(channel.request("initialize"), 1);
    QCOMPARE(channel.request("save"), 2);
    channel.notify("control");
    QCOMPARE(sent.size(), 3);
    QVERIFY(sent.first().endsWith('\n'));
    QCOMPARE(sent.first().count('\n'), 1);
}

// the default search path: the variable's folders, the user, the system (EDIT-PLUG-1)
void TestPluginCore::test_default_folders()
{
    QString variable = QString("/a") + QDir::listSeparator() + QDir::listSeparator() + "/b";
    // a rooted path: Windows gives the drive of the root to both sides
    const QString opt = QDir::rootPath() + "opt/editor/";
    QList<PluginFolder> folders = PluginRegistry::defaultFolders(opt + "bin", variable, "/home/u/plugins");
    QStringList origins;
    for (const PluginFolder& f: folders) origins << f.origin;
    QCOMPARE(origins, QStringList() << "env 1" << "env 2" << "user" << "system" << "system");
    QCOMPARE(folders.at(3).path, QDir(opt + PLUGIN_SYSTEM_FOLDER).absolutePath());
    QCOMPARE(folders.at(4).path, QDir(opt + "bin/" PLUGIN_SYSTEM_FOLDER).absolutePath());
}

// a separate configuration, the same on every system (EDIT-UI-7): the preferences in an
// ini file under CYBERIADA_PRO_CONFIG_HOME, the user plugins under CYBERIADA_PRO_DATA_HOME
void TestPluginCore::test_config_data_home()
{
    const QString config = qEnvironmentVariable(CONFIG_HOME_VARIABLE);
    if (config.isEmpty()) QSKIP("run by ctest, which sets " CONFIG_HOME_VARIABLE);
    QCOMPARE(QSettings::defaultFormat(), QSettings::IniFormat);
    const QString file = QDir::cleanPath(QSettings().fileName());
    QVERIFY2(file.startsWith(QDir::cleanPath(config) + "/") && file.endsWith(".ini"), qPrintable(file));

    QTemporaryDir data;
    QVERIFY(data.isValid());
    const QByteArray kept = qgetenv(DATA_HOME_VARIABLE);
    qputenv(DATA_HOME_VARIABLE, data.path().toUtf8());
    PluginRegistry::instance().setSearchPath(QStringList());
    QString user;
    for (const PluginFolder& f: PluginRegistry::instance().searchPath()) {
        if (f.origin == "user") user = f.path;
    }
    qputenv(DATA_HOME_VARIABLE, kept);
    QVERIFY2(user.startsWith(QDir(data.path()).absolutePath() + "/") && user.endsWith("/plugins"),
             qPrintable(user));
}

// the Preferences path first, then PATH (EDIT-PLUG-4)
void TestPluginCore::test_resolve_interpreter()
{
    QString program;
    QStringList arguments;
    QMap<QString, QString> prefs;
    prefs.insert("helper", PLUGIN_HELPER);
    QVERIFY(PluginRegistry::resolveInterpreter("helper", prefs, &program, &arguments));
    QCOMPARE(QFileInfo(program).canonicalFilePath(), QFileInfo(PLUGIN_HELPER).canonicalFilePath());
    QVERIFY(arguments.isEmpty());
    // a configured path that is not a program wins over PATH and fails
    prefs.insert("sh", "/nonexistent/sh");
    QVERIFY(!PluginRegistry::resolveInterpreter("sh", prefs, &program, &arguments));
    QVERIFY(!PluginRegistry::resolveInterpreter("no-such-interpreter", QMap<QString, QString>(),
                                                &program, &arguments));
}

// the variables in one pass, the conditional elements (EDIT-PLUG-8)
void TestPluginCore::test_expand_arguments()
{
    PluginDescriptor d = parsed(R"({"api": 1, "id": "p", "name": "P",
        "parameters": [{"id": "flag", "type": "bool"}, {"id": "text", "type": "text"},
                       {"id": "key", "type": "secret", "export": "K"}],
        "actions": [{"id": "a", "label": "A", "result": "log",
                     "argv": ["${input}", "$HOME", "${param:text}", "${param-file:text}",
                              "${meta:platform}", "${meta:absent}",
                              {"if": "flag", "argv": ["--on"]}, {"if": "text", "argv": ["--text"]}]},
                    {"id": "unknown", "label": "U", "result": "log", "argv": ["${nope}"]},
                    {"id": "open", "label": "O", "result": "log", "argv": ["${input"]}]})");
    QMap<QString, QString> variables;
    variables.insert("input", "/run/input.graphml");
    variables.insert("rundir", "/run");
    QList<QPair<QString, QString> > meta;
    meta.append(qMakePair(QString("platform"), QString("ArduinoUno")));
    PluginValues values;
    values.insert("flag", "false");
    // a value holding a variable stays as it is
    values.insert("text", "${input}");
    values.insert("key", "env:K");
    PluginExpansion out;
    QString error;
    QVERIFY2(PluginRunner::expandArguments(d, *d.action("a"), values, variables, meta, &out, &error),
             qPrintable(error));
    QCOMPARE(out.arguments, QStringList() << "/run/input.graphml" << "$HOME" << "${input}"
                                          << "/run/param-text.txt" << "ArduinoUno" << "" << "--text");
    QCOMPARE(out.parameterFiles.value("param-text.txt"), QString("${input}"));
    QVERIFY(!out.usesRequest);

    values.insert("flag", "true");
    QVERIFY(PluginRunner::expandArguments(d, *d.action("a"), values, variables, meta, &out, &error));
    QVERIFY(out.arguments.contains("--on"));

    QVERIFY(!PluginRunner::expandArguments(d, *d.action("unknown"), values, variables, meta, &out, &error));
    QVERIFY(error.contains("${nope}"));
    QVERIFY(!PluginRunner::expandArguments(d, *d.action("open"), values, variables, meta, &out, &error));
    QVERIFY(error.contains("not closed"));
}

// the defaults, the checks and the batch values (EDIT-PLUG-11, EDIT-PLUG-27)
void TestPluginCore::test_values()
{
    PluginDescriptor d = parsed(R"({"api": 1, "id": "p", "name": "P",
        "parameters": [{"id": "dir", "type": "dir", "default": "${document_dir}/out", "required": true},
                       {"id": "n", "type": "int", "default": 2, "min": 1, "max": 5},
                       {"id": "s", "type": "secret", "export": "S"},
                       {"id": "t", "type": "text"}],
        "actions": [{"id": "a", "label": "A", "result": "log", "argv": ["run"]}]})");
    const PluginAction& a = *d.action("a");
    // a rooted path: Windows gives the drive of the root to both sides
    PluginDocumentPaths doc = pluginDocumentPaths(QDir::rootPath() + "work/light.graphml");
    QCOMPARE(doc.name, QString("light"));
    QCOMPARE(pluginDefaultValue(*d.parameter("dir"), doc), QDir(QDir::rootPath() + "work").absolutePath() + "/out");
    // an untitled document has no directory: such a default is empty
    QCOMPARE(pluginDefaultValue(*d.parameter("dir"), PluginDocumentPaths()), QString());

    QString error;
    QVERIFY(pluginCheckValue(*d.parameter("n"), "5", &error));
    QVERIFY(!pluginCheckValue(*d.parameter("n"), "6", &error));
    QVERIFY(!pluginCheckValue(*d.parameter("n"), "two", &error));
    QVERIFY(pluginCheckValue(*d.parameter("s"), "env:KEY", &error));
    QVERIFY(!pluginCheckValue(*d.parameter("s"), "the key itself", &error));
    // the message never shows a secret value
    QVERIFY(!error.contains("the key itself"));

    QTemporaryDir tmp;
    QFile f(tmp.filePath("value.txt"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("line 1\nline 2\n");
    f.close();
    PluginValues values;
    QVERIFY2(pluginParseValues(d, a, QStringList() << "t=@" + f.fileName() << "n=3", doc, &values, &error),
             qPrintable(error));
    QCOMPARE(values.value("t"), QString("line 1\nline 2"));
    QCOMPARE(values.value("n"), QString("3"));
    QVERIFY(pluginParseValues(d, a, QStringList() << "t=@@x", doc, &values, &error));
    QCOMPARE(values.value("t"), QString("@x"));
    QVERIFY(!pluginParseValues(d, a, QStringList() << "nope=1", doc, &values, &error));
    // the required directory has no default for an untitled document
    QVERIFY(!pluginParseValues(d, a, QStringList(), PluginDocumentPaths(), &values, &error));
    QVERIFY(error.contains("'dir'"));

    QString key = pluginDocumentKey("/work/light.graphml");
    QCOMPARE(key.size(), 40);
    QCOMPARE(key, pluginDocumentKey("/work/../work/light.graphml"));
}

// the settings keys of docs/PLUGINS.md (EDIT-PLUG-11)
void TestPluginCore::test_settings_keys()
{
    SettingsManager& sm = SettingsManager::instance();
    sm.setPluginValue("core-test", "enabled", "a value");
    sm.setPluginDocumentValue("core-test", "0123", "dir", "/out");
    sm.setInterpreterPath("python3", "/usr/bin/python3");
    QSettings s;
    QCOMPARE(s.value("plugins/core-test/values/enabled").toString(), QString("a value"));
    QCOMPARE(s.value("plugins/core-test/documents/0123/dir").toString(), QString("/out"));
    QCOMPARE(s.value("plugin-interpreters/python3").toString(), QString("/usr/bin/python3"));
    // a parameter named enabled does not touch the plugin switch
    QVERIFY(sm.getPluginEnabled("core-test"));
    bool found = false;
    QCOMPARE(sm.getPluginValue("core-test", "enabled", &found), QString("a value"));
    QVERIFY(found);
    sm.getPluginValue("core-test", "absent", &found);
    QVERIFY(!found);
    QCOMPARE(sm.getInterpreterPaths().value("python3"), QString("/usr/bin/python3"));
    sm.setInterpreterPath("python3", QString());
    QVERIFY(!sm.getInterpreterPaths().contains("python3"));
    s.remove("plugins/core-test");
}

// the diagnostics file (docs/PLUGIN-API.md, EDIT-PLUG-15)
void TestPluginCore::test_read_diagnostics()
{
    QList<PluginDiagnostic> list;
    QString error;
    QVERIFY(PluginRunner::readDiagnostics(R"([{"severity": 1, "message": "a\nb"},
        {"severity": 4, "message": "m", "line": 3}, {"severity": "warning", "message": "w", "extra": 1}])",
        &list, &error));
    QCOMPARE(list.size(), 3);
    QCOMPARE(list.at(0).severity, QString("error"));
    QCOMPARE(list.at(0).message, QString("a b"));
    QCOMPARE(list.at(1).severity, QString("info"));
    QCOMPARE(list.at(1).line, 3);
    QVERIFY(!PluginRunner::readDiagnostics(R"({"severity": "error"})", &list, &error));
    QVERIFY(!PluginRunner::readDiagnostics(R"([{"severity": "fatal", "message": "m"}])", &list, &error));
    QVERIFY(!PluginRunner::readDiagnostics(R"([{"severity": "error"}])", &list, &error));
    QVERIFY(!PluginRunner::readDiagnostics(R"([{"severity": 5, "message": "m"}])", &list, &error));
}

// Cancel terminates the process (EDIT-PLUG-20)
void TestPluginCore::test_runner_cancel()
{
    QTemporaryDir folder;
    PluginEntry entry = helperEntry(QStringList() << "sleep" << "20000", folder.path());
    PluginRunner run(entry, "run", PluginValues(), PluginRunContext());
    QSignalSpy spy(&run, &PluginRunner::finished);
    QElapsedTimer clock;
    clock.start();
    run.start();
    QTRY_VERIFY(run.result().started);
    QTest::qWait(200);
    run.cancel();
    QVERIFY(spy.wait(PLUGIN_KILL_GRACE_MS * 2));
    QCOMPARE(run.result().state, PluginResult::stateCancelled);
    QVERIFY(clock.elapsed() < 20000);
}

// a fresh run dir, removed after the run unless kept (EDIT-PLUG-26)
void TestPluginCore::test_run_dir()
{
    QTemporaryDir folder;
    PluginEntry entry = helperEntry(QStringList() << "print" << "x", folder.path());
    PluginRunner first(entry, "run", PluginValues(), PluginRunContext());
    PluginResult r = runToEnd(&first);
    QCOMPARE(r.state, PluginResult::stateDone);
    QVERIFY(!r.runDir.isEmpty());
    QVERIFY(!QDir(r.runDir).exists());

    qputenv(PLUGIN_KEEP_VARIABLE, "1");
    PluginRunner kept(entry, "run", PluginValues(), PluginRunContext());
    r = runToEnd(&kept);
    qunsetenv(PLUGIN_KEEP_VARIABLE);
    QVERIFY(QFile::exists(QDir(r.runDir).filePath("input.graphml")));
    QDir(r.runDir).removeRecursively();
}

// UTF-8 output and the cap (EDIT-PLUG-17)
void TestPluginCore::test_output_decoding()
{
    QTemporaryDir folder;
    PluginEntry entry = helperEntry(QStringList() << "print" << QString::fromUtf8("Светофор"), folder.path());
    PluginRunner text(entry, "run", PluginValues(), PluginRunContext());
    PluginResult r = runToEnd(&text);
    QCOMPARE(r.stdoutText, QString::fromUtf8("Светофор\n"));

    // about 1.4 MiB: past the cap
    entry = helperEntry(QStringList() << "spam" << "150000", folder.path());
    PluginRunner flood(entry, "run", PluginValues(), PluginRunContext());
    r = runToEnd(&flood);
    QCOMPARE(r.state, PluginResult::stateDone);
    QVERIFY(r.stdoutText.contains("[the output is cut at the limit]"));
    QVERIFY(r.stdoutText.size() < PLUGIN_OUTPUT_CAP * 2);

    // the \r\n of a Windows program as \n, a pair split between two chunks too
    bool cr = false;
    QCOMPARE(PluginRunner::lineFeeds("a\r\nb\r", &cr), QString("a\nb"));
    QVERIFY(cr);
    QCOMPARE(PluginRunner::lineFeeds("\nc\r\n", &cr), QString("\nc\n"));
    QVERIFY(!cr);
}

// ${selection} follows the document order, not the selection order (EDIT-PLUG-9);
// the window keeps one selected element, a bare scene holds several
void TestPluginCore::test_selected_ids_order()
{
    CyberiadaSMModel model(nullptr);
    CyberiadaSMEditorScene scene(&model);
    QVERIFY(model.loadDocument("diagrams/geometry.graphml"));
    scene.loadScene();
    scene.clearSelection();
    scene.getMap().value("node-0-1")->setSelected(true);
    scene.getMap().value("node-0-0-2")->setSelected(true);
    QCOMPARE(scene.selectedIds(), QStringList() << "node-0-0-2" << "node-0-1");
}

// the texts from i18n/<lang>.json, the values kept, the language in defaults (EDIT-UI-4/PLUG-33)
void TestPluginCore::test_translation()
{
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkdir("i18n"));
    QFile ru(dir.filePath("i18n/ru.json"));
    QVERIFY(ru.open(QIODevice::WriteOnly));
    ru.write(u8"{\"Plugin\": \"Плагин\", \"Go\": \"Пуск\", \"Mode\": \"Режим\", \"fast\": \"быстро\", "
             u8"\"Lang\": 1}");
    ru.close();
    QFile de(dir.filePath("i18n/de.json"));
    QVERIFY(de.open(QIODevice::WriteOnly));
    de.write("[1, 2]");
    de.close();
    QString error;
    PluginDescriptor d;
    QVERIFY(PluginDescriptor::parse(R"({"api": 1, "id": "p", "name": "Plugin",
        "parameters": [{"id": "mode", "type": "enum", "label": "Mode", "choices": ["fast", "slow"]},
                       {"id": "lang", "type": "string", "label": "Lang", "default": "${language}"}],
        "actions": [{"id": "go", "label": "Go", "description": "Goes", "result": "log", "argv": ["run"]}]})",
        dir.path(), &d, &error));
    PluginDescriptor english = d;
    d.translate("ru");
    QCOMPARE(d.name, QString::fromUtf8("Плагин"));
    QCOMPARE(d.actions.first().label, QString::fromUtf8("Пуск"));
    // no translation, or a non-string one: the source text stays
    QCOMPARE(d.actions.first().description, QString("Goes"));
    QCOMPARE(d.parameter("lang")->label, QString("Lang"));
    QCOMPARE(d.parameter("mode")->choiceLabels, QStringList() << QString::fromUtf8("быстро") << "slow");
    QCOMPARE(d.parameter("mode")->choices, QStringList() << "fast" << "slow");
    QVERIFY(d.warnings.isEmpty());
    // no file for the language: nothing changes; a broken one is a warning
    PluginDescriptor french = english;
    french.translate("fr");
    QCOMPARE(french.name, QString("Plugin"));
    PluginDescriptor german = english;
    german.translate("de");
    QCOMPARE(german.name, QString("Plugin"));
    QCOMPARE(german.warnings.size(), 1);

    PluginRegistry::instance().setLanguage("ru");
    QCOMPARE(pluginDefaultValue(*english.parameter("lang"), PluginDocumentPaths()), QString("ru"));
    PluginRegistry::instance().setLanguage(QString());
    QCOMPARE(pluginDefaultValue(*english.parameter("lang"), PluginDocumentPaths()), QString("en"));
}

QTEST_MAIN(TestPluginCore)
#include "l4-plugin-core.moc"
