/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The file of a tab against the bytes the tab last read or wrote
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

#ifndef CYBERIADA_DISK_STATE
#define CYBERIADA_DISK_STATE

#include <QByteArray>
#include <QDateTime>
#include <QString>

// The file as a tab last read or wrote it (EDIT-DOC-13, docs/GIT.md, The disk check)
struct DiskState {
    bool       known = false;    // remembered at a read or a write
    bool       exists = false;
    qint64     size = -1;
    QDateTime  modified;
    QByteArray digest;           // SHA-1 of the bytes, empty for a missing file
    QDateTime  checked;          // when the bytes were read

    // the file now, its bytes read for the digest
    static DiskState read(const QString& path);
};

// how the file differs from the remembered state
enum class DiskCheck {
    same,       // the size and the time as remembered
    touched,    // the same bytes read again (another time, or too close to it): the state is taken
    changed,    // other bytes
    gone        // no file
};

// the file against the remembered state; *now gets the state of the file when it was read
DiskCheck checkDiskState(const DiskState& remembered, const QString& path, DiskState* now);

#endif
