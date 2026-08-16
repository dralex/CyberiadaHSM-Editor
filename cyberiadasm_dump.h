/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The canonical document/scene dump for the batch mode
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

#ifndef CYBERIADA_SM_DUMP
#define CYBERIADA_SM_DUMP

#include <ostream>

class CyberiadaSMModel;
class CyberiadaSMEditorScene;

// the document part reuses the libcyberiadamlpp dump verbatim
void dumpDocument(CyberiadaSMModel* model, std::ostream& os);
// the scene part lists the items in document order with fixed 2-decimal geometry
void dumpScene(CyberiadaSMEditorScene* scene, CyberiadaSMModel* model, std::ostream& os);

#endif
