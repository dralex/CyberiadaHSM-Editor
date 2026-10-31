/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
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
// the coordinate difference below which two positions are the same
#define GEOMETRY_TOLERANCE   0.001
// the grid spacing of a fresh profile and of every batch run
#define GRID_SPACING_DEFAULT 25
// a self-loop route (EDIT-EDGE-22): how far it runs past the border, the half span
// of a one-side bump, the step of a nested loop, the gap of its label from the
// outer segment and the distance difference that ties two sides of a press
#define LOOP_DEPTH           40
#define LOOP_LATERAL         30
#define LOOP_NEST_STEP       20
#define LOOP_LABEL_GAP       4
#define LOOP_SIDE_TIE        0.5

// the parameter key of the code language (EDIT-TEXT-7)
#define METAINFORMATION_KEY_PLATFORM_LANGUAGE "platformLanguage"
// the default markup of the informal comments (EDIT-TEXT-11)
#define METAINFORMATION_KEY_MARKUP_LANGUAGE   "markupLanguage"
// the document description, edited in the multiline dialog (EDIT-TEXT-10)
#define METAINFORMATION_KEY_DESCRIPTION       "description"

// the value removing an optional metainformation parameter
#define METAINFORMATION_VALUE_NONE        "none"

// A separate configuration (docs/EDITOR-SPEC.md): the preferences in an ini file under
// <config home>/<organization>/<application>.ini, the user data (plugins) under
// <data home>/<organization>/<application>, as XDG_CONFIG_HOME and XDG_DATA_HOME lay
// them out, on every system; the tests set both
#define CONFIG_HOME_VARIABLE    "CYBERIADA_PRO_CONFIG_HOME"
#define DATA_HOME_VARIABLE      "CYBERIADA_PRO_DATA_HOME"

// Plugin constants (docs/PLUGINS.md, docs/PLUGIN-API.md)
#define PLUGIN_API_MAJOR        1
#define PLUGIN_DESCRIPTOR_FILE  "plugin.json"
#define PLUGIN_SYSTEM_FOLDER    "share/cyberiada-pro/plugins"
#define PLUGIN_PATH_VARIABLE    "CYBERIADA_PRO_PLUGIN_PATH"
#define PLUGIN_KEEP_VARIABLE    "CYBERIADA_PRO_PLUGIN_KEEP"
// the interface language a run gets in its environment (EDIT-PLUG-33)
#define PLUGIN_LANGUAGE_VARIABLE "CYBERIADA_PRO_LANGUAGE"
// the translations of a plugin: <plugin>/i18n/<language>.json (EDIT-UI-4)
#define PLUGIN_I18N_FOLDER      "i18n"
// seconds
#define PLUGIN_TIMEOUT_DEFAULT  60
#define PLUGIN_KILL_GRACE_MS    3000
// the captured stdout and stderr of one run, in bytes
#define PLUGIN_OUTPUT_CAP       (1024 * 1024)
// the stderr lines reported with a failed run
#define PLUGIN_STDERR_TAIL      10

// Plugin sessions, level 2 (docs/PLUGINS.md, Level 2)
// the session.version the editor runs
#define SESSION_PROTOCOL_VERSION 1
#define SESSION_INIT_TIMEOUT_MS  30000
// the history rows kept per session tab
#define SESSION_HISTORY_LIMIT    10000
// seconds a batch session verb waits by default
#define SESSION_WAIT_S           30
// the canvas marks are repainted at most this often
#define RUN_MARK_INTERVAL_MS     50
#define RUN_ACTIVE_COLOR         "#2e7d32"
#define RUN_FIRED_COLOR          "#ef6c00"
#define RUN_MARK_WIDTH           4
#define RUN_MARK_FILL_ALPHA      40
// the JSON-RPC errors the editor sends
#define JSONRPC_METHOD_NOT_FOUND (-32601)

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
