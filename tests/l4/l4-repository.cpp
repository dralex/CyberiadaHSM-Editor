/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process tests of the Repository dock
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

#include <QtTest>
#include <QAbstractItemModelTester>
#include <QCheckBox>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QToolBar>
#include <QToolButton>
#include <QTabBar>
#include <QTreeView>
#include "smeditor_window.h"
#include "cyberiadasm_model.h"
#include "dialogs/preferences_dialog.h"
#include "code_editor.h"
#include "git_process.h"
#include "repository_icons.h"
#include "repository_model.h"
#include "repository_service.h"
#include "repository_view.h"
#include "settings_manager.h"
#include "history_view.h"
#include "plugin_dock.h"
#include "comparison_document.h"
#include "dialogs/commit_dialog.h"
#include "dialogs/revision_dialog.h"
#include <QComboBox>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>

// the Repository dock (EDIT-GIT-3..7, EDIT-UI-6, docs/GIT.md): the model in both layouts,
// the dock following the active tab, the Git page of the Preferences
class TestRepository: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void test_model_tree();
	void test_file_kinds();
	void test_marker_icons();
	void test_model_groups();
	void test_model_changed_only();
	void test_model_files();
	void test_model_marks();
	void test_dock_follows_tab();
	void test_open_row();
	void test_view_state();
	void test_toolbar_stored();
	void test_preferences();
	void test_read_after_save();
	void test_commit_dialog();
	void test_revision_dialog();
	void test_git_menu();
	void test_stage_output();
	void test_discard_asks();
	void test_history_dock();
	void test_branch_label();
	void test_branch_menu();
	void test_docks_right();

private:
	static RepositorySnapshot sample();
	static StatusEntry entry(const QString& path, char index, char tree, const QString& old = QString());
	static int marker(int places, int glyph = 0);
	static QStringList names(const RepositoryModel& model, const QModelIndex& parent = QModelIndex());
	QString makeRepository(const QString& name);
	void git(const QString& folder, const QStringList& args);
	void write(const QString& path, const QByteArray& bytes);

	QTemporaryDir home;
	bool haveGit = false;
};

void TestRepository::initTestCase()
{
	QVERIFY(home.isValid());
	write(home.filePath(".gitconfig"), QByteArray());
	qputenv("HOME", home.path().toUtf8());
	qputenv("XDG_CONFIG_HOME", home.filePath(".config").toUtf8());
	qputenv("GIT_CONFIG_NOSYSTEM", "1");
	qputenv("GIT_CONFIG_GLOBAL", home.filePath(".gitconfig").toUtf8());
	qputenv("GIT_CEILING_DIRECTORIES", home.path().toUtf8());
	qputenv("GIT_AUTHOR_NAME", "Cyberiada Test");
	qputenv("GIT_AUTHOR_EMAIL", "test@localhost");
	qputenv("GIT_COMMITTER_NAME", "Cyberiada Test");
	qputenv("GIT_COMMITTER_EMAIL", "test@localhost");
	SettingsManager::instance().setGitEnabled(true);
	SettingsManager::instance().setGitProgram(QString());
	SettingsManager::instance().setShowRepository(true);
	haveGit = !GitProcess::program().isEmpty();
}

void TestRepository::write(const QString& path, const QByteArray& bytes)
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile file(path);
	QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
	file.write(bytes);
}

void TestRepository::git(const QString& folder, const QStringList& args)
{
	GitResult r = GitProcess::run(folder, args);
	QVERIFY2(r.ok(), qPrintable(args.join(' ') + ": " + r.message()));
}

QString TestRepository::makeRepository(const QString& name)
{
	QString root = home.filePath(name);
	QDir().mkpath(root);
	git(root, QStringList() << "init" << "-q");
	git(root, QStringList() << "symbolic-ref" << "HEAD" << "refs/heads/main");
	QFile::copy("diagrams/hierarchy.graphml", QDir(root).filePath("h.graphml"));
	QFile::setPermissions(QDir(root).filePath("h.graphml"), QFile::ReadOwner | QFile::WriteOwner);
	QFile::copy("texts/notes.md", QDir(root).filePath("docs/notes.md"));
	write(QDir(root).filePath("docs/notes.md"), "# Notes\n");
	write(QDir(root).filePath("deep/inner/a.txt"), "a\n");
	git(root, QStringList() << "add" << "-A");
	git(root, QStringList() << "commit" << "-q" << "-m" << "first");
	return QDir::cleanPath(QFileInfo(root).canonicalFilePath());
}

StatusEntry TestRepository::entry(const QString& path, char index, char tree, const QString& old)
{
	StatusEntry e;
	e.path = path;
	e.index = QLatin1Char(index);
	e.tree = QLatin1Char(tree);
	e.oldPath = old;
	return e;
}

// the rows of every kind: unchanged, modified, staged and modified again, renamed,
// conflicted, untracked, ignored file and folder
RepositorySnapshot TestRepository::sample()
{
	RepositorySnapshot s;
	s.root = "/work";
	s.branch = "main";
	s.entries << entry("README.md", '.', '.')
	          << entry("b/beta.graphml", 'M', 'M')
	          << entry("b/alpha.graphml", '.', '.')
	          << entry("c/z.txt", 'R', '.', "c/y.txt")
	          << entry("conflict.txt", 'U', 'U')
	          << entry("d/e/deep.md", '.', '.')
	          << entry("light.graphml", '.', 'M')
	          << entry("new.txt", '.', '?')
	          << entry("build/", '.', '!')
	          << entry("run.log", '.', '!');
	std::sort(s.entries.begin(), s.entries.end(), [](const StatusEntry& a, const StatusEntry& b) { return a.path < b.path; });
	return s;
}

