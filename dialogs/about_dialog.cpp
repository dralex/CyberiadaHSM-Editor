/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The About Dialog
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

#include <QVBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QDialogButtonBox>
#include <Qt>

#include "about_dialog.h"
#include "version.h"

AboutDialog::AboutDialog(QWidget* parent):
    QDialog(parent)
{
    setWindowTitle(tr("About %1").arg(CYBERIADA_APP_NAME));

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignHCenter);

    QLabel* logo = new QLabel(this);
    QPixmap pixmap(":/Icons/images/logo.png");
    if (!pixmap.isNull()) {
        logo->setPixmap(pixmap.scaledToWidth(320, Qt::SmoothTransformation));
    }
    logo->setAlignment(Qt::AlignCenter);
    layout->addWidget(logo);

    QLabel* name = new QLabel(QString("<h2>%1</h2>").arg(CYBERIADA_APP_NAME), this);
    name->setAlignment(Qt::AlignCenter);
    layout->addWidget(name);

    // the version and the build revision on one line
    QLabel* version = new QLabel(tr("Version %1 (revision %2)")
                                 .arg(CYBERIADA_VERSION).arg(CYBERIADA_REVISION), this);
    version->setAlignment(Qt::AlignCenter);
    layout->addWidget(version);

    QLabel* authors = new QLabel(
        tr("<p align=\"center\">Alexey Fedoseev &lt;aleksey@fedoseev.net&gt;<br>"
           "Anastasia Viktorova &lt;viktorovaa.04@gmail.com&gt;</p>"), this);
    authors->setTextInteractionFlags(Qt::TextBrowserInteraction);
    authors->setOpenExternalLinks(true);
    layout->addWidget(authors);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
