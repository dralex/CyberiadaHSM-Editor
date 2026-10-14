/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Plugin Runner: one run of a plugin action (docs/PLUGINS.md, The run)
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

#ifndef PLUGIN_RUNNER_H
#define PLUGIN_RUNNER_H

#include <QDateTime>
#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QPair>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <string>

#include "plugin_registry.h"
#include "plugin_values.h"

class QTemporaryDir;
class QTextDecoder;

// what the window hands to a run: the document as it is now
struct PluginRunContext {
    std::string input;
    // empty for an untitled document
    QString documentPath;
    // the saved format: cyberiada, yed-ostranna or yed-berloga
    QString documentFormat;
    bool modified = false;
    QList<QPair<QString, QString> > meta;
    QStringList selection;
};

struct PluginDiagnostic {
    // error, warning or info
    QString severity;
    QString message;
    QString rule;
    QString element;
    int line = 0;
    // false when the document has no such element (set by the window)
    bool linked = true;
};

struct PluginFileChange {
    // relative to the outdir
    QString path;
    bool isNew = true;
};

struct PluginResult {
    enum State {
        stateDone = 0,
        stateFailed,
        stateCancelled
    };

    State state = stateFailed;
    bool started = false;
    // -1 when the plugin never exited normally
    int exitCode = -1;
    QString error;
    QString stdoutText;
    QStringList stderrTail;
    // replace, new-document: the output re-encoded as Cyberiada 1.0
    std::string document;
    QList<PluginDiagnostic> diagnostics;
    QList<PluginFileChange> files;
    // the directories a dump prints by name, never as paths (EDIT-PLUG-28)
    QString runDir;
    QString pluginDir;
    QString documentDir;
    // set by the window when the result is applied
    bool changed = false;
    int tab = -1;
};

// the argv after the variables: the arguments and the files to write
struct PluginExpansion {
    QStringList arguments;
    // run dir file name -> value, for ${param-file:x}
    QMap<QString, QString> parameterFiles;
    bool usesRequest = false;
};

class PluginRunner: public QObject {
    Q_OBJECT
public:
    PluginRunner(const PluginEntry& plugin, const QString& actionId, const PluginValues& values,
                 const PluginRunContext& context, QObject* parent = nullptr);
    ~PluginRunner();

    const PluginEntry&   plugin() const { return entry; }
    const PluginAction&  action() const;
    const PluginValues&  values() const { return runValues; }
    const PluginResult&  result() const { return res; }
    // the window completes the result when it applies it
    PluginResult*        mutableResult() { return &res; }
    bool                 isFinished() const { return finishedFlag; }
    qint64               elapsedMs() const;

    // stages the run dir and starts the process; a staging failure finishes at once
    void                 start();
    // terminate, then kill after the grace period (EDIT-PLUG-20)
    void                 cancel();
    // kill at once and wait: the document or the editor is closing
    void                 abort();

    // the variables of the argv (EDIT-PLUG-8): the fixed ones by name, plus
    // param:x, param-file:x and meta:k; false with the reason
    static bool          expandArguments(const PluginDescriptor& plugin, const PluginAction& action,
                                         const PluginValues& values,
                                         const QMap<QString, QString>& variables,
                                         const QList<QPair<QString, QString> >& meta,
                                         PluginExpansion* out, QString* error);
    // the diagnostics file (docs/PLUGIN-API.md); false when it is malformed
    static bool          readDiagnostics(const QByteArray& json, QList<PluginDiagnostic>* out,
                                         QString* error);

signals:
    void                 output(const QString& text, bool isError);
    void                 finished();

private slots:
    void                 slotStdout();
    void                 slotStderr();
    void                 slotFinished(int exitCode, QProcess::ExitStatus status);
    void                 slotError(QProcess::ProcessError error);
    void                 slotTimeout();
    void                 slotKill();

private:
    bool                 stage(QString* error);
    bool                 resolveProgram(const QStringList& arguments, QString* program,
                                        QStringList* programArguments, QString* error) const;
    bool                 prepareEnvironment(QProcessEnvironment* env, QString* error) const;
    bool                 prepareOutdir(QString* error);
    void                 writeRequest(const QString& path) const;
    QString              decode(const QByteArray& bytes, QTextDecoder** decoder);
    void                 validate();
    void                 finish(PluginResult::State state, const QString& error = QString());
    QMap<QString, QPair<qint64, qint64> > listOutdir() const;

    PluginEntry          entry;
    QString              actionId;
    PluginValues         runValues;
    PluginRunContext     context;
    PluginResult         res;

    QTemporaryDir*       runDir = nullptr;
    QString              runPath;
    QString              outdir;
    QString              workDir;
    QMap<QString, QPair<qint64, qint64> > outdirBefore;
    QProcess*            process = nullptr;
    QTimer               timeoutTimer;
    QTimer               killTimer;
    QElapsedTimer        clock;
    QTextDecoder*        stdoutDecoder = nullptr;
    QTextDecoder*        stderrDecoder = nullptr;
    QString              stderrText;
    qint64               outputBytes = 0;
    bool                 capped = false;
    bool                 timedOut = false;
    bool                 cancelled = false;
    bool                 finishedFlag = false;
};

#endif
