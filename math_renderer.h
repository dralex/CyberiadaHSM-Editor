/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The TeX formulas of Markdown drawn by JKQTMathText
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

#ifndef CYBERIADA_MATH_RENDERER
#define CYBERIADA_MATH_RENDERER

#include <QColor>
#include <QFont>
#include <QHash>
#include <QPicture>
#include <QString>

class JKQTMathText;

// a formula drawn once: the picture has its baseline at y = 0 and starts at x = 0
struct MathBox {
    QPicture picture;
    qreal    width = 0;
    qreal    ascent = 0;
    qreal    descent = 0;
    // the errors of the TeX, empty when it was read; the picture shows the source then
    QString  errors;
};

// the formulas at the size and in the colour of the text around them
// (docs/MARKDOWN.md, Formulas); a box is drawn once per (tex, display, font, colour)
class MathRenderer {
public:
    static MathRenderer& instance();
    ~MathRenderer();

    const MathBox& render(const QString& tex, bool display, const QFont& font, const QColor& color);

    MathRenderer(const MathRenderer&) = delete;
    MathRenderer& operator=(const MathRenderer&) = delete;

private:
    MathRenderer();

    // the source in red monospace: a formula the engine cannot read or a build without it
    static void drawSource(const QString& tex, const QFont& font, MathBox* box);

    JKQTMathText*           engine = nullptr;
    QHash<QString, MathBox> cache;
};

#endif
