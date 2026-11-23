/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * A git work tree of the editor: its status read in the background
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

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHash>
#include <QProcess>

#include "repository_service.h"

// the markers a merge and a rebase leave in the git folder
static const char* MERGE_HEAD = "MERGE_HEAD";
static const char* REBASE_MERGE = "rebase-merge";
static const char* REBASE_APPLY = "rebase-apply";
static const char* REFS_HEADS = "refs/heads";

// a folder looked up: its root and the git folder, both empty outside a work tree
struct RepositoryLookup {
    QString root;
    QString gitDir;
};

static QHash<QString, RepositoryLookup>& lookups()
{
    static QHash<QString, RepositoryLookup> folders;
    return folders;
}

static QHash<QString, RepositoryService*>& services()
{
    static QHash<QString, RepositoryService*> roots;
    return roots;
}

QString RepositoryService::rootOf(const QString& folder, bool again)
{
    if (folder.isEmpty() || GitProcess::program().isEmpty()) return QString();
    QString key = QDir::cleanPath(QFileInfo(folder).absoluteFilePath());
    if (!again && lookups().contains(key)) return lookups().value(key).root;
    RepositoryLookup lookup;
    GitResult r = GitProcess::run(key, QStringList() << "rev-parse" << "--show-toplevel" << "--absolute-git-dir");
    if (r.ok()) {
        QStringList lines = QString::fromUtf8(r.out).split('\n', QString::SkipEmptyParts);
        if (lines.size() >= 2) {
            lookup.root = QDir::cleanPath(lines[0]);
            lookup.gitDir = QDir::cleanPath(lines[1]);
        }
    }
    lookups().insert(key, lookup);
    if (!lookup.root.isEmpty() && !services().contains(lookup.root)) {
        services().insert(lookup.root, new RepositoryService(lookup.root, lookup.gitDir, qApp));
    }
    return lookup.root;
}

RepositoryService* RepositoryService::forRoot(const QString& root)
{
    if (root.isEmpty()) return nullptr;
    if (!services().contains(root)) rootOf(root);
    return services().value(root, nullptr);
}

void RepositoryService::reset()
{
    lookups().clear();
    for (RepositoryService* service : services()) service->deleteLater();
    services().clear();
}

RepositoryService::RepositoryService(const QString& root, const QString& gitDir, QObject* parent):
    QObject(parent), m_root(root), m_gitDir(gitDir)
{
    m_snapshot.root = root;
    m_snapshot.gitDir = gitDir;
}

void RepositoryService::setIgnored(bool on)
{
    m_ignored = on;
}

QStringList RepositoryService::readArguments(ReadStep step) const
{
    switch (step) {
    case readStatus: {
        // no optional lock: a read never holds the index against a git the user runs
        QStringList args = QStringList() << "--no-optional-locks" << "status" << "--porcelain=v2" << "-z"
                                         << "--branch" << "--untracked-files=all";
        if (m_ignored) args << "--ignored=matching";
        return args;
    }
    case readFiles:
        return QStringList() << "ls-files" << "-z";
    case readBranches:
        return QStringList() << "for-each-ref" << "--format=%(refname:short)%00" << REFS_HEADS;
    default:
        return QStringList();
    }
}

QString RepositoryService::gitState() const
{
    QDir dir(m_gitDir);
    if (dir.exists(REBASE_MERGE) || dir.exists(REBASE_APPLY)) return "rebasing";
    if (dir.exists(MERGE_HEAD)) return "merging";
    return QString();
}

void RepositoryService::applyRead(const QByteArray outputs[])
{
    RepositorySnapshot snapshot;
    snapshot.root = m_root;
    snapshot.gitDir = m_gitDir;
    if (!parseStatus(outputs[readStatus], &snapshot)) return;
    addTrackedFiles(&snapshot, parseFiles(outputs[readFiles]));
    snapshot.branches = parseBranches(outputs[readBranches]);
    snapshot.state = gitState();
    m_snapshot = snapshot;
    m_read = true;
    emit statusChanged();
}

void RepositoryService::refresh(bool wait)
{
    if (wait) {
        // a read in the background is overtaken: its end is dropped
        if (m_process) {
            m_process->disconnect(this);
            m_process->kill();
            m_process->deleteLater();
            m_process = nullptr;
        }
        m_again = false;
        QByteArray outputs[readSteps];
        for (int step = 0; step < readSteps; step++) {
            GitResult r = GitProcess::run(m_root, readArguments(ReadStep(step)));
            if (!r.ok()) return;
            outputs[step] = r.out;
        }
        applyRead(outputs);
        return;
    }
    if (m_process) {
        m_again = true;
        return;
    }
    startStep(readStatus);
}

