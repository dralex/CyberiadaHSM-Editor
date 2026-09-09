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
// the smallest width and height of a resized element
#define ELEMENT_MIN_SIZE     10

// Metainformation constants
#define METAINFORMATION_AUTHOR            "Author"
#define METAINFORMATION_CONTACT           "Contact"
#define METAINFORMATION_DATE              "Date"
#define METAINFORMATION_DESCRIPTION       "Description"
#define METAINFORMATION_EVENT_PROPAGATION "Event Propagation"
#define METAINFORMATION_GEOMETRY          "Geometry"
#define METAINFORMATION_MARKUP_LANGUAGE   "Markup Language"
#define METAINFORMATION_NAME              "Document Name"
#define METAINFORMATION_PLATFORM_LANGUAGE "Platform Language"
#define METAINFORMATION_PLATFORM_NAME     "Platform Name"
#define METAINFORMATION_PLATFORM_VERSION  "Platform Version"
#define METAINFORMATION_STANDARD_VERSION  "Standard Version"
#define METAINFORMATION_TARGET_SYSTEM     "Target System"
#define METAINFORMATION_TRANSITION_ORDER  "Transition Order"
#define METAINFORMATION_VERSION           "Version"

// the value removing an optional metainformation parameter
#define METAINFORMATION_VALUE_NONE        "none"

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
    ZoomIn,
    ZoomOut,
    Transition,
};

#endif
