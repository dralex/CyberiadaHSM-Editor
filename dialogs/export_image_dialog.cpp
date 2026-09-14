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

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include "export_image_dialog.h"
#include "settings_manager.h"


ExportImageDialog::ExportImageDialog(QWidget* parent)
    : CyberiadaFileDialog(parent, tr("Export the scene as an image"),
                          tr("PNG (*.png);;JPEG (*.jpg *.jpeg);;BMP (*.bmp);;TIFF (*.tiff);;"
                             "SVG (*.svg);;PDF (*.pdf)")),
      dpiSpin(nullptr)
{
    setFileMode(QFileDialog::AnyFile);
    setAcceptMode(QFileDialog::AcceptSave);
    setLabelText(QFileDialog::Accept, tr("Export"));
    setDefaultSuffix("png");

    // the raster resolution control (96 = one scene unit per pixel); the vector
    // formats are resolution-independent, so it is disabled for them
    QWidget* options = new QWidget(this);
    QHBoxLayout* row = new QHBoxLayout(options);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(new QLabel(tr("Resolution (DPI):"), options));
    dpiSpin = new QSpinBox(options);
    dpiSpin->setRange(48, 1200);
    dpiSpin->setSingleStep(6);
    dpiSpin->setValue(SettingsManager::instance().getExportDpi());
    row->addWidget(dpiSpin);
    row->addStretch();
    setOptionsWidget(options);

    connect(this, &QFileDialog::filterSelected, this, &ExportImageDialog::slotFilterSelected);
}

int ExportImageDialog::dpi() const
{
    return dpiSpin ? dpiSpin->value() : 96;
}

void ExportImageDialog::accept()
{
    // the filter may be selected without the user interaction
    updateSuffix();
    if (dpiSpin) SettingsManager::instance().setExportDpi(dpiSpin->value());
    CyberiadaFileDialog::accept();
}

void ExportImageDialog::updateSuffix()
{
    slotFilterSelected(selectedNameFilter());
}

void ExportImageDialog::slotFilterSelected(const QString& filter)
{
    bool vector = false;
    if (filter.startsWith("JPEG")) {
        setDefaultSuffix("jpg");
    } else if (filter.startsWith("BMP")) {
        setDefaultSuffix("bmp");
    } else if (filter.startsWith("TIFF")) {
        setDefaultSuffix("tiff");
    } else if (filter.startsWith("SVG")) {
        setDefaultSuffix("svg");
        vector = true;
    } else if (filter.startsWith("PDF")) {
        setDefaultSuffix("pdf");
        vector = true;
    } else {
        setDefaultSuffix("png");
    }
    // the DPI only affects the raster formats
    if (dpiSpin) dpiSpin->setEnabled(!vector);
}
