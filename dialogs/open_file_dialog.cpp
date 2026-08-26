/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The Open File Dialog
 *
 * Copyright (C) 2025 Alexey Fedoseev <aleksey@fedoseev.net>
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
#include <QGridLayout>

#include "open_file_dialog.h"
#include "ui_open_file_dialog.h"
#include "settings_manager.h"


OpenFileDialog::OpenFileDialog(QWidget *parent)
    : QFileDialog(parent, tr("Открыть файл диаграммы"),
                  SettingsManager::instance().getLastDirectory(),
                  tr("CyberiadaML graph (*.graphml)"))
    , ui(new Ui::OpenFileOptions)
    , options(new QWidget(this))
{
    // the options are placed into the dialog layout, so the Qt browser is required
    setOption(QFileDialog::DontUseNativeDialog, true);
    setFileMode(QFileDialog::ExistingFile);
    setAcceptMode(QFileDialog::AcceptOpen);
    setLabelText(QFileDialog::Accept, tr("Открыть"));

    ui->setupUi(options);
    QGridLayout *grid = qobject_cast<QGridLayout*>(layout());
    if (grid) {
        grid->addWidget(options, grid->rowCount(), 0, 1, grid->columnCount());
    }

    // the document is inspected until the user asks for the editing
    ui->inspectorCheckBox->setChecked(true);
    slotInspectorToggled(true);

    connect(ui->inspectorCheckBox, &QCheckBox::toggled, this, &OpenFileDialog::slotInspectorToggled);
    connect(this, &QDialog::accepted, this, &OpenFileDialog::slotAccepted);
}

OpenFileDialog::~OpenFileDialog()
{
    delete ui;
}

QString OpenFileDialog::selectedFile() const {
    return selectedFiles().value(0);
}

bool OpenFileDialog::inspectorModeEnabled() const {
    return ui->inspectorCheckBox->isChecked();
}

bool OpenFileDialog::reconstructionEnabled() const {
    return ui->reconstructCheckBox->isChecked();
}

bool OpenFileDialog::strictModeEnabled() const {
    return ui->strictCheckBox->isChecked();
}

void OpenFileDialog::slotInspectorToggled(bool on) {
    // the inspected document is never given the geometry it does not have
    if (on) {
        ui->reconstructCheckBox->setChecked(false);
    }
    ui->reconstructCheckBox->setEnabled(!on);
}

void OpenFileDialog::slotAccepted() {
    SettingsManager::instance().setLastDirectory(directory().absolutePath());
}
