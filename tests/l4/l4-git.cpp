/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process tests of git in the editor
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
#include "git_process.h"
#include "repository_service.h"
#include "repository_status.h"
#include "settings_manager.h"

// git in the editor (EDIT-GIT-1..23, docs/GIT.md): the machine output of git read, the
// program, the work trees of the folders and their status read
class TestGit: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void test_parse_status();
	void test_parse_heads();
	void test_parse_paths();
	void test_parse_refused();
	void test_files_and_branches();
	void test_parse_log();
	void test_git_off();
	void test_root_of();
	void test_refresh_wait();
	void test_refresh_background();
	void test_ignored();
	void test_watching();

private:
	// a repository of its own in the temporary folder, committed once
	QString makeRepository(const QString& name);
	void git(const QString& folder, const QStringList& args);
	void write(const QString& path, const QByteArray& bytes);

	QTemporaryDir home;
	bool haveGit = false;
};

void TestGit::initTestCase()
{
	QVERIFY(home.isValid());
	// git isolated as in the batch cases: an empty configuration, fixed names and dates, no
	// folder above the temporary one searched
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
	haveGit = !GitProcess::program().isEmpty();
}

void TestGit::write(const QString& path, const QByteArray& bytes)
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile file(path);
	QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
	file.write(bytes);
}

void TestGit::git(const QString& folder, const QStringList& args)
{
	GitResult r = GitProcess::run(folder, args);
	QVERIFY2(r.ok(), qPrintable(args.join(' ') + ": " + r.message()));
}

QString TestGit::makeRepository(const QString& name)
{
	QString root = home.filePath(name);
	QDir().mkpath(root);
	git(root, QStringList() << "init" << "-q");
	git(root, QStringList() << "symbolic-ref" << "HEAD" << "refs/heads/main");
	write(QDir(root).filePath("a.txt"), "a\n");
	git(root, QStringList() << "add" << "-A");
	git(root, QStringList() << "commit" << "-q" << "-m" << "first");
	return QDir::cleanPath(QFileInfo(root).canonicalFilePath());
}

// every kind of record of porcelain v2, the entries sorted by path
void TestGit::test_parse_status()
{
	QByteArray out;
	out += QByteArray("# branch.oid 0123456789abcdef0123456789abcdef01234567") + '\0';
	out += QByteArray("# branch.head devel") + '\0';
	out += QByteArray("# branch.upstream origin/devel") + '\0';
	out += QByteArray("# branch.ab +2 -1") + '\0';
	out += QByteArray("1 .M N... 100644 100644 100644 aaaa bbbb h.graphml") + '\0';
	out += QByteArray("1 AM N... 000000 100644 100644 0000 bbbb added.txt") + '\0';
	out += QByteArray("2 R. N... 100644 100644 100644 aaaa aaaa R100 new.txt") + '\0' + QByteArray("old.txt") + '\0';
	out += QByteArray("u UU N... 100644 100644 100644 100644 aaaa bbbb cccc c.txt") + '\0';
	out += QByteArray("? untracked/x.md") + '\0';
	out += QByteArray("! build/") + '\0';
	RepositorySnapshot s;
	QVERIFY(parseStatus(out, &s));
	QCOMPARE(s.branch, QString("devel"));
	QCOMPARE(s.oid.size(), 40);
	QVERIFY(!s.detached() && !s.unborn());
	QCOMPARE(s.upstream, QString("origin/devel"));
	QCOMPARE(s.ahead, 2);
	QCOMPARE(s.behind, 1);
	QStringList paths;
	for (const StatusEntry& e : s.entries) paths << e.path;
	QCOMPARE(paths, QStringList() << "added.txt" << "build/" << "c.txt" << "h.graphml" << "new.txt"
	                              << "untracked/x.md");
	const StatusEntry* modified = s.entry("h.graphml");
	QVERIFY(modified && modified->modified() && !modified->staged());
	const StatusEntry* added = s.entry("added.txt");
	QVERIFY(added && added->staged() && added->modified());
	const StatusEntry* renamed = s.entry("new.txt");
	QVERIFY(renamed && renamed->index == QLatin1Char('R'));
	QCOMPARE(renamed->oldPath, QString("old.txt"));
	const StatusEntry* conflict = s.entry("c.txt");
	QVERIFY(conflict && conflict->conflicted() && !conflict->staged() && !conflict->modified());
	QVERIFY(s.entry("untracked/x.md")->untracked());
	QVERIFY(s.entry("build/")->ignored());
	QVERIFY(!s.entry("missing"));
}