void RepositoryService::startStep(ReadStep step)
{
    if (GitProcess::program().isEmpty()) return;
    m_step = step;
    m_process = GitProcess::create(m_root, readArguments(step), this);
    m_process->setStandardInputFile(QProcess::nullDevice());
    connect(m_process.data(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &RepositoryService::stepFinished);
    connect(m_process.data(), &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_process) return;
        m_process->deleteLater();
        m_process = nullptr;
    });
    m_process->start();
}

void RepositoryService::stepFinished()
{
    QProcess* process = m_process.data();
    if (!process) return;
    GitResult r = GitProcess::result(process);
    process->deleteLater();
    m_process = nullptr;
    if (!r.ok()) {
        m_again = false;
        return;
    }
    m_outputs[m_step] = r.out;
    if (m_step + 1 < readSteps) {
        startStep(ReadStep(m_step + 1));
        return;
    }
    if (m_again) {
        // the read was asked for again while it ran: what it saw may be old
        m_again = false;
        startStep(readStatus);
        return;
    }
    applyRead(m_outputs);
}

GitResult RepositoryService::command(const QStringList& args, const QByteArray& input)
{
    GitResult r = GitProcess::run(m_root, args, input);
    emit commandRun(args, r);
    return r;
}

bool RepositoryService::base(const QString& path, const QString& revision, QByteArray* bytes,
                             QString* error) const
{
    bytes->clear();
    if (revision.isEmpty()) {
        // <mode> <hash> <stage> TAB <path> a record; stage 0 is a merged file
        GitResult r = GitProcess::run(m_root, QStringList() << "ls-files" << "--stage" << "-z" << "--" << path);
        if (!r.ok()) {
            if (error) *error = r.message();
            return false;
        }
        QString hash;
        for (const QByteArray& record : r.out.split('\0')) {
            QList<QByteArray> fields = record.left(record.indexOf('\t')).split(' ');
            if (fields.size() < 3) continue;
            if (fields.at(2) != "0") {
                if (error) *error = tr("%1 is not merged: the index has no file of it").arg(path);
                return false;
            }
            hash = QString::fromLatin1(fields.at(1));
        }
        // not in the index: an empty document
        if (hash.isEmpty()) return true;
        GitResult blob = GitProcess::run(m_root, QStringList() << "cat-file" << "blob" << hash);
        if (!blob.ok()) {
            if (error) *error = blob.message();
            return false;
        }
        *bytes = blob.out;
        return true;
    }
    if (revision.startsWith('-')) {
        if (error) *error = tr("a revision does not start with -");
        return false;
    }
    GitResult commit = GitProcess::run(m_root, QStringList() << "rev-parse" << "--verify" << "-q"
                                                             << revision + "^{commit}");
    if (!commit.ok()) {
        if (error) *error = tr("%1 is not a revision of the repository").arg(revision);
        return false;
    }
    QString hash = QString::fromLatin1(commit.out).trimmed();
    QStringList paths = QStringList() << path;
    // a staged rename: the revision has the old path
    const StatusEntry* entry = m_snapshot.entry(path);
    if (entry && !entry->oldPath.isEmpty()) paths << entry->oldPath;
    for (const QString& p : paths) {
        GitResult blob = GitProcess::run(m_root, QStringList() << "cat-file" << "blob" << hash + ":" + p);
        if (blob.ok()) {
            *bytes = blob.out;
            return true;
        }
    }
    // the revision lacks the file: an empty document
    return true;
}

QVector<LogEntry> RepositoryService::log(const QString& path, int count, int skip, QString* error) const
{
    QStringList args = QStringList() << "log" << "-z" << "--date=short"
                                     << "--format=%H%x1f%h%x1f%ad%x1f%an%x1f%s"
                                     << QString("-n%1").arg(count) << QString("--skip=%1").arg(skip);
    if (!path.isEmpty()) args << "--follow" << "--" << path;
    GitResult r = GitProcess::run(m_root, args);
    // a repository without commits has no log
    if (!r.ok()) {
        if (error && !m_snapshot.unborn()) *error = r.message();
        return QVector<LogEntry>();
    }
    return parseLog(r.out);
}

QString RepositoryService::lastMessage() const
{
    GitResult r = GitProcess::run(m_root, QStringList() << "log" << "-1" << "--format=%B");
    return r.ok() ? QString::fromUtf8(r.out).trimmed() : QString();
}

void RepositoryService::setWatching(bool on)
{
    if (!on) {
        delete m_watcher;
        m_watcher = nullptr;
        return;
    }
    if (m_watcher) return;
    // the folders, not the index: git replaces the index and HEAD by a rename
    m_watcher = new QFileSystemWatcher(this);
    QStringList folders = QStringList() << m_gitDir << QDir(m_gitDir).filePath(REFS_HEADS);
    for (const QString& folder : folders) {
        if (QFileInfo(folder).isDir()) m_watcher->addPath(folder);
    }
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &RepositoryService::gitChanged);
}
