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

class CyberiadaGeometryPointPropertyItem: public CyberiadaPropertyItem {
public:
	CyberiadaGeometryPointPropertyItem(const QString& _name,
									   const CyberiadaSMPoint& pos,
									   CyberiadaAbstractItem* parent);

	virtual QString getValue() const;
};

class CyberiadaGeometryRectPropertyItem: public CyberiadaPropertyItem {
public:
	CyberiadaGeometryRectPropertyItem(const QString& _name,
									  const CyberiadaSMPoint& pos,
									  const CyberiadaSMSize& size,
									  CyberiadaAbstractItem* parent);
	virtual QString getValue() const;
};

class CyberiadaSceneItem: public CyberiadaSMItem {
public:
	CyberiadaSceneItem(CyberiadaItemType t, const QString& id,
					   const QString& title, bool title_editable,
					   bool has_action, const QString& action,
					   CyberiadaAbstractItem* parent = NULL):
		CyberiadaSMItem(t, id, title, title_editable, has_action, action, parent)
		{
		}
};

class CyberiadaScenePointItem: public CyberiadaSceneItem {
public:
	CyberiadaScenePointItem(CyberiadaItemType t, const QString& id,
							const QString& title, bool title_editable,
							bool has_action, const QString& action,
							const CyberiadaSMPoint& pos,
							CyberiadaAbstractItem* parent = NULL);
};

class CyberiadaSceneRectItem: public CyberiadaSceneItem {
public:
	CyberiadaSceneRectItem(CyberiadaItemType t, const QString& id,
						   const QString& title, bool title_editable,
						   bool has_action, const QString& action,
						   const CyberiadaSMPoint& pos,
						   const CyberiadaSMSize& size,
						   CyberiadaAbstractItem* parent = NULL);	
};

class CyberiadaSimpleStateItem: public CyberiadaSceneRectItem {
public:
	CyberiadaSimpleStateItem(const QString& id, const QString& title, const QString& action,
							 const CyberiadaSMPoint& pos,
							 const CyberiadaSMSize& size,
							 CyberiadaAbstractItem* parent = NULL):
		CyberiadaSceneRectItem(nodeSimpleState, id, title, true, true,
							   action, pos, size, parent)
		{
		}
};

class CyberiadaCompositeStateItem: public CyberiadaSceneRectItem {
public:
	CyberiadaCompositeStateItem(const QString& id, const QString& title, const QString& action,
								const CyberiadaSMPoint& pos,
								const CyberiadaSMSize& size,
								CyberiadaAbstractItem* parent = NULL):
		CyberiadaSceneRectItem(nodeCompositeState, id, title, true, true,
							   action, pos, size, parent)
		{
		}
};

class CyberiadaInitialPseudostateItem: public CyberiadaScenePointItem {
public:
	CyberiadaInitialPseudostateItem(const QString& id,
							  const CyberiadaSMPoint& pos,
							  CyberiadaAbstractItem* parent = NULL):
		CyberiadaScenePointItem(nodeInitialPseudostate, id,
								CYBERIADA_INITIAL_NODE_TITLE,
								false, false, "", pos, parent)
		{
		}
};

class CyberiadaCommentItem: public CyberiadaSceneRectItem {
public:
	CyberiadaCommentItem(const QString& id,
						 const QString& action,
						 const CyberiadaSMPoint& pos,
						 const CyberiadaSMSize& size,
						 CyberiadaAbstractItem* parent = NULL):
		CyberiadaSceneRectItem(nodeComment, id,
							   CYBERIADA_COMMENT_NODE_TITLE,
							   false, true, action, pos, size, parent)	
		{
		}
};

#endif
