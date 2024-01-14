/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Model implementation
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

#include <QIcon>
#include <QList>
#include <QMimeData>
#include <QDebug>

#include "cyberiadasm_model.h"
#include "myassert.h"
#include "cyberiada_constants.h"

CyberiadaSMModel::CyberiadaSMModel(QObject *parent):
	QAbstractItemModel(parent), sm_name(SM_DEFAULT_TITLE)
{
	smRootIcon = QIcon(":/Icons/images/sm-root.png");
	stateRootIcon = QIcon(":/Icons/images/state-root.png");
	transRootIcon = QIcon(":/Icons/images/trans-root.png");
	stateIcon = QIcon(":/Icons/images/state.png");
	initialStateIcon = QIcon(":/Icons/images/init-state.png");
	transIcon = QIcon(":/Icons/images/trans.png");
	actionIcon = QIcon(":/Icons/images/action.png");
	commentIcon = QIcon(":/Icons/images/comment.png");
	geompropIcon = QIcon(":/Icons/images/geomprop.png");
	textpropIcon = QIcon(":/Icons/images/textprop.png");
	textpropnoneditIcon = QIcon(":/Icons/images/textpropnonedit.png");
	numpropIcon = QIcon(":/Icons/images/numprop.png");
	pointpropIcon = QIcon(":/Icons/images/pointprop.png");
	rectpropIcon = QIcon(":/Icons/images/rectprop.png");
	pathpropIcon = QIcon(":/Icons/images/pathprop.png");
	stateLinkIcon = QIcon(":/Icons/images/statelink.png");
	cyberiadaStateMimeType = CYBERIADA_MIME_TYPE_STATE;

	initTrees();
}

void CyberiadaSMModel::initTrees()
{
	root = new CyberiadaRootItem();
	sm_root = static_cast<CyberiadaVisibleItem*>(root->child(0));
	states_root = static_cast<CyberiadaVisibleItem*>(sm_root->child(1));
	trans_root = static_cast<CyberiadaVisibleItem*>(sm_root->child(2));
	//qDebug() << "root" << (void*)root;
	//qDebug() << "sm root" << (void*)sm_root;
	//qDebug() << "states root" << (void*)states_root;
	//qDebug() << "trans root" << (void*)trans_root;
}

void CyberiadaSMModel::cleanupTrees()
{
	states_map.clear();
	delete root;
	root = NULL;
	sm_root = states_root = trans_root = NULL;
}

CyberiadaSMModel::~CyberiadaSMModel()
{
	cleanupTrees();
}

void CyberiadaSMModel::reset()
{
	beginResetModel();
	cleanupTrees();
	initTrees();
	endResetModel();	
}

QString CyberiadaSMModel::generateID() const
{
	QString s;
	do {
		int num = qrand() % 10000;
		s = QString("id-%1").arg(num);
	} while (states_map.contains(s));
	return s;
}

CyberiadaSMItem* CyberiadaSMModel::convertNode(const CyberiadaNode* node,
											   CyberiadaAbstractItem* parent)
{
	if (node->type == cybNodeInitial) {
		CyberiadaSMPoint point(node->geometry_rect.x,
							   node->geometry_rect.y);
		return new CyberiadaInitialStateItem(node->id, point, parent);
	} else {
		CyberiadaSMPoint point(node->geometry_rect.x,
							   node->geometry_rect.y);
		CyberiadaSMSize size(node->geometry_rect.width,
							 node->geometry_rect.height);
		if (node->type == cybNodeComment) {
			QString id = node->id;
			if (id.isEmpty()) {
				generateID();
			}
			return new CyberiadaCommentItem(id, node->action,
											point, size,
											parent);
		} else {
			QString title = QString(node->title).trimmed();
			if (title.isEmpty())
				title = CYBERIADA_EMPTY_NODE_TITLE;
			return new CyberiadaStateItem(QString(node->id).trimmed(), title,
										  QString(node->action).trimmed(),
										  point, size,
										  parent);
		}
	}
}

