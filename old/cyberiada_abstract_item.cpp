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

#include "cyberiada_abstract_item.h"
#include "myassert.h"

CyberiadaAbstractItem::CyberiadaAbstractItem(CyberiadaItemType t, CyberiadaAbstractItem* parent):
	node_type(t), parent_item(parent)
{
}

CyberiadaAbstractItem::~CyberiadaAbstractItem()
{	
	removeAllChildren();
}

void CyberiadaAbstractItem::removeAllChildren()
{
	foreach(CyberiadaAbstractItem* item, children) {
		MY_ASSERT(item);
		delete item;
	}
	children.clear();
}

int CyberiadaAbstractItem::row() const
{
	if(isRoot()) return 0;
	MY_ASSERT(parent_item);
	MY_ASSERT(parent_item->childCount() > 0);
	return parent_item->children.indexOf(const_cast<CyberiadaAbstractItem*>(this));
}

int CyberiadaAbstractItem::rowNoProperties() const
{
	if(isRoot()) return 0;
	MY_ASSERT(parent_item);
	MY_ASSERT(parent_item->childCount() > 0);
	int index = 0;
	foreach(const CyberiadaAbstractItem* item,  parent_item->children) {
		if (item == this) {
			break;
		}
		if (!item->isProperty()) {
			index++;
		}
	}
	return index;
}

int CyberiadaAbstractItem::childCountNoProperties() const
{
	int num = 0;
	foreach(const CyberiadaAbstractItem* item, children) {
		if (!item->isProperty()) {
			num++;
		}
	}
	return num;
}

CyberiadaAbstractItem* CyberiadaAbstractItem::child(int index) const
{
	if(index < 0 || index >= children.size()) return NULL;
	if(children.size() == 0) return NULL;
	return children.value(index);
}

void CyberiadaAbstractItem::removeChild(CyberiadaAbstractItem* child)
{
	children.removeAll(child);
} 

void CyberiadaAbstractItem::addChild(CyberiadaAbstractItem* child)
{
	child->parent_item = this;
	children.append(child);
}

void CyberiadaAbstractItem::moveParent(CyberiadaAbstractItem* new_parent)
{
	parent_item = new_parent;
}
