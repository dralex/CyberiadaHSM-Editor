/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The Open File Dialog
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

#include "open_file_dialog.h"
#include "ui_open_file_dialog.h"


OpenFileDialog::OpenFileDialog(QWidget *parent)
    : CyberiadaFileDialog(parent, tr("Открыть файл диаграммы"),
                          tr("CyberiadaML graph (*.graphml)"))
    , ui(new Ui::OpenFileOptions)
    , options(new QWidget(this))
{
    setFileMode(QFileDialog::ExistingFile);
    setAcceptMode(QFileDialog::AcceptOpen);
    setLabelText(QFileDialog::Accept, tr("Открыть"));

    ui->setupUi(options);
    setOptionsWidget(options);

    // documents open in edit mode by default; inspection is opt-in
    ui->inspectorCheckBox->setChecked(false);
    slotInspectorToggled(false);
    slotReconstructToggled(false);

    connect(ui->inspectorCheckBox, &QCheckBox::toggled, this, &OpenFileDialog::slotInspectorToggled);
    connect(ui->reconstructCheckBox, &QCheckBox::toggled, this, &OpenFileDialog::slotReconstructToggled);
}

OpenFileDialog::~OpenFileDialog()
{
    delete ui;
}

bool OpenFileDialog::inspectorModeEnabled() const {
    return ui->inspectorCheckBox->isChecked();
}

bool OpenFileDialog::reconstructionEnabled() const {
    return ui->reconstructCheckBox->isChecked();
}

bool OpenFileDialog::reconstructionSMEnabled() const {
    return ui->reconstructSMCheckBox->isChecked();
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

void OpenFileDialog::slotReconstructToggled(bool on) {
    // the library creates the border only within the reconstruction
    if (!on) {
        ui->reconstructSMCheckBox->setChecked(false);
    }
    ui->reconstructSMCheckBox->setEnabled(on);
}
