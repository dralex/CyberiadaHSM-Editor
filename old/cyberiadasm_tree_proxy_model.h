/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Hierarchy Proxy Model
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

#ifndef CYBERIADA_SM_TREE_PROXY_MODEL_HEADER
#define CYBERIADA_SM_TREE_PROXY_MODEL_HEADER

#include <QSortFilterProxyModel>

class CyberiadaSMTreeProxyModel: public QSortFilterProxyModel {
Q_OBJECT
 
public:
	CyberiadaSMTreeProxyModel(QObject *parent);
	
	bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const;
	bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
};

#endif
