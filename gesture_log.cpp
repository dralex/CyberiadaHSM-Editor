/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The session logger implementation
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

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

#include "gesture_log.h"
#include "cyberiadasm_model.h"
#include "version.h"

GestureLog& GestureLog::instance()
{
    static GestureLog instance;
    return instance;
}

// the base folder holding the session subfolders; the env override keeps the
// tests self-contained, otherwise the per-user application data location
static QString sessionsRoot()
{
    QString env = qEnvironmentVariable("CYBERIADA_SESSION_LOG_DIR");
    if (!env.isEmpty()) return env;
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) base = QDir::currentPath();
    return QDir(base).filePath("sessions");
}

void GestureLog::startSession(CyberiadaSMModel* model)
{
    if (active) return;
    // a replayed test must never be re-logged
    if (qApp && qApp->property("batchMode").toBool()) return;

    QDateTime now = QDateTime::currentDateTime();
    QString stamp = now.toString("yyyyMMdd-HHmmss-zzz");
    dir = QDir(sessionsRoot()).filePath(stamp);
    if (!QDir().mkpath(dir)) {
        qWarning() << "cannot create the session log folder" << dir;
        dir.clear();
        return;
    }

    file.setFileName(QDir(dir).filePath("session.script"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning() << "cannot open the session log" << file.fileName();
        dir.clear();
        return;
    }
    stream.setDevice(&file);
    active = true;
    gestureDepth = 0;

    writeLine(QString("# %1 %2 rev %3")
              .arg(CYBERIADA_APP_NAME).arg(CYBERIADA_VERSION).arg(CYBERIADA_REVISION));
    // the document as it stands now; a from-scratch session has none and
    // replays with no start document (its first verb is new-sm)
    if (model && model->writeSnapshotFile(QDir(dir).filePath("start.graphml"))) {
        writeLine("# document start.graphml");
    } else {
        writeLine("# document (empty)");
    }
    writeLine("# start " + now.toString(Qt::ISODate));
}

void GestureLog::endSession()
{
    if (!active) return;
    writeLine("# exit " + QDateTime::currentDateTime().toString(Qt::ISODate));
    stream.flush();
    file.close();
    active = false;
}

void GestureLog::writeLine(const QString& line)
{
    stream << line << '\n';
    // flush after every line: a segfault kills the process but the OS keeps
    // the flushed bytes, so the log stops exactly at the crashing gesture
    stream.flush();
    file.flush();
}

void GestureLog::logGesture(const QString& line)
{
    if (!active) return;
    writeLine(line);
}

void GestureLog::logAction(const QString& line)
{
    if (!active || gestureDepth > 0) return;
    writeLine(line);
}
