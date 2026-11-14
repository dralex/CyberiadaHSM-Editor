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

#ifndef CYBERIADA_TEXT_SCHEME
#define CYBERIADA_TEXT_SCHEME

#include <QColor>
#include <QList>
#include <QString>

#include "code_highlighter.h"

// the colors of a scheme (EDIT-COLOR-2, docs/COLOR-SCHEMES.md): the editor, then the
// highlighting in the order of CodeTokenKind
enum TextSchemeRole {
    textSchemeBackground = 0,
    textSchemeText,
    textSchemeSelection,
    textSchemeSelectionText,
    textSchemeLineNumbersBackground,
    textSchemeLineNumbers,
    textSchemeFirstToken,
    textSchemeRolesCount = textSchemeFirstToken + codeTokenKindsCount
};

struct TextScheme {
    // a built-in one by its file name, a user one TEXT_SCHEME_USER_PREFIX<n>
    QString id;
    // as given for a user scheme, the untranslated name for a built-in one
    QString name;
    QColor  colors[textSchemeRolesCount];

    bool    isBuiltin() const;
    // the name shown: a built-in one translated
    QString displayName() const;
    QColor  color(TextSchemeRole role) const { return colors[role]; }
    QColor  tokenColor(CodeTokenKind kind) const { return colors[textSchemeFirstToken + kind]; }
    bool    sameColors(const TextScheme& other) const;
    bool    operator==(const TextScheme& other) const {
        return id == other.id && name == other.name && sameColors(other);
    }
    bool    operator!=(const TextScheme& other) const { return !(*this == other); }
};

typedef QList<TextScheme> TextSchemes;

// the key of a color in a scheme file and in the settings
QString textSchemeRoleKey(int role);
// the color name shown on the page
QString textSchemeRoleName(int role);
// the schemes of the resources, White first, read once
const TextSchemes& builtinTextSchemes();
// the built-in scheme of the id as built in, White for any other id
const TextScheme& builtinTextScheme(const QString& id);
// the scheme of the id in the list, null without one
const TextScheme* findTextScheme(const TextSchemes& schemes, const QString& id);

#endif
