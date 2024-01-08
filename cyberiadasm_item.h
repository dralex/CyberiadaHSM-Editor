/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The core item for the State Machine Model
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

#ifndef CYBERIADA_SM_ITEM_HEADER
#define CYBERIADA_SM_ITEM_HEADER

#include "cyberiada_visible_item.h"

struct CyberiadaSMPoint {
	CyberiadaSMPoint(double _x = 0.0, double _y = 0.0):
		x(_x), y(_y) {
	}
	double x, y;
};

struct CyberiadaSMSize {
	CyberiadaSMSize(double _w = 0.0, double _h = 0.0):
		width(_w), height(_h) {
	}
	double width, height;
};

class CyberiadaSMItem: public CyberiadaVisibleItem {
public:
	CyberiadaSMItem(CyberiadaItemType t, const QString& id,
					const QString& title, bool has_action,
					const QString& action,
					CyberiadaAbstractItem* parent = NULL);

	int findChild(const QString& id) const;
	const QString& getID() const { return id; }
	QString getAction() const;

	virtual QString getTitle() const {
		return CyberiadaVisibleItem::getTitle() + QString("[%1]").arg(id); 
	}
	
protected:

	bool	has_action;
	QString id;
};

#endif
