/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Color Schemes of the Text Editors
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
#include <QJsonDocument>
#include <QJsonObject>

#include "text_scheme.h"
#include "cyberiada_constants.h"

#define TEXT_SCHEME_CONTEXT "TextScheme"

static void initSchemeResource()
{
    // the resources live in the static core library (see FontManager)
    Q_INIT_RESOURCE(smeditor);
}

static const struct {
    const char* key;
    const char* name;
} roles[textSchemeRolesCount] = {
    { "background",            QT_TRANSLATE_NOOP("TextScheme", "Background") },
    { "text",                  QT_TRANSLATE_NOOP("TextScheme", "Text") },
    { "selection",             QT_TRANSLATE_NOOP("TextScheme", "Selection") },
    { "selectionText",         QT_TRANSLATE_NOOP("TextScheme", "Selected text") },
    { "lineNumbersBackground", QT_TRANSLATE_NOOP("TextScheme", "Line numbers background") },
    { "lineNumbers",           QT_TRANSLATE_NOOP("TextScheme", "Line numbers") },
    { "keyword",               QT_TRANSLATE_NOOP("TextScheme", "Keyword") },
    { "type",                  QT_TRANSLATE_NOOP("TextScheme", "Type") },
    { "string",                QT_TRANSLATE_NOOP("TextScheme", "String") },
    { "number",                QT_TRANSLATE_NOOP("TextScheme", "Number") },
    { "comment",               QT_TRANSLATE_NOOP("TextScheme", "Comment") },
    { "heading",               QT_TRANSLATE_NOOP("TextScheme", "Heading") },
    { "emphasis",              QT_TRANSLATE_NOOP("TextScheme", "Emphasis") },
    { "code",                  QT_TRANSLATE_NOOP("TextScheme", "Code") },
    { "link",                  QT_TRANSLATE_NOOP("TextScheme", "Link") },
    { "marker",                QT_TRANSLATE_NOOP("TextScheme", "Marker") },
    { "quote",                 QT_TRANSLATE_NOOP("TextScheme", "Quote") },
    { "math",                  QT_TRANSLATE_NOOP("TextScheme", "Formula") }
};

// the names of the built-in schemes for the translation
static const char* const builtinNames[] = {
    QT_TRANSLATE_NOOP("TextScheme", "White"),
    QT_TRANSLATE_NOOP("TextScheme", "Dark")
};

QString textSchemeRoleKey(int role)
{
    if (role < 0 || role >= textSchemeRolesCount) return QString();
    return QString::fromLatin1(roles[role].key);
}

QString textSchemeRoleName(int role)
{
    if (role < 0 || role >= textSchemeRolesCount) return QString();
    return QCoreApplication::translate(TEXT_SCHEME_CONTEXT, roles[role].name);
}

bool TextScheme::isBuiltin() const
{
    return !id.startsWith(TEXT_SCHEME_USER_PREFIX);
}

QString TextScheme::displayName() const
{
    if (!isBuiltin()) return name;
    for (size_t i = 0; i < sizeof(builtinNames) / sizeof(builtinNames[0]); i++) {
        if (name == builtinNames[i]) return QCoreApplication::translate(TEXT_SCHEME_CONTEXT, builtinNames[i]);
    }
    return name;
}

bool TextScheme::sameColors(const TextScheme& other) const
{
    for (int role = 0; role < textSchemeRolesCount; role++) {
        if (colors[role] != other.colors[role]) return false;
    }
    return true;
}

// a scheme file has a name and every color (docs/COLOR-SCHEMES.md, The schemes)
static bool readScheme(const QByteArray& json, const QString& id, TextScheme* scheme)
{
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) return false;
    QJsonObject o = doc.object();
    scheme->id = id;
    scheme->name = o.value("name").toString().trimmed();
    if (scheme->name.isEmpty()) return false;
    QJsonObject colors = o.value("colors").toObject();
    for (int role = 0; role < textSchemeRolesCount; role++) {
        QColor c(colors.value(textSchemeRoleKey(role)).toString());
        if (!c.isValid()) return false;
        scheme->colors[role] = c;
    }
    return true;
}

static TextSchemes loadBuiltins()
{
    initSchemeResource();
    TextSchemes result;
    QDir dir(":/Schemes/schemes");
    for (const QString& file : dir.entryList(QStringList("*.json"), QDir::Files, QDir::Name)) {
        QFile f(dir.filePath(file));
        TextScheme scheme;
        if (!f.open(QIODevice::ReadOnly) || !readScheme(f.readAll(), QFileInfo(file).completeBaseName(), &scheme)) {
            continue;
        }
        if (scheme.id == TEXT_SCHEME_DEFAULT) result.prepend(scheme);
        else result.append(scheme);
    }
    if (result.isEmpty() || result.first().id != TEXT_SCHEME_DEFAULT) {
        // never without White: black on white
        TextScheme white;
        white.id = TEXT_SCHEME_DEFAULT;
        white.name = builtinNames[0];
        for (int role = 0; role < textSchemeRolesCount; role++) white.colors[role] = Qt::black;
        white.colors[textSchemeBackground] = white.colors[textSchemeLineNumbersBackground] = Qt::white;
        white.colors[textSchemeSelectionText] = Qt::white;
        result.prepend(white);
    }
    return result;
}

const TextSchemes& builtinTextSchemes()
{
    static const TextSchemes builtins = loadBuiltins();
    return builtins;
}

const TextScheme& builtinTextScheme(const QString& id)
{
    const TextScheme* scheme = findTextScheme(builtinTextSchemes(), id);
    return scheme ? *scheme : builtinTextSchemes().first();
}

const TextScheme* findTextScheme(const TextSchemes& schemes, const QString& id)
{
    for (const TextScheme& scheme : schemes) {
        if (scheme.id == id) return &scheme;
    }
    return nullptr;
}
