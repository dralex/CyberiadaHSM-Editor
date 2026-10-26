/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Batch Session: a plugin session driven by a script in batch mode
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
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QTextStream>
#include <functional>

#include "batch_session.h"
#include "plugin_session.h"
#include "smeditor_window.h"

#define MS_IN_SECOND   1000
// the event loop turns this often while a verb waits
#define WAIT_SLICE_MS  50

namespace {

// the event loop runs until the condition holds or the time is up
bool waitFor(const std::function<bool()>& done, double seconds)
{
    QElapsedTimer clock;
    clock.start();
    while (!done()) {
        if (clock.elapsed() > seconds * MS_IN_SECOND) return false;
        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, WAIT_SLICE_MS);
    }
    return true;
}

QString sorted(QStringList ids)
{
    ids.sort();
    return ids.join(',');
}

// what the script sees of a session, kept after the session object is gone
struct Watch {
    QPointer<PluginSession> session;
    PluginSession::State state;
    bool ended = false;
    PluginResult result;
    QStringList dump;
};

} // namespace

bool runSessionStage(CyberiadaSMEditorWindow& win, const QString& pluginId, const QString& actionId,
                     const PluginValues& values, const QString& script, PluginResult* result,
                     QStringList* dump, QString* error)
{
    QFile file(script);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *error = QString("cannot read the session script %1").arg(script);
        return false;
    }
    QStringList lines = QTextStream(&file).readAll().split('\n');

    Watch watch;
    QString startError;
    PluginRunner* run = win.runPlugin(pluginId, actionId, values, &startError);
    if (!run) {
        result->state = PluginResult::stateFailed;
        result->error = startError;
        *error = startError;
        return false;
    }
    watch.session = run->session();
    watch.session->setRecording(true);
    QMetaObject::Connection states = QObject::connect(watch.session, &PluginSession::stateChanged, [&watch]() {
        if (watch.session) watch.state = watch.session->state();
    });
    QMetaObject::Connection finished = QObject::connect(&win, &CyberiadaSMEditorWindow::pluginRunFinished,
        [&watch, run](PluginRunner* done) {
            if (done != run) return;
            watch.result = done->result();
            if (done->session()) watch.dump = done->session()->dumpLines();
            watch.ended = true;
        });
    // a staging failure ends the run inside runPlugin
    if (run->isFinished() && !watch.ended) {
        watch.result = run->result();
        watch.ended = true;
    }
    auto live = [&watch]() { return !watch.ended && watch.session; };

    bool ok = true;
    // the model, then the first state (docs/PLUGIN-API.md, Sessions)
    waitFor([&]() { return !live() || (watch.session->phase() != PluginSession::phaseStarting &&
                                        watch.session->stateCount() > 0); },
            SESSION_INIT_TIMEOUT_MS / MS_IN_SECOND + SESSION_WAIT_S);
    for (int n = 0; ok && n < lines.size(); n++) {
        QString line = lines.at(n).trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        QStringList words = line.simplified().split(' ');
        QString verb = words.takeFirst();
        QString where = QString("line %1: %2").arg(n + 1).arg(verb);
        auto fail = [&](const QString& message) {
            *error = QString("%1: %2").arg(where, message);
            ok = false;
        };
        bool needsSession = verb != "wait" && !verb.startsWith("expect-");
        if (needsSession && !live()) {
            fail("the session has ended");
            break;
        }
        if (verb == "step" || verb == "restart") {
            int before = watch.session->stateCount();
            if (verb == "step") watch.session->step(words.value(0));
            else watch.session->restart();
            // the step is done with a state that is not running (EDIT-PLUG-47)
            if (!waitFor([&]() { return !live() || (watch.session->stateCount() > before &&
                                                     watch.session->state().status != "running"); },
                         SESSION_WAIT_S)) {
                fail("no state came");
            }
        } else if (verb == "run") {
            watch.session->run(words.isEmpty() ? watch.session->lastSpeed() : words.first().toDouble());
        } else if (verb == "pause") {
            watch.session->pause();
        } else if (verb == "event") {
            QString message;
            if (!watch.session->post(words.join(' '), &message)) fail(message);
        } else if (verb == "input" || verb == "save") {
            if (words.size() != 1) {
                fail("one file is expected");
                continue;
            }
            QString path = QFileInfo(words.first()).absoluteFilePath();
            int id = verb == "input" ? watch.session->loadStream(path) : watch.session->saveLog(path);
            bool done = false;
            bool accepted = false;
            QString message;
            QMetaObject::Connection c = QObject::connect(watch.session, &PluginSession::requestDone,
                [&](int replied, bool good, const QString& text) {
                    if (replied != id) return;
                    done = true;
                    accepted = good;
                    message = text;
                });
            if (!waitFor([&]() { return done || !live(); }, SESSION_WAIT_S)) fail("no reply came");
            else if (!done) fail("the session has ended");
            else if (!accepted) fail(message);
            QObject::disconnect(c);
        } else if (verb == "wait") {
            if (words.isEmpty()) {
                fail("a status is expected");
                continue;
            }
            QString status = words.first();
            double seconds = words.size() > 1 ? words.at(1).toDouble() : SESSION_WAIT_S;
            if (status == "ended") {
                // the plugin ends the session by itself
                if (!waitFor([&]() { return watch.ended; }, seconds)) fail("the session goes on");
            } else if (!waitFor([&]() { return watch.state.status == status || watch.ended; }, seconds) ||
                       watch.state.status != status) {
                fail(QString("the status is %1").arg(watch.state.status));
            }
        } else if (verb == "answer" || verb == "decline") {
            // a question may still be on its way
            if (!waitFor([&]() { return !live() || !watch.session->questions().isEmpty(); }, SESSION_WAIT_S) ||
                !live() || watch.session->questions().isEmpty()) {
                fail("no question is open");
            } else if (verb == "answer") {
                watch.session->answer(words.join(' '));
            } else {
                watch.session->decline();
            }
        } else if (verb == "expect-active" || verb == "expect-fired") {
            const QStringList& marks = verb == "expect-active" ? watch.state.active : watch.state.fired;
            if (sorted(marks) != sorted(words)) {
                fail(QString("%1 instead of %2").arg(sorted(marks), sorted(words)));
            }
        } else if (verb == "expect-status") {
            if (watch.state.status != words.value(0)) fail(QString("the status is %1").arg(watch.state.status));
        } else if (verb == "expect-verdict") {
            if (watch.state.verdict != words.value(0)) fail(QString("the verdict is %1").arg(watch.state.verdict));
        } else if (verb == "stop") {
            watch.session->stop();
            waitFor([&]() { return watch.ended; }, SESSION_WAIT_S);
        } else {
            fail("unknown verb");
        }
    }
    // the end of the script stops the session (EDIT-PLUG-47)
    if (live()) watch.session->stop();
    // the plugin has its grace after shutdown, then the terminate and the kill
    waitFor([&]() { return watch.ended; }, SESSION_WAIT_S + 2 * PLUGIN_KILL_GRACE_MS / MS_IN_SECOND);
    QObject::disconnect(finished);
    QObject::disconnect(states);
    *result = watch.result;
    *dump = watch.dump;
    if (!watch.ended) {
        result->state = PluginResult::stateFailed;
        result->error = "the session did not end";
    }
    return ok;
}
