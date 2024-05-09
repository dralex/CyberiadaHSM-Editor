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
#include "cyberiada_constants.h"

class CyberiadaPropertyItem: public CyberiadaAbstractItem {
public:
	CyberiadaPropertyItem(CyberiadaItemType t, const QString& _name,
						  bool _editable, CyberiadaAbstractItem* parent):
		CyberiadaAbstractItem(t, parent), name(_name), editable(_editable) 
		{
		}

	virtual QString getTitle() const { return name; };
	const QString& getName() const { return name; }
	virtual QString getValue() const = 0;
	virtual bool setValue(const QString& ) { return false; }
	bool isEditable() const { return editable; }

private:
	QString name;
	bool    editable;
};

class CyberiadaTextPropertyItem: public CyberiadaPropertyItem {
public:
	CyberiadaTextPropertyItem(const QString& _name, const QString& default_value, bool editable,
							  bool _allow_empty, CyberiadaAbstractItem* parent):
		CyberiadaPropertyItem(nodeTextProperty, _name, editable, parent),
		value(default_value), allow_empty(_allow_empty)
		{
		}

	virtual QString getValue() const;
	virtual bool setValue(const QString& _value);
	
private:
	QString value;
	bool	allow_empty;
};

class CyberiadaFloatPropertyItem: public CyberiadaPropertyItem {
public:
	CyberiadaFloatPropertyItem(const QString& _name, double default_value, bool editable,
							   int _precision = 1,
							   CyberiadaAbstractItem* parent = NULL):
		CyberiadaPropertyItem(nodeNumProperty, _name, editable, parent),
		value(default_value), precision(_precision) 
		{
		}

	virtual QString getValue() const;
	virtual bool setValue(const QString& value_str);
	
private:
	double  value;
	int 	precision;
};

class CyberiadaVisibleItem: public CyberiadaAbstractItem {
public:
	CyberiadaVisibleItem(CyberiadaItemType t, const QString& _title,
						 bool editable, bool allow_empty,
						 CyberiadaAbstractItem* parent);

	virtual QString getTitle() const;
	virtual bool rename(const QString& new_title);
};

#endif