// the branch line before the first commit and on a detached HEAD
void TestGit::test_parse_heads()
{
	RepositorySnapshot unborn;
	QVERIFY(parseStatus(QByteArray("# branch.oid (initial)") + '\0' + QByteArray("# branch.head main") + '\0', &unborn));
	QVERIFY(unborn.unborn());
	QCOMPARE(unborn.branch, QString("main"));
	QVERIFY(unborn.entries.isEmpty());
	RepositorySnapshot detached;
	QVERIFY(parseStatus(QByteArray("# branch.oid abcdef") + '\0' + QByteArray("# branch.head (detached)") + '\0',
	                    &detached));
	QVERIFY(detached.detached());
	QCOMPARE(detached.oid, QString("abcdef"));
	QVERIFY(detached.upstream.isEmpty());
}

// a path of -z is never quoted: spaces, a line end, other letters
void TestGit::test_parse_paths()
{
	QByteArray out;
	out += QByteArray("1 .M N... 100644 100644 100644 aaaa bbbb with space.txt") + '\0';
	out += QByteArray("? two\nlines.txt") + '\0';
	out += QString::fromUtf8("? диаграмма.graphml").toUtf8() + '\0';
	RepositorySnapshot s;
	QVERIFY(parseStatus(out, &s));
	QVERIFY(s.entry("with space.txt"));
	QVERIFY(s.entry("two\nlines.txt"));
	QVERIFY(s.entry(QString::fromUtf8("диаграмма.graphml")));
}

// an output not of porcelain v2 is refused
void TestGit::test_parse_refused()
{
	RepositorySnapshot s;
	QVERIFY(!parseStatus(QByteArray(" M h.graphml") + '\0', &s));
	// a rename without its old path
	QVERIFY(!parseStatus(QByteArray("2 R. N... 100644 100644 100644 aaaa aaaa R100 new.txt") + '\0', &s));
	QVERIFY(!parseStatus(QByteArray("1 .M short") + '\0', &s));
}

void TestGit::test_files_and_branches()
{
	// an unmerged file is listed once per stage
	QByteArray files = QByteArray("a.txt") + '\0' + "c.txt" + '\0' + "c.txt" + '\0' + "docs/b.md" + '\0';
	QCOMPARE(parseFiles(files), QStringList() << "a.txt" << "c.txt" << "docs/b.md");
	RepositorySnapshot s;
	QVERIFY(parseStatus(QByteArray("1 .M N... 100644 100644 100644 aaaa bbbb c.txt") + '\0', &s));
	addTrackedFiles(&s, parseFiles(files));
	QCOMPARE(s.entries.size(), 3);
	QVERIFY(s.entry("a.txt")->unchanged());
	QVERIFY(s.entry("c.txt")->modified());
	QVERIFY(s.entry("docs/b.md")->unchanged());
	QCOMPARE(parseBranches(QByteArray("devel") + '\0' + '\n' + "main" + '\0'), QStringList() << "devel" << "main");
	QVERIFY(parseBranches(QByteArray()).isEmpty());
}

void TestGit::test_parse_log()
{
	QByteArray out;
	out += QByteArray("0123456789abcdef0123456789abcdef01234567\x1f" "0123456\x1f" "2026-11-11\x1f" "A. Author\x1f" "night mode") + '\0';
	out += QByteArray("\nfedcba9876543210fedcba9876543210fedcba98\x1f" "fedcba9\x1f" "2026-11-02\x1f" "B\x1f" "a \x1f" " in it") + '\0';
	QVector<LogEntry> log = parseLog(out);
	QCOMPARE(log.size(), 2);
	QCOMPARE(log[0].shortHash, QString("0123456"));
	QCOMPARE(log[0].date, QString("2026-11-11"));
	QCOMPARE(log[0].author, QString("A. Author"));
	QCOMPARE(log[0].subject, QString("night mode"));
	QCOMPARE(log[1].hash.size(), 40);
	QCOMPARE(log[1].subject, QString("a \x1f" " in it"));
}

// EDIT-GIT-1: Use git off runs no git
void TestGit::test_git_off()
{
	SettingsManager::instance().setGitEnabled(false);
	QVERIFY(GitProcess::program().isEmpty());
	QCOMPARE(GitProcess::unavailableReason(), QString("git is off"));
	QVERIFY(RepositoryService::rootOf(home.path(), true).isEmpty());
	QVERIFY(!GitProcess::run(home.path(), QStringList() << "--version").started);
	SettingsManager::instance().setGitEnabled(true);
	// a program that is no git counts as not found
	SettingsManager::instance().setGitProgram(home.filePath("no-git"));
	QVERIFY(GitProcess::program().isEmpty());
	QCOMPARE(GitProcess::unavailableReason(), QString("git is not found"));
	QVERIFY(GitProcess::describe(home.filePath("no-git")).isEmpty());
	SettingsManager::instance().setGitProgram(QString());
	if (haveGit) {
		QVERIFY(GitProcess::unavailableReason().isEmpty());
		QVERIFY(GitProcess::describe(QString()).startsWith("git version"));
	}
}

