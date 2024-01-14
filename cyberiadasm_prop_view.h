/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Properties View
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

#ifndef CYBERIADA_SM_PROPERTIES_VIEW
#define CYBERIADA_SM_PROPERTIES_VIEW

#include <QTreeView>
#include <QSortFilterProxyModel>
#include "cyberiadasm_model.h"

class CyberiadaSMPropertiesView: public QTreeView {
Q_OBJECT
public:
	CyberiadaSMPropertiesView(QWidget* parent);
	void setModels(QSortFilterProxyModel* _prop_model,
				   QSortFilterProxyModel* _tree_model,
				   CyberiadaSMModel* _source_model);
											  
public slots:
	void slotIndexActivated(const QModelIndex& index);
	void setRootIndex(const QModelIndex& index);
	
private:
	QSortFilterProxyModel* prop_model;
	QSortFilterProxyModel* tree_model;
	CyberiadaSMModel* source_model;
};

#endif
