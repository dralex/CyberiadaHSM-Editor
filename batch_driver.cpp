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

int runBatchMode(CyberiadaSMEditorApplication& app, const QString& fileName, bool dump,
				 const QString& script, const QString& save)
{
	CyberiadaSMEditorWindow win;
	win.show();

	QString error;
	if (!win.openDocument(fileName, &error)) {
		fprintf(stderr, "cannot load %s\n%s\n", qPrintable(fileName), qPrintable(error));
		return batchLoadError;
	}

	if (!script.isEmpty()) {
		// batch edits address the model only; the live model->scene sync is
		// re-entrant (syncFromModel writes geometry back into the model and
		// crashes on reparent) - detach it, the scene is rebuilt below
		QObject::disconnect(win.getModel(), NULL, win.getScene(), NULL);
		if (!runEditScript(win.getModel(), script, &error)) {
			fprintf(stderr, "script %s failed\n%s\n", qPrintable(script), qPrintable(error));
			return batchScriptError;
		}
		// the scene does not track model insertions/removals - rebuild it
		if (win.getModel()->firstSMIndex().isValid()) {
			win.getScene()->loadScene();
		}
	}

	// let the loaded scene settle; assertions here are caught by notify()
	app.processEvents();
	if (app.errorReported()) {
		return batchInternalError;
	}

	if (dump) {
		std::cout << "== document" << std::endl;
		dumpDocument(win.getModel(), std::cout);
		std::cout << "== scene" << std::endl;
		dumpScene(win.getScene(), win.getModel(), std::cout);
	}

	if (!save.isEmpty()) {
		try {
			// rounded geometry keeps the written floats stable for the good files
			win.getModel()->saveAsDocument(save, Cyberiada::formatCyberiada10, true);
		} catch (const Cyberiada::Exception& e) {
			fprintf(stderr, "cannot save %s\n%s\n", qPrintable(save), e.str().c_str());
			return batchInternalError;
		}
	}
	return batchOK;
}
