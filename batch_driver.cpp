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

#include <cstdio>
#include <iostream>
#include <QMessageBox>

#include "batch_driver.h"
#include "main.h"
#include "smeditor_window.h"
#include "cyberiadasm_dump.h"
#include "batch_script.h"
#include "cyberiadasm_render.h"
#include "plugin_registry.h"
#include "plugin_runner.h"
#include "interface_language.h"

int runGuiMode(CyberiadaSMEditorApplication& app, const QStringList& files,
			   bool reconstruct, bool reconstruct_sm, bool strict)
{
	// the texts are set as the window is built, so the translators come first;
	// the batch mode installs none (EDIT-UI-2)
	installUiTranslators(app);
	CyberiadaSMEditorWindow win;
	QString error;
	bool ok = win.openFiles(files, &error, reconstruct, reconstruct_sm, strict);
	win.show();
	// the failed files are reported once the window is up; the rest are open
	if (!ok) QMessageBox::warning(&win, QObject::tr("Open State Machine"), error);
	return app.exec();
}

// let the pending events run; an assertion thrown by one is caught by
// notify() and reported by the flag
static bool stageFailed(CyberiadaSMEditorApplication& app)
{
	app.processEvents();
	return app.errorReported();
}

// the plugin stage (EDIT-PLUG-27): a usage error stops the run, a failed run is
// reported with the dumps and ends with batchPluginError
static int runPluginStage(CyberiadaSMEditorWindow& win, const BatchOptions& options,
						  const PluginEntry** entry, const PluginAction** action, PluginResult* result)
{
	QString id = options.plugin.section(':', 0, 0);
	QString actionId = options.plugin.section(':', 1);
	*entry = PluginRegistry::instance().find(id);
	if (!*entry) {
		fprintf(stderr, "the plugin %s is not found\n", qPrintable(id));
		return batchUsageError;
	}
	const PluginDescriptor& plugin = (*entry)->descriptor;
	if (actionId.isEmpty()) {
		if (plugin.actions.size() != 1) {
			fprintf(stderr, "the plugin %s has several actions: name one as %s:<action>\n",
					qPrintable(id), qPrintable(id));
			return batchUsageError;
		}
		actionId = plugin.actions.first().id;
	}
	*action = plugin.action(actionId);
	if (!*action) {
		fprintf(stderr, "the plugin %s has no action %s\n", qPrintable(id), qPrintable(actionId));
		return batchUsageError;
	}
	PluginValues values;
	QString error;
	PluginDocumentPaths paths = pluginDocumentPaths(win.currentDocument()->filePath());
	if (!pluginParseValues(plugin, **action, options.pluginParams, paths, &values, &error)) {
		fprintf(stderr, "%s\n", qPrintable(error));
		return batchUsageError;
	}
	bool dumping = options.dump || options.dumpPlugin;
	// stderr goes through as it comes; stdout to the dump, or through without one
	QMetaObject::Connection started = QObject::connect(&win, &CyberiadaSMEditorWindow::pluginRunStarted,
		[dumping](PluginRunner* run) {
			QObject::connect(run, &PluginRunner::output, [dumping](const QString& text, bool isError) {
				if (isError) {
					fputs(text.toLocal8Bit().constData(), stderr);
				} else if (!dumping) {
					fputs(text.toLocal8Bit().constData(), stdout);
					fflush(stdout);
				}
			});
		});
	*result = win.runPluginAndWait(id, actionId, values);
	QObject::disconnect(started);
	if (result->state != PluginResult::stateDone) {
		fprintf(stderr, "the plugin %s:%s failed\n%s\n", qPrintable(id), qPrintable(actionId),
				qPrintable(result->error));
		return batchPluginError;
	}
	return batchOK;
}

int runBatchMode(CyberiadaSMEditorApplication& app, const BatchOptions& options)
{
	CyberiadaSMEditorWindow win;
	win.show();

	// the plugins found, before anything else; no document needed
	if (options.listPlugins) {
		std::cout << "== plugins" << std::endl;
		dumpPlugins(PluginRegistry::instance().entries(), std::cout);
	}

	QString error;
	// a from-scratch session has no start document: the script's first verb
	// (new-sm) bootstraps it
	if (!options.fileName.isEmpty() &&
		!win.openDocument(options.fileName, &error, options.reconstruct, options.reconstruct_sm,
						  options.strict)) {
		fprintf(stderr, "cannot load %s\n%s\n", qPrintable(options.fileName), qPrintable(error));
		return batchLoadError;
	}

	if (!options.script.isEmpty()) {
		// the scene follows the model through its signals
		if (!runEditScript(&win, options.script, &error)) {
			fprintf(stderr, "script %s failed\n%s\n", qPrintable(options.script), qPrintable(error));
			return batchScriptError;
		}
	}

	// the assertions of every stage are caught by notify(); the flag says so
	if (stageFailed(app)) return batchInternalError;

	const PluginEntry* plugin = nullptr;
	const PluginAction* action = nullptr;
	PluginResult pluginResult;
	int pluginCode = batchOK;
	if (!options.plugin.isEmpty()) {
		pluginCode = runPluginStage(win, options, &plugin, &action, &pluginResult);
		if (pluginCode == batchUsageError) return pluginCode;
		if (stageFailed(app)) return batchInternalError;
	}

	if (options.dumpTabsLine) {
		std::cout << "== tabs" << std::endl;
		dumpTabs(&win, std::cout);
	}

	if (options.dump) {
		std::cout << "== document" << std::endl;
		dumpDocument(win.getModel(), std::cout);
		std::cout << "== scene" << std::endl;
		dumpScene(win.getScene(), win.getModel(), std::cout);
		if (stageFailed(app)) return batchInternalError;
	}

	if (options.dumpAudit) {
		std::cout << "== audit" << std::endl;
		for (const QString& line : win.getScene()->auditOrphans()) {
			std::cout << line.toStdString() << std::endl;
		}
	}

	if (plugin && action && (options.dump || options.dumpPlugin)) {
		std::cout << "== plugin" << std::endl;
		dumpPluginRun(*plugin, *action, pluginResult, std::cout);
		std::cout << "== diagnostics" << std::endl;
		dumpDiagnostics(pluginResult, std::cout);
	}

	if (options.dumpTextMetrics) {
		std::cout << "== text" << std::endl;
		dumpText(win.getScene(), win.getModel(), std::cout);
		if (stageFailed(app)) return batchInternalError;
	}

	if (options.dumpUndoStack) {
		std::cout << "== stack" << std::endl;
		dumpStack(win.getModel(), std::cout);
	}

	if (!options.exportImage.isEmpty()) {
		QString render_error;
		if (!renderScene(win.getScene(), options.exportImage, &render_error, options.exportDpi,
						 options.exportFont)) {
			fprintf(stderr, "cannot export %s\n%s\n", qPrintable(options.exportImage),
					qPrintable(render_error));
			return batchInternalError;
		}
		if (stageFailed(app)) return batchInternalError;
	}

	if (!options.save.isEmpty()) {
		try {
			// rounded geometry keeps the written floats stable for the good files
			win.getModel()->saveAsDocument(options.save, options.saveFormat, true, false, false,
										   false, false);
		} catch (const Cyberiada::Exception& e) {
			fprintf(stderr, "cannot save %s\n%s\n", qPrintable(options.save), e.str().c_str());
			return batchInternalError;
		}
		if (stageFailed(app)) return batchInternalError;
	}
	return pluginCode;
}