CyberiadaTransitionItem* CyberiadaSMModel::convertEdge(const CyberiadaEdge* edge,
													   CyberiadaAbstractItem* parent)
{
	CyberiadaSMPoint source_port(edge->geometry_source_point.x,
								 edge->geometry_source_point.y);
	CyberiadaSMPoint target_port(edge->geometry_target_point.x,
								 edge->geometry_target_point.y);
	CyberiadaSMPoint label;
	QList<CyberiadaSMPoint> path;
	for (CyberiadaPolyline* cp = edge->geometry_polyline; cp; cp = cp->next) {
		path.append(CyberiadaSMPoint(cp->point.x, cp->point.y));
	}
	const CyberiadaSMItem* source = states_map.value(QString(edge->source->id).trimmed(),
													 NULL);
	const CyberiadaSMItem* target = states_map.value(QString(edge->target->id).trimmed(),
													 NULL);
	if (!source || !target)
		return NULL;
	return new CyberiadaTransitionItem(source,
									   target,
									   QString(edge->id).trimmed(),
									   QString(edge->action).trimmed(),
									   label,
									   source_port,
									   target_port,
									   path,
									   parent);
}

void CyberiadaSMModel::addChildNodes(const CyberiadaNode* parent_node,
									 CyberiadaAbstractItem* parent_item,
									 bool toplevel)
{
	for (const CyberiadaNode* cur_node = parent_node; cur_node; cur_node = cur_node->next) {
		CyberiadaSMItem* item = convertNode(cur_node, parent_item);
		if (toplevel) {
			if (cur_node->children) {
				addChildNodes(cur_node->children, parent_item, false);
			}
		} else {
			addToMap(item->getID(), item);
			parent_item->addChild(item);
			if (cur_node->children) {
				addChildNodes(cur_node->children, item, false);
			}
		}
	}
}

void CyberiadaSMModel::addToMap(const QString& id, CyberiadaSMItem* item)
{
	QString new_id = id;
	while (states_map.contains(new_id)) {
		new_id += "_";
	}
	states_map.insert(new_id, item);
}

void CyberiadaSMModel::renameSM(const QString& new_name)
{
	if (new_name.length() > 0) {
		sm_name = new_name;
		sm_root->rename(sm_name);
	}
}

void CyberiadaSMModel::loadGraph(const QString& path)
{	
	reset();
	
	int res;
	CyberiadaSM sm;
	
	if ((res = cyberiada_read_sm(&sm, path.toUtf8(), cybxmlUnknown)) != CYBERIADA_NO_ERROR) {
		qDebug() << "Cannot load Cyberiada SM graph. Error code: " << res;
		return ;
	}

	beginResetModel();
	
	sm_version = sm.version;

	renameSM(sm.name);

	addChildNodes(sm.nodes, states_root, true);

	for (CyberiadaEdge* cur_edge = sm.edges; cur_edge; cur_edge = cur_edge->next) {
		CyberiadaTransitionItem* item = convertEdge(cur_edge, trans_root);
		if (item == NULL) {
			qDebug() << "Cannot load Cyberiada SM graph. Cannot load edge: " << cur_edge->id;			
			cyberiada_cleanup_sm(&sm);
			endResetModel();
			return ;
		}
		trans_root->addChild(item);
	}

	endResetModel();

	cyberiada_cleanup_sm(&sm);
}

QVariant CyberiadaSMModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || index == rootIndex())
		return QVariant();

	CyberiadaAbstractItem *item;
	int column = index.column();
	
	switch(role) {
	case Qt::DisplayRole:
	case Qt::ToolTipRole:
	case Qt::EditRole:
		item = static_cast<CyberiadaAbstractItem*>(index.internalPointer());
		MY_ASSERT(item);		
		if (column == 0) {
			return item->getTitle();
		} else if (column == 1 && item->isProperty()) {
			const CyberiadaPropertyItem* property_item = static_cast<const CyberiadaPropertyItem*>(item);
			return property_item->getValue();
		} else {
			return QVariant();
		}
	case Qt::DecorationRole:
		if (column == 0) {
			return QVariant(getIndexIcon(index));
		} else {
			return QVariant();
		}
	default:
		return QVariant();
	}
}

