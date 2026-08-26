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

#ifndef SAVE_FILE_DIALOG_H
#define SAVE_FILE_DIALOG_H

#include <cyberiadamlpp.h>

#include "file_dialog.h"

namespace Ui {
class SaveFileOptions;
}

// the file browser and the save options in a single window; the formats the
// open document cannot be written in are disabled with the reason shown
class SaveFileDialog : public CyberiadaFileDialog
{
    Q_OBJECT

public:
    SaveFileDialog(QWidget* parent, const Cyberiada::LocalDocument* document);
    ~SaveFileDialog();

    Cyberiada::DocumentFormat selectedFormat() const;
    bool roundEnabled() const;
    bool skipGeometryEnabled() const;
    bool checkInitialEnabled() const;
    bool strictActionsEnabled() const;
    bool skipEmptyBehaviorEnabled() const;

private slots:
    void slotFormatChanged(int index);
    void slotSkipGeometryToggled(bool on);

private:
    void addFormat(Cyberiada::DocumentFormat format, const QString& name,
                   const QString& icon, const QString& reason);

    Ui::SaveFileOptions* ui;
    QWidget*             options;
};

#endif // SAVE_FILE_DIALOG_H
