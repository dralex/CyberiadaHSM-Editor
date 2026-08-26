/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The Image Export Dialog
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

#ifndef EXPORT_IMAGE_DIALOG_H
#define EXPORT_IMAGE_DIALOG_H

#include "file_dialog.h"

// the scene image export: the suffix follows the selected image format
class ExportImageDialog : public CyberiadaFileDialog
{
    Q_OBJECT

public:
    explicit ExportImageDialog(QWidget* parent = nullptr);

    // the suffix of the selected image format
    void updateSuffix();

    void accept() override;

private slots:
    void slotFilterSelected(const QString& filter);
};

#endif // EXPORT_IMAGE_DIALOG_H
