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

#include <cstdio>
#include <string>

#include "cyberiadasm_dump.h"
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"

void dumpDocument(CyberiadaSMModel* model, std::ostream& os)
{
	os << *model->rootDocument() << std::endl;
}

static const char* elementTypeName(Cyberiada::ElementType t)
{
	switch (t) {
	case Cyberiada::elementRoot:           return "Document";
	case Cyberiada::elementSM:             return "State Machine";
	case Cyberiada::elementSimpleState:    return "Simple State";
	case Cyberiada::elementCompositeState: return "Composite State";
	case Cyberiada::elementComment:        return "Comment";
	case Cyberiada::elementFormalComment:  return "Formal Comment";
	case Cyberiada::elementInitial:        return "Initial";
	case Cyberiada::elementFinal:          return "Final";
	case Cyberiada::elementChoice:         return "Choice";
	case Cyberiada::elementTerminate:      return "Terminate";
	case Cyberiada::elementTransition:     return "Transition";
	default:                               return "Unknown";
	}
}

static void dumpSceneElement(CyberiadaSMEditorScene* scene, Cyberiada::Element* element,
							 int depth, std::ostream& os)
{
	Cyberiada::ID id = element->get_id();
	QGraphicsItem* item = scene->getMap().value(id, NULL);
	if (item && element->get_type() != Cyberiada::elementRoot) {
		QPointF p = item->pos();
		QRectF r = item->boundingRect();
		double v[6] = { p.x(), p.y(), r.x(), r.y(), r.width(), r.height() };
		for (int j = 0; j < 6; j++) {
			if (v[j] == 0.0) v[j] = 0.0;  // avoid the -0.00 output
		}
		char geometry[160];
		snprintf(geometry, sizeof(geometry),
				 "pos: (%.2f; %.2f), rect: (%.2f; %.2f; %.2f; %.2f)",
				 v[0], v[1], v[2], v[3], v[4], v[5]);
		os << std::string(size_t(depth) * 2, ' ')
		   << elementTypeName(element->get_type())
		   << ": {id: '" << id << "', " << geometry << "}" << std::endl;
	}
	Cyberiada::ElementCollection* collection = dynamic_cast<Cyberiada::ElementCollection*>(element);
	if (collection) {
		const Cyberiada::ElementList& children = collection->get_children();
		for (Cyberiada::ElementList::const_iterator i = children.begin(); i != children.end(); i++) {
			dumpSceneElement(scene, *i, depth + 1, os);
		}
	}
}

void dumpScene(CyberiadaSMEditorScene* scene, CyberiadaSMModel* model, std::ostream& os)
{
	dumpSceneElement(scene, model->rootDocument(), 0, os);
}
