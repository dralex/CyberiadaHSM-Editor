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

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <cmath>

#include "plugin_descriptor.h"

namespace {

const QStringList descriptorFields = {
    "api", "id", "name", "version", "description", "icon", "interpreter", "env",
    "parameters", "actions", "session"
};
const QStringList parameterFields = {
    "id", "type", "label", "description", "default", "scope", "required", "language",
    "min", "max", "choices", "filter", "save", "create", "export"
};
const QStringList actionFields = {
    "id", "label", "description", "icon", "result", "argv", "exit_ok", "timeout",
    "parameters", "outdir", "load", "save", "cwd", "toolbar"
};
const QStringList loadFields = { "strict", "reconstruct" };

const char* const typeNames[] = {
    "string", "text", "int", "double", "bool", "enum", "file", "dir", "secret"
};
const char* const scopeNames[] = { "run", "persistent", "document" };
const char* const resultNames[] = { "replace", "new-document", "diagnostics", "files", "log" };

bool fail(QString* error, const QString& message)
{
    if (error) *error = message;
    return false;
}

int lineAt(const QByteArray& json, int offset)
{
    return json.left(offset).count('\n') + 1;
}

void warnUnknown(const QJsonObject& object, const QStringList& known, const QString& prefix,
                 QStringList* warnings)
{
    for (const QString& key: object.keys()) {
        if (!known.contains(key)) {
            warnings->append(QString("unknown field '%1%2'").arg(prefix, key));
        }
    }
}

// an optional string field; false when it is present with another type
bool readString(const QJsonObject& object, const QString& key, const QString& where,
                QString* out, QString* error)
{
    if (!object.contains(key)) return true;
    QJsonValue v = object.value(key);
    if (!v.isString()) return fail(error, QString("the field '%1%2' is not a string").arg(where, key));
    *out = v.toString();
    return true;
}

bool readBool(const QJsonObject& object, const QString& key, const QString& where,
              bool* out, QString* error)
{
    if (!object.contains(key)) return true;
    QJsonValue v = object.value(key);
    if (!v.isBool()) return fail(error, QString("the field '%1%2' is not a bool").arg(where, key));
    *out = v.toBool();
    return true;
}

bool isInteger(const QJsonValue& v)
{
    if (!v.isDouble()) return false;
    double d = v.toDouble();
    return std::floor(d) == d && std::fabs(d) < 2147483647.0;
}

int indexOfName(const char* const names[], int count, const QString& name)
{
    for (int i = 0; i < count; i++) {
        if (name == QLatin1String(names[i])) return i;
    }
    return -1;
}

// the default of any JSON type as the text a value is kept in
bool defaultText(const QJsonValue& v, QString* out)
{
    if (v.isString()) {
        *out = v.toString();
    } else if (v.isBool()) {
        *out = v.toBool() ? "true" : "false";
    } else if (v.isDouble()) {
        double d = v.toDouble();
        *out = isInteger(v) ? QString::number(static_cast<qint64>(d)) : QString::number(d);
    } else {
        return false;
    }
    return true;
}

bool validParameterId(const QString& id)
{
    static const QRegularExpression re("^[A-Za-z0-9_][A-Za-z0-9_.-]*$");
    return re.match(id).hasMatch();
}

bool validVariableName(const QString& name)
{
    static const QRegularExpression re("^[A-Za-z_][A-Za-z0-9_]*$");
    return re.match(name).hasMatch();
}

bool parseParameter(const QJsonValue& value, int index, PluginParameter* p,
                    QStringList* warnings, QString* error)
{
    QString where = QString("parameters[%1].").arg(index);
    if (!value.isObject()) return fail(error, QString("the parameter %1 is not an object").arg(index));
    QJsonObject o = value.toObject();
    if (!readString(o, "id", where, &p->id, error)) return false;
    if (!validParameterId(p->id)) {
        return fail(error, QString("the parameter %1 has no valid id").arg(index));
    }
    where = QString("parameters/%1/").arg(p->id);
    warnUnknown(o, parameterFields, where, warnings);

    QString type;
    if (!readString(o, "type", where, &type, error)) return false;
    int t = indexOfName(typeNames, sizeof(typeNames) / sizeof(typeNames[0]), type);
    if (t < 0) return fail(error, QString("the parameter '%1' has an unknown type '%2'").arg(p->id, type));
    p->type = static_cast<PluginParameter::Type>(t);

    QString scope = scopeNames[PluginParameter::scopeRun];
    if (!readString(o, "scope", where, &scope, error)) return false;
    int sc = indexOfName(scopeNames, sizeof(scopeNames) / sizeof(scopeNames[0]), scope);
    if (sc < 0) return fail(error, QString("the parameter '%1' has an unknown scope '%2'").arg(p->id, scope));
    p->scope = static_cast<PluginParameter::Scope>(sc);

    if (!readString(o, "label", where, &p->label, error)) return false;
    if (p->label.isEmpty()) p->label = p->id;
    if (!readString(o, "description", where, &p->description, error)) return false;
    if (!readBool(o, "required", where, &p->required, error)) return false;
    if (!readString(o, "language", where, &p->language, error)) return false;
    if (!readString(o, "filter", where, &p->filter, error)) return false;
    if (!readBool(o, "save", where, &p->saveFile, error)) return false;
    if (!readBool(o, "create", where, &p->create, error)) return false;
    if (!readString(o, "export", where, &p->exportName, error)) return false;

    for (const char* bound: {"min", "max"}) {
        if (!o.contains(bound)) continue;
        if (!o.value(bound).isDouble()) {
            return fail(error, QString("the field '%1%2' is not a number").arg(where, bound));
        }
        if (QLatin1String(bound) == QLatin1String("min")) {
            p->hasMin = true;
            p->min = o.value(bound).toDouble();
        } else {
            p->hasMax = true;
            p->max = o.value(bound).toDouble();
        }
    }

    if (o.contains("choices")) {
        if (!o.value("choices").isArray()) {
            return fail(error, QString("the field '%1choices' is not a list").arg(where));
        }
        for (const QJsonValue& c: o.value("choices").toArray()) {
            if (!c.isString()) return fail(error, QString("a choice of '%1' is not a string").arg(p->id));
            p->choices.append(c.toString());
        }
        p->choiceLabels = p->choices;
    }

    if (o.contains("default") && !defaultText(o.value("default"), &p->defaultValue)) {
        return fail(error, QString("the default of '%1' is not a string, a number or a bool").arg(p->id));
    }

    switch (p->type) {
    case PluginParameter::typeBool:
        if (p->defaultValue.isEmpty()) p->defaultValue = "false";
        if (p->defaultValue != "true" && p->defaultValue != "false") {
            return fail(error, QString("the default of '%1' is not a bool").arg(p->id));
        }
        break;
    case PluginParameter::typeEnum:
        if (p->choices.isEmpty()) return fail(error, QString("the enum '%1' has no choices").arg(p->id));
        if (p->defaultValue.isEmpty()) p->defaultValue = p->choices.first();
        if (!p->choices.contains(p->defaultValue)) {
            return fail(error, QString("the default of '%1' is not a choice").arg(p->id));
        }
        break;
    case PluginParameter::typeSecret:
        if (!validVariableName(p->exportName)) {
            return fail(error, QString("the secret '%1' names no valid export variable").arg(p->id));
        }
        if (!p->defaultValue.isEmpty() && !p->defaultValue.startsWith("env:") &&
            !p->defaultValue.startsWith("file:")) {
            return fail(error, QString("the default of the secret '%1' is not env:NAME or file:PATH")
                        .arg(p->id));
        }
        break;
    case PluginParameter::typeInt: {
        bool ok = true;
        if (!p->defaultValue.isEmpty()) p->defaultValue.toInt(&ok);
        if (!ok) return fail(error, QString("the default of '%1' is not an integer").arg(p->id));
        break;
    }
    case PluginParameter::typeDouble: {
        bool ok = true;
        if (!p->defaultValue.isEmpty()) p->defaultValue.toDouble(&ok);
        if (!ok) return fail(error, QString("the default of '%1' is not a number").arg(p->id));
        break;
    }
    default:
        break;
    }
    return true;
}

bool parseArgv(const QJsonValue& value, const QString& actionId, PluginAction* a, QString* error)
{
    if (!value.isArray() || value.toArray().isEmpty()) {
        return fail(error, QString("the action '%1' has no argv").arg(actionId));
    }
    for (const QJsonValue& item: value.toArray()) {
        PluginArgument arg;
        if (item.isString()) {
            arg.text = item.toString();
        } else if (item.isObject()) {
            QJsonObject o = item.toObject();
            if (!o.value("if").isString() || o.value("if").toString().isEmpty() ||
                !o.value("argv").isArray() || o.keys().size() != 2) {
                return fail(error, QString("a conditional element of '%1' is not {\"if\": ..., \"argv\": [...]}")
                            .arg(actionId));
            }
            arg.condition = o.value("if").toString();
            for (const QJsonValue& s: o.value("argv").toArray()) {
                if (!s.isString()) {
                    return fail(error, QString("a conditional element of '%1' holds a non-string")
                                .arg(actionId));
                }
                arg.arguments.append(s.toString());
            }
        } else {
            return fail(error, QString("an argv element of '%1' is neither a string nor a condition")
                        .arg(actionId));
        }
        a->argv.append(arg);
    }
    return true;
}

bool parseAction(const QJsonValue& value, int index, PluginAction* a,
                 QStringList* warnings, QString* error)
{
    if (!value.isObject()) return fail(error, QString("the action %1 is not an object").arg(index));
    QJsonObject o = value.toObject();
    QString where = QString("actions[%1].").arg(index);
    if (!readString(o, "id", where, &a->id, error)) return false;
    if (!PluginDescriptor::validId(a->id)) {
        return fail(error, QString("the action %1 has no valid id").arg(index));
    }
    where = QString("actions/%1/").arg(a->id);
    warnUnknown(o, actionFields, where, warnings);

    if (!readString(o, "label", where, &a->label, error)) return false;
    if (a->label.isEmpty()) return fail(error, QString("the action '%1' has no label").arg(a->id));
    if (!readString(o, "description", where, &a->description, error)) return false;
    if (!readString(o, "icon", where, &a->icon, error)) return false;

    QString result;
    if (!readString(o, "result", where, &result, error)) return false;
    int r = indexOfName(resultNames, sizeof(resultNames) / sizeof(resultNames[0]), result);
    if (r < 0) return fail(error, QString("the action '%1' has an unknown result '%2'").arg(a->id, result));
    a->result = static_cast<PluginAction::Result>(r);

    if (!parseArgv(o.value("argv"), a->id, a, error)) return false;

    if (o.contains("exit_ok")) {
        if (!o.value("exit_ok").isArray()) {
            return fail(error, QString("the field '%1exit_ok' is not a list").arg(where));
        }
        for (const QJsonValue& c: o.value("exit_ok").toArray()) {
            if (!isInteger(c)) return fail(error, QString("an exit code of '%1' is not an integer").arg(a->id));
            a->exitOk.append(c.toInt());
        }
    }
    if (a->exitOk.isEmpty()) a->exitOk.append(0);

    if (o.contains("timeout")) {
        if (!isInteger(o.value("timeout")) || o.value("timeout").toInt() < 0) {
            return fail(error, QString("the timeout of '%1' is not a non-negative integer").arg(a->id));
        }
        a->timeout = o.value("timeout").toInt();
    }

    if (o.contains("parameters")) {
        if (!o.value("parameters").isArray()) {
            return fail(error, QString("the field '%1parameters' is not a list").arg(where));
        }
        a->hasParameterList = true;
        for (const QJsonValue& p: o.value("parameters").toArray()) {
            if (!p.isString()) return fail(error, QString("a parameter of '%1' is not a string").arg(a->id));
            a->parameters.append(p.toString());
        }
    }

    if (!readString(o, "outdir", where, &a->outdir, error)) return false;
    if (o.contains("load")) {
        if (!o.value("load").isObject()) {
            return fail(error, QString("the field '%1load' is not an object").arg(where));
        }
        QJsonObject load = o.value("load").toObject();
        warnUnknown(load, loadFields, where + "load/", warnings);
        if (!readBool(load, "strict", where + "load.", &a->loadStrict, error)) return false;
        if (!readBool(load, "reconstruct", where + "load.", &a->loadReconstruct, error)) return false;
    }
    if (!readBool(o, "save", where, &a->save, error)) return false;
    if (!readBool(o, "toolbar", where, &a->toolbar, error)) return false;
    QString cwd = "run";
    if (!readString(o, "cwd", where, &cwd, error)) return false;
    if (cwd != "run" && cwd != "document") {
        return fail(error, QString("the cwd of '%1' is neither run nor document").arg(a->id));
    }
    a->cwdDocument = cwd == "document";
    return true;
}

// the parameters a string refers to through ${param:x} and ${param-file:x}
QStringList referencedParameters(const QString& text)
{
    static const QRegularExpression re("\\$\\{param(?:-file)?:([^}]*)\\}");
    QStringList ids;
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext()) ids.append(it.next().captured(1));
    return ids;
}

} // namespace