QStringList TestRepository::names(const RepositoryModel& model, const QModelIndex& parent)
{
	QStringList list;
	for (int i = 0; i < model.rowCount(parent); i++) {
		list << model.data(model.index(i, RepositoryModel::columnName, parent)).toString();
	}
	return list;
}

// EDIT-GIT-4/5: the tree: the folders first, the changed before the unchanged, then by name
void TestRepository::test_model_tree()
{
	RepositoryModel model;
	QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
	model.setSnapshot(sample());
	QCOMPARE(names(model), QStringList() << "b" << "c" << "d" << "conflict.txt" << "light.graphml" << "new.txt"
	                                     << "README.md" << "build/" << "run.log");
	QModelIndex b = model.index(0, RepositoryModel::columnName);
	QCOMPARE(names(model, b), QStringList() << "beta.graphml" << "alpha.graphml");
	// one column: the letters are markers on the icons (EDIT-GIT-4, 0.81)
	QCOMPARE(model.columnCount(), 1);
	// a folder holding a change has the places of its changes, one without has none
	QCOMPARE(model.data(b, RepositoryModel::MarkerRole).toInt(),
	         marker(StatusMarker::placeIndex | StatusMarker::placeTree));
	QVERIFY(model.data(b, Qt::ToolTipRole).toString().contains("Staged and not staged changes inside"));
	QModelIndex d = model.index(2, RepositoryModel::columnName);
	QCOMPARE(model.data(d, RepositoryModel::MarkerRole).toInt(), 0);
	QVERIFY(!model.data(d, Qt::DecorationRole).value<QIcon>().isNull());
	// staged and changed again: both halves, the letters in words
	QModelIndex beta = model.indexOf("b/beta.graphml");
	QCOMPARE(model.data(beta, RepositoryModel::MarkerRole).toInt(),
	         marker(StatusMarker::placeIndex | StatusMarker::placeTree));
	QCOMPARE(model.data(beta, Qt::ToolTipRole).toString(),
	         QString("b/beta.graphml\nStaged: modified\nNot staged: modified"));
	QCOMPARE(model.data(model.indexOf("light.graphml"), RepositoryModel::MarkerRole).toInt(),
	         marker(StatusMarker::placeTree));
	// a rename tells its old path
	QModelIndex z = model.indexOf("c/z.txt");
	QCOMPARE(model.data(z, Qt::ToolTipRole).toString(), QString("c/z.txt\nRenamed from c/y.txt\nStaged: renamed"));
	QCOMPARE(model.data(z, RepositoryModel::MarkerRole).toInt(),
	         marker(StatusMarker::placeIndex, StatusMarker::glyphRenamed));
	QCOMPARE(model.data(z, RepositoryModel::PathRole).toString(), QString("c/z.txt"));
	QCOMPARE(model.data(model.indexOf("conflict.txt"), RepositoryModel::MarkerRole).toInt(),
	         marker(StatusMarker::placeConflict, StatusMarker::glyphConflict));
	QCOMPARE(model.data(model.indexOf("new.txt"), RepositoryModel::MarkerRole).toInt(),
	         marker(StatusMarker::placeTree, StatusMarker::glyphUntracked));
	QCOMPARE(model.data(model.indexOf("new.txt"), Qt::ToolTipRole).toString(), QString("new.txt\nUntracked"));
	// unchanged and ignored: no marker
	QCOMPARE(model.data(model.indexOf("README.md"), RepositoryModel::MarkerRole).toInt(), 0);
	QCOMPARE(model.data(model.indexOf("run.log"), RepositoryModel::MarkerRole).toInt(), 0);
	QCOMPARE(model.data(model.indexOf("run.log"), Qt::ToolTipRole).toString(), QString("run.log\nIgnored"));
	QCOMPARE(model.headerData(RepositoryModel::columnName, Qt::Horizontal).toString(), QString("Name"));
}

int TestRepository::marker(int places, int glyph)
{
	StatusMarker m;
	m.places = places;
	m.glyph = StatusMarker::Glyph(glyph);
	return m.key();
}

// EDIT-GIT-4: the kind of a file by its name
void TestRepository::test_file_kinds()
{
	QCOMPARE(fileKind("a/b.graphml"), FileKind::diagram);
	QCOMPARE(fileKind("B.GRAPHML"), FileKind::diagram);
	QCOMPARE(fileKind("notes.md"), FileKind::markdown);
	QCOMPARE(fileKind("x/notes.markdown"), FileKind::markdown);
	for (const char* code : {"blink.ino", "a.c", "a.hpp", "tool.py", "app.js", "Robot.cs", "build.sh",
	                         "Makefile", "sub/CMakeLists.txt", "Dockerfile", "data.json", "ci.yml"}) {
		QCOMPARE(fileKind(code), FileKind::code);
	}
	for (const char* text : {"notes.txt", "run.log", "LICENSE", ".gitignore", "image.svg", "dir.d/file"}) {
		QCOMPARE(fileKind(text), FileKind::text);
	}
	QCOMPARE(fileKind("build/"), FileKind::folder);
}

