/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Hierarchy Proxy Model implementation
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
#include <QItemSelection>
#include "cyberiadasm_tree_proxy_model.h"
#include "myassert.h"
#include "cyberiada_abstract_item.h"
#include "cyberiadasm_model.h"

CyberiadaSMTreeProxyModel::CyberiadaSMTreeProxyModel(QObject *parent):
	QSortFilterProxyModel(parent)
{
}

bool CyberiadaSMTreeProxyModel::filterAcceptsColumn(int source_column, const QModelIndex &) const
{
	return source_column == 0;
}

bool CyberiadaSMTreeProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
	//qDebug() << "filter tree" << source_row << (void*)(source_parent.internalPointer());
	if (!source_parent.isValid()) {
		//qDebug() << "filter tree result: invalid - true";
		return true;
	}
	const CyberiadaSMModel* source_model = static_cast<const CyberiadaSMModel*>(sourceModel());
	const CyberiadaAbstractItem* parent_item = source_model->indexToItem(source_parent);
	MY_ASSERT(parent_item);
	if (source_row < 0 || source_row >= parent_item->childCount()) {
		//qDebug() << "filter tree result: bad row - false";
		return false;
	}
	const CyberiadaAbstractItem* item = parent_item->child(source_row);
	MY_ASSERT(item);
	//qDebug() << "filter tree result:" << !item->isProperty();
	return !item->isProperty();
}

// QModelIndex CyberiadaSMTreeProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
// {
// 	//qDebug() << "mFs" << sourceIndex.row() << sourceIndex.column() << (void*)sourceIndex.internalPointer();
// 	QModelIndex res = QSortFilterProxyModel::mapFromSource(sourceIndex);
// 	//qDebug() << "mFs result" << res.row() << res.column() << (void*)res.internalPointer();
// 	return res;
// }

// QModelIndex CyberiadaSMTreeProxyModel::mapToSource(const QModelIndex &proxyIndex) const
// {
// 	//qDebug() << "mTs" << proxyIndex.row() << proxyIndex.column() << (void*)proxyIndex.internalPointer();
// 	QModelIndex res = QSortFilterProxyModel::mapToSource(proxyIndex);
// 	//qDebug() << "mTs result" << res.row() << res.column() << (void*)res.internalPointer();
// 	return res;
// }

// QVariant CyberiadaSMTreeProxyModel::data(const QModelIndex &index, int role) const
// {
// 	return sourceModel()->data(mapToSource(index), role);
// }

// QModelIndex CyberiadaSMTreeProxyModel::index(int row, int column, const QModelIndex &parent) const
// {
// 	return mapFromSource(sourceModel()->index(row, column, mapToSource(parent)));
// }

// QModelIndex CyberiadaSMTreeProxyModel::parent(const QModelIndex &index) const
// {
// 	return mapFromSource(sourceModel()->parent(mapToSource(index)));
// }

// int CyberiadaSMTreeProxyModel::rowCount(const QModelIndex &parent) const
// {
// 	if (!parent.isValid()) {
// 		return 1;
// 	}
// 	const CyberiadaAbstractItem* item = static_cast<const CyberiadaSMModel*>(sourceModel())->indexToItem(parent);
// 	MY_ASSERT(item);
// 	return item->childCountNoProperties();
// }

// int CyberiadaSMTreeProxyModel::columnCount(const QModelIndex&) const
// {
// 	return 1;
// }

// QModelIndex CyberiadaSMTreeProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
// {
// 	qDebug() << "mFs";
// 	if (!sourceIndex.isValid()) {
// 		return QModelIndex();
// 	}
// 	const CyberiadaSMModel* source_model = static_cast<const CyberiadaSMModel*>(sourceModel());
// 	const CyberiadaAbstractItem* item = source_model->indexToItem(sourceIndex);
// 	MY_ASSERT(item);
// 	if (item->isRoot()) {
// 		return createIndex(0, 0, (void*)item);
// 	}
// 	if (item->isProperty()) {
// 		const CyberiadaAbstractItem* parent = item->parent();
// 		while (parent && parent->isProperty()) parent = parent->parent();
// 		if (!parent) return QModelIndex();
// 		item = parent;
// 	}
// 	int row = item->rowNoProperties();
// 	return createIndex(row, 0, (void*)item);
// }

// QModelIndex CyberiadaSMTreeProxyModel::mapToSource(const QModelIndex &proxyIndex) const
// {
// 	qDebug() << "mTs";
// 	if (!proxyIndex.isValid()) {
// 		return QModelIndex();
// 	}
// 	const CyberiadaSMModel* source_model = static_cast<const CyberiadaSMModel*>(sourceModel());
// 	const CyberiadaAbstractItem* item = static_cast<const CyberiadaAbstractItem*>(proxyIndex.internalPointer());
// 	MY_ASSERT(item);
// 	return source_model->itemToIndex(item);
// }

// QItemSelection CyberiadaSMTreeProxyModel::mapSelectionFromSource(const QItemSelection &sourceSelection) const
// {
// 	return QAbstractProxyModel::mapSelectionFromSource(sourceSelection);
// }

// QItemSelection CyberiadaSMTreeProxyModel::mapSelectionToSource(const QItemSelection &proxySelection) const
// {
// 	return QAbstractProxyModel::mapSelectionToSource(proxySelection);
// }
