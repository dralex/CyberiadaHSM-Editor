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

#include <QSet>
#include <algorithm>

#include "repository_status.h"

// the fields before the path of the records of porcelain v2 (git-status(1))
static const int ORDINARY_FIELDS = 8;    // 1 XY sub mH mI mW hH hI <path>
static const int RENAMED_FIELDS = 9;     // 2 XY sub mH mI mW hH hI Xscore <path> NUL <old path>
static const int UNMERGED_FIELDS = 10;   // u XY sub m1 m2 m3 mW h1 h2 h3 <path>
static const int UNTRACKED_FIELDS = 1;   // ? <path>, ! <path>
static const char RECORD_END = '\0';
static const char LOG_FIELD = '\x1f';
static const int LOG_FIELDS = 5;

static const QByteArray HEADER = "# ";
static const QByteArray BRANCH_OID = "branch.oid ";
static const QByteArray BRANCH_HEAD = "branch.head ";
static const QByteArray BRANCH_UPSTREAM = "branch.upstream ";
static const QByteArray BRANCH_AB = "branch.ab ";
static const QByteArray INITIAL = "(initial)";
static const QByteArray DETACHED = "(detached)";

const StatusEntry* RepositorySnapshot::entry(const QString& path) const
{
    auto i = std::lower_bound(entries.cbegin(), entries.cend(), path,
                              [](const StatusEntry& e, const QString& p) { return e.path < p; });
    return (i != entries.cend() && i->path == path) ? &*i : nullptr;
}

// what follows the n-th space of a record: its path
static QByteArray afterFields(const QByteArray& record, int fields)
{
    int at = -1;
    for (int i = 0; i < fields; i++) {
        at = record.indexOf(' ', at + 1);
        if (at < 0) return QByteArray();
    }
    return record.mid(at + 1);
}

static QString pathOf(const QByteArray& bytes)
{
    return QString::fromUtf8(bytes);
}

static void sortEntries(QVector<StatusEntry>* entries)
{
    std::sort(entries->begin(), entries->end(),
              [](const StatusEntry& a, const StatusEntry& b) { return a.path < b.path; });
}

static void parseHeader(const QByteArray& line, RepositorySnapshot* s)
{
    if (line.startsWith(BRANCH_OID)) {
        QByteArray oid = line.mid(BRANCH_OID.size());
        s->oid = oid == INITIAL ? QString() : QString::fromLatin1(oid);
    } else if (line.startsWith(BRANCH_HEAD)) {
        QByteArray head = line.mid(BRANCH_HEAD.size());
        s->branch = head == DETACHED ? QString() : QString::fromUtf8(head);
    } else if (line.startsWith(BRANCH_UPSTREAM)) {
        s->upstream = QString::fromUtf8(line.mid(BRANCH_UPSTREAM.size()));
    } else if (line.startsWith(BRANCH_AB)) {
        // +<ahead> -<behind>
        QList<QByteArray> counts = line.mid(BRANCH_AB.size()).split(' ');
        if (counts.size() == 2) {
            s->ahead = counts[0].mid(1).toInt();
            s->behind = counts[1].mid(1).toInt();
        }
    }
}

bool parseStatus(const QByteArray& out, RepositorySnapshot* s)
{
    const QList<QByteArray> records = out.split(RECORD_END);
    s->entries.clear();
    for (int i = 0; i < records.size(); i++) {
        const QByteArray& r = records[i];
        if (r.isEmpty()) continue;
        StatusEntry e;
        switch (r[0]) {
        case '#':
            if (r.startsWith(HEADER)) parseHeader(r.mid(HEADER.size()), s);
            continue;
        case '1':
            e.path = pathOf(afterFields(r, ORDINARY_FIELDS));
            break;
        case '2':
            e.path = pathOf(afterFields(r, RENAMED_FIELDS));
            // the old path is the next record
            if (i + 1 >= records.size() || records[i + 1].isEmpty()) return false;
            e.oldPath = pathOf(records[++i]);
            break;
        case 'u':
            e.path = pathOf(afterFields(r, UNMERGED_FIELDS));
            e.index = e.tree = QLatin1Char('U');
            break;
        case '?':
        case '!':
            e.path = pathOf(afterFields(r, UNTRACKED_FIELDS));
            e.tree = QLatin1Char(r[0]);
            break;
        default:
            return false;
        }
        if (e.path.isEmpty()) return false;
        if (r[0] == '1' || r[0] == '2') {
            // XY, '.' for no change
            if (r.size() < 4) return false;
            e.index = QLatin1Char(r[2]);
            e.tree = QLatin1Char(r[3]);
        }
        s->entries.append(e);
    }
    sortEntries(&s->entries);
    return true;
}

QStringList parseFiles(const QByteArray& out)
{
    QStringList files;
    QSet<QString> seen;
    for (const QByteArray& record : out.split(RECORD_END)) {
        if (record.isEmpty()) continue;
        QString path = pathOf(record);
        if (seen.contains(path)) continue;
        seen.insert(path);
        files.append(path);
    }
    return files;
}

void addTrackedFiles(RepositorySnapshot* s, const QStringList& tracked)
{
    QSet<QString> known;
    for (const StatusEntry& e : s->entries) known.insert(e.path);
    for (const QString& path : tracked) {
        if (known.contains(path)) continue;
        StatusEntry e;
        e.path = path;
        s->entries.append(e);
    }
    sortEntries(&s->entries);
}

QStringList parseBranches(const QByteArray& out)
{
    QStringList branches;
    for (const QByteArray& record : out.split(RECORD_END)) {
        QByteArray name = record.trimmed();
        if (!name.isEmpty()) branches.append(QString::fromUtf8(name));
    }
    return branches;
}

QVector<LogEntry> parseLog(const QByteArray& out)
{
    QVector<LogEntry> log;
    for (const QByteArray& record : out.split(RECORD_END)) {
        // git log -z puts the separator before a record: the leading line ends are dropped
        QByteArray r = record;
        while (r.startsWith('\n')) r.remove(0, 1);
        if (r.isEmpty()) continue;
        QList<QByteArray> fields = r.split(LOG_FIELD);
        if (fields.size() < LOG_FIELDS) continue;
        LogEntry e;
        e.hash = QString::fromLatin1(fields[0]);
        e.shortHash = QString::fromLatin1(fields[1]);
        e.date = QString::fromUtf8(fields[2]);
        e.author = QString::fromUtf8(fields[3]);
        // a subject holding the separator keeps it
        e.subject = QString::fromUtf8(fields.mid(LOG_FIELDS - 1).join(LOG_FIELD));
        log.append(e);
    }
    return log;
}
