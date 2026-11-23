/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The bar of a tab whose file changed on disk
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

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include "disk_bar.h"

DiskBar::DiskBar(QWidget* parent):
    QWidget(parent)
{
    // the colors of a tooltip: a notice in the light and the dark schemes alike
    setAutoFillBackground(true);
    QPalette p = palette();
    p.setColor(QPalette::Window, p.color(QPalette::ToolTipBase));
    p.setColor(QPalette::WindowText, p.color(QPalette::ToolTipText));
    setPalette(p);

    QHBoxLayout* layout = new QHBoxLayout(this);
    m_label = new QLabel(this);
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_label, 1);
    m_reload = new QPushButton(tr("Reload"), this);
    m_keep = new QPushButton(this);
    m_compare = new QPushButton(tr("Compare"), this);
    m_close = new QPushButton(tr("Close"), this);
    for (QPushButton* button : {m_reload, m_keep, m_compare, m_close}) {
        button->setAutoDefault(false);
        layout->addWidget(button);
    }
    connect(m_reload, &QPushButton::clicked, this, &DiskBar::reloadRequested);
    connect(m_keep, &QPushButton::clicked, this, &DiskBar::keepRequested);
    connect(m_compare, &QPushButton::clicked, this, &DiskBar::compareRequested);
    connect(m_close, &QPushButton::clicked, this, &DiskBar::closeRequested);
    hide();
}

void DiskBar::setStatus(EditorDocument::DiskStatus status, const QString& name)
{
    switch (status) {
    case EditorDocument::diskChanged:
        m_label->setText(tr("%1 changed on disk").arg(name));
        break;
    case EditorDocument::diskUnreadable:
        m_label->setText(tr("%1 changed on disk and cannot be read").arg(name));
        break;
    case EditorDocument::diskAbsent:
        m_label->setText(tr("%1 is not on disk").arg(name));
        break;
    default:
        hide();
        return;
    }
    bool absent = status == EditorDocument::diskAbsent;
    m_reload->setVisible(!absent);
    m_keep->setText(absent ? tr("Keep") : tr("Keep my edits"));
    m_compare->setVisible(status == EditorDocument::diskChanged);
    m_close->setVisible(absent);
    show();
}

QString DiskBar::text() const
{
    return isHidden() ? QString() : m_label->text();
}
