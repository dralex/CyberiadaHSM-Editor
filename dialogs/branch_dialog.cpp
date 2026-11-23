/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The new branch dialog: its name, its start and the switch
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
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "branch_dialog.h"

BranchDialog::BranchDialog(const QStringList& branches, const Create& create, QWidget* parent):
    QDialog(parent), m_create(create)
{
    setObjectName("branchDialog");
    setWindowTitle(tr("New Branch"));
    QVBoxLayout* layout = new QVBoxLayout(this);
    QFormLayout* form = new QFormLayout();
    m_name = new QLineEdit(this);
    m_name->setObjectName("branchName");
    form->addRow(tr("Name:"), m_name);
    m_start = new QComboBox(this);
    m_start->setObjectName("branchStart");
    m_start->addItem(tr("The current commit"), QString());
    for (const QString& branch : branches) m_start->addItem(branch, branch);
    form->addRow(tr("From:"), m_start);
    layout->addLayout(form);
    m_switch = new QCheckBox(tr("Switch to it"), this);
    m_switch->setObjectName("branchSwitch");
    m_switch->setChecked(true);
    layout->addWidget(m_switch);
    m_error = new QLabel(this);
    m_error->setObjectName("branchError");
    m_error->setWordWrap(true);
    m_error->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_error->hide();
    layout->addWidget(m_error);
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_createButton = buttons->addButton(tr("Create"), QDialogButtonBox::AcceptRole);
    m_createButton->setEnabled(false);
    layout->addWidget(buttons);
    connect(m_name, &QLineEdit::textChanged, this,
            [this]() { m_createButton->setEnabled(!name().isEmpty()); });
    connect(buttons, &QDialogButtonBox::accepted, this, &BranchDialog::tryCreate);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString BranchDialog::name() const
{
    return m_name->text().trimmed();
}

QString BranchDialog::start() const
{
    return m_start->currentData().toString();
}

bool BranchDialog::switchTo() const
{
    return m_switch->isChecked();
}

QString BranchDialog::errorText() const
{
    return m_error->isHidden() ? QString() : m_error->text();
}

void BranchDialog::tryCreate()
{
    if (name().isEmpty()) return;
    QString error;
    if (m_create && !m_create(name(), start(), switchTo(), &error)) {
        m_error->setText(error);
        m_error->show();
        return;
    }
    accept();
}
