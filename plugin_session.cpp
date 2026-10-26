/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Plugin Session: the protocol of a session plugin (plugin level 2)
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

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

#include "plugin_session.h"
#include "plugin_registry.h"
#include "version.h"

#define STATUS_RUNNING  "running"
#define STATUS_PAUSED   "paused"
#define STATUS_WAITING  "waiting"
#define STATUS_FINISHED "finished"
#define ASK_CANCEL      "cancel"

namespace {

QStringList strings(const QJsonValue& value)
{
    QStringList list;
    for (const QJsonValue& v: value.toArray()) {
        if (v.isString()) list.append(v.toString());
        else if (v.isDouble()) list.append(QString::number(v.toDouble()));
    }
    return list;
}

// a parameter value as typed, a number in its shortest form
QString valueText(const QJsonValue& v)
{
    if (v.isString()) return v.toString();
    if (v.isDouble()) return QString::number(v.toDouble());
    if (v.isBool()) return v.toBool() ? "true" : "false";
    if (v.isArray()) {
        QStringList items;
        for (const QJsonValue& item: v.toArray()) items.append(valueText(item));
        return items.join(',');
    }
    if (v.isObject()) {
        QStringList items;
        QJsonObject o = v.toObject();
        for (const QString& key: o.keys()) items.append(key + "=" + valueText(o.value(key)));
        return "{" + items.join(',') + "}";
    }
    return QString();
}

// the fields of a message in the key order, one dump line (EDIT-PLUG-48)
QString fieldsText(const QJsonObject& o)
{
    QStringList items;
    for (const QString& key: o.keys()) items.append(key + "=" + valueText(o.value(key)));
    return items.join(' ');
}

} // namespace

PluginSession::PluginSession(PluginRunner* run):
    QObject(run), runner(run)
{
    initTimer.setSingleShot(true);
    marksTimer.setSingleShot(true);
    connect(&initTimer, &QTimer::timeout, this, &PluginSession::slotInitTimeout);
    connect(&marksTimer, &QTimer::timeout, this, &PluginSession::slotMarks);
    connect(&channel, &PluginChannel::send, this, &PluginSession::slotSend);
    connect(&channel, &PluginChannel::text, this, &PluginSession::slotText);
    connect(&channel, &PluginChannel::requestReceived, this, &PluginSession::slotRequest);
    connect(&channel, &PluginChannel::notificationReceived, this, &PluginSession::slotNotification);
    connect(&channel, &PluginChannel::replyReceived, this, &PluginSession::slotReply);
}

void PluginSession::begin()
{
    QJsonObject params;
    params.insert("session", SESSION_PROTOCOL_VERSION);
    params.insert("editor", CYBERIADA_VERSION);
    params.insert("language", PluginRegistry::instance().language());
    params.insert("plugin", runner->plugin().descriptor.id);
    params.insert("action", runner->action().id);
    initializeId = channel.request("initialize", params);
    pending.insert(initializeId, "initialize");
    initTimer.start(SESSION_INIT_TIMEOUT_MS);
}

void PluginSession::receive(const QString& line)
{
    channel.receive(line);
}

void PluginSession::end()
{
    initTimer.stop();
    currentPhase = phaseEnded;
    // the marks go with the session (EDIT-PLUG-40)
    currentState.active.clear();
    currentState.fired.clear();
    marksTimer.stop();
    emit marksChanged();
    if (!open.isEmpty()) {
        open.clear();
        emit questionsChanged();
    }
    emit stateChanged();
}

void PluginSession::slotSend(const QByteArray& line, const QJsonObject& message)
{
    if (recording) {
        if (message.contains("method")) {
            // initialize carries the editor version: only its name is recorded
            QJsonObject params = message.value("params").toObject();
            if (message.value("method").toString() == "initialize") params = QJsonObject();
            // a path is recorded by its file name, the same on every machine
            if (params.contains("path")) params.insert("path", QFileInfo(params.value("path").toString()).fileName());
            record("sent", message.value("method").toString(), params);
        } else {
            QJsonObject reply = message.value("result").toObject();
            if (message.contains("error")) reply = message.value("error").toObject();
            record("sent", "reply", reply);
        }
    }
    runner->write(line);
}

void PluginSession::slotText(const QString& line)
{
    runner->textLine(line);
}

void PluginSession::slotInitTimeout()
{
    if (currentPhase != phaseStarting) return;
    runner->failSession(tr("the plugin did not answer initialize within %1 s").arg(SESSION_INIT_TIMEOUT_MS / 1000));
}

