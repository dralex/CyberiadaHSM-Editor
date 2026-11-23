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

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "commit_dialog.h"

// the message box shows this many lines at the start
static const int MESSAGE_LINES = 6;
static const QString RENAME_ARROW = QString::fromUtf8(" → ");

CommitDialog::CommitDialog(const RepositorySnapshot& snapshot, const QStringList& unsaved, const QString& message,
                           const Commit& commit, const LastMessage& last, QWidget* parent):
    QDialog(parent), m_commit(commit), m_last(last)
{
    setObjectName("commitDialog");
    setWindowTitle(snapshot.detached() ? tr("Commit at %1").arg(snapshot.oid.left(GIT_SHORT_HASH))
                                       : tr("Commit to %1").arg(snapshot.branch));
    QVBoxLayout* layout = new QVBoxLayout(this);
    QStringList staged;
    for (const StatusEntry& e : snapshot.entries) {
        if (e.conflicted()) m_conflicted = true;
        if (!e.staged()) continue;
        staged << QString("%1  %2").arg(QString(e.index), e.oldPath.isEmpty() ? e.path : e.oldPath + RENAME_ARROW + e.path);
    }
    m_hasStaged = !staged.isEmpty();
    layout->addWidget(new QLabel(tr("Staged (%1):").arg(staged.size()), this));
    m_staged = new QListWidget(this);
    m_staged->setObjectName("commitStagedList");
    m_staged->addItems(staged);
    layout->addWidget(m_staged);
    QStringList notes;
    for (const QString& name : unsaved) notes << tr("! %1 has unsaved edits, they are not committed").arg(name);
    if (snapshot.state == "merging") notes << tr("The commit concludes the merge");
    if (m_conflicted) notes << tr("A file is in conflict: resolve it and stage it first");
    m_notes = new QLabel(notes.join('\n'), this);
    m_notes->setObjectName("commitNotes");
    m_notes->setWordWrap(true);
    m_notes->setVisible(!notes.isEmpty());
    layout->addWidget(m_notes);
    layout->addWidget(new QLabel(tr("Message:"), this));
    m_message = new QPlainTextEdit(this);
    m_message->setObjectName("commitMessage");
    m_message->setPlainText(message);
    m_message->setMinimumHeight(m_message->fontMetrics().lineSpacing() * MESSAGE_LINES);
    layout->addWidget(m_message);
    m_amend = new QCheckBox(tr("Amend the last commit"), this);
    m_amend->setObjectName("commitAmend");
    m_amend->setEnabled(!snapshot.unborn());
    layout->addWidget(m_amend);
    m_error = new QLabel(this);
    m_error->setObjectName("commitError");
    m_error->setWordWrap(true);
    m_error->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_error->hide();
    layout->addWidget(m_error);
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_commitButton = buttons->addButton(tr("Commit"), QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &CommitDialog::tryCommit);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_message, &QPlainTextEdit::textChanged, this, &CommitDialog::updateCommit);
    connect(m_amend, &QCheckBox::toggled, this, &CommitDialog::amendToggled);
    updateCommit();
}

QString CommitDialog::message() const
{
    return m_message->toPlainText();
}

bool CommitDialog::amend() const
{
    return m_amend->isChecked();
}

QString CommitDialog::notes() const
{
    QStringList all;
    if (!m_notes->isHidden()) all << m_notes->text();
    if (!m_error->isHidden()) all << m_error->text();
    return all.join('\n');
}

void CommitDialog::updateCommit()
{
    // a blank message, nothing to commit or a conflict keep Commit off (EDIT-GIT-14)
    bool can = !message().trimmed().isEmpty() && (m_hasStaged || amend()) && !m_conflicted;
    m_commitButton->setEnabled(can);
}

void CommitDialog::amendToggled(bool on)
{
    if (on && message().trimmed().isEmpty() && m_last) m_message->setPlainText(m_last());
    updateCommit();
}

void CommitDialog::tryCommit()
{
    if (!m_commitButton->isEnabled()) return;
    QString error;
    if (m_commit && !m_commit(message(), amend(), &error)) {
        m_error->setText(error);
        m_error->show();
        return;
    }
    accept();
}