// EDIT-GIT-4: the marker disk: green staged, orange not staged, halves for both, red conflicted
void TestRepository::test_marker_icons()
{
	const int side = 64;
	auto pixel = [side](const QIcon& icon, qreal x, qreal y) {
		return QColor(icon.pixmap(side, side).toImage().pixel(int(x * side), int(y * side))).rgb() & 0xffffff;
	};
	const QRgb green = 0x2e7d32, orange = 0xef6c00, red = 0xc62828;
	StatusMarker both;
	both.places = StatusMarker::placeIndex | StatusMarker::placeTree;
	// the disk is the lower right 0.6 of the side: its centre at 0.7, the halves left and right
	QIcon icon = markedIcon(FileKind::diagram, both, false);
	QCOMPARE(pixel(icon, 0.6, 0.7), green);
	QCOMPARE(pixel(icon, 0.8, 0.7), orange);
	StatusMarker conflict;
	conflict.places = StatusMarker::placeConflict;
	QCOMPARE(pixel(markedIcon(FileKind::text, conflict, false), 0.6, 0.7), red);
	// a group: the disk alone in the middle
	StatusMarker staged;
	staged.places = StatusMarker::placeIndex;
	QCOMPARE(pixel(markerIcon(staged), 0.5, 0.5), green);
	QCOMPARE(icon.pixmap(side, side).toImage().pixel(0, 0) >> 24, 0u);
	// an ignored file: the picture greyed, no marker
	QVERIFY(markedIcon(FileKind::code, StatusMarker(), true).pixmap(side, side).toImage() !=
	        markedIcon(FileKind::code, StatusMarker(), false).pixmap(side, side).toImage());
}

// EDIT-GIT-5: the groups: the changes only, a staged and changed file in both
void TestRepository::test_model_groups()
{
	RepositoryModel model;
	QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
	model.setSnapshot(sample());
	model.setLayout(RepositoryModel::layoutGroups);
	QCOMPARE(names(model), QStringList() << "Conflicts (1)" << "Staged (2)" << "Changes (2)" << "Untracked (1)"
	                                     << "Ignored (2)");
	QCOMPARE(names(model, model.index(1, RepositoryModel::columnName)),
	         QStringList() << "b/beta.graphml" << "c/z.txt");
	QCOMPARE(names(model, model.index(2, RepositoryModel::columnName)),
	         QStringList() << "b/beta.graphml" << "light.graphml");
	QCOMPARE(model.data(model.index(1, RepositoryModel::columnName), RepositoryModel::KindRole).toInt(),
	         int(RepositoryModel::kindGroup));
	// the icon of a group is its marker alone: the legend of the markers (EDIT-GIT-5)
	QCOMPARE(model.data(model.index(0, 0), RepositoryModel::MarkerRole).toInt(),
	         marker(StatusMarker::placeConflict, StatusMarker::glyphConflict));
	QCOMPARE(model.data(model.index(1, 0), RepositoryModel::MarkerRole).toInt(), marker(StatusMarker::placeIndex));
	QCOMPARE(model.data(model.index(2, 0), RepositoryModel::MarkerRole).toInt(), marker(StatusMarker::placeTree));
	QCOMPARE(model.data(model.index(3, 0), RepositoryModel::MarkerRole).toInt(),
	         marker(StatusMarker::placeTree, StatusMarker::glyphUntracked));
	QCOMPARE(model.data(model.index(4, 0), RepositoryModel::MarkerRole).toInt(), marker(StatusMarker::placeIgnored));
	// without the ignored files no Ignored group
	RepositorySnapshot plain = sample();
	plain.entries.erase(std::remove_if(plain.entries.begin(), plain.entries.end(),
	                                   [](const StatusEntry& e) { return e.ignored(); }), plain.entries.end());
	model.setSnapshot(plain);
	QVERIFY(!names(model).contains("Ignored (2)"));
	model.setLayout(RepositoryModel::layoutTree);
	QCOMPARE(model.layout(), RepositoryModel::layoutTree);
}

// EDIT-GIT-5: Changed only keeps the changed files and their folders
void TestRepository::test_model_changed_only()
{
	RepositoryModel model;
	QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
	model.setSnapshot(sample());
	model.setChangedOnly(true);
	QCOMPARE(names(model), QStringList() << "b" << "c" << "conflict.txt" << "light.graphml" << "new.txt");
	QCOMPARE(names(model, model.index(0, RepositoryModel::columnName)), QStringList() << "beta.graphml");
	model.setChangedOnly(false);
	QVERIFY(names(model).contains("README.md"));
}

// EDIT-GIT-8: a folder or a group stands for its files, each file once
void TestRepository::test_model_files()
{
	RepositoryModel model;
	model.setSnapshot(sample());
	QModelIndex b = model.index(0, RepositoryModel::columnName);
	QCOMPARE(model.files(QModelIndexList() << b), QStringList() << "b/beta.graphml" << "b/alpha.graphml");
	QModelIndex beta = model.indexOf("b/beta.graphml");
	QCOMPARE(model.files(QModelIndexList() << b << beta), QStringList() << "b/beta.graphml" << "b/alpha.graphml");
	QVERIFY(model.entry(beta) && model.entry(beta)->staged());
	QVERIFY(!model.entry(b));
	QVERIFY(!model.indexOf("missing").isValid());
	QCOMPARE(model.folderIndex(model.folderPath(b)), b);
	model.setLayout(RepositoryModel::layoutGroups);
	// a file in two groups is one file
	QModelIndexList groups;
	for (int i = 0; i < model.rowCount(); i++) groups << model.index(i, RepositoryModel::columnName);
	QStringList all = model.files(groups);
	QCOMPARE(all.count("b/beta.graphml"), 1);
}

// EDIT-GIT-4: the files open in tabs are bold, one with unsaved edits is marked
void TestRepository::test_model_marks()
{
	RepositoryModel model;
	QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
	model.setSnapshot(sample());
	QHash<QString, bool> open;
	open.insert("light.graphml", true);
	open.insert("README.md", false);
	model.setOpenFiles(open);
	QCOMPARE(model.data(model.indexOf("light.graphml")).toString(), QString("light.graphml ") + QChar(0x25CF));
	QCOMPARE(model.data(model.indexOf("README.md")).toString(), QString("README.md"));
	QVERIFY(model.data(model.indexOf("light.graphml"), Qt::FontRole).value<QFont>().bold());
	QVERIFY(model.data(model.indexOf("README.md"), Qt::FontRole).value<QFont>().bold());
	QVERIFY(!model.data(model.indexOf("new.txt"), Qt::FontRole).isValid());
	QVERIFY(model.data(model.indexOf("light.graphml"), Qt::ToolTipRole).toString().endsWith("\nOpen, unsaved edits"));
	QVERIFY(model.data(model.indexOf("README.md"), Qt::ToolTipRole).toString().endsWith("\nOpen"));
}