bool PluginSession::readModel(const QJsonObject& result, QString* error)
{
    Model m;
    m.kind = result.value("kind").toString();
    if (m.kind != "driven" && m.kind != "supervised" && m.kind != "observed") {
        *error = tr("the session model has no known kind");
        return false;
    }
    m.logic = result.value("logic").toString(m.kind == "driven" ? "async" : QString());
    if (result.contains("controls")) {
        m.controls = strings(result.value("controls"));
    } else if (m.kind == "driven") {
        m.controls << "step" << "run" << "pause" << "restart";
    } else if (m.kind == "supervised") {
        m.controls << "pause" << "run";
    }
    if (m.kind == "driven") {
        m.steps = strings(result.value("steps"));
        if (m.steps.isEmpty()) m.steps << "event";
        for (const QJsonValue& v: result.value("speeds").toArray()) {
            if (v.isDouble() && v.toDouble() >= 0) m.speeds.append(v.toDouble());
        }
        if (m.speeds.isEmpty()) m.speeds << 1;
        m.stream = result.value("stream").toBool();
        m.end = result.value("end").toString();
        m.verdict = result.value("verdict").toBool();
        m.output = result.value("output").toBool();
    } else {
        m.controls.removeAll("step");
        m.controls.removeAll("restart");
    }
    if (m.kind != "supervised") {
        m.events = strings(result.value("events"));
        m.save = result.value("save").toBool();
    }
    sessionModel = m;
    return true;
}

void PluginSession::slotReply(int id, const QJsonValue& result, const QJsonObject& error)
{
    QString method = pending.take(id);
    if (method.isEmpty()) return;
    if (method == "initialize") {
        initTimer.stop();
        QString problem;
        if (!error.isEmpty()) {
            problem = tr("the plugin refused initialize: %1").arg(error.value("message").toString());
        } else if (!readModel(result.toObject(), &problem)) {
            // problem is set
        } else {
            if (currentPhase == phaseStarting) currentPhase = phaseRunning;
            if (recording) record("model", QString(), result.toObject());
            emit modelReceived();
            emit stateChanged();
            return;
        }
        runner->failSession(problem);
        return;
    }
    if (recording) record("reply", method, error.isEmpty() ? result.toObject() : error);
    if (!error.isEmpty()) {
        // a bad stream names its lines (docs/PLUGIN-API.md, Sessions)
        QStringList problems;
        for (const QJsonValue& v: error.value("data").toArray()) {
            QJsonObject o = v.toObject();
            problems.append(tr("line %1: %2").arg(o.value("line").toInt()).arg(o.value("message").toString()));
        }
        QString message = error.value("message").toString();
        if (!problems.isEmpty()) message += ": " + problems.join("; ");
        emit requestDone(id, false, message);
        return;
    }
    if (method == "input") {
        // the items of the new stream wait as pending rows
        QList<HistoryRow> kept;
        for (const HistoryRow& r: rows) {
            if (!r.pending) kept.append(r);
        }
        rows = kept;
        for (const QJsonValue& v: result.toObject().value("items").toArray()) {
            QJsonObject item = v.toObject();
            HistoryRow r;
            r.event = item.value("event").toString();
            QStringList items;
            QJsonObject params = item.value("params").toObject();
            for (const QString& key: params.keys()) items.append(key + "=" + valueText(params.value(key)));
            r.parameters = items.join(' ');
            r.hasTime = item.value("time").isDouble();
            r.time = item.value("time").toDouble();
            r.origin = "input";
            r.pending = true;
            rows.append(r);
        }
        while (rows.size() > SESSION_HISTORY_LIMIT) rows.removeFirst();
        emit historyChanged();
    }
    emit requestDone(id, true, QString());
}

void PluginSession::slotRequest(const QJsonValue& id, const QString& method, const QJsonValue& value)
{
    QJsonObject params = value.toObject();
    if (recording) record("request", method, params);
    if (method == "ask") {
        Question q;
        q.id = id;
        q.text = params.value("text").toString();
        q.choices = strings(params.value("choices"));
        open.append(q);
        emit questionsChanged();
        return;
    }
    channel.replyError(id, JSONRPC_METHOD_NOT_FOUND, QString("unknown method %1").arg(method));
}

