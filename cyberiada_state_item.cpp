/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The implementation of the state item for the State Machine Model
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

#include "cyberiada_state_item.h"

CyberiadaGeometryPointPropertyItem::CyberiadaGeometryPointPropertyItem(const QString& _name,
																	   const CyberiadaSMPoint& pos,
																	   CyberiadaAbstractItem* parent):
	CyberiadaPropertyItem(nodeGeomPointProperty, _name, false, parent)
{
	addChild(new CyberiadaFloatPropertyItem(PROPERTIES_HEADER_GEOMETRY_X, pos.x, true, true, this));
	addChild(new CyberiadaFloatPropertyItem(PROPERTIES_HEADER_GEOMETRY_Y, pos.x, true, true, this));
}

QString CyberiadaGeometryPointPropertyItem::getValue() const
{
	QString x = static_cast<CyberiadaFloatPropertyItem*>(child(0))->getValue();
	QString y = static_cast<CyberiadaFloatPropertyItem*>(child(1))->getValue();
	return QString(PROPERTIES_HEADER_GEOMETRY_POINT_F).arg(x).arg(y);
}

CyberiadaGeometryRectPropertyItem::CyberiadaGeometryRectPropertyItem(const QString& _name,
																	 const CyberiadaSMPoint& pos,
																	 const CyberiadaSMSize& size,
																	 CyberiadaAbstractItem* parent):
	CyberiadaPropertyItem(nodeGeomRectProperty, _name, false, parent)
{
	addChild(new CyberiadaFloatPropertyItem(PROPERTIES_HEADER_GEOMETRY_X, pos.x, true, true, this));
	addChild(new CyberiadaFloatPropertyItem(PROPERTIES_HEADER_GEOMETRY_Y, pos.x, true, true, this));
	addChild(new CyberiadaFloatPropertyItem(PROPERTIES_HEADER_GEOMETRY_WIDTH, size.width, true, true, this));
	addChild(new CyberiadaFloatPropertyItem(PROPERTIES_HEADER_GEOMETRY_HEIGHT, size.height, true, true, this));
}

QString CyberiadaGeometryRectPropertyItem::getValue() const
{
	QString x = static_cast<CyberiadaFloatPropertyItem*>(child(0))->getValue();
	QString y = static_cast<CyberiadaFloatPropertyItem*>(child(1))->getValue();
	QString w = static_cast<CyberiadaFloatPropertyItem*>(child(2))->getValue();
	QString h = static_cast<CyberiadaFloatPropertyItem*>(child(3))->getValue();
	return QString(PROPERTIES_HEADER_GEOMETRY_RECT_F).arg(x).arg(y).arg(w).arg(h);
}

CyberiadaScenePointItem::CyberiadaScenePointItem(CyberiadaItemType t, const QString& id,
												 const QString& title, bool title_editable,
												 bool has_action, const QString& action,
												 const CyberiadaSMPoint& pos,
												 CyberiadaAbstractItem* parent):
	CyberiadaSceneItem(t, id, title, title_editable, has_action, action, parent)
{
	addChild(new CyberiadaGeometryPointPropertyItem(PROPERTIES_HEADER_GEOMETRY,
													pos, this));
}

CyberiadaSceneRectItem::CyberiadaSceneRectItem(CyberiadaItemType t, const QString& id,
											   const QString& title, bool title_editable,
											   bool has_action, const QString& action,
											   const CyberiadaSMPoint& pos,
											   const CyberiadaSMSize& size,
											   CyberiadaAbstractItem* parent):
	CyberiadaSceneItem(t, id, title, title_editable, has_action, action, parent)
{
	addChild(new CyberiadaGeometryRectPropertyItem(PROPERTIES_HEADER_GEOMETRY,
												   pos, size, this));
}

// const CyberiadaSMPoint& getPosition() const { return geometry.pos; }
// void setPositionX(double x) { geometry.pos.x = x; }
// void setPositionY(double y) { geometry.pos.y = y; }
// void changePosition(const CyberiadaSMPoint& new_pos) { geometry.pos = new_pos; }
// const CyberiadaSMSize& getSize() const { return geometry.size; }
// void setSizeWidth(double w) { geometry.size.width = w; }
// void setSizeHeight(double h) { geometry.size.height = h; }
// void changeSize(const CyberiadaSMSize& new_size) { geometry.size = new_size; }