// EDIT-GIT-2/3: the dock shows the work tree of the active tab, a line without one
void TestRepository::test_dock_follows_tab()
{
	if (!haveGit) QSKIP("no git 2.23 or later");
	QString first = makeRepository("first");
	QString second = makeRepository("second");
	QString outside = home.filePath("outside");
	QDir().mkpath(outside);
	QFile::copy("diagrams/hierarchy.graphml", QDir(outside).filePath("o.graphml"));
	CyberiadaSMEditorWindow window;
	window.resize(1200, 800);
	window.show();
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	RepositoryView* view = window.repositoryView();
	QVERIFY(view && window.repositoryDock && !window.repositoryDock->isHidden());
	QVERIFY(window.actionRepository->isEnabled());
	// an untitled tab
	QVERIFY(view->showsLine());
	QCOMPARE(view->lineText(), QString("The document has no file"));
	QString error;
	QVERIFY(window.openFile(QDir(first).filePath("h.graphml"), &error));
	QVERIFY(!view->showsLine());
	QCOMPARE(view->root(), first);
	QTRY_VERIFY_WITH_TIMEOUT(view->model()->indexOf("h.graphml").isValid(), 10000);
	QCOMPARE(window.repositoryDock->windowTitle(), QString("Repository"));
	QCOMPARE(view->heading(), QString("first"));
	QCOMPARE(view->branchButton()->text(), QString("main"));
	// the file of the tab is the current row
	QCOMPARE(view->tree()->currentIndex().data(RepositoryModel::PathRole).toString(), QString("h.graphml"));
	// the folder and the branch, then the tools in a row of their own, icons only; one column
	// (EDIT-GIT-3/4)
	QVERIFY(view->toolBar()->parentWidget() != view->branchButton()->parentWidget());
	QVERIFY(view->tree()->isHeaderHidden());
	for (QAction* action : view->toolBar()->actions()) {
		QVERIFY(action->isSeparator() || !action->icon().isNull());
	}
	QCOMPARE(view->toolBar()->toolButtonStyle(), Qt::ToolButtonIconOnly);
	QCOMPARE(view->findChild<QLabel*>("repositoryHeading")->toolTip(), QDir::toNativeSeparators(first));
	EditorDocument* inFirst = window.currentTab();
	QVERIFY(window.openFile(QDir(second).filePath("docs/notes.md"), &error));
	QCOMPARE(view->root(), second);
	QTRY_VERIFY_WITH_TIMEOUT(view->model()->indexOf("docs/notes.md").isValid(), 10000);
	QVERIFY(window.openFile(QDir(outside).filePath("o.graphml"), &error));
	QVERIFY(view->showsLine());
	QCOMPARE(view->lineText(), QString("o.graphml is not in a git repository"));
	QCOMPARE(view->heading(), QString());
	window.setCurrentDocument(inFirst);
	QCOMPARE(view->root(), first);
	QVERIFY(view->model()->indexOf("h.graphml").isValid());
}

// EDIT-GIT-7: a file row activated opens the file, a folder row opens and closes
void TestRepository::test_open_row()
{
	if (!haveGit) QSKIP("no git 2.23 or later");
	QString root = makeRepository("open-row");
	CyberiadaSMEditorWindow window;
	window.resize(1200, 800);
	window.show();
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	QString error;
	QVERIFY(window.openFile(QDir(root).filePath("h.graphml"), &error));
	RepositoryView* view = window.repositoryView();
	QTRY_VERIFY_WITH_TIMEOUT(view->model()->indexOf("docs/notes.md").isValid(), 10000);
	int tabs = window.documentCount();
	QModelIndex notes = view->model()->indexOf("docs/notes.md");
	emit view->tree()->activated(notes);
	QCOMPARE(window.documentCount(), tabs + 1);
	QVERIFY(window.currentText());
	QCOMPARE(QFileInfo(window.currentText()->filePath()).fileName(), QString("notes.md"));
	// a second activation finds the open tab
	emit view->tree()->activated(view->model()->indexOf("docs/notes.md"));
	QCOMPARE(window.documentCount(), tabs + 1);
	QModelIndex docs = view->model()->folderIndex("docs/");
	QVERIFY(docs.isValid());
	bool open = view->tree()->isExpanded(docs);
	emit view->tree()->activated(docs);
	QCOMPARE(view->tree()->isExpanded(docs), !open);
}

// EDIT-GIT-3: a work tree shown again has its folders as they were
void TestRepository::test_view_state()
{
	if (!haveGit) QSKIP("no git 2.23 or later");
	QString first = makeRepository("state-first");
	QString second = makeRepository("state-second");
	CyberiadaSMEditorWindow window;
	window.resize(1200, 800);
	window.show();
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	QString error;
	QVERIFY(window.openFile(QDir(first).filePath("h.graphml"), &error));
	RepositoryView* view = window.repositoryView();
	QTRY_VERIFY_WITH_TIMEOUT(view->model()->folderIndex("deep/").isValid(), 10000);
	// unchanged folders start closed; one opened by the user stays open
	QModelIndex deep = view->model()->folderIndex("deep/");
	QVERIFY(!view->tree()->isExpanded(deep));
	view->tree()->setExpanded(deep, true);
	EditorDocument* tab = window.currentTab();
	QVERIFY(window.openFile(QDir(second).filePath("h.graphml"), &error));
	QTRY_VERIFY_WITH_TIMEOUT(view->root() == second && view->model()->folderIndex("deep/").isValid(), 10000);
	QVERIFY(!view->tree()->isExpanded(view->model()->folderIndex("deep/")));
	window.setCurrentDocument(tab);
	QCOMPARE(view->root(), first);
	QVERIFY(view->tree()->isExpanded(view->model()->folderIndex("deep/")));
	// a changed file opens its folders when they are first shown
	write(QDir(second).filePath("deep/inner/a.txt"), "changed\n");
	RepositoryService* service = RepositoryService::forRoot(second);
	service->refresh(true);
	QVERIFY(service->snapshot().entry("deep/inner/a.txt")->modified());
}

