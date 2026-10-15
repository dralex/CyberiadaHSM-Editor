/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
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

#ifndef PLUGIN_VALUES_H
#define PLUGIN_VALUES_H

#include <QMap>
#include <QString>
#include <QStringList>

#include "plugin_descriptor.h"

// the values by parameter id, as text: a bool is true/false
typedef QMap<QString, QString> PluginValues;

// the document file as the variables see it; empty for an untitled document
struct PluginDocumentPaths {
    QString path;
    QString dir;
    QString name;

    bool untitled() const { return path.isEmpty(); }
};

PluginDocumentPaths pluginDocumentPaths(const QString& documentPath);

// the default of a parameter for the document: ${document_dir} and
// ${document_name} expand, and such a default is empty for an untitled one
QString pluginDefaultValue(const PluginParameter& parameter, const PluginDocumentPaths& document);
// the defaults of the action's parameters
PluginValues pluginDefaultValues(const PluginDescriptor& plugin, const PluginAction& action,
                                 const PluginDocumentPaths& document);
// false with the reason when the value does not fit the type; an empty value
// is checked by pluginMissingValue
bool pluginCheckValue(const PluginParameter& parameter, const QString& value, QString* error);
// the first required parameter of the action without a value, empty if none
QString pluginMissingValue(const PluginDescriptor& plugin, const PluginAction& action,
                           const PluginValues& values);
// the batch values: the defaults updated by name=value items (@file reads
// the value, @@ stands for a leading @); false with the reason
bool pluginParseValues(const PluginDescriptor& plugin, const PluginAction& action,
                       const QStringList& nameValues, const PluginDocumentPaths& document,
                       PluginValues* values, QString* error);
// the settings key of a document file: the SHA-1 of its canonical path
QString pluginDocumentKey(const QString& documentPath);
// a condition holds when its bool is true or another value is not empty
bool pluginConditionHolds(const PluginParameter& parameter, const QString& value);

#endif
