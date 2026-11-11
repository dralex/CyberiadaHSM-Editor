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

#include "cyberiada_constants.h"

// the mode ids of EDIT-MODE-1, also the ids a plugin action lists in its modes
extern const char* const MODE_HSM;
extern const char* const MODE_INSPECTION;
extern const char* const MODE_FA;
extern const char* const MODE_TEXT;
extern const char* const MODE_MARKDOWN;
// a language of syntax/*.json is a mode too: its id is the file name (EDIT-MODE-1)

// what a mode gives its tab (docs/MODES.md, The modes)
struct ModeTraits {
    const char* id;
    // the tab marker after the title (EDIT-MODE-2); empty for HSM
    const char* marker;
    // read-only whatever the flag of the tab says (inspection, diff)
    bool        readOnly;
    // the document is read as stored: no recovery on load (EDIT-INSPECT-2)
    bool        storedGeometry;
    // the creation tools are the finite automaton ones (EDIT-MODE-7)
    bool        faTools;
    // a text page instead of the canvas (EDIT-MODE-13)
    bool        text;
};

// the traits of a mode id: a language takes the text ones; HSM for an unknown id
const ModeTraits& modeTraits(const QString& id);

// a text mode: text, markdown or a language
bool isTextMode(const QString& id);
// the text modes in the menu order: Plain text, Markdown, the languages
QStringList textModes();
// the menu name of a mode: "HSM diagram", "Plain text", "C++"
QString modeName(const QString& id);
// the mode a file opens in by its extension (EDIT-MODE-13): hsm for .graphml,
// a language, markdown for .md, else text
QString modeForFile(const QString& path);
// the extension Save As proposes and a plugin input takes: ".txt", ".md", ".cpp"
QString modeExtension(const QString& id);
// the extensions Open names as text, "*.cpp *.md ..."
QString textFilePatterns();

// the mode offers the tool: an FA tab has no choice, terminate, history,
// submachine, entry or exit point and no second state machine (EDIT-MODE-7)
bool modeHasTool(const QString& mode, ToolType tool);

// the marker after a tab or a window title: "", " [inspection]", " [ro]";
// the window spells the flag out (" [read-only]")
QString modeMarker(const QString& mode, bool readOnlyFlag, bool window);

#endif
