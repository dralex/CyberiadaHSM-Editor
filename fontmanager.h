/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * Font Manager for the State Machine Editor
 *
 * Copyright (C) 2025 Anastasia Viktorova <viktorovaa.04@gmail.com>
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

#ifndef FONTMANAGER_H
#define FONTMANAGER_H

#include <QFont>
#include <QObject>
#include <QString>

#include "cyberiada_constants.h"

// builds the font of every text role from the stored settings; the bundled
// font is registered here so the library users get it without the executable
class FontManager : public QObject {
    Q_OBJECT

public:
    FontManager(FontManager &other) = delete;

    void operator=(const FontManager &) = delete;

    static FontManager& instance() {
        static FontManager instance;
        return instance;
    }

    // register the bundled font; call it once the application object exists
    void loadBundledFont();
    QString bundledFamily() const { return bundled; }

    QFont font(FontRole role) const;
    // the family at the default size, for the painter of the exported image
    QFont baseFont() const;

signals:
    void fontsChanged();

private:
    FontManager();

    QString bundled;
};
#endif // FONTMANAGER_H
