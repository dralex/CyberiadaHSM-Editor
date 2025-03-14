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
#define COMMENT_ANGLE_CORNER 10

// Metainformation constants
#define METAINFORMATION_AUTHOR            "Author"
#define METAINFORMATION_CONTACT           "Contact"
#define METAINFORMATION_DATE              "Date"
#define METAINFORMATION_DESCRIPTION       "Description"
#define METAINFORMATION_EVENT_PROPAGATION "Event Propagation"
#define METAINFORMATION_MARKUP_LANGUAGE   "Markup Language"
#define METAINFORMATION_NAME              "Document Name"
#define METAINFORMATION_PLATFORM_LANGUAGE "Platform Language"
#define METAINFORMATION_PLATFORM_NAME     "Platform Name"
#define METAINFORMATION_PLATFORM_VERSION  "Platform Version"
#define METAINFORMATION_STANDARD_VERSION  "Standard Version"
#define METAINFORMATION_TARGET_SYSTEM     "Target System"
#define METAINFORMATION_TRANSITION_ORDER  "Transition Order"
#define METAINFORMATION_VERSION           "Version"

// Text constants
#define FONT_SIZE 12
#define FONT_NAME "Monospace"

#define FORMAL_COMMENT_FONT_SIZE 12
#define FORMAL_COMMENT_FONT_NAME "Courier"

enum class ToolType {
    Select,
    Pan,
    ZoomIn,
    ZoomOut,
    CreateRect,
    CreateLine,
};

#endif