const QIcon& CyberiadaSMModel::getIndexIcon(const QModelIndex& index) const
{
	if (!index.isValid()) return emptyIcon;
	CyberiadaAbstractItem *item = static_cast<CyberiadaAbstractItem*>(index.internalPointer());
	const CyberiadaPropertyItem* property_item;
	MY_ASSERT(item);
	switch (item->getType()) {
	case nodeSM:
		return smRootIcon;
	case nodeStatesAggr:
		return stateRootIcon;
	case nodeTransitionsAggr:
		return transRootIcon;
	case nodeInitialState:
		return initialStateIcon;
	case nodeState:
		return stateIcon;
	case nodeComment:
		return commentIcon;
	case nodeTransition:
		return transIcon;
	case nodeTextProperty:
		property_item = static_cast<const CyberiadaPropertyItem*>(item);
		if (property_item->isEditable()) {
			return textpropIcon;
		} else {
			return textpropnoneditIcon;
		}
	case nodeNumProperty:
		return numpropIcon;
	case nodeGeomPointProperty:
		return pointpropIcon;
	case nodeGeomRectProperty:
		return rectpropIcon;
	case nodeGeomPathProperty:
	case nodeGeomTransProperty:
		return pathpropIcon;
	case nodeStateLink:
		return stateLinkIcon;
	default:
		return emptyIcon;
	}
}

bool CyberiadaSMModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
	//qDebug() << "set data" << index.row() << index.column() << (void*)index.internalPointer() << value << role;
	if (index.isValid() && role == Qt::EditRole) {
		MY_ASSERT(index.column() == 1);
		MY_ASSERT(isPropertyIndex(index));
		QString newStr = value.toString();
		if (newStr.isEmpty())
			return false;
		CyberiadaPropertyItem *item = static_cast<CyberiadaPropertyItem*>(index.internalPointer());
		MY_ASSERT(item);
		//qDebug() << "try edit" << item->getTitle() << newStr;
		if (item->setValue(newStr)) {
			//qDebug() << "edited";
			emitParentsChanged(item);
			return true;
		}
	}
	return false;
}

Qt::ItemFlags CyberiadaSMModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags defaultFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	if (index == statesRootIndex()) {
		return Qt::ItemIsDropEnabled | defaultFlags;
	} else if (isStateIndex(index) || isInitialStateIndex(index)) {
		defaultFlags |= Qt::ItemIsDragEnabled;
		if (isStateIndex(index)) {
			defaultFlags |= Qt::ItemIsDropEnabled;
		}
		return defaultFlags;
	} else if (isPropertyIndex(index)) {
		const CyberiadaPropertyItem* property_item = static_cast<const CyberiadaPropertyItem*>(indexToItem(index));
		MY_ASSERT(property_item);
		if (index.column() == 1 && property_item->isEditable()) {
			defaultFlags |= Qt::ItemIsEditable;
		}
	}
	return defaultFlags;
}

bool CyberiadaSMModel::hasIndex(int row, int column, const QModelIndex & parent) const
{
	if(!parent.isValid()) {
		return row == 0;
	}
	CyberiadaAbstractItem *parentItem = static_cast<CyberiadaAbstractItem*>(parent.internalPointer());
	MY_ASSERT(parentItem);
	if(parentItem->childCount() > 0) {
		return row >= 0 && row < parentItem->childCount();
	} else {
		return false;
	}
}

QModelIndex CyberiadaSMModel::index(int row, int column, const QModelIndex &parent) const
{
	//qDebug() << "index" << row << column << (void*)parent.internalPointer();
	if (!parent.isValid() || !hasIndex(row, column, parent)) {
		//qDebug() << "index result: empty";
		return rootIndex();
	}
	CyberiadaAbstractItem* parentItem = static_cast<CyberiadaAbstractItem*>(parent.internalPointer());
	MY_ASSERT(parentItem);
	CyberiadaAbstractItem* childItem = parentItem->child(row);
	MY_ASSERT(childItem);
	//qDebug() << "index result: child" << row << column << (void*)childItem;
	return createIndex(row, column, childItem);
}

QModelIndex CyberiadaSMModel::parent(const QModelIndex &index) const
{
	//qDebug() << "parent" << index.row() << index.column() << (void*)index.internalPointer();
	if (!index.isValid() || index == rootIndex()) {
		//qDebug() << "parent result: empty";
		return QModelIndex();
	}
	if (index == SMIndex()) {
		//qDebug() << "parent result: root";
		return rootIndex();
	}
	CyberiadaAbstractItem* childItem = static_cast<CyberiadaAbstractItem*>(index.internalPointer());
	MY_ASSERT(childItem);
	CyberiadaAbstractItem* parentItem = childItem->parent();
	MY_ASSERT(parentItem);
	if (parentItem == root) {
		//qDebug() << "parent result: root2";		
		return rootIndex();
	}
	MY_ASSERT(parentItem);
	//qDebug() << "parent result" << parentItem->row() << 0 << (void*)parentItem;
	return createIndex(parentItem->row(), 0, parentItem);
}

