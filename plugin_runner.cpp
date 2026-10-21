/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
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

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextCodec>
#include <QTextDecoder>
#include <cyberiadamlpp.h>

#include "plugin_runner.h"
#include "version.h"

#define INPUT_FILE        "input.graphml"
#define OUTPUT_FILE       "output.graphml"
#define DIAGNOSTICS_FILE  "diagnostics.json"
#define REQUEST_FILE      "request.json"

namespace {

bool fail(QString* error, const QString& message)
{
    if (error) *error = message;
    return false;
}

QString paramFileName(const QString& id)
{
    return QString("param-%1.txt").arg(id);
}

bool writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return f.write(data) == data.size();
}

} // namespace

PluginRunner::PluginRunner(const PluginEntry& plugin, const QString& actionId,
                           const PluginValues& values, const PluginRunContext& context,
                           QObject* parent):
    QObject(parent), entry(plugin), actionId(actionId), runValues(values), context(context)
{
    timeoutTimer.setSingleShot(true);
    killTimer.setSingleShot(true);
    connect(&timeoutTimer, &QTimer::timeout, this, &PluginRunner::slotTimeout);
    connect(&killTimer, &QTimer::timeout, this, &PluginRunner::slotKill);
    res.pluginDir = entry.folder;
    res.documentDir = pluginDocumentPaths(context.documentPath).dir;
}

PluginRunner::~PluginRunner()
{
    // a run destroyed while going reports to nobody
    blockSignals(true);
    abort();
    delete stdoutDecoder;
    delete stderrDecoder;
    delete runDir;
}

const PluginAction& PluginRunner::action() const
{
    static const PluginAction none;
    const PluginAction* a = entry.descriptor.action(actionId);
    return a ? *a : none;
}

qint64 PluginRunner::elapsedMs() const
{
    return clock.isValid() ? clock.elapsed() : 0;
}

bool PluginRunner::expandArguments(const PluginDescriptor& plugin, const PluginAction& action,
                                   const PluginValues& values,
                                   const QMap<QString, QString>& variables,
                                   const QList<QPair<QString, QString> >& meta,
                                   PluginExpansion* out, QString* error)
{
    // one pass: a substituted value is never expanded again
    auto expand = [&](const QString& text, QString* result) -> bool {
        result->clear();
        int i = 0;
        while (i < text.size()) {
            if (text.at(i) != '$' || i + 1 >= text.size() || text.at(i + 1) != '{') {
                result->append(text.at(i++));
                continue;
            }
            int close = text.indexOf('}', i + 2);
            if (close < 0) return fail(error, PluginRunner::tr("the variable at '%1' is not closed").arg(text.mid(i)));
            QString name = text.mid(i + 2, close - i - 2);
            i = close + 1;
            if (name.startsWith("param:") || name.startsWith("param-file:")) {
                bool file = name.startsWith("param-file:");
                QString id = name.mid(name.indexOf(':') + 1);
                const PluginParameter* p = plugin.parameter(id);
                if (!p || !values.contains(id)) {
                    return fail(error, PluginRunner::tr("the variable ${%1} is unknown").arg(name));
                }
                if (p->type == PluginParameter::typeSecret) {
                    return fail(error, PluginRunner::tr("the secret '%1' cannot be in the argv").arg(id));
                }
                if (file) {
                    out->parameterFiles.insert(paramFileName(id), values.value(id));
                    result->append(QDir(variables.value("rundir")).filePath(paramFileName(id)));
                } else {
                    result->append(values.value(id));
                }
            } else if (name.startsWith("meta:")) {
                QString key = name.mid(5);
                for (const QPair<QString, QString>& m: meta) {
                    if (m.first == key) {
                        result->append(m.second);
                        break;
                    }
                }
            } else if (variables.contains(name)) {
                if (name == "request") out->usesRequest = true;
                result->append(variables.value(name));
            } else {
                return fail(error, PluginRunner::tr("the variable ${%1} is unknown").arg(name));
            }
        }
        return true;
    };

    out->arguments.clear();
    for (const PluginArgument& arg: action.argv) {
        QStringList texts;
        if (arg.isConditional()) {
            const PluginParameter* p = plugin.parameter(arg.condition);
            if (!p || !pluginConditionHolds(*p, values.value(arg.condition))) continue;
            texts = arg.arguments;
        } else {
            texts.append(arg.text);
        }
        for (const QString& t: texts) {
            QString expanded;
            if (!expand(t, &expanded)) return false;
            out->arguments.append(expanded);
        }
    }
    if (out->arguments.isEmpty()) return fail(error, PluginRunner::tr("the argv is empty"));
    return true;
}

