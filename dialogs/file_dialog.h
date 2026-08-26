/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The Base File Dialog
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


#ifndef FILE_DIALOG_H
#define FILE_DIALOG_H

#include <QFileDialog>

// the file browser and the dialog options in a single window; the browsed
// directory and the window size are shared by the file dialogs of the editor
class CyberiadaFileDialog : public QFileDialog
{
    Q_OBJECT

public:
    CyberiadaFileDialog(QWidget* parent, const QString& caption, const QString& filter);

    QString selectedFile() const;

protected:
    void setOptionsWidget(QWidget* options);
    void done(int result) override;
};

#endif // FILE_DIALOG_H