// EDIT-GIT-5: the layout and the filters are the user's, stored
void TestRepository::test_toolbar_stored()
{
	CyberiadaSMEditorWindow window;
	RepositoryView* view = window.repositoryView();
	view->groupsAction()->trigger();
	QCOMPARE(view->model()->layout(), RepositoryModel::layoutGroups);
	QCOMPARE(SettingsManager::instance().getRepositoryLayout(), int(RepositoryModel::layoutGroups));
	view->changedOnlyAction()->trigger();
	QVERIFY(view->model()->changedOnly());
	QVERIFY(SettingsManager::instance().getRepositoryChangedOnly());
	view->ignoredAction()->trigger();
	QVERIFY(SettingsManager::instance().getRepositoryIgnored());
	{
		// a new window starts as the user left the dock
		CyberiadaSMEditorWindow again;
		QCOMPARE(again.repositoryView()->model()->layout(), RepositoryModel::layoutGroups);
		QVERIFY(again.repositoryView()->model()->changedOnly());
		QVERIFY(again.repositoryView()->ignored());
	}
	view->treeAction()->trigger();
	view->changedOnlyAction()->trigger();
	view->ignoredAction()->trigger();
	QCOMPARE(SettingsManager::instance().getRepositoryLayout(), int(RepositoryModel::layoutTree));
	QVERIFY(!SettingsManager::instance().getRepositoryChangedOnly());
	QVERIFY(!SettingsManager::instance().getRepositoryIgnored());
}

// EDIT-GIT-1: the Git page: the program found, Use git off hides the dock
void TestRepository::test_preferences()
{
	CyberiadaSMEditorWindow window;
	{
		PreferencesDialog dialog(&window);
		QVERIFY(dialog.gitEnabledBox()->isChecked());
		QCOMPARE(dialog.gitProgramEdit()->text(), QString());
		if (haveGit) QVERIFY(dialog.gitFoundLabel()->text().startsWith("git version"));
		dialog.gitProgramEdit()->setText(home.filePath("no-git"));
		QCOMPARE(dialog.gitFoundLabel()->text(), QString("git is not found"));
		dialog.gitProgramEdit()->clear();
		dialog.gitEnabledBox()->setChecked(false);
		QMetaObject::invokeMethod(&dialog, "slotApplySettings");
	}
	QVERIFY(!SettingsManager::instance().getGitEnabled());
	QVERIFY(window.repositoryDock->isHidden());
	QVERIFY(!window.actionRepository->isEnabled());
	QCOMPARE(window.actionRepository->toolTip(), QString("git is off"));
	SettingsManager::instance().setGitEnabled(true);
	if (haveGit) {
		QVERIFY(window.actionRepository->isEnabled());
		QVERIFY(!window.repositoryDock->isHidden());
	}
}

// EDIT-GIT-6: a save reads the status again in the background
void TestRepository::test_read_after_save()
{
	if (!haveGit) QSKIP("no git 2.23 or later");
	QString root = makeRepository("read-after-save");
	CyberiadaSMEditorWindow window;
	window.resize(1200, 800);
	window.show();
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	QString error;
	QVERIFY(window.openFile(QDir(root).filePath("docs/notes.md"), &error));
	RepositoryView* view = window.repositoryView();
	QTRY_VERIFY_WITH_TIMEOUT(view->model()->indexOf("docs/notes.md").isValid(), 10000);
	TextDocument* text = window.currentText();
	QVERIFY(text);
	text->editor()->insertPlainText("more\n");
	// the edits are marked before they are saved
	QTRY_VERIFY(view->model()->data(view->model()->indexOf("docs/notes.md")).toString().endsWith(QChar(0x25CF)));
	window.actionSave->trigger();
	QTRY_VERIFY_WITH_TIMEOUT(view->model()->entry(view->model()->indexOf("docs/notes.md")) &&
	                         view->model()->entry(view->model()->indexOf("docs/notes.md"))->modified(), 10000);
	QCOMPARE(view->model()->data(view->model()->indexOf("docs/notes.md")).toString(), QString("notes.md"));
}

