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

#ifndef PLUGIN_CONDITIONS_H
#define PLUGIN_CONDITIONS_H

#include <QCoreApplication>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>

namespace Cyberiada {
    class Document;
}

// the system a plugin or an action needs, checked by the scan (EDIT-PLUG-50/51)
struct PluginRequirements {
    QStringList programs;
    QStringList env;
    // <id> or <id>>=<version>
    QStringList plugins;
    // the keys this editor does not know: never met (EDIT-PLUG-53)
    QStringList unknown;
};

// what the when tokens test on a document (EDIT-PLUG-52)
struct PluginDocumentFacts {
    bool file = false;
    bool saved = false;
    bool selection = false;
    int machines = 0;
    // every state machine has a top-level initial pseudostate
    bool initial = false;
    bool fa = false;
    // the first EXT-FA violation, empty for a valid automaton
    QString faFinding;
    QMap<QString, QString> meta;
    // a .git directory in the directory of the file or above it (EDIT-PLUG-55)
    bool git = false;
};

class PluginConditions {
    Q_DECLARE_TR_FUNCTIONS(PluginConditions)
public:
    // a when token this editor knows, with or without the negation
    static bool knownToken(const QString& token);
    // the reason of the first failing token, empty when all hold; the reason given
    // replaces it for a known token; fills are the keys the action's meta writes into
    // a document lacking them (EDIT-PLUG-34)
    static QString check(const QStringList& tokens, const QString& reason,
                         const PluginDocumentFacts& facts,
                         const QList<QPair<QString, QString> >& fills);
    // the reason a program, a variable or an unknown key fails, empty when they hold;
    // env: the variables of the descriptor
    static QString checkSystem(const PluginRequirements& requirements, const QString& pluginDir,
                               const QList<QPair<QString, QString> >& env);
    // a .git directory in the directory of the file or above it; the .git file of a
    // linked work tree does not count, no process is run (EDIT-PLUG-55)
    static bool readGit(const QString& filePath);
    // the content facts of a document; file, saved and selection are the caller's
    static void readDocument(const Cyberiada::Document* document, PluginDocumentFacts* facts);
    // <id> or <id>>=<version>; false when it is neither
    static bool parsePluginReference(const QString& text, QString* id, QString* version);
    // the platform language is the same under any of its aliases (python3, py)
    static bool sameMetaValue(const QString& key, const QString& present, const QString& declared);
};

#endif
