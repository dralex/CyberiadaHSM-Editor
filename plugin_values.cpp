/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Values of the Plugin Parameters
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
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

#include "plugin_values.h"
#include "plugin_registry.h"

#define DOCUMENT_DIR_VARIABLE  "${document_dir}"
#define DOCUMENT_NAME_VARIABLE "${document_name}"
#define LANGUAGE_VARIABLE      "${language}"

PluginDocumentPaths pluginDocumentPaths(const QString& documentPath)
{
    PluginDocumentPaths paths;
    if (documentPath.isEmpty()) return paths;
    QFileInfo info(documentPath);
    paths.path = info.absoluteFilePath();
    paths.dir = info.absolutePath();
    paths.name = info.completeBaseName();
    return paths;
}

QString pluginDefaultValue(const PluginParameter& parameter, const PluginDocumentPaths& document)
{
    QString value = parameter.defaultValue;
    value.replace(LANGUAGE_VARIABLE, PluginRegistry::instance().language());
    bool usesDocument = value.contains(DOCUMENT_DIR_VARIABLE) || value.contains(DOCUMENT_NAME_VARIABLE);
    if (!usesDocument) return value;
    if (document.untitled()) return QString();
    value.replace(DOCUMENT_DIR_VARIABLE, document.dir);
    value.replace(DOCUMENT_NAME_VARIABLE, document.name);
    return value;
}

PluginValues pluginDefaultValues(const PluginDescriptor& plugin, const PluginAction& action,
                                 const PluginDocumentPaths& document)
{
    PluginValues values;
    for (const PluginParameter* p: plugin.actionParameters(action)) {
        values.insert(p->id, pluginDefaultValue(*p, document));
    }
    return values;
}

bool pluginCheckValue(const PluginParameter& parameter, const QString& value, QString* error)
{
    if (value.isEmpty()) return true;
    bool ok = true;
    double number = 0.0;
    switch (parameter.type) {
    case PluginParameter::typeInt:
        number = value.toInt(&ok);
        break;
    case PluginParameter::typeDouble:
        number = value.toDouble(&ok);
        break;
    case PluginParameter::typeBool:
        ok = value == "true" || value == "false";
        break;
    case PluginParameter::typeEnum:
        ok = parameter.choices.contains(value);
        break;
    case PluginParameter::typeSecret:
        ok = (value.startsWith("env:") && value.size() > 4) ||
             (value.startsWith("file:") && value.size() > 5);
        break;
    default:
        return true;
    }
    if (ok && parameter.hasMin && number < parameter.min) ok = false;
    if (ok && parameter.hasMax && number > parameter.max) ok = false;
    if (!ok && error) {
        *error = QCoreApplication::translate("PluginValues", "the value '%1' does not fit the %2 parameter '%3'")
            .arg(parameter.type == PluginParameter::typeSecret ? QString("...") : value,
                 PluginDescriptor::typeName(parameter.type), parameter.id);
    }
    return ok;
}

QString pluginMissingValue(const PluginDescriptor& plugin, const PluginAction& action,
                           const PluginValues& values)
{
    for (const PluginParameter* p: plugin.actionParameters(action)) {
        if (p->required && values.value(p->id).isEmpty()) return p->id;
    }
    return QString();
}

bool pluginParseValues(const PluginDescriptor& plugin, const PluginAction& action,
                       const QStringList& nameValues, const PluginDocumentPaths& document,
                       PluginValues* values, QString* error)
{
    *values = pluginDefaultValues(plugin, action, document);
    for (const QString& item: nameValues) {
        int eq = item.indexOf('=');
        if (eq <= 0) {
            if (error) *error = QCoreApplication::translate("PluginValues", "the plugin parameter '%1' is not name=value").arg(item);
            return false;
        }
        QString name = item.left(eq);
        QString value = item.mid(eq + 1);
        if (!values->contains(name)) {
            if (error) *error = QCoreApplication::translate("PluginValues", "the action '%1' has no parameter '%2'").arg(action.id, name);
            return false;
        }
        if (value.startsWith("@@")) {
            value = value.mid(1);
        } else if (value.startsWith('@')) {
            QFile f(value.mid(1));
            if (!f.open(QIODevice::ReadOnly)) {
                if (error) *error = QCoreApplication::translate("PluginValues", "cannot read the value of '%1' from %2").arg(name, value.mid(1));
                return false;
            }
            value = QString::fromUtf8(f.readAll());
            // a value file ends with a newline the value does not have
            if (value.endsWith("\r\n")) value.chop(2);
            else if (value.endsWith('\n')) value.chop(1);
        }
        values->insert(name, value);
    }
    for (const PluginParameter* p: plugin.actionParameters(action)) {
        if (!pluginCheckValue(*p, values->value(p->id), error)) return false;
    }
    QString missing = pluginMissingValue(plugin, action, *values);
    if (!missing.isEmpty()) {
        if (error) *error = QCoreApplication::translate("PluginValues", "the required parameter '%1' has no value").arg(missing);
        return false;
    }
    return true;
}

QString pluginDocumentKey(const QString& documentPath)
{
    QFileInfo info(documentPath);
    QString path = info.exists() ? info.canonicalFilePath() : info.absoluteFilePath();
#ifdef Q_OS_WIN
    path = path.toLower();
#endif
    return QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex());
}

bool pluginConditionHolds(const PluginParameter& parameter, const QString& value)
{
    if (parameter.type == PluginParameter::typeBool) return value == "true";
    return !value.isEmpty();
}
