/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The implementation of visible items for the State Machine Model tree
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

#include <QDebug>
#include "cyberiada_visible_item.h"
#include "myassert.h"

QString CyberiadaTextPropertyItem::getValue() const
{
	return value;
}

bool CyberiadaTextPropertyItem::setValue(const QString& _value)
{
	if (!isEditable()) {
		qDebug() << "1";
		return false;
	}
	if (_value == value) {
//		qDebug() << "2";
		return false;
	}
	if (_value.isEmpty() && !allow_empty) {
//		qDebug() << "3";
		return false;
	}
//	qDebug() << "4";
	value = _value;
	return true;
}	

QString CyberiadaFloatPropertyItem::getValue() const
{
	return QString::number(value, 'f', precision);
}

bool CyberiadaFloatPropertyItem::setValue(const QString& value_str)
{
	if (isEditable()) {
		bool ok = false;
		double num_value = value_str.toDouble(&ok);
		if (!ok) {
			num_value = 0.0;
		}
		if (value == num_value) {
			return false;
		}
		value = num_value;
		return true;
	} else {
		return false;
	}
}	

CyberiadaVisibleItem::CyberiadaVisibleItem(CyberiadaItemType t, const QString& _title,
										   bool editable, bool allow_empty,
										   CyberiadaAbstractItem* parent):
	CyberiadaAbstractItem(t, parent)
{
	addChild(new CyberiadaTextPropertyItem(PROPERTIES_HEADER_TITLE, _title,
										   editable, allow_empty, this));
}


QString CyberiadaVisibleItem::getTitle() const
{
	CyberiadaTextPropertyItem* property = static_cast<CyberiadaTextPropertyItem*>(child(0));
	MY_ASSERT(property);
	return property->getValue();
}

bool CyberiadaVisibleItem::rename(const QString& new_title)
{
	CyberiadaTextPropertyItem* property = static_cast<CyberiadaTextPropertyItem*>(child(0));
	MY_ASSERT(property);
	return property->setValue(new_title);
}
