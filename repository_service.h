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

#ifndef CYBERIADA_REPOSITORY_SERVICE
#define CYBERIADA_REPOSITORY_SERVICE

#include <QObject>
#include <QPointer>
#include <QStringList>

#include "git_process.h"
#include "repository_status.h"

class QFileSystemWatcher;
class QProcess;

// One git work tree (EDIT-GIT-2/6, docs/GIT.md): the root found for a folder, the status
// read by git in the background, the git folder watched; the services last while the
// editor runs, one per root
class RepositoryService: public QObject {
Q_OBJECT
public:
    // the root of the work tree of a folder, looked up once (git rev-parse); empty outside a
    // work tree and without git; again looks it up anew
    static QString            rootOf(const QString& folder, bool again = false);
    // the service of a root, made at the first ask
    static RepositoryService* forRoot(const QString& root);
    // the lookups and the services forgotten: the git program or Use git changed
    static void               reset();

    QString                   root() const { return m_root; }
    QString                   gitDir() const { return m_gitDir; }
    // the last status read; valid after the first read
    const RepositorySnapshot& snapshot() const { return m_snapshot; }
    bool                      hasSnapshot() const { return m_read; }
    // the status read again: in the background, statusChanged at its end, an ask during a
    // read reads again after it; wait reads at once (the batch mode, the tests)
    void                      refresh(bool wait = false);
    bool                      isReading() const { return !m_process.isNull(); }
    // the ignored files in the status (EDIT-GIT-5)
    void                      setIgnored(bool on);
    bool                      ignored() const { return m_ignored; }
    // the git folder watched: gitChanged after the index, HEAD or a branch changed
    void                      setWatching(bool on);

signals:
    void                      statusChanged();
    void                      gitChanged();

private:
    RepositoryService(const QString& root, const QString& gitDir, QObject* parent);

    // the commands of a read, run one after the other
    enum ReadStep { readStatus = 0, readFiles, readBranches, readSteps };
    QStringList               readArguments(ReadStep step) const;
    void                      startStep(ReadStep step);
    void                      stepFinished();
    // the outputs of a read into a snapshot
    void                      applyRead(const QByteArray outputs[]);
    QString                   gitState() const;

    QString                   m_root;
    QString                   m_gitDir;
    RepositorySnapshot        m_snapshot;
    bool                      m_read = false;
    bool                      m_ignored = false;
    bool                      m_again = false;
    ReadStep                  m_step = readStatus;
    QByteArray                m_outputs[readSteps];
    QPointer<QProcess>        m_process;
    QFileSystemWatcher*       m_watcher = nullptr;
};

#endif
