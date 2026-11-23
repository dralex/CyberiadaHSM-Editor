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

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

#include "disk_state.h"

// a file written again within the time step of its file system keeps its time: a state
// read so close to the time of the file proves nothing without its bytes (as git's
// "racy" check; FAT counts in two seconds)
static const int DISK_RACY_SECONDS = 2;

DiskState DiskState::read(const QString& path)
{
    DiskState state;
    state.known = true;
    state.checked = QDateTime::currentDateTime();
    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) return state;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        // a file that cannot be read is there; its bytes stay unknown
        state.exists = true;
        state.size = info.size();
        state.modified = info.lastModified();
        return state;
    }
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(&file);
    state.exists = true;
    state.size = info.size();
    state.modified = info.lastModified();
    state.digest = hash.result();
    return state;
}

DiskCheck checkDiskState(const DiskState& remembered, const QString& path, DiskState* now)
{
    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        if (now) *now = DiskState::read(path);
        return remembered.exists || !remembered.known ? DiskCheck::gone : DiskCheck::same;
    }
    if (remembered.exists && info.size() == remembered.size && info.lastModified() == remembered.modified &&
        remembered.checked > remembered.modified.addSecs(DISK_RACY_SECONDS)) {
        if (now) *now = remembered;
        return DiskCheck::same;
    }
    DiskState state = DiskState::read(path);
    if (now) *now = state;
    // the same bytes: the new state is taken, so a state read too close to its file's time
    // is read once more only
    if (remembered.exists && !state.digest.isEmpty() && state.digest == remembered.digest) {
        return DiskCheck::touched;
    }
    return DiskCheck::changed;
}
