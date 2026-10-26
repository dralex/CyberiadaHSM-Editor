/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The batch (console) mode driver
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

#ifndef CYBERIADA_SM_BATCH_DRIVER
#define CYBERIADA_SM_BATCH_DRIVER

#include <QString>
#include <QStringList>
#include <cyberiadamlpp.h>

class CyberiadaSMEditorApplication;

// exit codes of the batch mode (see docs/TESTING.md)
enum BatchExitCode {
	batchOK = 0,
	batchUsageError = 1,
	batchLoadError = 2,
	batchInternalError = 3,
	batchScriptError = 4,
	batchImageMismatch = 5,
	batchPluginError = 6
};

// the editor window is built here, so its generated form stays inside the
// library and no other target has to find a copy of it
// the files open as tabs in order, the last one active (EDIT-DOC-7)
int runGuiMode(CyberiadaSMEditorApplication& app, const QStringList& files = QStringList(),
			   bool reconstruct = false, bool reconstruct_sm = false, bool strict = false);

// the options of a batch run, set from the command line (see docs/TESTING.md)
struct BatchOptions {
	QString fileName;
	bool dump = false;
	QString script;
	QString save;
	QString exportImage;
	bool reconstruct = false;
	bool reconstruct_sm = false;
	bool strict = false;
	Cyberiada::DocumentFormat saveFormat = Cyberiada::formatCyberiada10;
	bool dumpTextMetrics = false;
	bool dumpUndoStack = false;
	bool dumpAudit = false;
	int exportDpi = 96;
	QString exportFont;
	bool dumpTabsLine = false;
	// the plugin stage (docs/TESTING.md, Plugins): id[:action] and name=value items
	QString plugin;
	QStringList pluginParams;
	bool listPlugins = false;
	bool dumpPlugin = false;
	// a session action runs from this script (EDIT-PLUG-47), the == session dump
	QString sessionScript;
	bool dumpSession = false;
};

int runBatchMode(CyberiadaSMEditorApplication& app, const BatchOptions& options);

#endif