bool PluginRunner::readDiagnostics(const QByteArray& json, QList<PluginDiagnostic>* out,
                                   QString* error)
{
    out->clear();
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isArray()) {
        return fail(error, PluginRunner::tr("the diagnostics file is not a JSON array"));
    }
    static const char* const severities[] = { "error", "warning", "info", "info" };
    for (const QJsonValue& v: doc.array()) {
        if (!v.isObject()) return fail(error, PluginRunner::tr("a finding is not an object"));
        QJsonObject o = v.toObject();
        PluginDiagnostic d;
        QJsonValue sev = o.value("severity");
        if (sev.isString() && (sev.toString() == "error" || sev.toString() == "warning" ||
                               sev.toString() == "info")) {
            d.severity = sev.toString();
        } else if (sev.isDouble() && sev.toInt() >= 1 && sev.toInt() <= 4 &&
                   sev.toDouble() == sev.toInt()) {
            // the LSP severity numbers
            d.severity = severities[sev.toInt() - 1];
        } else {
            return fail(error, PluginRunner::tr("a finding has no valid severity"));
        }
        if (!o.value("message").isString()) return fail(error, PluginRunner::tr("a finding has no message"));
        d.message = o.value("message").toString().simplified();
        if (o.contains("rule")) {
            if (!o.value("rule").isString()) return fail(error, PluginRunner::tr("the rule of a finding is not a string"));
            d.rule = o.value("rule").toString();
        }
        if (o.contains("element")) {
            if (!o.value("element").isString()) return fail(error, PluginRunner::tr("the element of a finding is not a string"));
            d.element = o.value("element").toString();
        }
        if (o.contains("line")) {
            QJsonValue line = o.value("line");
            if (!line.isDouble() || line.toDouble() != line.toInt() || line.toInt() < 0) {
                return fail(error, PluginRunner::tr("the line of a finding is not a number"));
            }
            d.line = line.toInt();
        }
        out->append(d);
    }
    return true;
}

bool PluginRunner::resolveProgram(const QStringList& arguments, QString* program,
                                  QStringList* programArguments, QString* error) const
{
    if (!entry.program.isEmpty()) {
        *program = entry.program;
        *programArguments = entry.programArguments + arguments;
        return true;
    }
    QString first = arguments.first();
    QStringList candidates;
    QFileInfo info(first);
    if (info.isAbsolute()) {
        candidates << first;
    } else {
        candidates << QDir(entry.folder).absoluteFilePath(first);
    }
#ifdef Q_OS_WIN
    if (QFileInfo(first).suffix().isEmpty()) {
        QStringList withExe;
        for (const QString& c: candidates) withExe << c + ".exe";
        candidates += withExe;
    }
#endif
    for (const QString& c: candidates) {
        QFileInfo ci(c);
        if (ci.isFile() && ci.isExecutable()) {
            *program = ci.absoluteFilePath();
            *programArguments = arguments.mid(1);
            return true;
        }
    }
    // a bare name is looked up in PATH
    if (!first.contains('/') && !first.contains('\\')) {
        QString found = QStandardPaths::findExecutable(first);
        if (!found.isEmpty()) {
            *program = found;
            *programArguments = arguments.mid(1);
            return true;
        }
    }
    // the file name only: the message must not depend on the machine (EDIT-PLUG-28)
    return fail(error, PluginRunner::tr("the program '%1' is not found").arg(QFileInfo(first).fileName()));
}

