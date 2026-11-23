/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The git program: its path, its environment and its runs
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
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include "git_process.h"
#include "settings_manager.h"

// git restore and git switch came with 2.23
static const int GIT_MIN_MAJOR = 2;
static const int GIT_MIN_MINOR = 23;
// a command that hangs (a lock held, a hook waiting) is ended
static const int GIT_TIMEOUT_MS = 120000;
static const int MS_PER_SECOND = 1000;
static const char* GIT_NAME = "git";

QString GitResult::message() const
{
    QString text = err.trimmed();
    if (!text.isEmpty()) return text;
    if (!started) return GitProcess::tr("git did not run");
    return GitProcess::tr("git exited with %1").arg(exitCode);
}

QString GitProcess::resolve(const QString& configured, QString* version)
{
    static QHash<QString, QString> versions;    // the path and its version line, "" when too old
    QString path = configured.isEmpty() ? QStandardPaths::findExecutable(GIT_NAME) : configured;
    if (path.isEmpty() || !QFileInfo(path).isExecutable()) return QString();
    if (!versions.contains(path)) {
        QProcess process;
        process.setProcessEnvironment(environment());
        process.setStandardInputFile(QProcess::nullDevice());
        process.start(path, QStringList() << "--version");
        QString line;
        if (process.waitForFinished(GIT_TIMEOUT_MS) && process.exitCode() == 0) {
            line = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
            QRegularExpressionMatch m = QRegularExpression("(\\d+)\\.(\\d+)").match(line);
            int major = m.hasMatch() ? m.captured(1).toInt() : 0;
            int minor = m.hasMatch() ? m.captured(2).toInt() : 0;
            if (major < GIT_MIN_MAJOR || (major == GIT_MIN_MAJOR && minor < GIT_MIN_MINOR)) line.clear();
        }
        versions.insert(path, line);
    }
    if (versions.value(path).isEmpty()) return QString();
    if (version) *version = versions.value(path);
    return path;
}

QString GitProcess::program()
{
    if (!SettingsManager::instance().getGitEnabled()) return QString();
    return resolve(SettingsManager::instance().getGitProgram(), nullptr);
}

QString GitProcess::describe(const QString& configured)
{
    QString version;
    QString path = resolve(configured, &version);
    if (path.isEmpty()) return QString();
    return QString("%1 (%2)").arg(version, QDir::toNativeSeparators(path));
}

QString GitProcess::unavailableReason()
{
    if (!SettingsManager::instance().getGitEnabled()) return tr("git is off");
    if (program().isEmpty()) return tr("git is not found");
    return QString();
}

QProcessEnvironment GitProcess::environment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // no command waits for a password on a terminal it has not got
    env.insert("GIT_TERMINAL_PROMPT", "0");
    // a path is never a pattern
    env.insert("GIT_LITERAL_PATHSPECS", "1");
    return env;
}

QProcess* GitProcess::create(const QString& folder, const QStringList& args, QObject* parent)
{
    QProcess* process = new QProcess(parent);
    process->setProgram(program());
    process->setArguments(args);
    process->setWorkingDirectory(folder);
    process->setProcessEnvironment(environment());
    return process;
}

GitResult GitProcess::result(QProcess* process)
{
    GitResult r;
    r.started = process->exitStatus() == QProcess::NormalExit && process->error() != QProcess::FailedToStart;
    r.exitCode = process->exitCode();
    r.out = process->readAllStandardOutput();
    r.err = QString::fromUtf8(process->readAllStandardError());
    return r;
}

GitResult GitProcess::run(const QString& folder, const QStringList& args, const QByteArray& input)
{
    GitResult r;
    if (program().isEmpty()) {
        r.err = unavailableReason();
        return r;
    }
    QProcess process;
    process.setProgram(program());
    process.setArguments(args);
    process.setWorkingDirectory(folder);
    process.setProcessEnvironment(environment());
    if (input.isNull()) process.setStandardInputFile(QProcess::nullDevice());
    process.start();
    if (!process.waitForStarted(GIT_TIMEOUT_MS)) {
        r.err = process.errorString();
        return r;
    }
    if (!input.isNull()) {
        process.write(input);
        process.closeWriteChannel();
    }
    if (!process.waitForFinished(GIT_TIMEOUT_MS)) {
        process.kill();
        process.waitForFinished();
        r.err = tr("git did not end in %1 s").arg(GIT_TIMEOUT_MS / MS_PER_SECOND);
        return r;
    }
    return result(&process);
}
