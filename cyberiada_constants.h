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

#define SM_DEFAULT_TITLE                   "State Machine"
#define STATES_AGGREGATOR_TITLE            "States"
#define TRANSITIONS_AGGREGATOR_TITLE       "Transitions"

#define CYBERIADA_EMPTY_NODE_TITLE         "<node>"
#define CYBERIADA_INITIAL_NODE_TITLE       "<initial>"
#define CYBERIADA_COMMENT_NODE_TITLE       "<comment>"

#define CYBERIADA_SM_EDITOR_VERSION        "1.0"

#define PROPERTIES_HEADER_NAME             "Property"
#define PROPERTIES_HEADER_VALUE            "Value"
#define PROPERTIES_HEADER_TITLE            "Title"
#define PROPERTIES_HEADER_ID               "ID"
#define PROPERTIES_HEADER_ACTION           "Action"
#define PROPERTIES_HEADER_SOURCE           "Source"
#define PROPERTIES_HEADER_TARGET           "Target"
#define PROPERTIES_HEADER_GEOMETRY         "Geometry"
#define PROPERTIES_HEADER_GEOMETRY_POINT_F "(%1, %2)"
#define PROPERTIES_HEADER_GEOMETRY_RECT_F  "{(%1, %2), [%3, %4]}"
#define PROPERTIES_HEADER_GEOMETRY_TRANS_F "{source %1, target: %2}"
#define PROPERTIES_HEADER_GEOMETRY_TRANSP_F "{source %1, target: %2, %3}"
#define PROPERTIES_HEADER_GEOMETRY_PATH_F  "points: %1"
#define PROPERTIES_HEADER_GEOMETRY_X       "X"
#define PROPERTIES_HEADER_GEOMETRY_Y       "Y"
#define PROPERTIES_HEADER_GEOMETRY_WIDTH   "Width"
#define PROPERTIES_HEADER_GEOMETRY_HEIGHT  "Height"
#define PROPERTIES_HEADER_GEOMETRY_SOURCE  "Source Port"
#define PROPERTIES_HEADER_GEOMETRY_TARGET  "Target Port"
#define PROPERTIES_HEADER_GEOMETRY_LABEL   "Label"
#define PROPERTIES_HEADER_GEOMETRY_PATH    "Path"
#define PROPERTIES_HEADER_GEOMETRY_POINT   "Point"

#endif
