/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The document modes: what a tab edits, how, and with which parts of the window
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
#include <QFileInfo>
#include <QStringList>

#include "document_mode.h"
#include "code_highlighter.h"

const char* const MODE_HSM = "hsm";
const char* const MODE_INSPECTION = "inspection";
const char* const MODE_FA = "fa";
const char* const MODE_DIFF = "diff";
const char* const MODE_TEXT = "text";
const char* const MODE_MARKDOWN = "markdown";

static const ModeTraits modes[] = {
    // id               marker                                     readOnly stored faTools text
    { MODE_HSM,        "",                                           false, false, false, false },
    { MODE_INSPECTION, QT_TRANSLATE_NOOP("DocumentMode", "inspection"), true, true, false, false },
    { MODE_FA,         QT_TRANSLATE_NOOP("DocumentMode", "FA"),     false, false, true, false },
    { MODE_DIFF,       QT_TRANSLATE_NOOP("DocumentMode", "diff"),   true, true, false, false },
    { MODE_TEXT,       QT_TRANSLATE_NOOP("DocumentMode", "text"),   false, false, false, true },
    { MODE_MARKDOWN,   "Markdown",                                   false, false, false, true },
};
// a language: the marker is its name
static const ModeTraits languageTraits = { "", "", false, false, false, true };

const ModeTraits& modeTraits(const QString& id)
{
    for (const ModeTraits& traits : modes) {
        if (id == traits.id) return traits;
    }
    if (CodeStyle::instance().findId(id)) return languageTraits;
    return modes[0];
}

bool isTextMode(const QString& id)
{
    return modeTraits(id).text;
}

bool isKnownMode(const QString& id)
{
    for (const ModeTraits& traits : modes) {
        if (id == traits.id) return true;
    }
    return CodeStyle::instance().findId(id) != nullptr;
}

QStringList textModes()
{
    return QStringList() << MODE_TEXT << MODE_MARKDOWN << CodeStyle::instance().ids();
}

QString modeName(const QString& id)
{
    if (id == MODE_HSM) return QCoreApplication::translate("DocumentMode", "HSM diagram");
    if (id == MODE_INSPECTION) return QCoreApplication::translate("DocumentMode", "HSM inspection");
    if (id == MODE_FA) return QCoreApplication::translate("DocumentMode", "FA diagram");
    if (id == MODE_DIFF) return QCoreApplication::translate("DocumentMode", "HSM diff");
    if (id == MODE_TEXT) return QCoreApplication::translate("DocumentMode", "Plain text");
    if (id == MODE_MARKDOWN) return QStringLiteral("Markdown");
    const CodeLanguage* lang = CodeStyle::instance().findId(id);
    return lang ? lang->name : id;
}

QString modeForFile(const QString& path)
{
    QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "graphml") return MODE_HSM;
    if (suffix == "md" || suffix == "markdown") return MODE_MARKDOWN;
    const CodeLanguage* lang = CodeStyle::instance().forExtension("." + suffix);
    return lang ? lang->id : QString(MODE_TEXT);
}

bool isDocumentFile(const QString& path)
{
    QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "graphml" || suffix == "txt" || suffix == "md" || suffix == "markdown") return true;
    return !suffix.isEmpty() && CodeStyle::instance().forExtension("." + suffix);
}

QString modeExtension(const QString& id)
{
    if (id == MODE_MARKDOWN) return ".md";
    const CodeLanguage* lang = CodeStyle::instance().findId(id);
    if (lang && !lang->extensions.isEmpty()) return lang->extensions.first();
    return isTextMode(id) ? ".txt" : ".graphml";
}

QString textFilePatterns()
{
    QStringList patterns;
    patterns << "*.txt" << "*.md";
    for (const QString& id : CodeStyle::instance().ids()) {
        for (const QString& e : CodeStyle::instance().findId(id)->extensions) patterns << "*" + e;
    }
    return patterns.join(' ');
}

bool modeHasTool(const QString& mode, ToolType tool)
{
    if (!modeTraits(mode).faTools) return true;
    switch (tool) {
    case ToolType::NewChoice:
    case ToolType::NewTerminate:
    case ToolType::NewShallowHistory:
    case ToolType::NewDeepHistory:
    case ToolType::NewSubmachineState:
    case ToolType::NewEntryPoint:
    case ToolType::NewExitPoint:
    case ToolType::NewSM:
        return false;
    default:
        return true;
    }
}

QString modeMarker(const QString& mode, bool readOnlyFlag, bool window)
{
    const ModeTraits& traits = modeTraits(mode);
    QStringList parts;
    if (&traits == &languageTraits) parts << modeName(mode);
    else if (*traits.marker) parts << QCoreApplication::translate("DocumentMode", traits.marker);
    // a read-only mode needs no flag in its marker
    if (readOnlyFlag && !traits.readOnly) {
        parts << (window ? QCoreApplication::translate("DocumentMode", "read-only")
                         : QCoreApplication::translate("DocumentMode", "ro"));
    }
    return parts.isEmpty() ? QString() : " [" + parts.join(", ") + "]";
}
