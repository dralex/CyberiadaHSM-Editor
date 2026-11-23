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

#ifndef CYBERIADA_GIT_PROCESS
#define CYBERIADA_GIT_PROCESS

#include <QByteArray>
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

class QObject;
class QProcess;

// the end of a git command (docs/GIT.md, The git program)
struct GitResult {
    bool       started = false;    // the program ran to its end
    int        exitCode = -1;
    QByteArray out;                // stdout as bytes: the -z output is split at NUL
    QString    err;                // stderr, git's message as it is

    bool       ok() const { return started && exitCode == 0; }
    // git's message, else the reason git did not run
    QString    message() const;
};

// The git program of the editor (EDIT-GIT-1, EDIT-GIT-17): the one of Preferences > Git,
// else git in PATH, 2.23 or later; every run has stdin closed (the input aside), no
// terminal prompt and literal paths
class GitProcess {
    Q_DECLARE_TR_FUNCTIONS(GitProcess)
public:
    // the program to run; empty without one or with Use git off
    static QString             program();
    // "git version 2.43.0 (/usr/bin/git)" for the program a path names, empty for the git
    // of PATH; an empty string when none is found (the Git page of the Preferences)
    static QString             describe(const QString& configured);
    // why no git runs: git is off, git is not found; empty when one does
    static QString             unavailableReason();
    static QProcessEnvironment environment();

    // run in the folder and wait (the batch mode, the commands of the user)
    static GitResult           run(const QString& folder, const QStringList& args,
                                   const QByteArray& input = QByteArray());
    // a process set up to run in the folder, not started (the reads in the GUI)
    static QProcess*           create(const QString& folder, const QStringList& args, QObject* parent);
    // the result of a finished process made by create()
    static GitResult           result(QProcess* process);

private:
    // the program a path names, or the git of PATH, when it is 2.23 or later
    static QString             resolve(const QString& configured, QString* version);
};

#endif