void PluginSession::slotNotification(const QString& method, const QJsonValue& value)
{
    QJsonObject params = value.toObject();
    if (method == "state") {
        if (recording) record("state", QString(), params);
        readState(params);
        return;
    }
    if (method == "history") {
        if (recording) record("history", QString(), params);
        HistoryRow r;
        r.event = params.value("event").toString();
        QStringList items;
        QJsonObject values = params.value("params").toObject();
        for (const QString& key: values.keys()) items.append(key + "=" + valueText(values.value(key)));
        r.parameters = items.join(' ');
        r.hasTime = params.value("time").isDouble();
        r.time = params.value("time").toDouble();
        r.origin = params.value("origin").toString();
        r.outcome = params.value("outcome").toString();
        r.fired = strings(params.value("fired"));
        appendRow(r);
        return;
    }
    if (method == "log") {
        if (recording) record("log", QString(), params);
        QString level = params.value("level").toString();
        emit logLine(params.value("text").toString(), level == "warning" || level == "error");
        return;
    }
    if (method == "diagnostics") {
        // the level-1 array of the diagnostics file (EDIT-PLUG-49)
        QJsonArray array = value.toArray();
        QList<PluginDiagnostic> list;
        QString problem;
        if (!PluginRunner::readDiagnostics(QJsonDocument(array).toJson(), &list, &problem)) {
            emit logLine(tr("the session diagnostics are malformed: %1").arg(problem), true);
            return;
        }
        if (recording) lines.append(QString("diagnostics %1").arg(list.size()));
        findings = list;
        findingsReceived = true;
        emit diagnosticsChanged();
        return;
    }
    // an unknown notification is ignored (EDIT-PLUG-38)
}

void PluginSession::readState(const QJsonObject& params)
{
    bool marks = false;
    if (params.contains("status")) currentState.status = params.value("status").toString();
    if (params.contains("text")) currentState.text = params.value("text").toString();
    if (params.contains("active")) {
        currentState.active = strings(params.value("active"));
        marks = true;
    }
    if (params.contains("fired")) {
        currentState.fired = strings(params.value("fired"));
        marks = true;
    }
    if (params.contains("events")) currentState.events = strings(params.value("events"));
    if (params.contains("queue")) currentState.queue = strings(params.value("queue"));
    if (params.contains("output")) currentState.output = strings(params.value("output"));
    if (params.contains("time")) {
        currentState.hasTime = params.value("time").isDouble();
        currentState.time = params.value("time").toDouble();
    }
    if (params.contains("position")) {
        currentState.position = params.value("position").toInt(-1);
        // the stream items before the position are handled
        int index = 0;
        for (HistoryRow& r: rows) {
            if (!r.pending) continue;
            if (index++ < currentState.position) r.pending = false;
        }
    }
    if (params.contains("verdict")) currentState.verdict = params.value("verdict").toString();
    states++;
    if (marks) scheduleMarks();
    emit stateChanged();
}

void PluginSession::appendRow(const HistoryRow& row)
{
    // a handled stream item takes the place of its pending row
    int at = rows.size();
    if (row.origin == "input") {
        for (int i = 0; i < rows.size(); i++) {
            if (rows.at(i).pending) {
                at = i;
                break;
            }
        }
    }
    if (at < rows.size()) rows[at] = row;
    else rows.append(row);
    while (rows.size() > SESSION_HISTORY_LIMIT) rows.removeFirst();
    emit historyChanged();
}

// a fast run shows the latest marks, never a backlog (EDIT-PLUG-40)
void PluginSession::scheduleMarks()
{
    if (marksTimer.isActive()) return;
    qint64 since = marksClock.isValid() ? marksClock.elapsed() : RUN_MARK_INTERVAL_MS;
    if (since >= RUN_MARK_INTERVAL_MS) {
        slotMarks();
    } else {
        marksTimer.start(static_cast<int>(RUN_MARK_INTERVAL_MS - since));
    }
}

void PluginSession::slotMarks()
{
    marksClock.start();
    emit marksChanged();
}

void PluginSession::record(const QString& prefix, const QString& method, const QJsonObject& params)
{
    QString line = prefix;
    if (!method.isEmpty()) line += " " + method;
    QString fields = fieldsText(params);
    if (!fields.isEmpty()) line += " " + fields;
    lines.append(line);
}

QString PluginSession::lastUnit() const
{
    if (!unit.isEmpty()) return unit;
    return sessionModel.steps.value(0);
}

double PluginSession::lastSpeed() const
{
    if (speed >= 0) return speed;
    return sessionModel.speeds.isEmpty() ? 1.0 : sessionModel.speeds.first();
}

