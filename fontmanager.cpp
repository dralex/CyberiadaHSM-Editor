/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * Font Manager for the State Machine Editor
 *
 * Copyright (C) 2025 Anastasia Viktorova <viktorovaa.04@gmail.com>
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

#include <QFontDatabase>
#include <QStringList>

#include "fontmanager.h"
#include "settings_manager.h"

FontManager::FontManager():
    bundled(FONT_FALLBACK_NAME)
{
    connect(&SettingsManager::instance(), &SettingsManager::fontSettingsChanged,
            this, &FontManager::fontsChanged);
}

void FontManager::loadBundledFont()
{
    // the resources are compiled into the static core library, so they are
    // linked in only when something refers to them
    Q_INIT_RESOURCE(smeditor);

    int font_id = QFontDatabase::addApplicationFont(":/Fonts/fonts/courier.ttf");
    if (font_id == -1) return;
    QStringList families = QFontDatabase::applicationFontFamilies(font_id);
    if (!families.isEmpty()) {
        bundled = families.first();
    }
}

QFont FontManager::font(FontRole role) const
{
    const SettingsManager& settings = SettingsManager::instance();
    QString family = settings.getFontFamily();
    // the formal comment is written in the monospace of the standard
    if (family.isEmpty() || role == fontRoleFormalComment) {
        family = bundled;
    }
    QFont result(family, settings.getFontSize(role));
    if (role == fontRoleStateTitle) {
        result.setBold(true);
    }
    // the metrics then follow the font file instead of the hinting of the
    // machine, so the text layout is the same everywhere
    result.setHintingPreference(QFont::PreferNoHinting);
    return result;
}

QFont FontManager::baseFont() const
{
    QString family = SettingsManager::instance().getFontFamily();
    if (family.isEmpty()) {
        family = bundled;
    }
    QFont result(family, FONT_SIZE);
    result.setHintingPreference(QFont::PreferNoHinting);
    return result;
}