int CyberiadaSMModel::rowCount(const QModelIndex &parent) const
{
	//qDebug() << "row count" << (void*)parent.internalPointer();
	CyberiadaAbstractItem* item;
	if (!parent.isValid() || parent == rootIndex()) {
		item = root;
	} else {
		item = static_cast<CyberiadaAbstractItem*>(parent.internalPointer());
	}
	MY_ASSERT(item);
	int r = item->childCount();
	//qDebug() << "result:" << r;
	return r;
}

int CyberiadaSMModel::columnCount(const QModelIndex &) const
{
	return 2;
}

bool CyberiadaSMModel::hasChildren(const QModelIndex & parent) const
{
	return rowCount(parent) > 0;
}

QModelIndex CyberiadaSMModel::rootIndex() const
{
	return createIndex(0, 0, (void*)root);
}

QModelIndex CyberiadaSMModel::SMIndex() const
{
	return createIndex(0, 0, (void*)sm_root);
}

QModelIndex CyberiadaSMModel::statesRootIndex() const
{
	return createIndex(0, 0, (void*)states_root);
}

QModelIndex CyberiadaSMModel::transitionsRootIndex() const
{
	return createIndex(1, 0, (void*)trans_root);
}

QModelIndex CyberiadaSMModel::itemToIndex(const CyberiadaAbstractItem* item) const
{
	if (item == NULL) return QModelIndex();
	if (item->isRoot()) {
		return rootIndex();
	} else {
		MY_ASSERT(item);
		const CyberiadaAbstractItem* parent = item->parent();
		MY_ASSERT(parent);
		return index(item->row(), 0, itemToIndex(parent));
	}
}

void CyberiadaSMModel::emitParentsChanged(const CyberiadaAbstractItem* item)
{
	MY_ASSERT(item);
	MY_ASSERT(item->isProperty());
	const CyberiadaAbstractItem* next_item = item;
	do {
		item = next_item;
		QModelIndex index = itemToIndex(item);
		MY_ASSERT(index.isValid());
		//qDebug() << "emit" << item->getType() << item->getTitle() << "changed";
		emit dataChanged(index, index);
		next_item = item->parent();
		MY_ASSERT(next_item);
	} while (next_item->isProperty() || item->isProperty());
}

const CyberiadaAbstractItem* CyberiadaSMModel::indexToItem(const QModelIndex& index) const
{
	if (!index.isValid()) return root;
	return static_cast<const CyberiadaAbstractItem*>(index.internalPointer());
}

CyberiadaAbstractItem* CyberiadaSMModel::indexToItem(const QModelIndex& index)
{
	if (!index.isValid()) return root;
	return static_cast<CyberiadaAbstractItem*>(index.internalPointer());
}

bool CyberiadaSMModel::isTrivialIndex(const QModelIndex& index) const
{
	return (!index.isValid() ||
			index == rootIndex() ||
			index == SMIndex() ||
			index == statesRootIndex() ||
			index == transitionsRootIndex());
}

bool CyberiadaSMModel::isStateIndex(const QModelIndex& index) const
{
	if (!index.isValid()) return false;
	CyberiadaAbstractItem* item = static_cast<CyberiadaAbstractItem*>(index.internalPointer());
	MY_ASSERT(item);
	return item->getType() == nodeState;
}

bool CyberiadaSMModel::isPropertyIndex(const QModelIndex& index) const
{
	if (!index.isValid()) return false;
	CyberiadaAbstractItem* item = static_cast<CyberiadaAbstractItem*>(index.internalPointer());
	MY_ASSERT(item);
	return item->isProperty();
}

bool CyberiadaSMModel::isInitialStateIndex(const QModelIndex& index) const
{
	if (!index.isValid()) return false;
	CyberiadaAbstractItem* item = static_cast<CyberiadaAbstractItem*>(index.internalPointer());
	MY_ASSERT(item);
	return item->getType() == nodeInitialState;
}

bool CyberiadaSMModel::isTransitionIndex(const QModelIndex& index) const
{
	if (!index.isValid()) return false;
	CyberiadaAbstractItem* item = static_cast<CyberiadaAbstractItem*>(index.internalPointer());
	MY_ASSERT(item);
	return item->getType() == nodeTransition;
}

