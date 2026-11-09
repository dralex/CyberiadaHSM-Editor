/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Plugin Conditions: requires and when (docs/PLUGIN-API.md, Conditions)
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
#include <QFileInfo>
#include <QStandardPaths>
#include <QVersionNumber>

#include <cyberiadamlpp.h>

#include "plugin_conditions.h"
#include "plugin_descriptor.h"
#include "code_highlighter.h"
#include "cyberiada_constants.h"

namespace {

const char* const plainTokens[] = { "file", "saved", "selection", "sm", "initial", "fa", "fa-valid" };
const QString negation = "!";
const QString metaPrefix = "meta:";
const QString versionAtLeast = ">=";

bool plainToken(const QString& name)
{
    for (size_t i = 0; i < sizeof(plainTokens) / sizeof(plainTokens[0]); i++) {
        if (name == plainTokens[i]) return true;
    }
    return false;
}

// the value the action's meta writes into a document lacking the key
QString filledValue(const QList<QPair<QString, QString> >& fills, const QString& key)
{
    for (const QPair<QString, QString>& item: fills) {
        if (item.first == key) return item.second;
    }
    return QString();
}

// the reason the token (without the negation) fails, empty when it does not
QString tokenFailure(const QString& name, bool negated, const PluginDocumentFacts& facts,
                     const QList<QPair<QString, QString> >& fills)
{
    bool holds = false;
    // the reason of the token failing and of its negation failing
    QString unmet, met;
    if (name == "file") {
        holds = facts.file;
        unmet = PluginConditions::tr("the document has no file");
        met = PluginConditions::tr("the document has a file");
    } else if (name == "saved") {
        holds = facts.saved;
        unmet = PluginConditions::tr("the document is not saved");
        met = PluginConditions::tr("the document is saved");
    } else if (name == "selection") {
        holds = facts.selection;
        unmet = PluginConditions::tr("nothing is selected");
        met = PluginConditions::tr("an element is selected");
    } else if (name == "sm") {
        holds = facts.machines > 0;
        unmet = PluginConditions::tr("the document has no state machine");
        met = PluginConditions::tr("the document has a state machine");
    } else if (name == "initial") {
        holds = facts.initial;
        unmet = PluginConditions::tr("a state machine has no initial state");
        met = PluginConditions::tr("every state machine has an initial state");
    } else if (name == "fa") {
        holds = facts.fa;
        unmet = PluginConditions::tr("the diagram is not a finite automaton");
        met = PluginConditions::tr("the diagram is a finite automaton");
    } else if (name == "fa-valid") {
        holds = facts.fa && facts.faFinding.isEmpty();
        unmet = facts.fa ? PluginConditions::tr("the automaton breaks EXT-FA: %1").arg(facts.faFinding)
                         : PluginConditions::tr("the diagram is not a finite automaton");
        met = PluginConditions::tr("the automaton is valid");
    } else {
        QString text = name.mid(metaPrefix.size());
        int at = text.indexOf('=');
        QString key = at < 0 ? text : text.left(at);
        QString present = facts.meta.value(key);
        if (present.isEmpty()) present = filledValue(fills, key);
        if (at < 0) {
            holds = !present.isEmpty();
            unmet = PluginConditions::tr("the metainformation has no '%1'").arg(key);
            met = PluginConditions::tr("the metainformation has '%1'").arg(key);
        } else {
            QString value = text.mid(at + 1);
            holds = !present.isEmpty() && PluginConditions::sameMetaValue(key, present, value);
            unmet = present.isEmpty() ? PluginConditions::tr("the metainformation has no '%1'").arg(key)
                                      : PluginConditions::tr("'%1' is '%2', not '%3'").arg(key, present, value);
            met = PluginConditions::tr("'%1' is '%2'").arg(key, present);
        }
    }
    if (holds != negated) return QString();
    return negated ? met : unmet;
}

// a name on PATH, or a path, absolute or in the plugin directory
bool programFound(const QString& program, const QString& pluginDir)
{
    QString path = program;
    path.replace("${plugin_dir}", pluginDir);
    if (path == program && !program.contains('/') && !program.contains('\\')) {
        return !QStandardPaths::findExecutable(program).isEmpty();
    }
    // the lookup in the one directory adds the Windows suffixes
    QFileInfo info(QDir(pluginDir).absoluteFilePath(path));
    return !QStandardPaths::findExecutable(info.fileName(), QStringList() << info.absolutePath()).isEmpty();
}

// the descriptor's env is what the run gets (docs/PLUGIN-API.md, plugin.json)
bool variableSet(const QString& name, const QList<QPair<QString, QString> >& env)
{
    for (const QPair<QString, QString>& item: env) {
        if (item.first == name) return !item.second.isEmpty();
    }
    return !qEnvironmentVariableIsEmpty(name.toLocal8Bit().constData());
}

}