// EDIT-GIT-14: Commit greyed without a message, nothing staged or with a conflict; Amend
// fills the last message; a failure keeps the dialog open with git's message
void TestRepository::test_commit_dialog()
{
	RepositorySnapshot s;
	s.branch = "devel";
	s.oid = "0123456789abcdef";
	s.entries << entry("a.txt", 'M', '.') << entry("b.txt", '.', 'M') << entry("new.txt", 'R', '.', "old.txt");
	QString committed;
	int calls = 0;
	CommitDialog::Commit commit = [&](const QString& message, bool amend, QString* error) {
		calls++;
		if (calls == 1) {
			*error = "hook refused";
			return false;
		}
		committed = message + (amend ? " amend" : "");
		return true;
	};
	CommitDialog dialog(s, QStringList() << "blink.graphml", QString(), commit, []() { return QString("last one"); });
	QCOMPARE(dialog.windowTitle(), QString("Commit to devel"));
	QCOMPARE(dialog.stagedList()->count(), 2);
	QCOMPARE(dialog.stagedList()->item(0)->text(), QString("M  a.txt"));
	QVERIFY(dialog.stagedList()->item(1)->text().contains("old.txt"));
	QVERIFY(dialog.notes().contains("blink.graphml has unsaved edits"));
	QVERIFY(!dialog.commitButton()->isEnabled());
	dialog.messageEdit()->setPlainText("   ");
	QVERIFY(!dialog.commitButton()->isEnabled());
	dialog.messageEdit()->setPlainText("night mode\n\nthe body");
	QVERIFY(dialog.commitButton()->isEnabled());
	dialog.commitButton()->click();
	QCOMPARE(calls, 1);
	QVERIFY(dialog.notes().contains("hook refused"));
	QCOMPARE(dialog.result(), int(QDialog::Rejected));
	dialog.commitButton()->click();
	QCOMPARE(committed, QString("night mode\n\nthe body"));
	QCOMPARE(dialog.result(), int(QDialog::Accepted));

	// nothing staged: Amend alone enables Commit and fills the message
	RepositorySnapshot empty;
	empty.branch = "main";
	empty.oid = "0123456789abcdef";
	CommitDialog amend(empty, QStringList(), QString(), commit, []() { return QString("last one"); });
	QVERIFY(!amend.commitButton()->isEnabled());
	amend.amendBox()->setChecked(true);
	QCOMPARE(amend.message(), QString("last one"));
	QVERIFY(amend.commitButton()->isEnabled());
	// a conflict keeps Commit off; a detached HEAD names the commit
	RepositorySnapshot conflict;
	conflict.oid = "fedcba9876543210";
	conflict.entries << entry("c.txt", 'U', 'U') << entry("a.txt", 'M', '.');
	CommitDialog stopped(conflict, QStringList(), QString("message"), commit, CommitDialog::LastMessage());
	QCOMPARE(stopped.windowTitle(), QString("Commit at fedcba9"));
	QVERIFY(!stopped.commitButton()->isEnabled());
	QVERIFY(stopped.notes().contains("conflict"));
}

// EDIT-GIT-13: HEAD first, a commit chosen gives its hash, a revision refused keeps the dialog
void TestRepository::test_revision_dialog()
{
	LogEntry e;
	e.hash = "0123456789abcdef0123456789abcdef01234567";
	e.shortHash = "0123456";
	e.subject = "night mode";
	RevisionDialog dialog("h.graphml", QVector<LogEntry>() << e, [](const QString& revision, QString* error) {
		if (revision == "nope") {
			*error = "nope is not a revision";
			return false;
		}
		return true;
	});
	QCOMPARE(dialog.revisionBox()->count(), 2);
	QCOMPARE(dialog.revision(), QString("HEAD"));
	dialog.revisionBox()->setCurrentIndex(1);
	QCOMPARE(dialog.revision(), e.hash);
	dialog.revisionBox()->setEditText("HEAD~1");
	QCOMPARE(dialog.revision(), QString("HEAD~1"));
	dialog.revisionBox()->setEditText("nope");
	QPushButton* compare = nullptr;
	for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
		if (button->text() == "Compare") compare = button;
	}
	QVERIFY(compare);
	compare->click();
	QCOMPARE(dialog.errorText(), QString("nope is not a revision"));
	QCOMPARE(dialog.result(), int(QDialog::Rejected));
}

// EDIT-GIT-16: the Git menu between View and Plugins, its keys, greyed without a repository
void TestRepository::test_git_menu()
{
	CyberiadaSMEditorWindow window;
	QMenu* git = window.gitMenu();
	QVERIFY(git);
	QStringList titles;
	for (QAction* top : window.QMainWindow::menuBar()->actions()) titles << top->text().remove('&');
	int at = titles.indexOf("Git");
	QVERIFY(at > 0);
	QCOMPARE(titles.at(at - 1), QString("View"));
	QCOMPARE(titles.at(at + 1), QString("Plugins"));
	QCOMPARE(window.actionGitStage->shortcut(), QKeySequence("Ctrl+Alt+A"));
	QCOMPARE(window.actionGitCompareHead->shortcut(), QKeySequence("Ctrl+D"));
	QCOMPARE(window.actionGitCommit->shortcut(), QKeySequence("Ctrl+Shift+K"));
	// an untitled tab has no repository
	QVERIFY(!window.actionGitStage->isEnabled());
	QCOMPARE(window.actionGitStage->toolTip(), QString("The document has no file"));
	if (!haveGit) return;
	QString root = makeRepository("menu");
	QString error;
	QVERIFY(window.openFile(QDir(root).filePath("h.graphml"), &error));
	QVERIFY(window.actionGitStage->isEnabled());
	QVERIFY(window.actionGitCompareHead->isEnabled());
	QVERIFY(window.actionGitCommit->isEnabled());
}

// EDIT-GIT-9/17: Stage of the menu writes the command and its result in the Git output
void TestRepository::test_stage_output()
{
	if (!haveGit) QSKIP("no git 2.23 or later");
	QString root = makeRepository("stage-output");
	write(QDir(root).filePath("docs/notes.md"), "# Changed\n");
	CyberiadaSMEditorWindow window;
	QString error;
	QVERIFY(window.openFile(QDir(root).filePath("docs/notes.md"), &error));
	window.actionGitStage->trigger();
	QPlainTextEdit* output = window.pluginDock()->output("git");
	QVERIFY(output);
	QVERIFY(output->toPlainText().contains("== git add -- docs/notes.md"));
	RepositoryService* service = window.shownRepository();
	QVERIFY(service);
	service->refresh(true);
	QVERIFY(service->snapshot().entry("docs/notes.md")->staged());
	// a failure is written too: the index of an untracked file cannot be restored
	QVERIFY(!window.gitUnstage(QStringList() << "missing.txt", &error));
	QVERIFY(!error.isEmpty());
	QVERIFY(output->toPlainText().contains("== git restore --staged -- missing.txt"));
}