bool PluginRunner::prepareEnvironment(QProcessEnvironment* env, QString* error) const
{
    *env = QProcessEnvironment::systemEnvironment();
    for (const QPair<QString, QString>& e: entry.descriptor.env) {
        QString value = e.second;
        value.replace("${plugin_dir}", entry.folder);
        env->insert(e.first, value);
    }
    // the plugin's own messages may follow the interface language (EDIT-PLUG-33)
    env->insert(PLUGIN_LANGUAGE_VARIABLE, PluginRegistry::instance().language());
    // a secret reaches the child through its export variable only (EDIT-PLUG-10)
    for (const PluginParameter* p: entry.descriptor.actionParameters(action())) {
        if (p->type != PluginParameter::typeSecret) continue;
        QString ref = runValues.value(p->id);
        if (ref.isEmpty()) continue;
        QString secret;
        if (ref.startsWith("env:")) {
            QString name = ref.mid(4);
            if (!qEnvironmentVariableIsSet(name.toLocal8Bit().constData())) {
                return fail(error, PluginRunner::tr("the secret '%1' is not set: the variable %2 is absent")
                            .arg(p->id, name));
            }
            secret = qEnvironmentVariable(name.toLocal8Bit().constData());
        } else {
            QFile f(ref.mid(5));
            if (!f.open(QIODevice::ReadOnly)) {
                return fail(error, PluginRunner::tr("the secret '%1' is not set: its file cannot be read").arg(p->id));
            }
            secret = QString::fromUtf8(f.readLine()).trimmed();
        }
        env->insert(p->exportName, secret);
    }
    return true;
}

bool PluginRunner::prepareOutdir(QString* error)
{
    const PluginAction& a = action();
    if (a.outdir.isEmpty()) {
        outdir = runPath;
        return true;
    }
    const PluginParameter* p = entry.descriptor.parameter(a.outdir);
    QString value = runValues.value(a.outdir);
    if (value.isEmpty()) return fail(error, PluginRunner::tr("the outdir parameter '%1' has no value").arg(a.outdir));
    QString dir = p && p->type == PluginParameter::typeFile ? QFileInfo(value).absolutePath()
                                                            : QDir(value).absolutePath();
    if (!QDir(dir).exists()) {
        if (!(p && p->create) || !QDir().mkpath(dir)) {
            return fail(error, PluginRunner::tr("the output directory of '%1' does not exist").arg(a.outdir));
        }
    }
    outdir = QDir(dir).canonicalPath();
    return true;
}

QMap<QString, QPair<qint64, qint64> > PluginRunner::listOutdir() const
{
    QMap<QString, QPair<qint64, qint64> > files;
    if (action().result != PluginAction::resultFiles) return files;
    QDir base(outdir);
    QDirIterator it(outdir, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QFileInfo info = it.fileInfo();
        files.insert(base.relativeFilePath(info.absoluteFilePath()),
                     qMakePair(info.size(), info.lastModified().toMSecsSinceEpoch()));
    }
    return files;
}

void PluginRunner::writeRequest(const QString& path) const
{
    QJsonObject document;
    document.insert("path", context.documentPath);
    document.insert("format", context.documentFormat);
    document.insert("modified", context.modified);
    QJsonObject meta;
    for (const QPair<QString, QString>& m: context.meta) meta.insert(m.first, m.second);
    QJsonArray selection;
    for (const QString& id: context.selection) selection.append(id);
    QJsonObject params;
    for (const PluginParameter* p: entry.descriptor.actionParameters(action())) {
        if (p->type == PluginParameter::typeSecret) continue;
        params.insert(p->id, runValues.value(p->id));
    }
    QJsonObject outputs;
    outputs.insert("output", QDir(runPath).filePath(OUTPUT_FILE));
    outputs.insert("diagnostics", QDir(runPath).filePath(DIAGNOSTICS_FILE));
    outputs.insert("outdir", outdir);
    QJsonObject request;
    request.insert("api", PLUGIN_API_MAJOR);
    request.insert("plugin", entry.descriptor.id);
    request.insert("action", actionId);
    request.insert("editor_version", CYBERIADA_VERSION);
    request.insert("language", PluginRegistry::instance().language());
    request.insert("input", QDir(runPath).filePath(INPUT_FILE));
    request.insert("document", document);
    request.insert("meta", meta);
    request.insert("selection", selection);
    request.insert("params", params);
    request.insert("outputs", outputs);
    writeFile(path, QJsonDocument(request).toJson(QJsonDocument::Indented));
}

