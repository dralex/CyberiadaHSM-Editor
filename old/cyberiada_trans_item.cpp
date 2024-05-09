/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The implementaton of the transition item for the State Machine Model
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

#include "cyberiada_trans_item.h"
#include "myassert.h"
#include "cyberiada_constants.h"

CyberiadaGeometryPathPropertyItem::CyberiadaGeometryPathPropertyItem(const QString& _name,
																	 const QList<CyberiadaSMPoint>& points,
																	 CyberiadaAbstractItem* parent):
	CyberiadaPropertyItem(nodeGeomPathProperty, _name, false, parent), points_num(points.count())
{
	foreach(const CyberiadaSMPoint& point, points) {
		addChild(new CyberiadaGeometryPointPropertyItem(PROPERTIES_HEADER_GEOMETRY_POINT, point, this));
	}
}

QString CyberiadaGeometryPathPropertyItem::getValue() const
{
	return QString(PROPERTIES_HEADER_GEOMETRY_PATH_F).arg(points_num);
}

CyberiadaGeometryTransPropertyItem::CyberiadaGeometryTransPropertyItem(const QString& _name,
																	   const CyberiadaSMPoint& label,
																	   const CyberiadaSMPoint& source,
																	   const CyberiadaSMPoint& target,
																	   const QList<CyberiadaSMPoint>& points,
																	   CyberiadaAbstractItem* parent):
	CyberiadaPropertyItem(nodeGeomTransProperty, _name, false, parent), has_points(points.count() > 0)
{
	addChild(new CyberiadaGeometryPointPropertyItem(PROPERTIES_HEADER_GEOMETRY_SOURCE, source, this));
	addChild(new CyberiadaGeometryPointPropertyItem(PROPERTIES_HEADER_GEOMETRY_TARGET, target, this));
	addChild(new CyberiadaGeometryPointPropertyItem(PROPERTIES_HEADER_GEOMETRY_LABEL, label, this));
	if (has_points) {
		addChild(new CyberiadaGeometryPathPropertyItem(PROPERTIES_HEADER_GEOMETRY_PATH, points, this));
	}
}

QString CyberiadaGeometryTransPropertyItem::getValue() const
{
	const CyberiadaPropertyItem* source =  static_cast<CyberiadaPropertyItem*>(child(0));
	MY_ASSERT(source);
	const CyberiadaPropertyItem* target = static_cast<CyberiadaPropertyItem*>(child(1));
	MY_ASSERT(target);
	//CyberiadaAbstractItem* label = child(2);
	if (has_points) {
		MY_ASSERT(childCount() == 4);
		const CyberiadaPropertyItem* path = static_cast<CyberiadaPropertyItem*>(child(3));
		MY_ASSERT(path);
		return QString(PROPERTIES_HEADER_GEOMETRY_TRANSP_F).arg(source->getValue(),
																target->getValue(),
																path->getValue());
	} else {
		return QString(PROPERTIES_HEADER_GEOMETRY_TRANS_F).arg(source->getValue(),
															   target->getValue());
	}
}

CyberiadaTransitionItem::CyberiadaTransitionItem(const CyberiadaAbstractItem* from,
												 const CyberiadaAbstractItem* to,
												 const QString& id,
												 const QString& action,
												 const CyberiadaSMPoint& label,
												 const CyberiadaSMPoint& source,
												 const CyberiadaSMPoint& target,
												 const QList<CyberiadaSMPoint>& points,
												 CyberiadaAbstractItem* parent):
	CyberiadaSceneItem(nodeTransition, id, "", false, true, action, parent)
{
	addChild(new CyberiadaStateLinkItem(PROPERTIES_HEADER_SOURCE, from, this));
	addChild(new CyberiadaStateLinkItem(PROPERTIES_HEADER_TARGET, to, this));
	addChild(new CyberiadaGeometryTransPropertyItem(PROPERTIES_HEADER_GEOMETRY,
													label, source, target, points,
													this));
}

QString CyberiadaTransitionItem::getTitle() const
{
	MY_ASSERT(childCount() >= 3);
	CyberiadaStateLinkItem* source = static_cast<CyberiadaStateLinkItem*>(child(3));
	CyberiadaStateLinkItem* target = static_cast<CyberiadaStateLinkItem*>(child(4));
	return source->nodeTitle() + " -> " + target->nodeTitle();
}

CyberiadaStateLinkItem::CyberiadaStateLinkItem(const QString& _prefix,
											   const CyberiadaAbstractItem* _node,
											   CyberiadaAbstractItem* parent):
	CyberiadaVisibleItem(nodeStateLink, "", false, false, parent), prefix(_prefix), node(_node)
{
}

QString CyberiadaStateLinkItem::getTitle() const
{
	MY_ASSERT(node);
	return prefix + ": " + nodeTitle();
}
