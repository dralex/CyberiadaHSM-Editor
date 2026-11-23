/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The revision of Compare with a Revision...
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

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "revision_dialog.h"

static const char* HEAD = "HEAD";

RevisionDialog::RevisionDialog(const QString& file, const QVector<LogEntry>& commits, const Check& check,
                               QWidget* parent):
    QDialog(parent), m_check(check)
{
    setObjectName("revisionDialog");
    setWindowTitle(tr("Compare with a Revision"));
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("The revision to compare %1 with:").arg(file), this));
    m_revision = new QComboBox(this);
    m_revision->setObjectName("revisionBox");
    m_revision->setEditable(true);
    m_revision->setInsertPolicy(QComboBox::NoInsert);
    m_revision->addItem(HEAD, QString(HEAD));
    for (const LogEntry& e : commits) m_revision->addItem(e.shortHash + "  " + e.subject, e.hash);
    m_revision->setCurrentIndex(0);
    layout->addWidget(m_revision);
    m_error = new QLabel(this);
    m_error->setObjectName("revisionError");
    m_error->setWordWrap(true);
    m_error->hide();
    layout->addWidget(m_error);
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttons->addButton(tr("Compare"), QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &RevisionDialog::tryAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString RevisionDialog::revision() const
{
    QString typed = m_revision->currentText().trimmed();
    int at = m_revision->findText(typed);
    return at >= 0 ? m_revision->itemData(at).toString() : typed;
}

QString RevisionDialog::errorText() const
{
    return m_error->isHidden() ? QString() : m_error->text();
}

void RevisionDialog::tryAccept()
{
    QString chosen = revision();
    QString error;
    if (chosen.isEmpty()) {
        error = tr("Type a revision: a branch, a tag, a hash, HEAD~1");
    } else if (m_check && !m_check(chosen, &error)) {
        if (error.isEmpty()) error = tr("%1 is not a revision of the repository").arg(chosen);
    } else {
        accept();
        return;
    }
    m_error->setText(error);
    m_error->show();
}