bool PluginRunner::stage(QString* error)
{
    const PluginAction& a = action();
    if (a.id.isEmpty()) return fail(error, PluginRunner::tr("the plugin has no action '%1'").arg(actionId));
    runDir = new QTemporaryDir(QDir::tempPath() + "/cyberiada-plugin-XXXXXX");
    if (!runDir->isValid()) return fail(error, PluginRunner::tr("cannot create the run directory"));
    if (!qEnvironmentVariableIsEmpty(PLUGIN_KEEP_VARIABLE)) runDir->setAutoRemove(false);
    // the canonical form, so the dumps can name it (macOS tmp is a link)
    runPath = QDir(runDir->path()).canonicalPath();
    res.runDir = runPath;

    if (!writeFile(QDir(runPath).filePath(INPUT_FILE), QByteArray::fromStdString(context.input))) {
        return fail(error, PluginRunner::tr("cannot write the input document"));
    }
    if (!prepareOutdir(error)) return false;

    PluginDocumentPaths doc = pluginDocumentPaths(context.documentPath);
    if (a.cwdDocument && doc.untitled()) return fail(error, PluginRunner::tr("the document is not saved"));
    workDir = a.cwdDocument ? doc.dir : runPath;

    QMap<QString, QString> variables;
    variables.insert("input", QDir(runPath).filePath(INPUT_FILE));
    variables.insert("output", QDir(runPath).filePath(OUTPUT_FILE));
    variables.insert("diagnostics", QDir(runPath).filePath(DIAGNOSTICS_FILE));
    variables.insert("request", QDir(runPath).filePath(REQUEST_FILE));
    variables.insert("outdir", outdir);
    variables.insert("rundir", runPath);
    variables.insert("plugin_dir", entry.folder);
    variables.insert("document", doc.path);
    variables.insert("document_dir", doc.dir);
    variables.insert("document_name", doc.name);
    variables.insert("selection", context.selection.join(','));
    variables.insert("language", PluginRegistry::instance().language());

    PluginExpansion expansion;
    if (!expandArguments(entry.descriptor, a, runValues, variables, context.meta, &expansion, error)) {
        return false;
    }
    for (QMap<QString, QString>::const_iterator i = expansion.parameterFiles.begin();
         i != expansion.parameterFiles.end(); i++) {
        if (!writeFile(QDir(runPath).filePath(i.key()), i.value().toUtf8())) {
            return fail(error, PluginRunner::tr("cannot write a parameter file"));
        }
    }
    if (expansion.usesRequest) writeRequest(QDir(runPath).filePath(REQUEST_FILE));

    QString program;
    QStringList arguments;
    if (!resolveProgram(expansion.arguments, &program, &arguments, error)) return false;
    QProcessEnvironment env;
    if (!prepareEnvironment(&env, error)) return false;

    outdirBefore = listOutdir();
    process = new QProcess(this);
    process->setProgram(program);
    process->setArguments(arguments);
    process->setProcessEnvironment(env);
    process->setWorkingDirectory(workDir);
    process->setStandardInputFile(QProcess::nullDevice());
    connect(process, &QProcess::readyReadStandardOutput, this, &PluginRunner::slotStdout);
    connect(process, &QProcess::readyReadStandardError, this, &PluginRunner::slotStderr);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PluginRunner::slotFinished);
    connect(process, &QProcess::errorOccurred, this, &PluginRunner::slotError);
    return true;
}

void PluginRunner::start()
{
    clock.start();
    QString error;
    if (!stage(&error)) {
        finish(PluginResult::stateFailed, error);
        return;
    }
    res.started = true;
    if (action().timeout > 0) timeoutTimer.start(action().timeout * 1000);
    process->start();
}

