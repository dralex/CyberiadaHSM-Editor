/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The state item for the State Machine Model
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

#ifndef STATE_ITEM_HEADER
#define STATE_ITEM_HEADER

#include "cyberiadasm_item.h"
#include "cyberiada_constants.h"

struct CyberiadaStateGeometry {
	CyberiadaStateGeometry() {}
	CyberiadaStateGeometry(const CyberiadaSMPoint& _pos,
						   const CyberiadaSMSize& _size = CyberiadaSMSize(0, 0)):
		pos(_pos), size(_size) {}
	
	CyberiadaSMPoint	pos;
	CyberiadaSMSize	    size;
};

class CyberiadaGeometryItem: public CyberiadaSMItem {
public:
	CyberiadaGeometryItem(CyberiadaItemType t,
						  const QString& id, const QString& title,
						  bool has_action, const QString& action,
						  const CyberiadaStateGeometry& g,
						  CyberiadaAbstractItem* parent = NULL):
		CyberiadaSMItem(t, id, title, has_action, action, parent), geometry(g)
		{
		}

	const CyberiadaSMPoint& getPosition() const { return geometry.pos; }
	void changePosition(const CyberiadaSMPoint& new_pos) { geometry.pos = new_pos; }
	
protected:
	
	CyberiadaStateGeometry geometry;
};

class CyberiadaStateItem: public CyberiadaGeometryItem {
public:
	CyberiadaStateItem(const QString& id, const QString& title, const QString& action,
					   const CyberiadaStateGeometry& g,
					   CyberiadaAbstractItem* parent = NULL):
		CyberiadaGeometryItem(nodeState, id, title, true, action, g, parent)	
		{
		}
};

class CyberiadaInitialStateItem: public CyberiadaGeometryItem {
public:
	CyberiadaInitialStateItem(const QString& id, const CyberiadaSMPoint& position,
							  CyberiadaAbstractItem* parent = NULL):
		CyberiadaGeometryItem(nodeInitialState, id, CYBERIADA_INITIAL_NODE_TITLE, false, "",
							  CyberiadaStateGeometry(position), parent)
		{
		}
};

class CyberiadaCommentItem: public CyberiadaGeometryItem {
public:
	CyberiadaCommentItem(const QString& id,
						 const QString& title,
						 const CyberiadaStateGeometry& g,
						 CyberiadaAbstractItem* parent = NULL):
		CyberiadaGeometryItem(nodeComment, id, title, false, "", g, parent)	
		{
		}
};

#endif
