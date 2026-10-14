/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Plugins Menu and the Plugin Toolbars (docs/PLUGINS.md, The interface)
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

#include <QAction>
#include <QFile>
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <algorithm>

#include "plugins_menu.h"
#include "plugin_registry.h"
#include "settings_manager.h"

PluginsMenu::PluginsMenu(QMainWindow* window, QMenu* helpMenu, QToolBar* anchor, QMenu* viewMenu,
                         QAction* viewBefore):
    QObject(window), window(window), anchor(anchor)
{
    pluginsMenu = new QMenu(tr("Plugins"), window);
    pluginsMenu->setObjectName("menuPlugins");
    pluginsMenu->setToolTipsVisible(true);
    // Edit > Tools holds the element tools, so the plugins get a menu of their own
    window->menuBar()->insertMenu(helpMenu->menuAction(), pluginsMenu);
    viewToolbars = new QMenu(tr("Plugin toolbars"), window);
    viewToolbars->setObjectName("menuPluginToolbars");
    viewMenu->insertMenu(viewBefore, viewToolbars);
}

void PluginsMenu::setTailActions(const QList<QAction*>& list)
{
    tail = list;
    rebuild();
}

void PluginsMenu::clear()
{
    pluginsMenu->clear();
    viewToolbars->clear();
    qDeleteAll(actions);
    actions.clear();
    settings.clear();
    qDeleteAll(submenus);
    submenus.clear();
    for (QToolBar* tb: toolbars) {
        window->removeToolBar(tb);
        delete tb;
    }
    toolbars.clear();
}

void PluginsMenu::rebuild()
{
    clear();
    QList<const PluginEntry*> entries;
    for (const PluginEntry& e: PluginRegistry::instance().entries()) {
        if (e.usable()) entries.append(&e);
    }
    std::sort(entries.begin(), entries.end(), [](const PluginEntry* a, const PluginEntry* b) {
        return a->descriptor.name.compare(b->descriptor.name, Qt::CaseInsensitive) < 0;
    });

    for (const PluginEntry* e: entries) {
        const PluginDescriptor& d = e->descriptor;
        const QString pluginId = d.id;
        // a single action without stored values is a single entry (EDIT-PLUG-5)
        bool single = d.actions.size() == 1 && !d.hasStoredParameters();
        QMenu* target = pluginsMenu;
        if (!single) {
            target = new QMenu(d.name, pluginsMenu);
            target->setToolTipsVisible(true);
            pluginsMenu->addMenu(target);
            submenus.append(target);
        }
        QToolBar* tb = nullptr;
        for (const PluginAction& a: d.actions) {
            QAction* act = new QAction(a.label, this);
            QString icon = d.iconPath(a.icon.isEmpty() ? d.icon : a.icon);
            if (!icon.isEmpty() && QFile::exists(icon)) act->setIcon(QIcon(icon));
            const QString actionId = a.id;
            connect(act, &QAction::triggered, this, [this, pluginId, actionId]() {
                emit actionTriggered(pluginId, actionId);
            });
            target->addAction(act);
            actions.insert(qMakePair(pluginId, actionId), act);
            if (!a.toolbar) continue;
            if (!tb) {
                tb = new QToolBar(d.name, window);
                tb->setObjectName("pluginToolBar-" + pluginId);
                window->insertToolBar(anchor, tb);
                toolbars.insert(pluginId, tb);
            }
            tb->addAction(act);
        }
        if (d.hasStoredParameters()) {
            target->addSeparator();
            QAction* s = target->addAction(tr("Settings..."));
            connect(s, &QAction::triggered, this, [this, pluginId]() { emit settingsTriggered(pluginId); });
            settings.insert(pluginId, s);
        }
        if (tb) {
            tb->setVisible(SettingsManager::instance().getPluginToolbarVisible(pluginId));
            QAction* toggle = tb->toggleViewAction();
            viewToolbars->addAction(toggle);
            // only the user's choice is kept (EDIT-PLUG-6)
            connect(toggle, &QAction::triggered, this, [pluginId](bool on) {
                SettingsManager::instance().setPluginToolbarVisible(pluginId, on);
            });
        }
    }
    if (entries.isEmpty()) {
        QAction* none = pluginsMenu->addAction(tr("No plugins"));
        none->setEnabled(false);
    }
    if (!tail.isEmpty()) {
        pluginsMenu->addSeparator();
        for (QAction* a: tail) pluginsMenu->addAction(a);
    }
    viewToolbars->menuAction()->setEnabled(!toolbars.isEmpty());
}

QAction* PluginsMenu::action(const QString& pluginId, const QString& actionId) const
{
    return actions.value(qMakePair(pluginId, actionId), nullptr);
}
