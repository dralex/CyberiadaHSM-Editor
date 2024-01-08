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

#include "cyberiadasm_item.h"
#include "cyberiada_state_item.h"

struct CyberiadaTransitionGeometry {
	CyberiadaTransitionGeometry() {}
	CyberiadaTransitionGeometry(const CyberiadaSMPoint& source, const CyberiadaSMPoint& target):
		source_port(source), target_port(target) {}
	CyberiadaTransitionGeometry(const CyberiadaSMPoint& source, const CyberiadaSMPoint& target,
								const QList<CyberiadaSMPoint>& points):
		source_port(source), target_port(target), path(points) {}

	CyberiadaSMPoint        source_port;
	CyberiadaSMPoint        target_port;
	QList<CyberiadaSMPoint> path;
};

class CyberiadaTransitionItem: public CyberiadaSMItem {
public:
	CyberiadaTransitionItem(const CyberiadaGeometryItem* from,
							const CyberiadaGeometryItem* to,
							const QString& id,
							const QString& action,
							const CyberiadaTransitionGeometry& g,
							CyberiadaAbstractItem* parent = NULL):
		CyberiadaSMItem(nodeTransition, id, "", true, action, parent),
		item_from(from), item_to(to), geometry(g)
		{
		}

	void moveSourcePort(const CyberiadaSMPoint& point) { geometry.source_port = point; }
	void moveTargetPort(const CyberiadaSMPoint& point) { geometry.target_port = point; }
	const CyberiadaSMPoint& getSourcePort() const { return geometry.source_port; }
	const CyberiadaSMPoint& getTargetPort() const { return geometry.target_port; }
	const QList<CyberiadaSMPoint>& getPath() const { return geometry.path; }

	virtual QString getTitle() const;
	virtual void rename(const QString& new_title);
	
private:

	const CyberiadaGeometryItem* item_from;
	const CyberiadaGeometryItem* item_to;
	CyberiadaTransitionGeometry	 geometry;
};

#endif
