/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Application
 *
 * Copyright (C) 2024 Alexey Fedoseev <aleksey@fedoseev.net>
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

#include <clocale>
#include <QDateTime>
#include <QCommandLineParser>
#include <QFontDatabase>
#include "main.h"
#include "smeditor_window.h"
#include "cyberiada_constants.h"
#include "settings_manager.h"
#include "fontmanager.h"
#include "batch_driver.h"
#include "cyberiadasm_render.h"

int main(int argc, char *argv[])
{
	qsrand(QDateTime::currentDateTime().toTime_t());
	CyberiadaSMEditorApplication app(argc, argv);
	// QApplication adopts the user's locale; keep the printf-family numeric
	// formatting locale-independent - the graphml writer depends on it
	setlocale(LC_NUMERIC, "C");

	// pin the bundled font so text metrics and rendering do not depend
	// on the machine's font environment; the font dialog still overrides
	int font_id = QFontDatabase::addApplicationFont(":/Fonts/fonts/courier.ttf");
	if (font_id != -1) {
		QStringList families = QFontDatabase::applicationFontFamilies(font_id);
		if (!families.isEmpty()) {
			FontManager::instance().setFont(QFont(families.first(), FONT_SIZE));
		}
	}

	QCommandLineParser parser;
	parser.setApplicationDescription("Cyberiada State Machine Editor");
	parser.addHelpOption();
	QCommandLineOption batchOption("batch", "Batch mode: open the document and exit (see docs/TESTING.md).");
	parser.addOption(batchOption);
	QCommandLineOption dumpOption("dump", "Print the canonical document/scene dump in batch mode.");
	parser.addOption(dumpOption);
	QCommandLineOption scriptOption("script", "Run the edit script in batch mode.", "file");
	parser.addOption(scriptOption);
	QCommandLineOption saveOption("save", "Save the document in batch mode after the edits.", "file");
	parser.addOption(saveOption);
	QCommandLineOption exportOption("export", "Export the scene image in batch mode.", "file");
	parser.addOption(exportOption);
	QCommandLineOption compareOption("compare", "Compare two image files with tolerance and exit.");
	parser.addOption(compareOption);
	QCommandLineOption epsilonOption("epsilon", "Comparison per-channel tolerance (0-255, default 8).", "n", "8");
	parser.addOption(epsilonOption);
	QCommandLineOption maxDiffOption("max-diff", "Comparison allowed differing pixel fraction (default 0).", "f", "0");
	parser.addOption(maxDiffOption);
	parser.addPositionalArgument("file", "The CyberiadaML document to open in batch mode.", "[file]");
	parser.process(app);

	bool batch = parser.isSet(batchOption);
	app.setBatchMode(batch);

    try {
		if (parser.isSet(compareOption)) {
			QStringList args = parser.positionalArguments();
			if (args.size() != 2) {
				fprintf(stderr, "image comparison requires exactly two image files\n");
				return batchUsageError;
			}
			QString report;
			int res = compareImages(args.at(0), args.at(1),
									parser.value(epsilonOption).toInt(),
									parser.value(maxDiffOption).toDouble(), &report);
			fprintf(stderr, "%s\n", qPrintable(report));
			if (res < 0) return batchInternalError;
			return res == 0 ? batchOK : batchImageMismatch;
		}
		if (batch) {
			QStringList args = parser.positionalArguments();
			if (args.size() != 1) {
				fprintf(stderr, "batch mode requires exactly one document file\n");
				return batchUsageError;
			}
			return runBatchMode(app, args.first(), parser.isSet(dumpOption),
								parser.value(scriptOption), parser.value(saveOption),
								parser.value(exportOption));
		}
		CyberiadaSMEditorWindow win;
		win.show();
		int res = app.exec();

		return res;
	} catch(const QString& error) {
		app.printMessage(error);
	} catch(...) {
		app.printMessage();
	}
	return app.batchMode() ? batchInternalError : 1;
}
