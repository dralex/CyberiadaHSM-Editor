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
	nodeMetainfo,
	nodeStatesAggr,
	nodeTransitionsAggr,
	nodeSimpleState,
	nodeCompositeState,
	nodeMachineComment,
	nodeHumanComment,
	nodeTransition,
	nodeInitialPseudostate,
	nodeChoicePseudostate,
	nodeFinalState,
	nodeBehavior,
	nodeEntryBehavior,
	nodeExitBehavior,
	nodeTransitionBehavior,
	nodeBehaviorTrigger,
	nodeBehaviorGuard,
	nodeBehaviorAction,
	nodeTextProperty,
	nodeNumProperty,
	nodeGeomPointProperty,
	nodeGeomRectProperty,
	nodeGeomPathProperty,
	nodeGeomTransProperty,
	nodeStateLink
};

class CyberiadaAbstractItem {
public:
	CyberiadaAbstractItem(CyberiadaItemType t, CyberiadaAbstractItem* parent = NULL);
	virtual ~CyberiadaAbstractItem();

	int childCount() const { return children.size(); }
	int childCountNoProperties() const;
	CyberiadaAbstractItem* child(int index) const;
	CyberiadaAbstractItem* parent() const { return parent_item; }
	int row() const;
	int rowNoProperties() const;
	CyberiadaItemType getType() const { return node_type; }
	virtual QString getTitle() const = 0;
	virtual bool rename(const QString&) const { return false; };
	
	bool isRoot() const { return parent_item == NULL; }
	bool isSM() const { return node_type == nodeSM; }
	bool isStatesAggregate() const { return node_type == nodeStatesAggr; }
	bool isTransionsAggregate() const { return node_type == nodeTransitionsAggr; }
	bool isCompositeState() const { return node_type == nodeCompositeState; }
	bool isSimpleState() const { return node_type == nodeSimpleState; }
	bool isInitialPseudostate() const { return node_type == nodeInitialPseudostate; }
	bool isChoicePseudostate() const { return node_type == nodeChoicePseudostate; }
	bool isTransition() const { return node_type == nodeTransition; }
	bool isProperty() const { return isTextProperty() || isNumProperty() || isGeometry(); }
	bool isTextProperty() const { return node_type == nodeTextProperty; }
	bool isNumProperty() const { return node_type == nodeNumProperty; }
	bool isGeometry() const { return (node_type == nodeGeomPointProperty ||
									  node_type == nodeGeomRectProperty ||
									  node_type == nodeGeomPathProperty ||
									  node_type == nodeGeomTransProperty); }
	bool isPointGeometry() const { return node_type == nodeGeomPointProperty; }
	bool isRectGeometry() const { return node_type == nodeGeomRectProperty; }
	bool isPathGeometry() const { return node_type == nodeGeomPathProperty; }
	bool isTransGeometry() const { return node_type == nodeGeomTransProperty; }
	bool isStateLink() const { return node_type == nodeStateLink; }
	
	void addChild(CyberiadaAbstractItem* child);
	void removeChild(CyberiadaAbstractItem* child);
	void moveParent(CyberiadaAbstractItem* new_parent);

protected:
	void removeAllChildren();

	CyberiadaItemType		      node_type;
	CyberiadaAbstractItem*        parent_item;
	QList<CyberiadaAbstractItem*> children;
};

#endif
