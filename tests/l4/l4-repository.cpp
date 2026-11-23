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
#include <QTreeView>
#include "smeditor_window.h"
#include "dialogs/preferences_dialog.h"
#include "code_editor.h"
#include "git_process.h"
#include "repository_model.h"
#include "repository_service.h"
#include "repository_view.h"
#include "settings_manager.h"

// the Repository dock (EDIT-GIT-3..7, EDIT-UI-6, docs/GIT.md): the model in both layouts,
// the dock following the active tab, the Git page of the Preferences
class TestRepository: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void test_model_tree();
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

private:
	static RepositorySnapshot sample();
	static StatusEntry entry(const QString& path, char index, char tree, const QString& old = QString());
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
	// a folder holding a change is marked, one without is not
	QCOMPARE(model.data(model.index(0, RepositoryModel::columnTree)).toString(), QString("*"));
	QModelIndex d = model.index(2, RepositoryModel::columnName);
	QCOMPARE(model.data(d.sibling(d.row(), RepositoryModel::columnTree)).toString(), QString());
	// the two letters, none for '.'
	QModelIndex beta = model.index(0, RepositoryModel::columnIndex, b);
	QCOMPARE(model.data(beta).toString(), QString("M"));
	QCOMPARE(model.data(beta.sibling(0, RepositoryModel::columnTree)).toString(), QString("M"));
	QModelIndex light = model.indexOf("light.graphml");
	QCOMPARE(model.data(light.sibling(light.row(), RepositoryModel::columnIndex)).toString(), QString());
	QCOMPARE(model.data(light.sibling(light.row(), RepositoryModel::columnTree)).toString(), QString("M"));
	// a rename tells its old path
	QModelIndex z = model.indexOf("c/z.txt");
	QVERIFY(model.data(z, Qt::ToolTipRole).toString().contains("c/y.txt"));
	QCOMPARE(model.data(z, RepositoryModel::PathRole).toString(), QString("c/z.txt"));
	QCOMPARE(model.data(model.indexOf("conflict.txt").sibling(model.indexOf("conflict.txt").row(),
	                                                          RepositoryModel::columnIndex)).toString(), QString("U"));
	QCOMPARE(model.headerData(RepositoryModel::columnName, Qt::Horizontal).toString(), QString("Name"));
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

// EDIT-GIT-4: the file of a tab with unsaved edits is marked
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
	QCOMPARE(window.repositoryDock->windowTitle(), QString("Repository: first (main)"));
	// the file of the tab is the current row
	QCOMPARE(view->tree()->currentIndex().data(RepositoryModel::PathRole).toString(), QString("h.graphml"));
	EditorDocument* inFirst = window.currentTab();
	QVERIFY(window.openFile(QDir(second).filePath("docs/notes.md"), &error));
	QCOMPARE(view->root(), second);
	QTRY_VERIFY_WITH_TIMEOUT(view->model()->indexOf("docs/notes.md").isValid(), 10000);
	QVERIFY(window.openFile(QDir(outside).filePath("o.graphml"), &error));
	QVERIFY(view->showsLine());
	QCOMPARE(view->lineText(), QString("o.graphml is not in a git repository"));
	QCOMPARE(window.repositoryDock->windowTitle(), QString("Repository"));
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

QTEST_MAIN(TestRepository)
#include "l4-repository.moc"
