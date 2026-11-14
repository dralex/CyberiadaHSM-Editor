/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The JSON texts: the strict check of a text tab
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

#ifndef CYBERIADA_JSON_TEXT
#define CYBERIADA_JSON_TEXT

#include <QString>

// the first error of a JSON text (EDIT-MODE-26, docs/MODES.md, JSON)
struct JsonCheck {
    // a valid, an empty or a blank text
    bool    ok = true;
    // the underlined token: [position, position + length) of the text
    int     position = 0;
    int     length = 0;
    // from 1; the column counts the characters
    int     line = 0;
    int     column = 0;
    QString message;
};

// the text read as strict JSON (RFC 8259): one value, no comment, no trailing comma
JsonCheck checkJson(const QString& text);

#endif