// EDIT-GIT-10: Discard asks first, No the default: No changes nothing
void TestRepository::test_discard_asks()
{
	if (!haveGit) QSKIP("no git 2.23 or later");
	QString root = makeRepository("discard-asks");
	write(QDir(root).filePath("docs/notes.md"), "# Changed\n");
	CyberiadaSMEditorWindow window;
	window.show();
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	QString error;
	QVERIFY(window.openFile(QDir(root).filePath("docs/notes.md"), &error));
	bool asked = false;
	QTimer::singleShot(50, [&asked]() {
		QMessageBox* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
		if (!box) return;
		asked = box->defaultButton() == box->button(QMessageBox::No) && box->text().contains("docs/notes.md");
		box->button(QMessageBox::No)->click();
	});
	window.actionGitDiscard->trigger();
	QVERIFY(asked);
	QFile file(QDir(root).filePath("docs/notes.md"));
	QVERIFY(file.open(QIODevice::ReadOnly));
	QCOMPARE(file.readAll(), QByteArray("# Changed\n"));
	// the command itself discards and reads the open tab again
	QVERIFY(window.gitDiscard(QStringList() << "docs/notes.md", &error));
	QCOMPARE(window.currentText()->text(), QString("# Notes\n"));
}

// EDIT-GIT-15: the History follows the current file row, compares on a double click, More...
void TestRepository::test_history_dock()
{
	if (!haveGit) QSKIP("no git 2.23 or later");
	QString root = makeRepository("history-dock");
	for (int i = 0; i < 3; i++) {
		write(QDir(root).filePath("h-log.txt"), QByteArray::number(i));
		git(root, QStringList() << "add" << "-A");
		git(root, QStringList() << "commit" << "-q" << "-m" << QString("log %1").arg(i));
	}
	CyberiadaSMEditorWindow window;
	window.resize(1200, 800);
	window.show();
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	window.historyDock->show();
	window.historyDock->raise();
	QString error;
	QVERIFY(window.openFile(QDir(root).filePath("h.graphml"), &error));
	HistoryView* history = window.historyView();
	QTRY_COMPARE(history->model()->entries().size(), 1);
	QCOMPARE(history->model()->entries().first().subject, QString("first"));
	QCOMPARE(history->title(), QString("History of h.graphml"));
	RepositoryView* view = window.repositoryView();
	QTRY_VERIFY_WITH_TIMEOUT(view->model()->indexOf("h-log.txt").isValid(), 10000);
	view->tree()->setCurrentIndex(view->model()->indexOf("h-log.txt"));
	QTRY_COMPARE(history->model()->entries().size(), 3);
	QCOMPARE(history->title(), QString("History of h-log.txt"));
	// the rows alternate; the commit of the current version is bold (EDIT-GIT-15, 0.81)
	QVERIFY(history->list()->alternatingRowColors());
	QVERIFY(history->list()->palette().color(QPalette::AlternateBase) != history->list()->palette().color(QPalette::Base));
	QCOMPARE(history->model()->currentHash(), history->model()->entries().first().hash);
	QVERIFY(history->model()->index(0, HistoryModel::columnSubject).data(Qt::FontRole).value<QFont>().bold());
	QVERIFY(!history->model()->index(1, HistoryModel::columnSubject).data(Qt::FontRole).isValid());
	history->repositoryAction()->trigger();
	QCOMPARE(history->model()->entries().size(), 4);
	QVERIFY(!history->model()->hasMore());
	// in the repository scope HEAD, the newest commit
	QCOMPARE(history->model()->currentHash(), window.repositoryOf(window.currentTab())->snapshot().oid);
	QCOMPARE(history->model()->currentHash(), history->model()->entries().first().hash);
	// a double click compares the active tab's file with the commit
	history->fileAction()->trigger();
	view->tree()->setCurrentIndex(view->model()->indexOf("h.graphml"));
	QTRY_COMPARE(history->model()->entries().size(), 1);
	int tabs = window.documentCount();
	emit history->list()->activated(history->model()->index(0, HistoryModel::columnHash));
	QCOMPARE(window.documentCount(), tabs + 1);
	QVERIFY(window.currentComparison());
	QVERIFY(window.currentComparison()->title().contains("h.graphml"));
}

// EDIT-GIT-18: the branch, ahead and behind, detached, before the first commit, a merge
void TestRepository::test_branch_label()
{
	RepositorySnapshot s;
	s.branch = "devel";
	s.oid = "0123456789abcdef";
	QCOMPARE(CyberiadaSMEditorWindow::branchLabel(s), QString("devel"));
	s.upstream = "origin/devel";
	s.ahead = 1;
	s.behind = 2;
	QCOMPARE(CyberiadaSMEditorWindow::branchLabel(s), QString("devel ") + QChar(0x2191) + "1 " + QChar(0x2193) + "2");
	s.behind = 0;
	s.state = "merging";
	QCOMPARE(CyberiadaSMEditorWindow::branchLabel(s), QString("devel ") + QChar(0x2191) + "1, merging");
	RepositorySnapshot detached;
	detached.oid = "3f2a9c1abcdef";
	QCOMPARE(CyberiadaSMEditorWindow::branchLabel(detached), QString("detached at 3f2a9c1"));
	RepositorySnapshot unborn;
	unborn.branch = "main";
	QCOMPARE(CyberiadaSMEditorWindow::branchLabel(unborn), QString("main (no commits)"));
}

