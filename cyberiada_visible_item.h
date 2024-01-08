/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The visible items for the State Machine Model tree
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

#ifndef CYBERIADA_VISIBLE_ITEM_HEADER
#define CYBERIADA_VISIBLE_ITEM_HEADER

#include "cyberiada_abstract_item.h"

class CyberiadaVisibleItem: public CyberiadaAbstractItem {
public:
	CyberiadaVisibleItem(CyberiadaItemType t, const QString& _title, CyberiadaAbstractItem* parent):
		CyberiadaAbstractItem(parent), title(_title)
		{
			node_type = t;
		}

	virtual QString getTitle() const { return title; };
	virtual void rename(const QString& new_title) { title = new_title; };

protected:

	QString title;
};

#endif
