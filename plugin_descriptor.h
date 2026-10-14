/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Plugin Descriptor (plugin.json, docs/PLUGIN-API.md)
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

#ifndef PLUGIN_DESCRIPTOR_H
#define PLUGIN_DESCRIPTOR_H

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

#include "cyberiada_constants.h"

struct PluginParameter {
    enum Type {
        typeString = 0,
        typeText,
        typeInt,
        typeDouble,
        typeBool,
        typeEnum,
        typeFile,
        typeDir,
        typeSecret
    };
    enum Scope {
        scopeRun = 0,
        scopePersistent,
        scopeDocument
    };

    QString id;
    Type type = typeString;
    QString label;
    QString description;
    // the default as text: a bool is true/false, a number its decimal form
    QString defaultValue;
    Scope scope = scopeRun;
    bool required = false;

    // the type options
    QString language;            // text: the highlighting rules
    bool hasMin = false;         // int, double
    bool hasMax = false;
    double min = 0.0;
    double max = 0.0;
    QStringList choices;         // enum
    QString filter;              // file
    bool saveFile = false;       // file: choose a new file
    bool create = false;         // dir: make a missing directory
    QString exportName;          // secret: the variable of the child process

    bool isStored() const { return scope != scopeRun; }
};

// an argv element: a plain string, or the strings added when a parameter is
// true or not empty
struct PluginArgument {
    QString text;
    QString condition;
    QStringList arguments;

    bool isConditional() const { return !condition.isEmpty(); }
};

struct PluginAction {
    enum Result {
        resultReplace = 0,
        resultNewDocument,
        resultDiagnostics,
        resultFiles,
        resultLog
    };

    QString id;
    QString label;
    QString description;
    QString icon;
    Result result = resultLog;
    QList<PluginArgument> argv;
    QList<int> exitOk;
    int timeout = PLUGIN_TIMEOUT_DEFAULT;
    // the parameters the action uses; all of them without the list
    bool hasParameterList = false;
    QStringList parameters;
    QString outdir;
    bool loadStrict = false;
    bool loadReconstruct = false;
    bool save = false;
    bool cwdDocument = false;
    bool toolbar = false;

    bool changesDocument() const { return result == resultReplace; }
    // save and cwd: document need the document file to be current
    bool needsSavedDocument() const { return save || cwdDocument; }
};

struct PluginDescriptor {
    int api = 0;
    QString id;
    QString name;
    QString version;
    QString description;
    QString icon;
    QString interpreter;
    // the plugin directory, absolute
    QString dir;
    QList<QPair<QString, QString> > env;
    QList<PluginParameter> parameters;
    QList<PluginAction> actions;
    // the unknown fields, ignored within the major (EDIT-PLUG-3)
    QStringList warnings;

    const PluginAction* action(const QString& actionId) const;
    const PluginParameter* parameter(const QString& parameterId) const;
    QList<const PluginParameter*> actionParameters(const PluginAction& action) const;
    // a plugin with stored parameters gets a Settings... entry
    bool hasStoredParameters() const;
    // the absolute path of an icon given relative to the plugin directory
    QString iconPath(const QString& icon) const;

    // false with the reason when the descriptor breaks a rule of the contract
    static bool parse(const QByteArray& json, const QString& dir,
                      PluginDescriptor* out, QString* error);
    static QString resultName(PluginAction::Result result);
    static QString typeName(PluginParameter::Type type);
    // the id of a plugin, an action or a parameter
    static bool validId(const QString& id);
};

#endif