// EDIT-GIT-2: the root of a folder in a work tree, none outside one
void TestGit::test_root_of()
{
	if (!haveGit) QSKIP("no git 2.23 or later");
	QString root = makeRepository("root-of");
	QString sub = QDir(root).filePath("deep/er");
	QDir().mkpath(sub);
	QCOMPARE(RepositoryService::rootOf(sub), root);
	QCOMPARE(RepositoryService::rootOf(root), root);
	RepositoryService* service = RepositoryService::forRoot(root);
	QVERIFY(service);
	QCOMPARE(service->root(), root);
	QVERIFY(service->gitDir().endsWith(".git"));
	QCOMPARE(RepositoryService::forRoot(root), service);
	QString outside = home.filePath("outside");
	QDir().mkpath(outside);
	QVERIFY(RepositoryService::rootOf(outside).isEmpty());
	QVERIFY(!RepositoryService::forRoot(QString()));
}

// EDIT-GIT-6: a read waited for
void TestGit::test_refresh_wait()
{
	if (!haveGit) QSKIP("no git 2.23 or later");
	QString root = makeRepository("refresh-wait");
	RepositoryService* service = RepositoryService::forRoot(RepositoryService::rootOf(root));
	QVERIFY(service);
	QVERIFY(!service->hasSnapshot());
	write(QDir(root).filePath("new.txt"), "new\n");
	write(QDir(root).filePath("a.txt"), "changed\n");
	service->refresh(true);
	QVERIFY(service->hasSnapshot());
	const RepositorySnapshot& s = service->snapshot();
	QCOMPARE(s.branch, QString("main"));
	QCOMPARE(s.branches, QStringList() << "main");
	QVERIFY(s.entry("new.txt") && s.entry("new.txt")->untracked());
	QVERIFY(s.entry("a.txt") && s.entry("a.txt")->modified());
	QVERIFY(s.state.isEmpty());
}

// EDIT-GIT-6: a read in the background, an ask during it reads again after it
void TestGit::test_refresh_background()
{
	if (!haveGit) QSKIP("no git 2.23 or later");
	QString root = makeRepository("refresh-background");
	RepositoryService* service = RepositoryService::forRoot(RepositoryService::rootOf(root));
	QVERIFY(service);
	QSignalSpy spy(service, &RepositoryService::statusChanged);
	service->refresh();
	QVERIFY(service->isReading());
	// asked again while reading: one read after it, not two at once
	write(QDir(root).filePath("later.txt"), "later\n");
	service->refresh();
	QTRY_VERIFY_WITH_TIMEOUT(!service->isReading() && spy.count() >= 1, 10000);
	QVERIFY(service->snapshot().entry("later.txt"));
	QCOMPARE(spy.count(), 1);
}

// EDIT-GIT-5: the ignored files only when asked for
void TestGit::test_ignored()
{
	if (!haveGit) QSKIP("no git 2.23 or later");
	QString root = makeRepository("ignored");
	write(QDir(root).filePath(".gitignore"), "*.log\nbuild/\n");
	write(QDir(root).filePath("run.log"), "log\n");
	write(QDir(root).filePath("build/out.o"), "o\n");
	RepositoryService* service = RepositoryService::forRoot(RepositoryService::rootOf(root));
	QVERIFY(service);
	service->refresh(true);
	QVERIFY(!service->snapshot().entry("run.log"));
	service->setIgnored(true);
	QVERIFY(service->ignored());
	service->refresh(true);
	QVERIFY(service->snapshot().entry("run.log") && service->snapshot().entry("run.log")->ignored());
	// an ignored folder is one row
	QVERIFY(service->snapshot().entry("build/") && service->snapshot().entry("build/")->ignored());
	QVERIFY(!service->snapshot().entry("build/out.o"));
	service->setIgnored(false);
}

// EDIT-GIT-6: the git folder watched: a stage changes the index
void TestGit::test_watching()
{
	if (!haveGit) QSKIP("no git 2.23 or later");
	QString root = makeRepository("watching");
	RepositoryService* service = RepositoryService::forRoot(RepositoryService::rootOf(root));
	QVERIFY(service);
	service->setWatching(true);
	QSignalSpy spy(service, &RepositoryService::gitChanged);
	write(QDir(root).filePath("b.txt"), "b\n");
	git(root, QStringList() << "add" << "b.txt");
	QTRY_VERIFY_WITH_TIMEOUT(spy.count() > 0, 5000);
	service->setWatching(false);
}

QTEST_MAIN(TestGit)
#include "l4-git.moc"
