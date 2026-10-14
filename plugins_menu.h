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

#ifndef PLUGINS_MENU_H
#define PLUGINS_MENU_H

#include <QList>
#include <QMap>
#include <QObject>
#include <QPair>
#include <QString>

class QAction;
class QMainWindow;
class QMenu;
class QToolBar;
struct PluginEntry;
struct PluginAction;

// Plugins before Help: a submenu per plugin (EDIT-PLUG-5); the actions marked
// toolbar form one toolbar per plugin right of the main toolbar (EDIT-PLUG-6);
// the menu and the toolbar share each action
class PluginsMenu : public QObject {
    Q_OBJECT

public:
    // the menu goes before the help menu, the toolbars before the anchor
    // toolbar, the Plugin toolbars submenu before the view entry
    PluginsMenu(QMainWindow* window, QMenu* helpMenu, QToolBar* anchor, QMenu* viewMenu,
                QAction* viewBefore);

    QMenu*      menu() const { return pluginsMenu; }
    QMenu*      toolbarsMenu() const { return viewToolbars; }
    // the entries below the plugins: the output panel, the plugin manager
    void        setTailActions(const QList<QAction*>& actions);
    // the menu and the toolbars from the registry, after a scan
    void        rebuild();

    // the action of a plugin action; the toolbar of a plugin (the tests use them)
    QAction*    action(const QString& pluginId, const QString& actionId) const;
    QAction*    settingsAction(const QString& pluginId) const { return settings.value(pluginId); }
    QToolBar*   toolbar(const QString& pluginId) const { return toolbars.value(pluginId); }
    QList<QPair<QString, QString> > actionIds() const { return actions.keys(); }

signals:
    void        actionTriggered(const QString& pluginId, const QString& actionId);
    void        settingsTriggered(const QString& pluginId);

private:
    void        clear();

    QMainWindow* window;
    QMenu*      pluginsMenu;
    QToolBar*   anchor;
    QMenu*      viewToolbars;
    QList<QAction*> tail;
    QMap<QPair<QString, QString>, QAction*> actions;
    QMap<QString, QAction*> settings;
    QMap<QString, QToolBar*> toolbars;
    QList<QMenu*> submenus;
};

#endif
