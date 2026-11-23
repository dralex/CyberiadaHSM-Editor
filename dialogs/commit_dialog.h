/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The commit dialog: the staged files and the message
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

#ifndef CYBERIADA_COMMIT_DIALOG
#define CYBERIADA_COMMIT_DIALOG

#include <functional>
#include <QDialog>
#include <QStringList>

#include "repository_status.h"

class QCheckBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

// Commit... (EDIT-GIT-14, docs/GIT.md, The commit): the index of the work tree committed with
// the message; the commit itself is the caller's, the dialog stays open after a failure
class CommitDialog: public QDialog {
Q_OBJECT
public:
    // commit: runs git, false with git's message; last: the message of the last commit
    using Commit = std::function<bool(const QString& message, bool amend, QString* error)>;
    using LastMessage = std::function<QString()>;

    CommitDialog(const RepositorySnapshot& snapshot, const QStringList& unsaved, const QString& message,
                 const Commit& commit, const LastMessage& last, QWidget* parent = nullptr);

    QString         message() const;
    bool            amend() const;
    QListWidget*    stagedList() const { return m_staged; }
    QPlainTextEdit* messageEdit() const { return m_message; }
    QCheckBox*      amendBox() const { return m_amend; }
    QPushButton*    commitButton() const { return m_commitButton; }
    // the notes over the message: the unsaved tabs, the merge, git's message
    QString         notes() const;

private:
    void            updateCommit();
    void            amendToggled(bool on);
    void            tryCommit();

    bool            m_hasStaged = false;
    bool            m_conflicted = false;
    Commit          m_commit;
    LastMessage     m_last;
    QListWidget*    m_staged;
    QLabel*         m_notes;
    QLabel*         m_error;
    QPlainTextEdit* m_message;
    QCheckBox*      m_amend;
    QPushButton*    m_commitButton;
};

#endif
