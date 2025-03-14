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

class FontManager : public QObject {
    Q_OBJECT

public:
    FontManager(FontManager &other) = delete;

    void operator=(const FontManager &) = delete;

    static FontManager& instance() {
        static FontManager instance;
        return instance;
    }

    QFont getFont() const {
        return currentFont;
    }

    void setFont(const QFont &font) {
        if (currentFont != font) {
            currentFont = font;
            emit fontChanged(currentFont);
        }
    }

signals:
    void fontChanged(const QFont &newFont);

private:
    FontManager() = default;
    QFont currentFont;
};
#endif // FONTMANAGER_H
