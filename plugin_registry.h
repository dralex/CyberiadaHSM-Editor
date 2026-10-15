/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Plugin Registry: the search path, the scan and the interpreters
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

#ifndef PLUGIN_REGISTRY_H
#define PLUGIN_REGISTRY_H

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

#include "plugin_descriptor.h"

// a folder holding plugin directories; the origin names it in the dumps
struct PluginFolder {
    QString origin;
    QString path;
};

struct PluginEntry {
    enum Status {
        statusEnabled = 0,
        statusDisabled,
        statusShadowed,
        statusRejected
    };

    PluginDescriptor descriptor;
    Status status = statusRejected;
    QString reason;
    QString origin;
    // the plugin directory, absolute
    QString folder;
    // the resolved interpreter and its leading arguments (py -3); empty
    // when the plugin runs its argv[0]
    QString program;
    QStringList programArguments;

    // the descriptor id, or the directory name of a rejected descriptor
    QString name() const;
    bool usable() const { return status == statusEnabled || status == statusDisabled; }
    static QString statusName(Status status);
    // the translated status for the GUI; statusName stays the dump token
    static QString statusText(Status status);
};

class PluginRegistry: public QObject {
    Q_OBJECT
public:
    static PluginRegistry& instance();

    // --plugin-path replaces the default folders; an empty list restores them
    void setSearchPath(const QStringList& dirs);
    QList<PluginFolder> searchPath() const;
    // the default folders: the variable's list, the user folder, the system
    // folder next to or above the executable
    static QList<PluginFolder> defaultFolders(const QString& appDir, const QString& variable,
                                              const QString& userDir);
    // the interpreters set in the Preferences, by logical name
    void setInterpreterPaths(const QMap<QString, QString>& paths);
    // the language of the plugin texts and runs: the interface language, or the one
    // set here (the tests); the batch mode has en (EDIT-UI-2)
    void setLanguage(const QString& code) { languageOverride = code; }
    QString language() const;
    // the Preferences, then PATH; on Windows python3 falls back to python and py -3
    static bool resolveInterpreter(const QString& name, const QMap<QString, QString>& paths,
                                   QString* program, QStringList* arguments);

    void scan();
    // every plugin directory found, in the scan order
    const QList<PluginEntry>& entries() const { return list; }
    // the enabled or disabled entry of the id, nullptr if none
    const PluginEntry* find(const QString& id) const;

signals:
    void scanned();

private:
    PluginRegistry() {}
    PluginRegistry(const PluginRegistry&) = delete;
    PluginRegistry& operator=(const PluginRegistry&) = delete;

    QStringList customPath;
    QString languageOverride;
    QMap<QString, QString> interpreterPaths;
    QList<PluginEntry> list;
};

#endif
