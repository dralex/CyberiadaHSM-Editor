/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
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

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

#include "plugin_registry.h"
#include "interface_language.h"

QString PluginEntry::name() const
{
    return descriptor.id.isEmpty() ? QFileInfo(folder).fileName() : descriptor.id;
}

QString PluginEntry::statusName(Status status)
{
    switch (status) {
    case statusEnabled:  return "enabled";
    case statusDisabled: return "disabled";
    case statusShadowed: return "shadowed";
    default:             return "rejected";
    }
}

QString PluginEntry::statusText(Status status)
{
    switch (status) {
    case statusEnabled:  return PluginRegistry::tr("enabled", "plugin status");
    case statusDisabled: return PluginRegistry::tr("disabled", "plugin status");
    case statusShadowed: return PluginRegistry::tr("shadowed", "plugin status");
    default:             return PluginRegistry::tr("rejected", "plugin status");
    }
}

PluginRegistry& PluginRegistry::instance()
{
    static PluginRegistry registry;
    return registry;
}

void PluginRegistry::setSearchPath(const QStringList& dirs)
{
    customPath = dirs;
}

QList<PluginFolder> PluginRegistry::defaultFolders(const QString& appDir, const QString& variable,
                                                   const QString& userDir)
{
    QList<PluginFolder> folders;
    int n = 1;
    for (const QString& dir: variable.split(QDir::listSeparator(), QString::SkipEmptyParts)) {
        folders.append(PluginFolder{QString("env %1").arg(n++), QDir(dir).absolutePath()});
    }
    if (!userDir.isEmpty()) folders.append(PluginFolder{"user", QDir(userDir).absolutePath()});
    // a Debian install has the share dir above bin; the Windows zip next to the exe
    folders.append(PluginFolder{"system", QDir(appDir + "/../" PLUGIN_SYSTEM_FOLDER).absolutePath()});
    folders.append(PluginFolder{"system", QDir(appDir + "/" PLUGIN_SYSTEM_FOLDER).absolutePath()});
    return folders;
}

QList<PluginFolder> PluginRegistry::searchPath() const
{
    if (!customPath.isEmpty()) {
        QList<PluginFolder> folders;
        int n = 1;
        for (const QString& dir: customPath) {
            folders.append(PluginFolder{QString("plugin-path %1").arg(n++), QDir(dir).absolutePath()});
        }
        return folders;
    }
    QString user = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!user.isEmpty()) user += "/plugins";
    return defaultFolders(QCoreApplication::applicationDirPath(),
                          QString::fromLocal8Bit(qgetenv(PLUGIN_PATH_VARIABLE)), user);
}

void PluginRegistry::setInterpreterPaths(const QMap<QString, QString>& paths)
{
    interpreterPaths = paths;
}

#ifdef Q_OS_WIN
// the python3 of the Microsoft Store is a stub that only opens the store
static bool storeStub(const QString& path)
{
    return path.contains("WindowsApps", Qt::CaseInsensitive);
}
#endif

bool PluginRegistry::resolveInterpreter(const QString& name, const QMap<QString, QString>& paths,
                                        QString* program, QStringList* arguments)
{
    arguments->clear();
    QString configured = paths.value(name);
    if (!configured.isEmpty()) {
        QFileInfo info(configured);
        if (info.isFile() && info.isExecutable()) {
            *program = info.absoluteFilePath();
            return true;
        }
        return false;
    }
    QString found = QStandardPaths::findExecutable(name);
#ifdef Q_OS_WIN
    if (storeStub(found)) found.clear();
    if (found.isEmpty() && name == "python3") {
        found = QStandardPaths::findExecutable("python");
        if (storeStub(found)) found.clear();
        if (found.isEmpty()) {
            found = QStandardPaths::findExecutable("py");
            if (!found.isEmpty()) arguments->append("-3");
        }
    }
#endif
    if (found.isEmpty()) return false;
    *program = found;
    return true;
}

QString PluginRegistry::language() const
{
    return languageOverride.isEmpty() ? currentUiLanguage() : languageOverride;
}

void PluginRegistry::scan()
{
    list.clear();
    // the source texts are English; another language comes from i18n/ (EDIT-UI-4)
    QString translation = language() == UI_LANGUAGE_SOURCE ? QString() : language();
    QSet<QString> seenFolders;
    QMap<QString, QString> owners;
    for (const PluginFolder& folder: searchPath()) {
        QDir dir(folder.path);
        if (!dir.exists()) continue;
        QString canonical = dir.canonicalPath();
        if (seenFolders.contains(canonical)) continue;
        seenFolders.insert(canonical);
        QStringList names = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString& name: names) {
            QString pluginDir = dir.absoluteFilePath(name);
            QFile file(QDir(pluginDir).filePath(PLUGIN_DESCRIPTOR_FILE));
            if (!file.exists()) continue;
            PluginEntry entry;
            entry.origin = folder.origin;
            entry.folder = pluginDir;
            QString error;
            if (!file.open(QIODevice::ReadOnly)) {
                entry.reason = PluginRegistry::tr("cannot read %1").arg(PLUGIN_DESCRIPTOR_FILE);
            } else if (!PluginDescriptor::parse(file.readAll(), pluginDir, &entry.descriptor, &error)) {
                entry.descriptor = PluginDescriptor();
                entry.reason = error;
            } else {
                entry.descriptor.translate(translation);
                if (owners.contains(entry.descriptor.id)) {
                    // a rejected descriptor claims no id, so it never shadows (EDIT-PLUG-1)
                    entry.status = PluginEntry::statusShadowed;
                    entry.reason = PluginRegistry::tr("shadowed by %1").arg(owners.value(entry.descriptor.id));
                } else {
                    owners.insert(entry.descriptor.id, entry.origin);
                    entry.status = PluginEntry::statusEnabled;
                    const QString& interpreter = entry.descriptor.interpreter;
                    if (!interpreter.isEmpty() &&
                        !resolveInterpreter(interpreter, interpreterPaths, &entry.program,
                                            &entry.programArguments)) {
                        entry.status = PluginEntry::statusDisabled;
                        entry.reason = PluginRegistry::tr("the interpreter '%1' is not found")
                            .arg(interpreter);
                    }
                }
            }
            list.append(entry);
        }
    }
    emit scanned();
}

const PluginEntry* PluginRegistry::find(const QString& id) const
{
    for (const PluginEntry& e: list) {
        if (e.usable() && e.descriptor.id == id) return &e;
    }
    return nullptr;
}
