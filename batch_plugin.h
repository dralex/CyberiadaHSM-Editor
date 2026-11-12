/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Batch Plugin Runs: the --run-plugin stage and the plugin verb of the script
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

#ifndef CYBERIADA_SM_BATCH_PLUGIN
#define CYBERIADA_SM_BATCH_PLUGIN

#include <ostream>
#include <QList>
#include <QString>
#include <QStringList>

#include "plugin_runner.h"

class CyberiadaSMEditorWindow;
struct PluginEntry;
struct PluginAction;

// one plugin run of the batch mode: the --run-plugin stage (EDIT-PLUG-27) or a
// plugin verb of the script (EDIT-PLUG-61)
struct BatchPluginRun {
	const PluginEntry* plugin = nullptr;
	const PluginAction* action = nullptr;
	PluginResult result;
	// the messages of a session (EDIT-PLUG-48)
	QStringList sessionDump;
};

// the plugin runs of a script, numbered from 1 in the dumps (EDIT-PLUG-64)
struct BatchPluginRuns {
	// the stdout of a run goes to its section, else through to stdout
	bool dumping = false;
	QList<BatchPluginRun> runs;
	bool failed() const;
};

// id[:action] to the plugin and its action; false with the reason
bool findBatchAction(const QString& spec, BatchPluginRun* run, QString* error);

// runs the action found on the active tab with the name=value items and waits;
// a session action reads the verbs of sessionLines, the first one on the line
// firstLine of the script named by sessionName (nullptr: no script, a usage
// error); batchOK, batchUsageError for a bad value, batchPluginError for a
// failed run, the last two with the reason
int runBatchAction(CyberiadaSMEditorWindow& win, BatchPluginRun* run, const QStringList& items,
				   const QStringList* sessionLines, int firstLine, const QString& sessionName,
				   bool dumping, QString* error);

// the name=value items of a plugin verb: separated by whitespace, a double-quoted
// part keeps its spaces, with \" and \\ inside it; false with the reason
bool splitPluginItems(const QString& text, QStringList* items, QString* error);

// == actions: every action of the enabled plugins on the active tab (EDIT-PLUG-63)
void dumpActions(CyberiadaSMEditorWindow& win, std::ostream& os);

// == plugin <n>, == diagnostics <n> and == session <n> of the runs (EDIT-PLUG-64)
void dumpPluginRuns(const BatchPluginRuns& runs, bool plugin, bool session, std::ostream& os);

#endif
