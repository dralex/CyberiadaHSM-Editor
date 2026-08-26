/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The Base File Dialog
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


#include <QGridLayout>

#include "file_dialog.h"
#include "settings_manager.h"


CyberiadaFileDialog::CyberiadaFileDialog(QWidget* parent, const QString& caption, const QString& filter)
    : QFileDialog(parent, caption, SettingsManager::instance().getLastDirectory(), filter)
{
    // the options are placed into the dialog layout, so the Qt browser is required
    setOption(QFileDialog::DontUseNativeDialog, true);
    resize(SettingsManager::instance().getDialogSize());
}

QString CyberiadaFileDialog::selectedFile() const
{
    return selectedFiles().value(0);
}

void CyberiadaFileDialog::setOptionsWidget(QWidget* options)
{
    QGridLayout* grid = qobject_cast<QGridLayout*>(layout());
    if (grid) {
        grid->addWidget(options, grid->rowCount(), 0, 1, grid->columnCount());
    }
}

void CyberiadaFileDialog::done(int result)
{
    SettingsManager::instance().setDialogSize(size());
    if (result == QDialog::Accepted) {
        SettingsManager::instance().setLastDirectory(directory().absolutePath());
    }
    QFileDialog::done(result);
}
