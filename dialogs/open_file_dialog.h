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

#ifndef OPEN_FILE_DIALOG_H
#define OPEN_FILE_DIALOG_H

#include "file_dialog.h"

namespace Ui {
class OpenFileOptions;
}

// the file browser and the open options in a single window
class OpenFileDialog : public CyberiadaFileDialog
{
    Q_OBJECT

public:
    explicit OpenFileDialog(QWidget *parent = nullptr);
    ~OpenFileDialog();

    bool inspectorModeEnabled() const;
    bool reconstructionEnabled() const;
    bool reconstructionSMEnabled() const;
    bool strictModeEnabled() const;

private slots:
    void slotInspectorToggled(bool on);
    void slotReconstructToggled(bool on);

private:
    Ui::OpenFileOptions *ui;
    QWidget             *options;
};

#endif // OPEN_FILE_DIALOG_H
