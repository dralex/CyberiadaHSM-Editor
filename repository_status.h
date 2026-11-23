/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The status of a git work tree read from the machine output of git
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

#ifndef CYBERIADA_REPOSITORY_STATUS
#define CYBERIADA_REPOSITORY_STATUS

#include <QByteArray>
#include <QChar>
#include <QString>
#include <QStringList>
#include <QVector>

// the letter of no change
#define STATUS_NONE      QLatin1Char('.')
// the length of a short hash, as git abbreviates it by default
#define GIT_SHORT_HASH   7

// A file of the work tree and its two letters, the index and the work tree (EDIT-GIT-4):
// M modified, T type changed, A added, D deleted, R renamed, U conflicted (both), ?
// untracked and ! ignored (the work tree letter), '.' none
struct StatusEntry {
    QString path;                  // relative to the root, '/' between the parts; an ignored
                                   // folder ends with '/'
    QString oldPath;               // a rename: the path it had
    QChar   index = STATUS_NONE;
    QChar   tree = STATUS_NONE;

    bool    conflicted() const { return index == QLatin1Char('U'); }
    bool    untracked() const { return tree == QLatin1Char('?'); }
    bool    ignored() const { return tree == QLatin1Char('!'); }
    bool    unchanged() const { return index == STATUS_NONE && tree == STATUS_NONE; }
    // a change in the index, a commit takes it
    bool    staged() const { return index != STATUS_NONE && !conflicted(); }
    // a change of the work tree the index has not got
    bool    modified() const { return tree != STATUS_NONE && !untracked() && !ignored() && !conflicted(); }
};

// The work tree as one read saw it: the branch line and the files (docs/GIT.md, The status)
struct RepositorySnapshot {
    QString     root;              // the work tree
    QString     gitDir;            // its git folder, absolute
    QString     branch;            // the branch, empty when detached
    QString     oid;               // the commit of HEAD, empty before the first commit
    QString     upstream;          // empty without one
    int         ahead = 0;
    int         behind = 0;
    QString     state;             // "merging", "rebasing" or empty
    QStringList branches;          // the local branches
    QVector<StatusEntry> entries;  // sorted by path

    bool        detached() const { return branch.isEmpty(); }
    bool        unborn() const { return oid.isEmpty(); }
    // the entry of a path, nullptr when the work tree has no such file
    const StatusEntry* entry(const QString& path) const;
};

// git --no-optional-locks status --porcelain=v2 -z --branch: the branch line and the
// changed, untracked and ignored files into the snapshot; false for an output not read
bool parseStatus(const QByteArray& out, RepositorySnapshot* snapshot);
// git ls-files -z: the tracked files, each once (an unmerged file has several stages)
QStringList parseFiles(const QByteArray& out);
// the tracked files the status left out are unchanged; the entries sorted by path
void addTrackedFiles(RepositorySnapshot* snapshot, const QStringList& tracked);
// git for-each-ref --format=%(refname:short)%00 refs/heads: the local branches
QStringList parseBranches(const QByteArray& out);

// a commit of git log -z --format=%H%x1f%h%x1f%ad%x1f%an%x1f%s
struct LogEntry {
    QString hash;
    QString shortHash;
    QString date;
    QString author;
    QString subject;
};
QVector<LogEntry> parseLog(const QByteArray& out);

#endif
