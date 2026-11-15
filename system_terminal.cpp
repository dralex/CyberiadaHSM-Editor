/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The system terminal opened in a folder
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
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

#include "system_terminal.h"

bool openSystemTerminal(const QString& folder, QString* error)
{
    if (!QFileInfo(folder).isDir()) {
        if (error) *error = QCoreApplication::translate("SystemTerminal", "The folder %1 does not exist.")
                                .arg(folder);
        return false;
    }
    QString program;
    QStringList arguments;
#if defined(Q_OS_WIN)
    // a window process starts cmd without a console: start gives it one
    program = "cmd.exe";
    arguments << "/c" << "start" << "cmd.exe";
#elif defined(Q_OS_MACOS)
    program = "open";
    arguments << "-a" << "Terminal" << folder;
#else
    // the user's choice, the Debian alternative, then the terminals of the desktops
    QStringList candidates = QStringList() << "x-terminal-emulator" << "gnome-terminal"
        << "konsole" << "xfce4-terminal" << "mate-terminal" << "lxterminal" << "xterm";
    QString chosen = qEnvironmentVariable("TERMINAL");
    if (!chosen.isEmpty()) candidates.prepend(chosen);
    for (const QString& name : candidates) {
        program = QStandardPaths::findExecutable(name);
        if (!program.isEmpty()) break;
    }
    if (program.isEmpty()) {
        if (error) *error = QCoreApplication::translate("SystemTerminal",
                                "No terminal found: set TERMINAL to the terminal program.");
        return false;
    }
#endif
    if (!QProcess::startDetached(program, arguments, folder)) {
        if (error) *error = QCoreApplication::translate("SystemTerminal", "Cannot start the terminal %1.")
                                .arg(program);
        return false;
    }
    return true;
}