bool PluginConditions::knownToken(const QString& token)
{
    QString name = token.startsWith(negation) ? token.mid(negation.size()) : token;
    if (plainToken(name)) return true;
    if (!name.startsWith(metaPrefix)) return false;
    QString key = name.mid(metaPrefix.size()).section('=', 0, 0);
    return !key.isEmpty();
}

QString PluginConditions::check(const QStringList& tokens, const QString& reason,
                                const PluginDocumentFacts& facts,
                                const QList<QPair<QString, QString> >& fills)
{
    // the plugin's reason cannot tell that the editor is too old (EDIT-PLUG-53)
    for (const QString& token: tokens) {
        if (!knownToken(token)) return tr("the condition '%1' needs a newer editor").arg(token);
    }
    for (const QString& token: tokens) {
        bool negated = token.startsWith(negation);
        QString failure = tokenFailure(negated ? token.mid(negation.size()) : token, negated, facts, fills);
        if (!failure.isEmpty()) return reason.isEmpty() ? failure : reason;
    }
    return QString();
}

QString PluginConditions::checkSystem(const PluginRequirements& requirements, const QString& pluginDir,
                                      const QList<QPair<QString, QString> >& env)
{
    if (!requirements.unknown.isEmpty()) {
        return tr("the requirement '%1' needs a newer editor").arg(requirements.unknown.first());
    }
    for (const QString& program: requirements.programs) {
        if (!programFound(program, pluginDir)) return tr("the program '%1' is not found").arg(program);
    }
    for (const QString& name: requirements.env) {
        if (!variableSet(name, env)) return tr("the environment variable '%1' is not set").arg(name);
    }
    return QString();
}

void PluginConditions::readDocument(const Cyberiada::Document* document, PluginDocumentFacts* facts)
{
    facts->machines = 0;
    facts->initial = false;
    facts->fa = false;
    facts->faFinding.clear();
    facts->meta.clear();
    if (!document) return;
    Cyberiada::ConstStateMachineList machines = document->get_state_machines();
    facts->machines = static_cast<int>(machines.size());
    facts->initial = !machines.empty();
    for (const Cyberiada::StateMachine* sm: machines) {
        if (!sm->has_initial()) facts->initial = false;
    }
    const Cyberiada::DocumentMetainformation& meta = document->meta();
    if (!meta.standard_version.empty()) {
        facts->meta.insert("standardVersion", QString::fromStdString(meta.standard_version));
    }
    for (const std::pair<Cyberiada::String, Cyberiada::String>& item: meta.strings) {
        facts->meta.insert(QString::fromStdString(item.first), QString::fromStdString(item.second));
    }
    facts->fa = document->is_fa();
    if (!facts->fa) return;
    try {
        Cyberiada::FAFindingList findings = document->check_fa();
        if (!findings.empty()) {
            const Cyberiada::FAFinding& first = findings.front();
            facts->faFinding = QString("%1 %2").arg(QString::fromStdString(first.requirement),
                                                    QString::fromStdString(first.message));
        }
    } catch (const Cyberiada::Exception& e) {
        facts->faFinding = QString::fromStdString(e.str());
    }
}

bool PluginConditions::parsePluginReference(const QString& text, QString* id, QString* version)
{
    int at = text.indexOf(versionAtLeast);
    QString name = (at < 0 ? text : text.left(at)).trimmed();
    QString least = at < 0 ? QString() : text.mid(at + versionAtLeast.size()).trimmed();
    if (!PluginDescriptor::validId(name)) return false;
    if (at >= 0) {
        int suffix = 0;
        QVersionNumber number = QVersionNumber::fromString(least, &suffix);
        if (number.isNull() || suffix != least.size()) return false;
    }
    if (id) *id = name;
    if (version) *version = least;
    return true;
}

bool PluginConditions::sameMetaValue(const QString& key, const QString& present, const QString& declared)
{
    if (key == METAINFORMATION_KEY_PLATFORM_LANGUAGE) {
        const CodeLanguage* a = CodeStyle::instance().find(present);
        const CodeLanguage* b = CodeStyle::instance().find(declared);
        if (a && b) return a == b;
        return present.compare(declared, Qt::CaseInsensitive) == 0;
    }
    return present == declared;
}
