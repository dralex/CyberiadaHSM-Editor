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
#include <QStringList>

#include "document_mode.h"

const char* const MODE_HSM = "hsm";
const char* const MODE_INSPECTION = "inspection";
const char* const MODE_FA = "fa";

static const ModeTraits modes[] = {
    // id               marker                                     readOnly storedGeometry faTools
    { MODE_HSM,        "",                                           false, false, false },
    { MODE_INSPECTION, QT_TRANSLATE_NOOP("DocumentMode", "inspection"), true, true, false },
    { MODE_FA,         QT_TRANSLATE_NOOP("DocumentMode", "FA"),     false, false, true },
};

const ModeTraits& modeTraits(const QString& id)
{
    for (const ModeTraits& traits : modes) {
        if (id == traits.id) return traits;
    }
    return modes[0];
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
    if (*traits.marker) parts << QCoreApplication::translate("DocumentMode", traits.marker);
    // a read-only mode needs no flag in its marker
    if (readOnlyFlag && !traits.readOnly) {
        parts << (window ? QCoreApplication::translate("DocumentMode", "read-only")
                         : QCoreApplication::translate("DocumentMode", "ro"));
    }
    return parts.isEmpty() ? QString() : " [" + parts.join(", ") + "]";
}
