/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Properties View implementation
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

#include "cyberiadasm_model.h"
#include "cyberiadasm_prop_view.h"
#include "cyberiadasm_prop_proxy_model.h"
#include "myassert.h"

CyberiadaSMPropertiesView::CyberiadaSMPropertiesView(QWidget* parent):
	QTreeView(parent), prop_model(NULL), tree_model(NULL), source_model(NULL)
{
}

void CyberiadaSMPropertiesView::setModels(QSortFilterProxyModel* _prop_model,
										  QSortFilterProxyModel* _tree_model,
										  CyberiadaSMModel* _source_model)
{
	prop_model = _prop_model;
	tree_model = _tree_model;
	source_model = _source_model;
	setModel(prop_model);
}

void CyberiadaSMPropertiesView::setRootIndex(const QModelIndex& index)
{
	if (prop_model == NULL) {
		//qDebug() << "no model, set null root index";
		QTreeView::setRootIndex(QModelIndex());
	} else if (!index.isValid()) {
		//qDebug() << "index is inivalid, set null root index";
		static_cast<CyberiadaSMPropertyProxyModel*>(prop_model)->setRootItem(NULL);
		prop_model->invalidate();
		QTreeView::setRootIndex(QModelIndex());
	} else {
		CyberiadaAbstractItem* item = source_model->indexToItem(tree_model->mapToSource(index));
		MY_ASSERT(item);
		static_cast<CyberiadaSMPropertyProxyModel*>(prop_model)->setRootItem(item);
		prop_model->invalidate();
		//qDebug() << "set root" << item->getType() << item->getTitle();
		//qDebug() << "root children" << item->childCount();
		QModelIndex source_index = source_model->itemToIndex(item);
		///qDebug() << "source" << source_index.row() << source_index.column() << (void*)source_index.internalPointer();
		//qDebug() << "source children" << source_model->rowCount(source_index);
		QModelIndex prop_index = prop_model->mapFromSource(source_index);
		//qDebug() << "prop" << prop_index.row() << prop_index.column() << (void*)prop_index.internalPointer();
		//QModelIndex prop_parent_index = prop_model->parent(prop_index);
		//qDebug() << "prop parent" << prop_parent_index.row() << prop_parent_index.column() << (void*)prop_parent_index.internalPointer();
		//qDebug() << "prop rows" << prop_model->rowCount(prop_index);
		//qDebug() << "prop data" << prop_model->data(prop_index, Qt::DisplayRole);
		//qDebug() << "prop data value" << prop_model->data(prop_model->index(prop_index.row(), 1, prop_parent_index),
		//												  Qt::DisplayRole);
		QTreeView::setRootIndex(prop_index);
	}
}

void CyberiadaSMPropertiesView::slotIndexActivated(const QModelIndex& index)
{
	//qDebug() << "index activated" << index.row() << index.column() << (void*)index.internalPointer();
	if (!index.isValid()) {
		//qDebug() << "index is inivalid, set null model";
		setModel(NULL);
	} else {
		MY_ASSERT(index.model() == tree_model);
		// qDebug() << "index data" << tree_model->data(index, Qt::DisplayRole);
		// qDebug() << "index data value" << tree_model->data(tree_model->index(index.row(),
		// 																	 1,
		// 																	 tree_model->parent(index)),
		// 												   Qt::DisplayRole);
		if (model() == NULL) {
			setModel(prop_model);
		}
		setRootIndex(index);
	}
}