bool PluginDescriptor::validId(const QString& id)
{
    static const QRegularExpression re("^[a-z0-9][a-z0-9._-]*$");
    return re.match(id).hasMatch();
}

QString PluginDescriptor::resultName(PluginAction::Result result)
{
    return resultNames[result];
}

QString PluginDescriptor::typeName(PluginParameter::Type type)
{
    return typeNames[type];
}

const PluginAction* PluginDescriptor::action(const QString& actionId) const
{
    for (const PluginAction& a: actions) {
        if (a.id == actionId) return &a;
    }
    return nullptr;
}

const PluginParameter* PluginDescriptor::parameter(const QString& parameterId) const
{
    for (const PluginParameter& p: parameters) {
        if (p.id == parameterId) return &p;
    }
    return nullptr;
}

QList<const PluginParameter*> PluginDescriptor::actionParameters(const PluginAction& action) const
{
    QList<const PluginParameter*> list;
    if (action.hasParameterList) {
        for (const QString& id: action.parameters) {
            const PluginParameter* p = parameter(id);
            if (p) list.append(p);
        }
    } else {
        for (const PluginParameter& p: parameters) list.append(&p);
    }
    return list;
}

bool PluginDescriptor::hasStoredParameters() const
{
    for (const PluginParameter& p: parameters) {
        if (p.isStored()) return true;
    }
    return false;
}

