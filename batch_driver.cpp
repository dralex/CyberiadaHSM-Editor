/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
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

#include "batch_driver.h"
#include "main.h"
#include "smeditor_window.h"
#include "cyberiadasm_dump.h"
#include "batch_script.h"
#include "cyberiadasm_render.h"

int runGuiMode(CyberiadaSMEditorApplication& app)
{
	CyberiadaSMEditorWindow win;
	win.show();
	return app.exec();
}

// let the pending events run; an assertion thrown by one is caught by
// notify() and reported by the flag
static bool stageFailed(CyberiadaSMEditorApplication& app)
{
	app.processEvents();
	return app.errorReported();
}

int runBatchMode(CyberiadaSMEditorApplication& app, const QString& fileName, bool dump,
				 const QString& script, const QString& save, const QString& exportImage,
				 bool reconstruct, bool reconstruct_sm, bool strict,
				 Cyberiada::DocumentFormat saveFormat, bool dumpTextMetrics, bool dumpUndoStack)
{
	CyberiadaSMEditorWindow win;
	win.show();

	QString error;
	// a from-scratch session has no start document: the script's first verb
	// (new-sm) bootstraps it
	if (!fileName.isEmpty() &&
		!win.openDocument(fileName, &error, reconstruct, reconstruct_sm, strict)) {
		fprintf(stderr, "cannot load %s\n%s\n", qPrintable(fileName), qPrintable(error));
		return batchLoadError;
	}

	if (!script.isEmpty()) {
		// the scene follows the model through its signals
		if (!runEditScript(&win, script, &error)) {
			fprintf(stderr, "script %s failed\n%s\n", qPrintable(script), qPrintable(error));
			return batchScriptError;
		}
	}

	// the assertions of every stage are caught by notify(); the flag says so
	if (stageFailed(app)) return batchInternalError;

	if (dump) {
		std::cout << "== document" << std::endl;
		dumpDocument(win.getModel(), std::cout);
		std::cout << "== scene" << std::endl;
		dumpScene(win.getScene(), win.getModel(), std::cout);
		if (stageFailed(app)) return batchInternalError;
	}

	if (dumpTextMetrics) {
		std::cout << "== text" << std::endl;
		dumpText(win.getScene(), win.getModel(), std::cout);
		if (stageFailed(app)) return batchInternalError;
	}

	if (dumpUndoStack) {
		std::cout << "== stack" << std::endl;
		dumpStack(win.getModel(), std::cout);
	}

	if (!exportImage.isEmpty()) {
		QString render_error;
		if (!renderScene(win.getScene(), exportImage, &render_error)) {
			fprintf(stderr, "cannot export %s\n%s\n", qPrintable(exportImage), qPrintable(render_error));
			return batchInternalError;
		}
		if (stageFailed(app)) return batchInternalError;
	}

	if (!save.isEmpty()) {
		try {
			// rounded geometry keeps the written floats stable for the good files
			win.getModel()->saveAsDocument(save, saveFormat, true, false, false, false, false);
		} catch (const Cyberiada::Exception& e) {
			fprintf(stderr, "cannot save %s\n%s\n", qPrintable(save), e.str().c_str());
			return batchInternalError;
		}
		if (stageFailed(app)) return batchInternalError;
	}
	return batchOK;
}
