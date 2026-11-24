/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The icons of the Repository dock: the kind of a file and its status marker
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

#ifndef CYBERIADA_REPOSITORY_ICONS
#define CYBERIADA_REPOSITORY_ICONS

#include <QIcon>
#include <QString>

#include "repository_status.h"

// the kind of a file told by its name (EDIT-GIT-4)
enum class FileKind { diagram = 0, markdown, code, image, text, folder };

FileKind fileKind(const QString& path);

// The status marker of a row (EDIT-GIT-4/5, docs/GIT.md, The model): the places of the
// change color the disk, the glyph tells its kind
struct StatusMarker {
    enum Place { placeIndex = 1, placeTree = 2, placeConflict = 4, placeIgnored = 8 };
    enum Glyph { glyphNone = 0, glyphAdded, glyphDeleted, glyphRenamed, glyphUntracked, glyphConflict };

    int   places = 0;
    Glyph glyph = glyphNone;

    bool  isEmpty() const { return places == 0; }
    // the places of a change under a folder, the glyph dropped
    void  merge(const StatusMarker& other) { places |= other.places; glyph = glyphNone; }
    int   key() const;

    // the marker of a file; an unchanged or an ignored one has none
    static StatusMarker of(const StatusEntry& e);
};

// the picture of a kind, greyed for an ignored file, with the marker over it
QIcon markedIcon(FileKind kind, const StatusMarker& marker, bool greyed);
// the marker alone: the icon of a group
QIcon markerIcon(const StatusMarker& marker);

#endif
