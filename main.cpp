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
#include <QCommandLineParser>
#include "main.h"
#include "cyberiada_constants.h"
#include "settings_manager.h"
#include "fontmanager.h"
#include "batch_driver.h"
#include "cyberiadasm_render.h"
#include "version.h"

int main(int argc, char *argv[])
{
	CyberiadaSMEditorApplication app(argc, argv);
	// name the application so QSettings stores under Cyberiada/<app>, not bare
	// ~/.config; must precede the first SettingsManager (QSettings) access
	QCoreApplication::setOrganizationName(CYBERIADA_VENDOR);
	QCoreApplication::setApplicationName(CYBERIADA_APP_NAME);
	QCoreApplication::setApplicationVersion(CYBERIADA_VERSION);
	// QApplication adopts the user's locale; keep the printf-family numeric
	// formatting locale-independent - the graphml writer depends on it
	setlocale(LC_NUMERIC, "C");

	// pin the bundled font so the text metrics and the rendering do not
	// depend on the font environment of the machine
	FontManager::instance().loadBundledFont();

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
	QCommandLineOption dpiOption("dpi", "Raster export resolution (96 = 1:1, default 96).", "n", "96");
	parser.addOption(dpiOption);
	QCommandLineOption noTextOption("no-text", "Hide the text elements in batch mode (font-independent output).");
	parser.addOption(noTextOption);
	QCommandLineOption textOption("text", "Show the text elements in batch mode, overriding --no-text.");
	parser.addOption(textOption);
	QCommandLineOption dumpTextOption("dump-text", "Dump the font and the layout of the text elements.");
	parser.addOption(dumpTextOption);
	QCommandLineOption dumpStackOption("dump-stack", "Dump the undo stack: the step count, the index and the clean state.");
	parser.addOption(dumpStackOption);
	QCommandLineOption reconstructOption("reconstruct", "Reconstruct absent or malformed geometry on load.");
	parser.addOption(reconstructOption);
	QCommandLineOption reconstructSMOption("reconstruct-sm", "Reconstruct the absent state machine border too (with --reconstruct).");
	parser.addOption(reconstructSMOption);
	QCommandLineOption strictOption("strict", "Check the standard requirements strictly on load.");
	parser.addOption(strictOption);
	QCommandLineOption inspectOption("inspect", "Open the document read-only, as the file stores it.");
	parser.addOption(inspectOption);
	QCommandLineOption serviceOption("service", "Draw the service objects: the region borders and the coordinate origins.");
	parser.addOption(serviceOption);
	QCommandLineOption saveFormatOption("save-format",
										"The format of the saved document: cyberiada (default), "
										"yed-ostranna or yed-berloga.", "format", "cyberiada");
	parser.addOption(saveFormatOption);
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
	if (parser.isSet(noTextOption)) {
		// font metrics differ across Qt versions even with the pinned font;
		// the tests hide all text so the output is identical everywhere
		SettingsManager::instance().setShowText(false);
	}
	if (parser.isSet(textOption)) {
		// the tests hide the text by default, the text cases ask it back
		SettingsManager::instance().setShowText(true);
	}
	if (parser.isSet(inspectOption)) {
		// the GUI turns the mode on through the open dialog
		SettingsManager::instance().setInspectorMode(true);
	}
	if (parser.isSet(serviceOption)) {
		SettingsManager::instance().overrideShowServiceObjects(true);
	}

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
			// a document file is required unless a script bootstraps the
			// document from scratch (a from-scratch recorded session, whose
			// first verb is new-sm)
			if (args.size() > 1 ||
				(args.size() == 0 && !parser.isSet(scriptOption))) {
				fprintf(stderr, "batch mode requires one document file, or a --script with no file\n");
				return batchUsageError;
			}
			QString document = args.isEmpty() ? QString() : args.first();
			Cyberiada::DocumentFormat save_format = Cyberiada::formatCyberiada10;
			QString format_str = parser.value(saveFormatOption);
			if (format_str == "yed-ostranna") {
				save_format = Cyberiada::formatLegacyYEDOstranna;
			} else if (format_str == "yed-berloga") {
				save_format = Cyberiada::formatLegacyYEDBerloga16;
			} else if (format_str != "cyberiada") {
				fprintf(stderr, "unknown save format %s\n", qPrintable(format_str));
				return batchUsageError;
			}
			return runBatchMode(app, document, parser.isSet(dumpOption),
								parser.value(scriptOption), parser.value(saveOption),
								parser.value(exportOption), parser.isSet(reconstructOption),
								parser.isSet(reconstructSMOption),
								parser.isSet(strictOption), save_format,
								parser.isSet(dumpTextOption), parser.isSet(dumpStackOption),
								parser.value(dpiOption).toInt());
		}
		return runGuiMode(app);
	} catch(const QString& error) {
		app.printMessage(error);
	} catch(...) {
		app.printMessage();
	}
	return app.batchMode() ? batchInternalError : 1;
}
