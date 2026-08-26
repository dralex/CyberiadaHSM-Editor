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

#include "export_image_dialog.h"


ExportImageDialog::ExportImageDialog(QWidget* parent)
    : CyberiadaFileDialog(parent, tr("Экспорт сцены как изображение"),
                          tr("PNG (*.png);;JPEG (*.jpg *.jpeg);;BMP (*.bmp);;TIFF (*.tiff)"))
{
    setFileMode(QFileDialog::AnyFile);
    setAcceptMode(QFileDialog::AcceptSave);
    setLabelText(QFileDialog::Accept, tr("Экспортировать"));
    setDefaultSuffix("png");

    connect(this, &QFileDialog::filterSelected, this, &ExportImageDialog::slotFilterSelected);
}

void ExportImageDialog::accept()
{
    // the filter may be selected without the user interaction
    updateSuffix();
    CyberiadaFileDialog::accept();
}

void ExportImageDialog::updateSuffix()
{
    slotFilterSelected(selectedNameFilter());
}

void ExportImageDialog::slotFilterSelected(const QString& filter)
{
    if (filter.startsWith("JPEG")) {
        setDefaultSuffix("jpg");
    } else if (filter.startsWith("BMP")) {
        setDefaultSuffix("bmp");
    } else if (filter.startsWith("TIFF")) {
        setDefaultSuffix("tiff");
    } else {
        setDefaultSuffix("png");
    }
}
