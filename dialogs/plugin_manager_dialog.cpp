/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
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
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "plugin_manager_dialog.h"
#include "document_mode.h"
#include "plugin_parameter_dialog.h"
#include "plugin_registry.h"
#include "settings_manager.h"

// the least height of the list, in text lines
#define MANAGER_LINES 16

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
    fill();

    // every column fits its content: the reasons and the folder paths are long
    tree->resizeColumnToContents(columnFolder);
    int wanted = tree->frameWidth() * 2 + layout->contentsMargins().left() + layout->contentsMargins().right();
    for (int c = 0; c < columnsCount; c++) wanted += tree->header()->sectionSize(c);
    wanted += tree->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    int height = qMax(sizeHint().height(), fontMetrics().lineSpacing() * MANAGER_LINES);
    resize(pluginDialogWidth(this, qMax(wanted, sizeHint().width())), height);
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
        // the languages it brings (EDIT-PLUG-60)
        if (!e.languages.isEmpty()) {
            QStringList names;
            for (const QString& id: e.languages) names.append(modeName(id));
            QTreeWidgetItem* row = new QTreeWidgetItem(item);
            row->setText(columnName, tr("Languages"));
            row->setText(columnState, names.join(", "));
        }
        // the actions off by their requires (EDIT-PLUG-54)
        for (const PluginAction& a: e.descriptor.actions) {
            if (!e.unavailable.contains(a.id)) continue;
            QTreeWidgetItem* row = new QTreeWidgetItem(item);
            QString reason = tr("unavailable: %1").arg(e.unavailable.value(a.id));
            row->setText(columnName, a.label);
            row->setText(columnState, reason);
            row->setToolTip(columnState, reason);
        }
        item->setExpanded(true);
    }
    for (int c = 0; c < columnsCount - 1; c++) tree->resizeColumnToContents(c);
}

void PluginManagerDialog::slotItemChanged(QTreeWidgetItem* item, int column)
{
    QString id = item->data(columnName, Qt::UserRole).toString();
    if (column != columnName || id.isEmpty()) return;
    SettingsManager::instance().setPluginEnabled(id, item->checkState(columnName) == Qt::Checked);
}
