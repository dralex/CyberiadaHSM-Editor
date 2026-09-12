/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The session logger: records the user's gestures and model actions to a file
 * in the batch script language, so a live session replays as a test
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

#ifndef CYBERIADA_GESTURE_LOG_HEADER
#define CYBERIADA_GESTURE_LOG_HEADER

#include <QFile>
#include <QString>
#include <QTextStream>

class CyberiadaSMModel;

// A peer of SettingsManager, reachable from the scene and the model without
// threading a pointer. A session writes one folder: start.graphml (the
// document snapshot when logging began, absent for a from-scratch session) and
// session.script (the recorded verbs). A missing exit line marks a crash.
class GestureLog {
public:
    static GestureLog& instance();

    // create the session folder, snapshot the document and open the script;
    // a no-op under batch mode so a replayed test is never re-logged
    void startSession(CyberiadaSMModel* model);
    // write the exit line and close; a no-op if no session is open
    void endSession();
    bool isActive() const { return active; }
    const QString& sessionDir() const { return dir; }

    // a raw mouse/keyboard gesture verb (press/drag/release/...)
    void logGesture(const QString& line);
    // a semantic model verb; suppressed while a gesture is in flight, since the
    // gesture already records the same edit and would apply it twice on replay
    void logAction(const QString& line);

    // bracket a mouse gesture: the model actions it triggers are not logged
    void enterGesture() { gestureDepth++; }
    void leaveGesture() { if (gestureDepth > 0) gestureDepth--; }

private:
    GestureLog() {}
    GestureLog(const GestureLog&) = delete;
    GestureLog& operator=(const GestureLog&) = delete;

    void writeLine(const QString& line);

    QString  dir;
    QFile    file;
    QTextStream stream;
    bool     active = false;
    int      gestureDepth = 0;
};

#endif