bool PluginSession::statusIs(const QStringList& statuses) const
{
    return statuses.contains(currentState.status);
}

bool PluginSession::canStep() const
{
    return running() && sessionModel.controls.contains("step") &&
           statusIs(QStringList() << STATUS_PAUSED << STATUS_WAITING);
}

bool PluginSession::canRun() const
{
    return running() && sessionModel.controls.contains("run") &&
           statusIs(QStringList() << STATUS_PAUSED << STATUS_WAITING);
}

bool PluginSession::canPause() const
{
    return running() && sessionModel.controls.contains("pause") && statusIs(QStringList() << STATUS_RUNNING);
}

bool PluginSession::canRestart() const
{
    return running() && sessionModel.controls.contains("restart");
}

bool PluginSession::canPost() const
{
    return running() && (!sessionModel.events.isEmpty() || !sessionModel.end.isEmpty());
}

bool PluginSession::canStream() const
{
    return running() && sessionModel.stream;
}

bool PluginSession::canSave() const
{
    return running() && sessionModel.save;
}

void PluginSession::step(const QString& stepUnit)
{
    if (!running()) return;
    if (!stepUnit.isEmpty()) unit = stepUnit;
    QJsonObject params;
    params.insert("command", "step");
    params.insert("unit", lastUnit());
    channel.notify("control", params);
}

void PluginSession::run(double runSpeed)
{
    if (!running()) return;
    speed = runSpeed;
    QJsonObject params;
    params.insert("command", "run");
    params.insert("speed", runSpeed);
    channel.notify("control", params);
}

void PluginSession::pause()
{
    if (!running()) return;
    QJsonObject params;
    params.insert("command", "pause");
    channel.notify("control", params);
}

void PluginSession::restart()
{
    if (!running()) return;
    QJsonObject params;
    params.insert("command", "restart");
    channel.notify("control", params);
}

bool PluginSession::post(const QString& line, QString* error)
{
    QString simple = line.simplified();
    if (!running() || simple.isEmpty()) {
        if (error) *error = tr("no event to post");
        return false;
    }
    QStringList words = simple.split(' ');
    QJsonObject values;
    for (int i = 1; i < words.size(); i++) {
        int eq = words.at(i).indexOf('=');
        if (eq <= 0) {
            if (error) *error = tr("'%1' is not name=value").arg(words.at(i));
            return false;
        }
        values.insert(words.at(i).left(eq), words.at(i).mid(eq + 1));
    }
    QJsonObject params;
    params.insert("name", words.first());
    params.insert("params", values);
    channel.notify("event", params);
    return true;
}

int PluginSession::loadStream(const QString& path)
{
    if (!running()) return 0;
    QJsonObject params;
    params.insert("path", path);
    int id = channel.request("input", params);
    pending.insert(id, "input");
    return id;
}

int PluginSession::editStream(const QString& text)
{
    if (!running()) return 0;
    lastStreamText = text;
    QJsonObject params;
    params.insert("text", text);
    int id = channel.request("input", params);
    pending.insert(id, "input");
    return id;
}

int PluginSession::saveLog(const QString& path)
{
    if (!running()) return 0;
    QJsonObject params;
    params.insert("path", path);
    int id = channel.request("save", params);
    pending.insert(id, "save");
    return id;
}

bool PluginSession::answer(const QString& text)
{
    if (open.isEmpty()) return false;
    Question q = open.takeFirst();
    QJsonObject result;
    result.insert("action", "accept");
    result.insert("text", text);
    channel.reply(q.id, result);
    emit questionsChanged();
    return true;
}

bool PluginSession::decline()
{
    if (open.isEmpty()) return false;
    Question q = open.takeFirst();
    QJsonObject result;
    result.insert("action", "decline");
    channel.reply(q.id, result);
    emit questionsChanged();
    return true;
}

void PluginSession::stop()
{
    if (currentPhase == phaseStopping || currentPhase == phaseEnded) return;
    initTimer.stop();
    currentPhase = phaseStopping;
    // every open question is cancelled first (EDIT-PLUG-43)
    QJsonObject cancel;
    cancel.insert("action", ASK_CANCEL);
    for (const Question& q: open) channel.reply(q.id, cancel);
    open.clear();
    emit questionsChanged();
    pending.insert(channel.request("shutdown"), "shutdown");
    runner->endInput();
    emit stateChanged();
}
