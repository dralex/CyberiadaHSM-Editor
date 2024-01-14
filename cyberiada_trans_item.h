/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The transition item for the State Machine Model
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

#ifndef TRANSITION_ITEM_HEADER
#define TRANSITION_ITEM_HEADER

#include <QList>

#include "cyberiadasm_item.h"
#include "cyberiada_state_item.h"

class CyberiadaGeometryPathPropertyItem: public CyberiadaPropertyItem {
public:
	CyberiadaGeometryPathPropertyItem(const QString& _name,
									  const QList<CyberiadaSMPoint>& points,
									  CyberiadaAbstractItem* parent);
	virtual QString getValue() const;
	int points() const { return points_num; }

private:
	int points_num;
};

class CyberiadaGeometryTransPropertyItem: public CyberiadaPropertyItem {
public:
	CyberiadaGeometryTransPropertyItem(const QString& _name,
									   const CyberiadaSMPoint& label,
									   const CyberiadaSMPoint& source,
									   const CyberiadaSMPoint& target,
									   const QList<CyberiadaSMPoint>& points,
									   CyberiadaAbstractItem* parent);
	virtual QString getValue() const;
	bool hasPoints() const { return has_points; }
	
private:
	bool has_points;
};

class CyberiadaTransitionItem: public CyberiadaSceneItem {
public:
	CyberiadaTransitionItem(const CyberiadaAbstractItem* from,
							const CyberiadaAbstractItem* to,
							const QString& id,
							const QString& action,
							const CyberiadaSMPoint& label,
							const CyberiadaSMPoint& source,
							const CyberiadaSMPoint& target,
							const QList<CyberiadaSMPoint>& points,
							CyberiadaAbstractItem* parent = NULL);

	virtual QString getTitle() const;
};

class CyberiadaStateLinkItem: public CyberiadaVisibleItem {
public:
	CyberiadaStateLinkItem(const QString& _prefix,
						   const CyberiadaAbstractItem* node,
						   CyberiadaAbstractItem* parent);

	virtual QString getTitle() const;
	QString nodeTitle() const { return node->getTitle(); }
	
private:
	QString                      prefix;
	const CyberiadaAbstractItem* node;
};

#endif
