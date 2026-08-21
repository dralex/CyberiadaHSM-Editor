/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The scene rendering and image comparison for exports and the batch mode
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

#ifndef CYBERIADA_SM_RENDER
#define CYBERIADA_SM_RENDER

#include <QString>

class CyberiadaSMEditorScene;

// render the scene 1:1 into an image file (the selection is cleared first)
bool renderScene(CyberiadaSMEditorScene* scene, const QString& path, QString* error);

// compare two images: a pixel differs when any channel delta exceeds epsilon,
// the images match when the differing fraction is not above max_diff_fraction;
// returns 0 on match, 1 on mismatch, -1 when an image cannot be read
int compareImages(const QString& a, const QString& b, int epsilon,
				  double max_diff_fraction, QString* report);

#endif
