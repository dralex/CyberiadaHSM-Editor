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

#ifndef PLUGIN_SESSION_H
#define PLUGIN_SESSION_H

#include <QElapsedTimer>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QObject>
#include <QStringList>
#include <QTimer>

#include "plugin_channel.h"
#include "plugin_runner.h"

// the session of one run of a session action (docs/PLUGINS.md, Level 2): the
// model the plugin answers initialize with, the latest state, the history, the
// open questions and the findings; the runner does the process and the lines
class PluginSession : public QObject {
    Q_OBJECT

public:
    enum Phase {
        phaseStarting = 0,
        phaseRunning,
        phaseStopping,
        phaseEnded
    };

    // the initialize result (docs/PLUGIN-API.md, Sessions, The model)
    struct Model {
        QString     kind;
        QString     logic;
        QStringList controls;
        QStringList steps;
        QList<double> speeds;
        QStringList events;
        QString     end;
        bool        stream = false;
        bool        save = false;
        bool        verdict = false;
        bool        output = false;
    };

    // the fields a state message replaces; the ones left out are kept
    struct State {
        QString     status;
        QString     text;
        QStringList active;
        QStringList fired;
        QStringList events;
        QStringList queue;
        QStringList output;
        bool        hasTime = false;
        double      time = 0.0;
        int         position = -1;
        QString     verdict;
    };

    struct HistoryRow {
        QString     event;
        // name=value, space separated
        QString     parameters;
        bool        hasTime = false;
        double      time = 0.0;
        QString     origin;
        QString     outcome;
        QStringList fired;
        // an item of a loaded stream not yet handled
        bool        pending = false;
    };

    struct Question {
        QJsonValue  id;
        QString     text;
        QStringList choices;
    };

    explicit PluginSession(PluginRunner* runner);

    Phase       phase() const { return currentPhase; }
    const Model& model() const { return sessionModel; }
    const State& state() const { return currentState; }
    // the state messages received so far: a batch step waits for the next one
    int         stateCount() const { return states; }
    const QList<HistoryRow>& history() const { return rows; }
    const QList<Question>& questions() const { return open; }
    const QList<PluginDiagnostic>& diagnostics() const { return findings; }
    bool        hasDiagnostics() const { return findingsReceived; }
    QString     lastUnit() const;
    // the speed of the last Run, else the first one offered
    double      lastSpeed() const;
    QString     streamText() const { return lastStreamText; }

    // the controls the model offers and the status allows now (EDIT-PLUG-39)
    bool        canStep() const;
    bool        canRun() const;
    bool        canPause() const;
    bool        canRestart() const;
    bool        canPost() const;
    bool        canStream() const;
    bool        canSave() const;

    // the commands of the user; an empty unit is the last one chosen
    void        step(const QString& unit = QString());
    void        run(double runSpeed);
    void        pause();
    void        restart();
    // NAME name=value ...: false when the line is not such
    bool        post(const QString& line, QString* error = nullptr);
    // the request id; the reply arrives through requestDone()
    int         loadStream(const QString& path);
    int         editStream(const QString& text);
    int         saveLog(const QString& path);
    // the oldest open question; false when there is none
    bool        answer(const QString& text);
    bool        decline();
    // cancel the questions, shutdown, close stdin; the runner ends the process
    void        stop();

    // every message both ways as a dump line, for --dump-session (EDIT-PLUG-48)
    void        setRecording(bool on) { recording = on; }
    const QStringList& dumpLines() const { return lines; }

    // the process has started: initialize goes out
    void        begin();
    // a stdout line of the process
    void        receive(const QString& line);
    // the process has ended
    void        end();

signals:
    void        modelReceived();
    void        stateChanged();
    // the latest active and fired ids, at most every RUN_MARK_INTERVAL_MS
    void        marksChanged();
    void        historyChanged();
    void        questionsChanged();
    void        diagnosticsChanged();
    void        logLine(const QString& text, bool isError);
    void        requestDone(int id, bool ok, const QString& message);

private slots:
    void        slotRequest(const QJsonValue& id, const QString& method, const QJsonValue& value);
    void        slotNotification(const QString& method, const QJsonValue& value);
    void        slotReply(int id, const QJsonValue& result, const QJsonObject& error);
    void        slotSend(const QByteArray& line, const QJsonObject& message);
    void        slotText(const QString& line);
    void        slotInitTimeout();
    void        slotMarks();

private:
    bool        readModel(const QJsonObject& result, QString* error);
    void        readState(const QJsonObject& params);
    void        appendRow(const HistoryRow& row);
    void        scheduleMarks();
    void        record(const QString& prefix, const QString& method, const QJsonObject& params);
    bool        running() const { return currentPhase == phaseRunning; }
    bool        statusIs(const QStringList& statuses) const;

    PluginRunner* runner;
    PluginChannel channel;
    Phase       currentPhase = phaseStarting;
    Model       sessionModel;
    State       currentState;
    int         states = 0;
    QList<HistoryRow> rows;
    QList<Question> open;
    QList<PluginDiagnostic> findings;
    bool        findingsReceived = false;
    QString     unit;
    double      speed = -1.0;
    QString     lastStreamText;
    int         initializeId = 0;
    // the requests waiting for a reply, by id: the method
    QMap<int, QString> pending;
    QTimer      initTimer;
    QTimer      marksTimer;
    QElapsedTimer marksClock;
    bool        recording = false;
    QStringList lines;
};

#endif
