/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The git verbs of the batch script and the dump of the repository
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

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>

#include "batch_git.h"
#include "editor_document.h"
#include "repository_service.h"
#include "repository_view.h"
#include "smeditor_window.h"

// the length of a short hash, as git abbreviates it by default
static const int SHORT_HASH = 7;

bool isGitVerb(const QString& cmd)
{
	return cmd == "git" || cmd == "repository";
}

// repository layout tree|groups, filter all|changed, ignored on|off: the state of the dock
// (EDIT-GIT-5), not stored
static bool runRepositoryVerb(CyberiadaSMEditorWindow* win, const QStringList& tokens, QString* error)
{
	RepositoryView* view = win->repositoryView();
	if (tokens.size() != 3) {
		*error = "repository needs layout, filter or ignored and its value";
		return false;
	}
	const QString& what = tokens.at(1);
	const QString& value = tokens.at(2);
	if (what == "layout" && (value == "tree" || value == "groups")) {
		view->setLayoutMode(value == "tree" ? RepositoryModel::layoutTree : RepositoryModel::layoutGroups);
		return true;
	}
	if (what == "filter" && (value == "all" || value == "changed")) {
		view->setChangedOnly(value == "changed");
		return true;
	}
	if (what == "ignored" && (value == "on" || value == "off")) {
		view->setIgnored(value == "on");
		RepositoryService* service = win->repositoryOf(win->currentTab());
		if (service) {
			service->setIgnored(value == "on");
			service->refresh(true);
		}
		return true;
	}
	*error = QString("repository %1 %2: layout tree|groups, filter all|changed, ignored on|off").arg(what, value);
	return false;
}

bool runGitVerb(CyberiadaSMEditorWindow* win, const QString& line, QString* error)
{
	QStringList tokens = line.trimmed().split(QRegularExpression("\\s+"));
	if (tokens.first() == "repository") return runRepositoryVerb(win, tokens, error);
	if (tokens.size() < 2) {
		*error = "git needs a verb";
		return false;
	}
	const QString& verb = tokens.at(1);
	EditorDocument* doc = win->currentTab();
	RepositoryService* service = win->repositoryOf(doc, verb == "refresh");
	if (!service) {
		*error = win->noRepositoryReason(doc);
		return false;
	}
	if (verb == "refresh") {
		if (tokens.size() != 2) {
			*error = "git refresh takes no argument";
			return false;
		}
		service->refresh(true);
		if (!service->hasSnapshot()) {
			*error = "the status of the repository was not read";
			return false;
		}
		return true;
	}
	*error = "unknown git verb " + verb;
	return false;
}

void dumpRepository(CyberiadaSMEditorWindow* win, std::ostream& os)
{
	EditorDocument* doc = win->currentTab();
	RepositoryService* service = win->repositoryOf(doc);
	if (!service) {
		os << "none " << win->noRepositoryReason(doc).toStdString() << std::endl;
		return;
	}
	if (!service->hasSnapshot()) service->refresh(true);
	const RepositorySnapshot& s = service->snapshot();
	os << "root " << QFileInfo(s.root).fileName().toStdString() << std::endl;
	if (s.detached()) {
		os << "detached " << s.oid.left(SHORT_HASH).toStdString() << std::endl;
	} else {
		os << "branch " << s.branch.toStdString() << (s.unborn() ? " (no commits)" : "") << std::endl;
	}
	if (!s.upstream.isEmpty()) {
		os << "upstream " << s.upstream.toStdString() << " ahead " << s.ahead << " behind " << s.behind
		   << std::endl;
	}
	if (!s.state.isEmpty()) os << "state " << s.state.toStdString() << std::endl;
	for (const QString& branch : s.branches) {
		os << (branch == s.branch ? "* " : "  ") << branch.toStdString() << std::endl;
	}
	// the open files of the work tree and their unsaved edits
	QHash<QString, bool> open;
	QDir root(s.root);
	for (int i = 0; i < win->documentCount(); i++) {
		EditorDocument* tab = win->tabAt(i);
		if (!tab->hasFile() || win->gitFilePath(tab) != tab->filePath()) continue;
		QString relative = root.relativeFilePath(QFileInfo(tab->filePath()).absoluteFilePath());
		open[relative] = open.value(relative) || tab->hasEdits();
	}
	for (const StatusEntry& e : s.entries) {
		os << QString(e.index).toStdString() << QString(e.tree).toStdString() << " " << e.path.toStdString();
		if (!e.oldPath.isEmpty()) os << " from " << e.oldPath.toStdString();
		if (open.contains(e.path)) os << (open.value(e.path) ? " open unsaved" : " open");
		os << std::endl;
	}
}
