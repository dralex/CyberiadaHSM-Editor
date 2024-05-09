/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The implementaton of the root item for the State Machine Model tree
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

#include "cyberiada_root_item.h"
#include "myassert.h"
#include "cyberiada_visible_item.h"
#include "cyberiada_constants.h"

CyberiadaRootItem::CyberiadaRootItem():
	CyberiadaAbstractItem(nodeRoot, NULL)
{
	CyberiadaVisibleItem* sm = new CyberiadaVisibleItem(nodeSM,
														SM_DEFAULT_TITLE,
														false,
														false,
														this);
	addChild(sm);

	CyberiadaVisibleItem* meta = new CyberiadaVisibleItem(nodeMetainfo,
														  METAINFORMATION_TITLE,
														  false,
														  false,
														  sm);
	meta->addChild(new CyberiadaTextPropertyItem(METAINFO_STANDARD_VERSION, "", true, true, meta));
	meta->addChild(new CyberiadaTextPropertyItem(METAINFO_PLATFORM_NAME, "", true, true, meta));
	meta->addChild(new CyberiadaTextPropertyItem(METAINFO_PLATFORM_VERSION, "", true, true, meta));
	meta->addChild(new CyberiadaTextPropertyItem(METAINFO_PLATFORM_LANGUAGE, "", true, true, meta));
	meta->addChild(new CyberiadaTextPropertyItem(METAINFO_TARGET_SYSTEM, "", true, true, meta));
	meta->addChild(new CyberiadaTextPropertyItem(METAINFO_NAME, "", true, true, meta));
	meta->addChild(new CyberiadaTextPropertyItem(METAINFO_AUTHOR, "", true, true, meta));
	meta->addChild(new CyberiadaTextPropertyItem(METAINFO_AUTHOR_CONTACT, "", true, true, meta));
	meta->addChild(new CyberiadaTextPropertyItem(METAINFO_DESCRIPTION, "", true, true, meta));
	meta->addChild(new CyberiadaTextPropertyItem(METAINFO_VERSION, "", true, true, meta));
	meta->addChild(new CyberiadaTextPropertyItem(METAINFO_DATE, "", true, true, meta));
	meta->addChild(new CyberiadaChoicePropertyItem(METAINFO_ACTIONS_ORDER, "", true, true, meta));
	meta->addChild(new CyberiadaChoicePropertyItem(METAINFO_EVENT_PROPAGATION, "", true, true, meta));

	sm->addChild(meta);
	sm->addChild(new CyberiadaVisibleItem(nodeStatesAggr,
										  STATES_AGGREGATOR_TITLE,
										  false,
										  false,
										  sm));
	sm->addChild(new CyberiadaVisibleItem(nodeTransitionsAggr,
										  TRANSITIONS_AGGREGATOR_TITLE,
										  false,
										  false,
										  sm));
}

void CyberiadaRootItem::removeChild(CyberiadaAbstractItem*)
{
	// root's children cannot be removed
	MY_ASSERT(false);
} 

void CyberiadaRootItem::moveParent(CyberiadaAbstractItem*)
{
	// root's parent cannot be moved
	MY_ASSERT(false);
}

void CyberiadaRootItem::rename(const QString&)
{
	// root's title cannot be changed
	MY_ASSERT(false);
}
