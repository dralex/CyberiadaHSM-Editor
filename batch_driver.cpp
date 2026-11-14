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
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextStream>

#include "batch_driver.h"
#include "main.h"
#include "smeditor_window.h"
#include "cyberiadasm_dump.h"
#include "batch_script.h"
#include "cyberiadasm_render.h"
#include "document_mode.h"
#include "plugin_registry.h"
#include "plugin_runner.h"
#include "batch_plugin.h"
#include "interface_language.h"

int runGuiMode(CyberiadaSMEditorApplication& app, const QStringList& files,
			   bool reconstruct, bool reconstruct_sm, bool strict, bool inspect)
{
	// the texts are set as the window is built, so the translators come first;
	// the batch mode installs none (EDIT-UI-2)
	installUiTranslators(app);
	CyberiadaSMEditorWindow win;
	QString error;
	bool ok = win.openFiles(files, &error, reconstruct, reconstruct_sm, strict, inspect);
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
static int runPluginStage(CyberiadaSMEditorWindow& win, const BatchOptions& options, BatchPluginRun* run)
{
	QString error;
	if (!findBatchAction(options.plugin, run, &error)) {
		fprintf(stderr, "%s\n", qPrintable(error));
		return batchUsageError;
	}
	QStringList lines;
	if (run->action->session && !options.sessionScript.isEmpty()) {
		QFile file(options.sessionScript);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			run->result.error = QString("cannot read the session script %1").arg(options.sessionScript);
			fprintf(stderr, "the session script %s failed\n%s\n", qPrintable(options.sessionScript),
					qPrintable(run->result.error));
			return batchPluginError;
		}
		lines = QTextStream(&file).readAll().split('\n');
	}
	const QStringList* session = options.sessionScript.isEmpty() ? nullptr : &lines;
	int code = runBatchAction(win, run, options.pluginParams, session, 1, options.sessionScript,
							  options.dump || options.dumpPlugin, &error);
	if (code != batchOK) fprintf(stderr, "%s\n", qPrintable(error));
	return code;
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
						  options.strict, options.inspect)) {
		fprintf(stderr, "cannot load %s\n%s\n", qPrintable(options.fileName), qPrintable(error));
		return batchLoadError;
	}

	// the plugin verbs of the script (EDIT-PLUG-61): a failed run ends the batch with 6
	BatchPluginRuns scriptRuns;
	scriptRuns.dumping = options.dump || options.dumpPlugin;
	if (!options.script.isEmpty()) {
		// the scene follows the model through its signals
		if (!runEditScript(&win, options.script, &error, &scriptRuns)) {
			fprintf(stderr, "script %s failed\n%s\n", qPrintable(options.script), qPrintable(error));
			return batchScriptError;
		}
	}

	// the assertions of every stage are caught by notify(); the flag says so
	if (stageFailed(app)) return batchInternalError;

	int pluginCode = scriptRuns.failed() ? batchPluginError : batchOK;
	BatchPluginRun stage;
	bool staged = false;
	if (!options.plugin.isEmpty()) {
		int code = runPluginStage(win, options, &stage);
		if (code == batchUsageError) return code;
		if (stageFailed(app)) return batchInternalError;
		staged = true;
		if (code != batchOK) pluginCode = code;
	}

	if (options.dumpTabsLine) {
		std::cout << "== tabs" << std::endl;
		dumpTabs(&win, std::cout);
	}

	// after the sections of the active tab: the actions on it (EDIT-PLUG-63), the runs
	// of the script (EDIT-PLUG-64), then the run of the plugin stage
	bool dumpingPlugins = options.dump || options.dumpPlugin;
	auto dumpPluginSections = [&]() {
		if (options.dumpActions) {
			std::cout << "== actions" << std::endl;
			dumpActions(win, std::cout);
		}
		dumpPluginRuns(scriptRuns, dumpingPlugins, options.dump || options.dumpSession, std::cout);
		if (!staged || !dumpingPlugins) return;
		std::cout << "== plugin" << std::endl;
		dumpPluginRun(*stage.plugin, *stage.action, stage.result, std::cout);
		std::cout << "== diagnostics" << std::endl;
		dumpDiagnostics(stage.result, std::cout);
	};

	// a diff tab dumps its comparison and has nothing to draw or save (EDIT-MODE-16)
	if (DiffDocument* diff = win.currentDiff()) {
		if (options.dump) {
			std::cout << "== diff" << std::endl;
			dumpDiffDocument(diff, std::cout);
		}
		dumpPluginSections();
		if (!options.exportImage.isEmpty() || options.dumpAudit || options.dumpTextMetrics ||
			options.dumpUndoStack || !options.save.isEmpty()) {
			fprintf(stderr, "the active tab is a diff tab\n");
			return batchUsageError;
		}
		return pluginCode;
	}

	// a text tab has its text and no diagram to dump, draw or save as one (EDIT-MODE-16)
	if (TextDocument* text = win.currentText()) {
		if (options.dump) {
			std::cout << "== text" << std::endl;
			dumpTextDocument(text, std::cout);
		}
		dumpPluginSections();
		// a Markdown text exports its rendered PDF, nothing else (EDIT-MODE-18)
		bool pdf = text->mode() == MODE_MARKDOWN &&
				   QFileInfo(options.exportImage).suffix().toLower() == "pdf";
		if (!options.exportImage.isEmpty() && !pdf && text->mode() == MODE_MARKDOWN) {
			fprintf(stderr, "a Markdown text exports to PDF only\n");
			return batchUsageError;
		}
		if ((!options.exportImage.isEmpty() && !pdf) || options.dumpAudit ||
			options.dumpTextMetrics || options.dumpUndoStack) {
			fprintf(stderr, "the active tab holds no diagram\n");
			return batchUsageError;
		}
		if (pdf) {
			QString pdf_error;
			if (!text->exportPdf(options.exportImage, &pdf_error)) {
				fprintf(stderr, "cannot export %s\n%s\n", qPrintable(options.exportImage),
						qPrintable(pdf_error));
				return batchInternalError;
			}
		}
		if (!options.save.isEmpty()) {
			QString save_error;
			if (!text->saveAs(options.save, &save_error)) {
				fprintf(stderr, "cannot save %s\n%s\n", qPrintable(options.save), qPrintable(save_error));
				return batchInternalError;
			}
		}
		return pluginCode;
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

	dumpPluginSections();
	// the messages of a session both ways (EDIT-PLUG-48)
	if (staged && stage.action->session && (options.dump || options.dumpSession)) {
		std::cout << "== session" << std::endl;
		for (const QString& line: stage.sessionDump) std::cout << line.toStdString() << std::endl;
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