void PluginRunner::cancel()
{
    if (finishedFlag || cancelled) return;
    cancelled = true;
    if (!process || process->state() == QProcess::NotRunning) {
        finish(PluginResult::stateCancelled, PluginRunner::tr("cancelled"));
        return;
    }
    process->terminate();
    killTimer.start(PLUGIN_KILL_GRACE_MS);
}

void PluginRunner::abort()
{
    if (finishedFlag) return;
    cancelled = true;
    if (process && process->state() != QProcess::NotRunning) {
        // the finished signal arrives during the wait and ends the run
        process->kill();
        process->waitForFinished(PLUGIN_KILL_GRACE_MS);
    }
    if (!finishedFlag) finish(PluginResult::stateCancelled, PluginRunner::tr("cancelled"));
}

QString PluginRunner::decode(const QByteArray& bytes, QTextDecoder** decoder)
{
    if (!*decoder) *decoder = QTextCodec::codecForName("UTF-8")->makeDecoder();
    QString text = (*decoder)->toUnicode(bytes);
    if ((*decoder)->hasFailure()) {
        // not UTF-8: the local 8-bit encoding, and a fresh decoder for the next chunk
        delete *decoder;
        *decoder = nullptr;
        text = QString::fromLocal8Bit(bytes);
    }
    return text;
}

// Windows programs end their lines with \r\n: the output keeps \n on every system, as
// the dumps, the logs and the output tab expect (a \r left at the end is dropped)
QString PluginRunner::lineFeeds(QString text, bool* pendingCr)
{
    if (*pendingCr) {
        text.prepend(QLatin1Char('\r'));
        *pendingCr = false;
    }
    if (text.endsWith(QLatin1Char('\r'))) {
        text.chop(1);
        *pendingCr = true;
    }
    return text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
}

void PluginRunner::slotStdout()
{
    if (!process) return;
    QByteArray bytes = process->readAllStandardOutput();
    if (bytes.isEmpty() || capped) return;
    outputBytes += bytes.size();
    QString text = lineFeeds(decode(bytes, &stdoutDecoder), &stdoutCr);
    if (outputBytes > PLUGIN_OUTPUT_CAP) {
        capped = true;
        text += "\n[the output is cut at the limit]\n";
    }
    res.stdoutText += text;
    emit output(text, false);
}

void PluginRunner::slotStderr()
{
    if (!process) return;
    QByteArray bytes = process->readAllStandardError();
    if (bytes.isEmpty() || capped) return;
    outputBytes += bytes.size();
    QString text = lineFeeds(decode(bytes, &stderrDecoder), &stderrCr);
    if (outputBytes > PLUGIN_OUTPUT_CAP) {
        capped = true;
        text += "\n[the output is cut at the limit]\n";
    }
    stderrText += text;
    emit output(text, true);
}

void PluginRunner::slotTimeout()
{
    if (finishedFlag || !process) return;
    timedOut = true;
    process->terminate();
    killTimer.start(PLUGIN_KILL_GRACE_MS);
}

void PluginRunner::slotKill()
{
    if (process && process->state() != QProcess::NotRunning) process->kill();
}

void PluginRunner::slotError(QProcess::ProcessError error)
{
    if (error != QProcess::FailedToStart || finishedFlag) return;
    QString name = QFileInfo(process->program()).fileName();
    finish(PluginResult::stateFailed, PluginRunner::tr("the program '%1' cannot start").arg(name));
}

void PluginRunner::slotFinished(int exitCode, QProcess::ExitStatus status)
{
    if (finishedFlag) return;
    timeoutTimer.stop();
    killTimer.stop();
    slotStdout();
    slotStderr();
    if (timedOut) {
        finish(PluginResult::stateFailed, PluginRunner::tr("the run timed out after %1 s").arg(action().timeout));
        return;
    }
    if (cancelled) {
        finish(PluginResult::stateCancelled, PluginRunner::tr("cancelled"));
        return;
    }
    if (status == QProcess::CrashExit) {
        finish(PluginResult::stateFailed, PluginRunner::tr("the plugin crashed"));
        return;
    }
    res.exitCode = exitCode;
    if (!action().exitOk.contains(exitCode)) {
        finish(PluginResult::stateFailed, PluginRunner::tr("the exit code %1 is not a success").arg(exitCode));
        return;
    }
    validate();
}

