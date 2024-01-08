/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The abstract item for the State Machine Model
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

#ifndef CYBERIADA_ABSTRACT_ITEM_HEADER
#define CYBERIADA_ABSTRACT_ITEM_HEADER

#include <QList>

enum CyberiadaItemType {
	nodeRoot,
	nodeSM,
	nodeStatesAggr,
	nodeTransitionsAggr,
	nodeState,
	nodeComment,
	nodeTransition,
	nodeInitialState,
	nodeAction
};

class CyberiadaAbstractItem {
public:
	CyberiadaAbstractItem(CyberiadaAbstractItem* parent = NULL);
	virtual ~CyberiadaAbstractItem();

	int childCount() const { return children.size(); }	
	CyberiadaAbstractItem* child(int index) const;
	CyberiadaAbstractItem* parent() const { return parent_item; }
	int row() const;

	CyberiadaItemType getType() const { return node_type; }
	bool isRoot() const { return parent_item == NULL; }
	bool isSMAggregate() const { return node_type == nodeSM; }
	bool isStatesAggregate() const { return node_type == nodeStatesAggr; }
	bool isTransionsAggregate() const { return node_type == nodeTransitionsAggr; }
	bool isState() const { return node_type == nodeState; }
	bool isInitialState() const { return node_type == nodeInitialState; }
	bool isTransition() const { return node_type == nodeTransition; }
	bool isAction() const { return node_type == nodeAction; }

	void addChild(CyberiadaAbstractItem* child);
	void removeChild(CyberiadaAbstractItem* child);
	void moveParent(CyberiadaAbstractItem* new_parent);

	virtual QString getTitle() const = 0;
	virtual void rename(const QString& new_title) = 0;

protected:
	void removeAllChildren();

	CyberiadaItemType		      node_type;
	CyberiadaAbstractItem*        parent_item;
	QList<CyberiadaAbstractItem*> children;
};

#endif
