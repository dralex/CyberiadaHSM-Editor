/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Plugin Manager: the plugins found, their state and the enable switch
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

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "plugin_manager_dialog.h"
#include "plugin_registry.h"
#include "settings_manager.h"

enum ManagerColumn {
    columnName = 0,
    columnVersion,
    columnState,
    columnFolder,
    columnsCount
};

PluginManagerDialog::PluginManagerDialog(QWidget* parent):
    QDialog(parent)
{
    setWindowTitle(tr("Plugin Manager"));
    QVBoxLayout* layout = new QVBoxLayout(this);
    tree = new QTreeWidget(this);
    tree->setColumnCount(columnsCount);
    tree->setHeaderLabels(QStringList() << tr("Plugin") << tr("Version") << tr("State") << tr("Folder"));
    tree->setRootIsDecorated(false);
    tree->header()->setStretchLastSection(true);
    layout->addWidget(tree);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    QPushButton* rescanButton = buttons->addButton(tr("Rescan"), QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(rescanButton, &QPushButton::clicked, this, &PluginManagerDialog::rescan);
    connect(&PluginRegistry::instance(), &PluginRegistry::scanned, this, &PluginManagerDialog::fill);
    connect(tree, &QTreeWidget::itemChanged, this, &PluginManagerDialog::slotItemChanged);
    resize(720, 360);
    fill();
}

void PluginManagerDialog::rescan()
{
    PluginRegistry& registry = PluginRegistry::instance();
    registry.setInterpreterPaths(SettingsManager::instance().getInterpreterPaths());
    registry.scan();
}

void PluginManagerDialog::fill()
{
    QSignalBlocker block(tree);
    tree->clear();
    for (const PluginEntry& e: PluginRegistry::instance().entries()) {
        QTreeWidgetItem* item = new QTreeWidgetItem(tree);
        QString name = e.descriptor.name.isEmpty() ? e.name() : QString("%1 (%2)").arg(e.descriptor.name, e.name());
        item->setText(columnName, name);
        item->setText(columnVersion, e.descriptor.version);
        QString state = PluginEntry::statusText(e.status);
        if (!e.reason.isEmpty()) state += ": " + e.reason;
        item->setText(columnState, state);
        item->setText(columnFolder, QString("%1 (%2)").arg(e.folder, e.origin));
        item->setToolTip(columnState, state);
        if (e.usable()) {
            item->setData(columnName, Qt::UserRole, e.descriptor.id);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(columnName, SettingsManager::instance().getPluginEnabled(e.descriptor.id) ?
                                Qt::Checked : Qt::Unchecked);
        }
    }
    for (int c = 0; c < columnsCount - 1; c++) tree->resizeColumnToContents(c);
}

void PluginManagerDialog::slotItemChanged(QTreeWidgetItem* item, int column)
{
    QString id = item->data(columnName, Qt::UserRole).toString();
    if (column != columnName || id.isEmpty()) return;
    SettingsManager::instance().setPluginEnabled(id, item->checkState(columnName) == Qt::Checked);
}
