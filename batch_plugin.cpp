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

#include <cstdio>

#include "batch_plugin.h"
#include "batch_driver.h"
#include "batch_session.h"
#include "cyberiadasm_dump.h"
#include "plugin_registry.h"
#include "plugin_values.h"
#include "smeditor_window.h"

bool BatchPluginRuns::failed() const
{
	for (const BatchPluginRun& run: runs) {
		if (run.result.state != PluginResult::stateDone) return true;
	}
	return false;
}

bool findBatchAction(const QString& spec, BatchPluginRun* run, QString* error)
{
	QString id = spec.section(':', 0, 0);
	QString actionId = spec.section(':', 1);
	run->plugin = PluginRegistry::instance().find(id);
	if (!run->plugin) {
		*error = QString("the plugin %1 is not found").arg(id);
		return false;
	}
	const PluginDescriptor& plugin = run->plugin->descriptor;
	if (actionId.isEmpty()) {
		if (plugin.actions.size() != 1) {
			*error = QString("the plugin %1 has several actions: name one as %1:<action>").arg(id);
			return false;
		}
		actionId = plugin.actions.first().id;
	}
	run->action = plugin.action(actionId);
	if (!run->action) {
		*error = QString("the plugin %1 has no action %2").arg(id, actionId);
		return false;
	}
	return true;
}

int runBatchAction(CyberiadaSMEditorWindow& win, BatchPluginRun* run, const QStringList& items,
				   const QStringList* sessionLines, int firstLine, const QString& sessionName,
				   bool dumping, QString* error)
{
	const PluginDescriptor& plugin = run->plugin->descriptor;
	const QString& id = plugin.id;
	const QString& actionId = run->action->id;
	PluginValues values;
	PluginDocumentPaths paths = pluginDocumentPaths(win.currentTab()->filePath());
	if (!pluginParseValues(plugin, *run->action, items, paths, &values, error)) return batchUsageError;
	// a session needs its script (EDIT-PLUG-47)
	if (run->action->session && !sessionLines) {
		*error = QString("the session action %1:%2 needs a --session-script").arg(id, actionId);
		return batchUsageError;
	}
	// stderr goes through as it comes; stdout to the dump, or through without one
	QMetaObject::Connection started = QObject::connect(&win, &CyberiadaSMEditorWindow::pluginRunStarted,
		[dumping](PluginRunner* runner) {
			QObject::connect(runner, &PluginRunner::output, [dumping](const QString& text, bool isError) {
				if (isError) {
					fputs(text.toLocal8Bit().constData(), stderr);
				} else if (!dumping) {
					fputs(text.toLocal8Bit().constData(), stdout);
					fflush(stdout);
				}
			});
		});
	if (run->action->session) {
		QString scriptError;
		bool scripted = runSessionLines(win, id, actionId, values, *sessionLines, firstLine, &run->result,
										&run->sessionDump, &scriptError);
		QObject::disconnect(started);
		if (!scripted) {
			// a failed verb fails a run that ended well, keeping the reason of one that did not
			if (run->result.state == PluginResult::stateDone) {
				run->result.state = PluginResult::stateFailed;
				run->result.error = scriptError;
			}
			*error = QString("the session script %1 failed\n%2").arg(sessionName, scriptError);
			return batchPluginError;
		}
	} else {
		run->result = win.runPluginAndWait(id, actionId, values);
		QObject::disconnect(started);
	}
	if (run->result.state != PluginResult::stateDone) {
		*error = QString("the plugin %1:%2 failed\n%3").arg(id, actionId, run->result.error);
		return batchPluginError;
	}
	return batchOK;
}

bool splitPluginItems(const QString& text, QStringList* items, QString* error)
{
	QString item;
	bool inItem = false;
	bool quoted = false;
	for (int i = 0; i < text.size(); i++) {
		QChar c = text.at(i);
		if (quoted) {
			bool escape = c == '\\' && i + 1 < text.size() &&
				(text.at(i + 1) == '"' || text.at(i + 1) == '\\');
			if (escape) item += text.at(++i);
			else if (c == '"') quoted = false;
			else item += c;
		} else if (c.isSpace()) {
			if (inItem) items->append(item);
			item.clear();
			inItem = false;
		} else {
			inItem = true;
			if (c == '"') quoted = true;
			else item += c;
		}
	}
	if (quoted) {
		*error = "a quoted value has no closing quote";
		return false;
	}
	if (inItem) items->append(item);
	return true;
}

void dumpActions(CyberiadaSMEditorWindow& win, std::ostream& os)
{
	for (const PluginEntry& entry: PluginRegistry::instance().entries()) {
		if (entry.status != PluginEntry::statusEnabled) continue;
		for (const PluginAction& action: entry.descriptor.actions) {
			QString reason = win.pluginDisabledReason(entry, action, win.currentTab());
			os << entry.descriptor.id.toStdString() << ":" << action.id.toStdString();
			if (reason.isEmpty()) os << " enabled";
			else os << " disabled: " << reason.toStdString();
			os << std::endl;
		}
	}
}

void dumpPluginRuns(const BatchPluginRuns& runs, bool plugin, bool session, std::ostream& os)
{
	// numbered from 1, as a reader counts the plugin lines of the script
	for (int n = 0; n < runs.runs.size(); n++) {
		const BatchPluginRun& run = runs.runs.at(n);
		if (plugin) {
			os << "== plugin " << n + 1 << std::endl;
			dumpPluginRun(*run.plugin, *run.action, run.result, os);
			os << "== diagnostics " << n + 1 << std::endl;
			dumpDiagnostics(run.result, os);
		}
		if (session && run.action->session) {
			os << "== session " << n + 1 << std::endl;
			for (const QString& line: run.sessionDump) os << line.toStdString() << std::endl;
		}
	}
}