const CyberiadaSMItem* CyberiadaSMModel::idToItem(const QString& id) const
{
	return states_map.value(id, NULL);
}

CyberiadaSMItem* CyberiadaSMModel::idToItem(const QString& id)
{
	return states_map.value(id, NULL);
}

Qt::DropActions CyberiadaSMModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

void CyberiadaSMModel::move(CyberiadaAbstractItem* item, CyberiadaAbstractItem* target_item)
{
/*	QModelIndex srcindex = itemToIndex(item);
	QModelIndex parentindex = parent(srcindex);	
	QModelIndex dstindex;
	
	if (target_item == NULL) {
		dstindex = itemToIndex(target_item);
	} else {
		dstindex = statesRootIndex();
	}

	MY_ASSERT(srcindex.isValid());

	int remove_index = srcindex.row();
	int add_index = rowCount(dstindex);

	CyberiadaSMItem* source_parent = item->parent();

	beginRemoveRows(parentindex, remove_index, remove_index);
	if (source_parent == NULL) {
		states.removeAll(static_cast<CyberiadaStateItem*>(item));
	} else {
		source_parent->removeChild(item);
	}
	endRemoveRows();

	beginInsertRows(dstindex, add_index, add_index);
	if (target_item == NULL) {
		MY_ASSERT(item->isState());
		states.append(static_cast<CyberiadaStateItem*>(item));
		item->removeParent();
	} else {
		target_item->addChild(item);
	}
	endInsertRows();*/
}

bool CyberiadaSMModel::dropMimeData(const QMimeData *data,
									Qt::DropAction action, int, int column,
									const QModelIndex &parent)
{
/*	if(action == Qt::IgnoreAction) {
		return true;
	}
	if(column > 0) return false;
	CyberiadaSMItem* target_item = static_cast<CyberiadaSMItem*>(parent.internalPointer());
	if (parent != statesRootIndex()) {
		MY_ASSERT(target_item);
		MY_ASSERT(target_item->isState());
	}
	if(data->hasFormat(cyberiadaStateMimeType)) {
		QByteArray encodedData = data->data(cyberiadaStateMimeType);
		QDataStream stream(&encodedData, QIODevice::ReadOnly);
		QStringList items;
		while (!stream.atEnd()) {
			QString path;
			stream >> path;
			items.append(path);
		}
		foreach(QString id_str, items) {
			CyberiadaSMItem* source_item = static_cast<CyberiadaSMItem*>(states_map.value(id_str, NULL));
			MY_ASSERT(source_item);
			CyberiadaSMItem* source_parent = source_item->parent();
			if (source_parent == target_item)
				return false;
			move(source_item, target_item);
		}
		return true;
	} else {
		return false;
		}*/
}

QStringList CyberiadaSMModel::mimeTypes() const
{
	return QStringList(cyberiadaStateMimeType);
}

QMimeData* CyberiadaSMModel::mimeData(const QModelIndexList &indexes) const
{
	QMimeData *mimeData = new QMimeData;
	QByteArray encodedData;
	QDataStream stream(&encodedData, QIODevice::WriteOnly);
	QString path;
	foreach(QModelIndex index, indexes) {
		if (!isStateIndex(index) && !isInitialStateIndex(index)) continue;
		const CyberiadaSMItem* item = static_cast<const CyberiadaSMItem*>(indexToItem(index));
		stream << item->getID();
	}
	mimeData->setData(cyberiadaStateMimeType, encodedData);	
	return mimeData;
}

void CyberiadaSMModel::dump() const
{
//	dumpRecursively(root, "");
}

/*void CyberiadaSMModel::dumpRecursively(const CyberiadaSMItem* root, const QString& indent)
{
	QString type;
	QString descr;
	QString label;
	if(root->isRoot()) {
		type = "Root";
	} else if(root->isBox()) {
		type = "B";
		label = root->getLabel();
	} else {
		type = "F";
		const BookDescription& bd = root->getBookDescr();
		descr = bd.toString();
		if((bd.toTextString()) != root->getLabel()) {
			label = root->getLabel();
		}
	}
	logger.write(QString("%1%2%3{%4}{%5}").arg(indent).arg(type).arg(root->getID()).arg(descr).arg(label));
	QString newindent = indent + "-";
	for(int i = 0; i < root->childCount(); i++) {
		dumpRecursively(root->child(i), newindent);
		}
		}*/
