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

#include <QCoreApplication>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QPainter>
#include <QRegularExpression>
#include <QStringList>

#include "math_renderer.h"

#ifdef CYBERIADA_WITH_MATH
#include "jkqtmathtext/jkqtmathtext.h"

// the XITS fonts live in the static CyberiadaMathText library
static void initMathFonts()
{
    Q_INIT_RESOURCE(xits);
}
#endif

namespace {

// the number of boxes kept before the cache starts again
const int MATH_CACHE_LIMIT = 1000;
const QChar KEY_SEPARATOR(0x1f);

MathRenderer* theRenderer = nullptr;

// the fonts and the pictures go before the application does
void dropRenderer()
{
    delete theRenderer;
    theRenderer = nullptr;
}

} // namespace

MathRenderer& MathRenderer::instance()
{
    if (!theRenderer) {
        theRenderer = new MathRenderer();
        qAddPostRoutine(dropRenderer);
    }
    return *theRenderer;
}

MathRenderer::MathRenderer()
{
#ifdef CYBERIADA_WITH_MATH
    initMathFonts();
    engine = new JKQTMathText();
    engine->useXITS(false);
#endif
}

MathRenderer::~MathRenderer()
{
#ifdef CYBERIADA_WITH_MATH
    delete engine;
#endif
}

const MathBox& MathRenderer::render(const QString& tex, bool display, const QFont& font, const QColor& color)
{
    QString key = QString::number(display) + KEY_SEPARATOR + font.key() + KEY_SEPARATOR +
                  color.name(QColor::HexArgb) + KEY_SEPARATOR + tex;
    QHash<QString, MathBox>::const_iterator found = cache.constFind(key);
    if (found != cache.constEnd()) return found.value();
    if (cache.size() >= MATH_CACHE_LIMIT) cache.clear();

    MathBox box;
#ifdef CYBERIADA_WITH_MATH
    static const QRegularExpression colon("\\\\colon(?![A-Za-z])");
    QString source = tex.trimmed();
    source.replace(colon, ":");
    if (display) source.prepend("\\displaystyle ");
    engine->setFontColor(color);
    if (font.pointSizeF() > 0) engine->setFontSize(font.pointSizeF());
    else engine->setFontSizePixels(font.pixelSize());
    bool ok = engine->parse(source, JKQTMathText::LatexParser, JKQTMathText::StartWithMathMode);
    QStringList errors = engine->getErrorList();
    if (ok && errors.isEmpty()) {
        QPainter painter(&box.picture);
        double width = 0, ascent = 0, descent = 0, strikeout = 0;
        engine->getSizeDetail(painter, width, ascent, descent, strikeout);
        engine->draw(painter, 0, 0);
        painter.end();
        box.width = width;
        box.ascent = ascent;
        box.descent = descent;
    } else {
        box.errors = errors.isEmpty() ? QCoreApplication::translate("MathRenderer", "The formula cannot be read")
                                      : errors.join("\n");
    }
#else
    box.errors = QCoreApplication::translate("MathRenderer", "The editor is built without the TeX formulas");
#endif
    if (!box.errors.isEmpty()) drawSource(tex, font, &box);
    return cache.insert(key, box).value();
}

void MathRenderer::drawSource(const QString& tex, const QFont& font, MathBox* box)
{
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (font.pointSizeF() > 0) mono.setPointSizeF(font.pointSizeF());
    else mono.setPixelSize(font.pixelSize());
    QString text = tex.simplified();
    QFontMetricsF metrics(mono);
    box->width = metrics.horizontalAdvance(text);
    box->ascent = metrics.ascent();
    box->descent = metrics.descent();
    box->picture = QPicture();
    QPainter painter(&box->picture);
    painter.setFont(mono);
    painter.setPen(Qt::red);
    painter.drawText(QPointF(0, 0), text);
}
