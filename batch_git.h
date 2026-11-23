/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The git verbs of the batch script and the dump of the repository
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

#ifndef CYBERIADA_BATCH_GIT
#define CYBERIADA_BATCH_GIT

#include <ostream>
#include <QString>

class CyberiadaSMEditorWindow;

// the verbs git ... and repository ... (EDIT-GIT-21): they wait for git and ask nothing
bool isGitVerb(const QString& cmd);
// the line of the script as written: a message keeps its spaces
bool runGitVerb(CyberiadaSMEditorWindow* win, const QString& line, QString* error);
// --dump-repository: the repository of the active tab (EDIT-GIT-22)
void dumpRepository(CyberiadaSMEditorWindow* win, std::ostream& os);
// --dump-history: the rows of the History dock (EDIT-GIT-22)
void dumpHistory(CyberiadaSMEditorWindow* win, std::ostream& os);

#endif
