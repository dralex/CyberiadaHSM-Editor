/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The implementaton of the abstract item for the State Machine Model
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

#include "cyberiadasm_item.h"
#include "myassert.h"

CyberiadaSMItem::CyberiadaSMItem(CyberiadaItemType t, const QString& _id,
								 const QString& title, bool _has_action,
								 const QString& action,
								 CyberiadaAbstractItem* parent):
	CyberiadaVisibleItem(t, title, parent), has_action(_has_action), id(_id)
{
	if (has_action) {
		addChild(new CyberiadaVisibleItem(nodeAction, action, this));
	}
}

int CyberiadaSMItem::findChild(const QString& id) const
{
	int idx = 0;
	foreach(const CyberiadaAbstractItem* child, children) {
		const CyberiadaSMItem* smitem = static_cast<const CyberiadaSMItem*>(child);
		if(smitem->id == id) {
			return idx;
		}		
		idx++;
	}
	return -1;
}

QString CyberiadaSMItem::getAction() const
{
	if (has_action) {
		MY_ASSERT(children.size() >= 1);
		const CyberiadaAbstractItem* child = children.at(0);
		return child->getTitle();
	} else {
		return "";
	}
}
