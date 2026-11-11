/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The document of a tab: its mode, its read-only flag and its title
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

#include "editor_document.h"

void EditorDocument::setReadOnlyFlag(bool on)
{
    // a read-only mode keeps its flag off: leaving the mode leaves no flag behind
    if (on && traits().readOnly) return;
    if (m_readOnly == on) return;
    m_readOnly = on;
    applyReadOnlyFlag(on);
    emit readOnlyChanged(on);
}

QString EditorDocument::modeMarker(bool window) const
{
    return ::modeMarker(mode(), m_readOnly, window);
}
