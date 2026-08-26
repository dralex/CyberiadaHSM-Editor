/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The Save File Dialog
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
#include <QComboBox>
#include <QLabel>
#include <QStandardItemModel>

#include "save_file_dialog.h"
#include "ui_save_file_dialog.h"

// the reason a format cannot be used is kept beside the format itself
static const int formatReasonRole = Qt::UserRole + 1;


SaveFileDialog::SaveFileDialog(QWidget* parent, const Cyberiada::LocalDocument* document)
    : CyberiadaFileDialog(parent, tr("Сохранить диаграмму"),
                          tr("CyberiadaML graph (*.graphml)"))
    , ui(new Ui::SaveFileOptions)
    , options(new QWidget(this))
{
    setFileMode(QFileDialog::AnyFile);
    setAcceptMode(QFileDialog::AcceptSave);
    setDefaultSuffix("graphml");
    setLabelText(QFileDialog::Accept, tr("Сохранить"));

    ui->setupUi(options);
    setOptionsWidget(options);

    // the yEd format keeps a single state machine with the geometry and has no
    // shape for the terminate pseudostate
    QString yed_reason;
    if (document) {
        if (document->get_state_machines().size() != 1) {
            yed_reason = tr("формат yEd хранит только одну машину состояний");
        } else if (!document->has_geometry()) {
            yed_reason = tr("формат yEd требует геометрию диаграммы");
        } else if (!document->find_elements_by_type(Cyberiada::elementTerminate).empty()) {
            yed_reason = tr("формат yEd не поддерживает терминальные псевдосостояния");
        }
    } else {
        yed_reason = tr("документ не открыт");
    }

    addFormat(Cyberiada::formatCyberiada10, tr("CyberiadaML-GraphML 1.0"),
              ":/Icons/images/format-cyberiada.png", QString());
    addFormat(Cyberiada::formatLegacyYEDOstranna, tr("yEd Ostranna"),
              ":/Icons/images/format-yed.png", yed_reason);
    addFormat(Cyberiada::formatLegacyYEDBerloga16, tr("yEd Berloga 1.6"),
              ":/Icons/images/format-yed.png", yed_reason);

    connect(ui->formatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotFormatChanged(int)));
    connect(ui->skipGeometryCheckBox, &QCheckBox::toggled, this, &SaveFileDialog::slotSkipGeometryToggled);
    slotFormatChanged(ui->formatComboBox->currentIndex());
}

SaveFileDialog::~SaveFileDialog()
{
    delete ui;
}

void SaveFileDialog::addFormat(Cyberiada::DocumentFormat format, const QString& name,
                               const QString& icon, const QString& reason)
{
    ui->formatComboBox->addItem(QIcon(icon), name, int(format));
    int index = ui->formatComboBox->count() - 1;
    ui->formatComboBox->setItemData(index, reason, formatReasonRole);
    if (!reason.isEmpty()) {
        // the format is shown but cannot be chosen
        QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->formatComboBox->model());
        if (model && model->item(index)) {
            model->item(index)->setEnabled(false);
        }
    }
}

Cyberiada::DocumentFormat SaveFileDialog::selectedFormat() const
{
    return Cyberiada::DocumentFormat(ui->formatComboBox->currentData().toInt());
}

bool SaveFileDialog::roundEnabled() const {
    return ui->roundCheckBox->isChecked();
}

bool SaveFileDialog::skipGeometryEnabled() const {
    return ui->skipGeometryCheckBox->isEnabled() && ui->skipGeometryCheckBox->isChecked();
}

bool SaveFileDialog::checkInitialEnabled() const {
    return ui->checkInitialCheckBox->isEnabled() && ui->checkInitialCheckBox->isChecked();
}

bool SaveFileDialog::strictActionsEnabled() const {
    return ui->strictActionsCheckBox->isEnabled() && ui->strictActionsCheckBox->isChecked();
}

bool SaveFileDialog::skipEmptyBehaviorEnabled() const {
    return ui->skipEmptyCheckBox->isEnabled() && ui->skipEmptyCheckBox->isChecked();
}

void SaveFileDialog::slotFormatChanged(int index)
{
    // the geometry is mandatory for the yEd formats
    bool yed = Cyberiada::is_legacy_yed_format(selectedFormat());
    if (yed) {
        ui->skipGeometryCheckBox->setChecked(false);
    }
    ui->skipGeometryCheckBox->setEnabled(!yed);

    QString hint;
    for (int i = 0; i < ui->formatComboBox->count(); i++) {
        QString reason = ui->formatComboBox->itemData(i, formatReasonRole).toString();
        if (!reason.isEmpty()) {
            hint = tr("Форматы yEd недоступны: ") + reason;
            break;
        }
    }
    ui->formatHintLabel->setText(hint);
    ui->formatHintLabel->setVisible(!hint.isEmpty());
    Q_UNUSED(index);
}

void SaveFileDialog::slotSkipGeometryToggled(bool on)
{
    // the library allows no other option beside the skipped geometry
    ui->roundCheckBox->setEnabled(!on);
    ui->checkInitialCheckBox->setEnabled(!on);
    ui->strictActionsCheckBox->setEnabled(!on);
    ui->skipEmptyCheckBox->setEnabled(!on);
}
