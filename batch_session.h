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

#ifndef BATCH_SESSION_H
#define BATCH_SESSION_H

#include <QString>
#include <QStringList>

#include "plugin_runner.h"

class CyberiadaSMEditorWindow;

// a session action run from the verbs of the script (EDIT-PLUG-47); the dump
// lines of the session come back for --dump-session (EDIT-PLUG-48); false with
// the reason when a verb fails, the result set once the session has ended
bool runSessionStage(CyberiadaSMEditorWindow& win, const QString& pluginId, const QString& actionId,
                     const PluginValues& values, const QString& script, PluginResult* result,
                     QStringList* dump, QString* error);

#endif
