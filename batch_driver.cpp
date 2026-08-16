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

int runBatchMode(CyberiadaSMEditorApplication& app, const QString& fileName, bool dump)
{
	CyberiadaSMEditorWindow win;
	win.show();

	QString error;
	if (!win.openDocument(fileName, &error)) {
		fprintf(stderr, "cannot load %s\n%s\n", qPrintable(fileName), qPrintable(error));
		return batchLoadError;
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
	return batchOK;
}
