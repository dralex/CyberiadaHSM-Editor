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

#ifndef PLUGIN_MANAGER_DIALOG_H
#define PLUGIN_MANAGER_DIALOG_H

#include <QDialog>

class QTreeWidget;
class QTreeWidgetItem;

// every plugin folder found with its state; the switch is kept and Rescan
// rebuilds the menu and the toolbars (EDIT-PLUG-30)
class PluginManagerDialog : public QDialog {
    Q_OBJECT

public:
    PluginManagerDialog(QWidget* parent = nullptr);

    QTreeWidget* list() const { return tree; }

public slots:
    void         rescan();

private slots:
    void         fill();
    void         slotItemChanged(QTreeWidgetItem* item, int column);

private:
    QTreeWidget* tree;
};

#endif