QString PluginDescriptor::iconPath(const QString& iconName) const
{
    if (iconName.isEmpty()) return QString();
    return QDir(dir).absoluteFilePath(iconName);
}

void PluginDescriptor::translate(const QString& language)
{
    if (language.isEmpty()) return;
    QFile file(QDir(dir).filePath(QString("%1/%2.json").arg(PLUGIN_I18N_FOLDER, language)));
    if (!file.exists()) return;
    QJsonParseError pe;
    QJsonDocument doc;
    if (file.open(QIODevice::ReadOnly)) doc = QJsonDocument::fromJson(file.readAll(), &pe);
    if (!file.isOpen() || pe.error != QJsonParseError::NoError || !doc.isObject()) {
        warnings.append(QString("the translation %1/%2.json is not a JSON object")
                        .arg(PLUGIN_I18N_FOLDER, language));
        return;
    }
    const QJsonObject map = doc.object();
    auto translated = [&map](QString* text) {
        QJsonValue v = map.value(*text);
        if (!text->isEmpty() && v.isString() && !v.toString().isEmpty()) *text = v.toString();
    };
    translated(&name);
    translated(&description);
    for (PluginAction& a: actions) {
        translated(&a.label);
        translated(&a.description);
    }
    for (PluginParameter& p: parameters) {
        translated(&p.label);
        translated(&p.description);
        for (QString& label: p.choiceLabels) translated(&label);
    }
}

