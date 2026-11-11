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

#ifndef CYBERIADA_DOCUMENT_MODE
#define CYBERIADA_DOCUMENT_MODE

#include <QString>

// the mode ids of EDIT-MODE-1, also the ids a plugin action lists in its modes
extern const char* const MODE_HSM;
extern const char* const MODE_INSPECTION;

// what a mode gives its tab (docs/MODES.md, The modes)
struct ModeTraits {
    const char* id;
    // the tab marker after the title (EDIT-MODE-2); empty for HSM
    const char* marker;
    // read-only whatever the flag of the tab says (inspection, diff)
    bool        readOnly;
    // the document is read as stored: no recovery on load (EDIT-INSPECT-2)
    bool        storedGeometry;
};

// the traits of a mode id; the HSM ones for an unknown id
const ModeTraits& modeTraits(const QString& id);

// the marker after a tab or a window title: "", " [inspection]", " [ro]";
// the window spells the flag out (" [read-only]")
QString modeMarker(const QString& mode, bool readOnlyFlag, bool window);

#endif