// EDIT-GIT-18/19: the branch menu lists the branches, the current one checked; a switch reads
// the clean tab again
void TestRepository::test_branch_menu()
{
	if (!haveGit) QSKIP("no git 2.23 or later");
	QString root = makeRepository("branch-menu");
	git(root, QStringList() << "checkout" << "-q" << "-b" << "other");
	QFile::remove(QDir(root).filePath("h.graphml"));
	QFile::copy("diagrams/geometry.graphml", QDir(root).filePath("h.graphml"));
	git(root, QStringList() << "commit" << "-q" << "-a" << "-m" << "geometry");
	git(root, QStringList() << "checkout" << "-q" << "main");
	CyberiadaSMEditorWindow window;
	QString error;
	QVERIFY(window.openFile(QDir(root).filePath("h.graphml"), &error));
	QMenu* menu = window.gitBranchMenu();
	QVERIFY(menu);
	QCOMPARE(window.repositoryView()->branchButton()->menu(), menu);
	QMetaObject::invokeMethod(menu, "aboutToShow");
	QStringList entries;
	for (QAction* action : menu->actions()) {
		if (!action->isSeparator()) entries << action->text() + (action->isChecked() ? "*" : "");
	}
	QCOMPARE(entries, QStringList() << "main*" << "other" << "New Branch..." << "Delete Branch...");
	CyberiadaSMEditorDocument* doc = window.currentDocument();
	const Cyberiada::LocalDocument* before = doc->model()->rootDocument();
	QVERIFY(window.gitSwitch("other", &error));
	QVERIFY(doc->model()->rootDocument() != before);
	QVERIFY(doc->isClean());
	QTRY_COMPARE(window.repositoryView()->branchButton()->text(), QString("other"));
}

// EDIT-UI-6: the git docks at the right, tabs of the structure tree, the tree in front in a
// diagram tab, the Repository in a tab without the tree
static QString frontTab(QMainWindow* window, const QString& title)
{
	for (QTabBar* bar : window->findChildren<QTabBar*>()) {
		for (int i = 0; i < bar->count(); i++) {
			if (bar->tabText(i) == title) return bar->tabText(bar->currentIndex());
		}
	}
	return QString();
}

void TestRepository::test_docks_right()
{
	CyberiadaSMEditorWindow window;
	window.resize(1200, 800);
	window.show();
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	QCOMPARE(window.dockWidgetArea(window.repositoryDock), Qt::RightDockWidgetArea);
	QCOMPARE(window.dockWidgetArea(window.historyDock), Qt::RightDockWidgetArea);
	QList<QDockWidget*> tabs = window.tabifiedDockWidgets(window.treeDock);
	QVERIFY(tabs.contains(window.repositoryDock) && tabs.contains(window.historyDock));
	if (!haveGit) return;
	QCOMPARE(frontTab(&window, "Structure tree"), QString("Structure tree"));
	QString error;
	QVERIFY(window.openFile("texts/notes.md", &error));
	QTest::qWait(50);
	QVERIFY(window.treeDock->isHidden());
	QCOMPARE(frontTab(&window, "Repository"), QString("Repository"));
	QVERIFY(window.openFile("diagrams/hierarchy.graphml", &error));
	QTest::qWait(50);
	QCOMPARE(frontTab(&window, "Structure tree"), QString("Structure tree"));
	// the Repository in front over a work tree with the markers of every kind, for the eye:
	// changed, staged, staged and changed again, added, deleted, untracked code
	QString root = makeRepository("docks-right");
	for (const char* name : {"guide", "tasks"}) {
		write(QDir(root).filePath(QString("docs/%1.md").arg(name)), "# Text\n");
		git(root, QStringList() << "add" << "-A");
		git(root, QStringList() << "commit" << "-q" << "-m" << QString("the %1").arg(name));
	}
	write(QDir(root).filePath("docs/guide.md"), "# Changed\n");
	write(QDir(root).filePath("deep/inner/a.txt"), "b\n");
	write(QDir(root).filePath("docs/plan.md"), "# Plan\n");
	git(root, QStringList() << "add" << "deep/inner/a.txt" << "docs/plan.md");
	write(QDir(root).filePath("docs/plan.md"), "# Plan, again\n");
	git(root, QStringList() << "rm" << "-q" << "docs/notes.md");
	write(QDir(root).filePath("src/blink.ino"), "void setup() {}\n");
	QVERIFY(window.openFile(QDir(root).filePath("h.graphml"), &error));
	window.repositoryDock->raise();
	RepositoryView* view = window.repositoryView();
	QTRY_VERIFY_WITH_TIMEOUT(view->model()->indexOf("src/blink.ino").isValid(), 10000);
	view->tree()->expandAll();
	// the tools fit the first line of the dock as it opens: no overflow (EDIT-GIT-3)
	QToolButton* more = view->toolBar()->findChild<QToolButton*>("qt_toolbar_ext_button");
	QVERIFY(!more || !more->isVisible());
	QVERIFY(view->branchButton()->isVisible());
	QVERIFY(window.grab().save(QT_TESTCASE_BUILDDIR "/tests/repository-docks.png"));
	QVERIFY(window.repositoryDock->grab().save(QT_TESTCASE_BUILDDIR "/tests/repository-dock.png"));
	window.historyDock->raise();
	window.gitHistory(QString(), true);
	QTRY_VERIFY(!window.historyView()->model()->entries().isEmpty());
	QVERIFY(window.historyDock->grab().save(QT_TESTCASE_BUILDDIR "/tests/history-dock.png"));
}

QTEST_MAIN(TestRepository)
#include "l4-repository.moc"
