/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The constants for the State Machine Editor
 *
 * Copyright (C) 2024 Alexey Fedoseev <aleksey@fedoseev.net>
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

#ifndef CYBERIADA_CONSTANTS_HEADER
#define CYBERIADA_CONSTANTS_HEADER

#define CYBERIADA_MIME_TYPE_STATE          "application/kruzhok.cyberiada.state"

// Geometry constants
#define ROUNDED_RECT_RADIUS 10
#define VERTEX_POINT_RADIUS 10
#define CHOICE_DEFAULT_SIZE 40
#define COMMENT_ANGLE_CORNER 10
// the inset of the comment text from each side of the box
#define COMMENT_TEXT_MARGIN  8
// the smallest width and height of a resized element
#define ELEMENT_MIN_SIZE     10
// the clearance kept between a placed child and its siblings (paste/reparent)
#define PLACEMENT_GAP        20
// the grid spacing of a fresh profile and of every batch run
#define GRID_SPACING_DEFAULT 25

// the parameter key of the code language (EDIT-TEXT-7)
#define METAINFORMATION_KEY_PLATFORM_LANGUAGE "platformLanguage"
// the default markup of the informal comments (EDIT-TEXT-11)
#define METAINFORMATION_KEY_MARKUP_LANGUAGE   "markupLanguage"

// the value removing an optional metainformation parameter
#define METAINFORMATION_VALUE_NONE        "none"

// Plugin constants (docs/PLUGINS.md, docs/PLUGIN-API.md)
#define PLUGIN_API_MAJOR        1
#define PLUGIN_DESCRIPTOR_FILE  "plugin.json"
#define PLUGIN_SYSTEM_FOLDER    "share/cyberiada-editor/plugins"
#define PLUGIN_PATH_VARIABLE    "CYBERIADA_EDITOR_PLUGIN_PATH"
#define PLUGIN_KEEP_VARIABLE    "CYBERIADA_EDITOR_PLUGIN_KEEP"
// seconds
#define PLUGIN_TIMEOUT_DEFAULT  60
#define PLUGIN_KILL_GRACE_MS    3000
// the captured stdout and stderr of one run, in bytes
#define PLUGIN_OUTPUT_CAP       (1024 * 1024)
// the stderr lines reported with a failed run
#define PLUGIN_STDERR_TAIL      10

// Text constants
#define FONT_SIZE     12
#define FONT_SIZE_MIN 6
#define FONT_SIZE_MAX 72

// the family of the bundled font, used when the resource cannot be loaded
#define FONT_FALLBACK_NAME "Courier"

// the text of every element is drawn in the font of its role; the sizes are
// set separately, the boldness and the formal comment family are fixed here
enum FontRole {
    fontRoleStateTitle = 0,
    fontRoleStateAction,
    fontRoleTransition,
    fontRoleComment,
    // not selected by the user: the comment size in the bundled monospace
    fontRoleFormalComment,
    fontRolesCount
};

enum class ToolType {
    Select,
    Pan,
    Zoom,
    Transition,
    // the element creation tools: rect-drawing (SM, state) and click-placement
    // (initial, final, choice, terminate, comment, formal comment)
    NewSM,
    NewState,
    NewInitial,
    NewFinal,
    NewChoice,
    NewTerminate,
    NewShallowHistory,
    NewDeepHistory,
    NewSubmachineState,
    NewEntryPoint,
    NewExitPoint,
    NewComment,
    NewFormalComment,
};

#endif