bool PluginDescriptor::parse(const QByteArray& json, const QString& dir,
                             PluginDescriptor* out, QString* error)
{
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError) {
        return fail(error, QString("invalid JSON at line %1: %2")
                    .arg(lineAt(json, pe.offset)).arg(pe.errorString()));
    }
    if (!doc.isObject()) return fail(error, "the descriptor is not a JSON object");
    QJsonObject root = doc.object();

    PluginDescriptor d;
    d.dir = dir;

    if (!isInteger(root.value("api"))) return fail(error, "the api is not given");
    d.api = root.value("api").toInt();
    if (d.api != PLUGIN_API_MAJOR) return fail(error, QString("the api %1 is not supported").arg(d.api));
    warnUnknown(root, descriptorFields, QString(), &d.warnings);

    if (!readString(root, "id", QString(), &d.id, error)) return false;
    if (!validId(d.id)) return fail(error, QString("the id '%1' is not valid").arg(d.id));
    if (!readString(root, "name", QString(), &d.name, error)) return false;
    if (d.name.isEmpty()) return fail(error, "the name is not given");
    if (!readString(root, "version", QString(), &d.version, error)) return false;
    if (!readString(root, "description", QString(), &d.description, error)) return false;
    if (!readString(root, "icon", QString(), &d.icon, error)) return false;
    if (!readString(root, "interpreter", QString(), &d.interpreter, error)) return false;

    if (root.contains("env")) {
        if (!root.value("env").isObject()) return fail(error, "the field 'env' is not an object");
        QJsonObject env = root.value("env").toObject();
        for (const QString& key: env.keys()) {
            if (!validVariableName(key) || !env.value(key).isString()) {
                return fail(error, QString("the environment variable '%1' is not a name with a string")
                            .arg(key));
            }
            d.env.append(qMakePair(key, env.value(key).toString()));
        }
    }

    if (root.contains("parameters")) {
        if (!root.value("parameters").isArray()) return fail(error, "the field 'parameters' is not a list");
        QJsonArray params = root.value("parameters").toArray();
        QSet<QString> ids;
        for (int i = 0; i < params.size(); i++) {
            PluginParameter p;
            if (!parseParameter(params.at(i), i, &p, &d.warnings, error)) return false;
            if (ids.contains(p.id)) return fail(error, QString("the parameter '%1' is duplicated").arg(p.id));
            ids.insert(p.id);
            d.parameters.append(p);
        }
    }

    if (!root.value("actions").isArray() || root.value("actions").toArray().isEmpty()) {
        return fail(error, "the plugin has no actions");
    }
    QJsonArray actions = root.value("actions").toArray();
    QSet<QString> actionIds;
    for (int i = 0; i < actions.size(); i++) {
        PluginAction a;
        if (!parseAction(actions.at(i), i, &a, &d.warnings, error)) return false;
        if (actionIds.contains(a.id)) return fail(error, QString("the action '%1' is duplicated").arg(a.id));
        actionIds.insert(a.id);
        d.actions.append(a);
    }

    // the cross references of the actions and the parameters
    for (const PluginAction& a: d.actions) {
        for (const QString& id: a.parameters) {
            if (!d.parameter(id)) {
                return fail(error, QString("the action '%1' names an unknown parameter '%2'").arg(a.id, id));
            }
        }
        if (!a.outdir.isEmpty()) {
            const PluginParameter* p = d.parameter(a.outdir);
            if (!p || (p->type != PluginParameter::typeDir && p->type != PluginParameter::typeFile)) {
                return fail(error, QString("the outdir of '%1' is not a dir or file parameter").arg(a.id));
            }
        } else if (a.result == PluginAction::resultFiles) {
            return fail(error, QString("the files action '%1' names no outdir").arg(a.id));
        }
        for (const PluginArgument& arg: a.argv) {
            QStringList refs;
            if (arg.isConditional()) {
                if (!d.parameter(arg.condition)) {
                    return fail(error, QString("a condition of '%1' names an unknown parameter '%2'")
                                .arg(a.id, arg.condition));
                }
                for (const QString& s: arg.arguments) refs += referencedParameters(s);
            } else {
                refs = referencedParameters(arg.text);
            }
            // a secret travels in the environment only (EDIT-PLUG-10)
            for (const QString& id: refs) {
                const PluginParameter* p = d.parameter(id);
                if (p && p->type == PluginParameter::typeSecret) {
                    return fail(error, QString("the secret '%1' is used in the argv of '%2'").arg(id, a.id));
                }
            }
        }
    }

    *out = d;
    return true;
}