void PluginRunner::validate()
{
    const PluginAction& a = action();
    if (a.result == PluginAction::resultReplace || a.result == PluginAction::resultNewDocument) {
        QFile f(QDir(runPath).filePath(OUTPUT_FILE));
        if (!f.exists()) {
            finish(PluginResult::stateFailed, PluginRunner::tr("the output document is missing"));
            return;
        }
        if (!f.open(QIODevice::ReadOnly)) {
            finish(PluginResult::stateFailed, PluginRunner::tr("the output document cannot be read"));
            return;
        }
        std::string buffer = f.readAll().toStdString();
        if (buffer.empty()) {
            finish(PluginResult::stateFailed, PluginRunner::tr("the output document is empty"));
            return;
        }
        // decoded before anything is touched (EDIT-PLUG-19)
        try {
            Cyberiada::LocalDocument doc;
            Cyberiada::DocumentFormat format = Cyberiada::formatDetect;
            Cyberiada::String format_str;
            doc.decode(buffer, format, format_str, Cyberiada::geometryFormatQt,
                       a.loadReconstruct, a.loadReconstruct, false, false, false, a.loadStrict);
            if (doc.get_state_machines().empty()) {
                finish(PluginResult::stateFailed, PluginRunner::tr("the output document has no state machine"));
                return;
            }
            doc.encode(res.document, Cyberiada::formatCyberiada10);
        } catch (const Cyberiada::XMLException&) {
            // the library's own text names its source line, not the document
            finish(PluginResult::stateFailed, PluginRunner::tr("the output document is not well-formed XML"));
            return;
        } catch (const Cyberiada::MetainformationException&) {
            finish(PluginResult::stateFailed, PluginRunner::tr("the output document has invalid metainformation"));
            return;
        } catch (const Cyberiada::CybMLException&) {
            finish(PluginResult::stateFailed, PluginRunner::tr("the output document is not a valid CyberiadaML document"));
            return;
        } catch (const Cyberiada::Exception&) {
            finish(PluginResult::stateFailed, PluginRunner::tr("the output document cannot be read"));
            return;
        }
    }

    QFile diagnostics(QDir(runPath).filePath(DIAGNOSTICS_FILE));
    if (diagnostics.exists()) {
        QString error;
        if (!diagnostics.open(QIODevice::ReadOnly) ||
            !readDiagnostics(diagnostics.readAll(), &res.diagnostics, &error)) {
            finish(PluginResult::stateFailed, error.isEmpty() ? PluginRunner::tr("the diagnostics file cannot be read")
                                                               : error);
            return;
        }
    } else if (a.result == PluginAction::resultDiagnostics) {
        finish(PluginResult::stateFailed, PluginRunner::tr("the diagnostics file is missing"));
        return;
    }

    if (a.result == PluginAction::resultFiles) {
        QMap<QString, QPair<qint64, qint64> > after = listOutdir();
        for (QMap<QString, QPair<qint64, qint64> >::const_iterator i = after.begin(); i != after.end(); i++) {
            if (!outdirBefore.contains(i.key())) {
                PluginFileChange c;
                c.path = i.key();
                c.isNew = true;
                res.files.append(c);
            } else if (outdirBefore.value(i.key()) != i.value()) {
                PluginFileChange c;
                c.path = i.key();
                c.isNew = false;
                res.files.append(c);
            }
        }
    }
    finish(PluginResult::stateDone);
}

void PluginRunner::finish(PluginResult::State state, const QString& error)
{
    if (finishedFlag) return;
    finishedFlag = true;
    timeoutTimer.stop();
    killTimer.stop();
    res.state = state;
    res.error = error;
    QStringList lines = stderrText.split('\n', QString::SkipEmptyParts);
    res.stderrTail = lines.mid(qMax(0, lines.size() - PLUGIN_STDERR_TAIL));
    if (process) process->disconnect(this);
    // the run dir goes now unless kept for debugging (EDIT-PLUG-26)
    delete runDir;
    runDir = nullptr;
    emit finished();
}
