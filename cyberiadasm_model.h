/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Model
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

#ifndef CYBERIADA_SM_MODEL_HEADER
#define CYBERIADA_SM_MODEL_HEADER

#include <QAbstractItemModel>
#include <QIcon>
#include <QDateTime>
#include <cyberiadaml.h>
#include "cyberiada_root_item.h"
#include "cyberiada_state_item.h"
#include "cyberiada_trans_item.h"

class CyberiadaSMModel: public QAbstractItemModel {
Q_OBJECT

public:
	CyberiadaSMModel(QObject *parent);
	~CyberiadaSMModel();

	// CORE FUNCTIONALITY
	void reset();
	void loadGraph(const QString& path);
	void renameSM(const QString& new_name);

	// DATA REPRESENTATION
	QVariant data(const QModelIndex &index, int role) const;	
	Qt::ItemFlags flags(const QModelIndex &index) const;
	bool hasIndex(int row, int column, const QModelIndex & parent = QModelIndex()) const;
	QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
	QModelIndex parent(const QModelIndex &index) const;
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex &parent = QModelIndex()) const;
	bool hasChildren(const QModelIndex & parent = QModelIndex()) const;
	const QIcon& getIndexIcon(const QModelIndex& index) const; 
	
	// EDITING
	bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole);

	// DRAG & DROP
	Qt::DropActions supportedDropActions() const;
	bool dropMimeData(const QMimeData *data,
					  Qt::DropAction action, int row, int column, const QModelIndex &parent);
	QMimeData* mimeData(const QModelIndexList &indexes) const;
	QStringList mimeTypes() const;

	// INDEXES
	QModelIndex rootIndex() const;
	QModelIndex SMIndex() const;
	QModelIndex statesRootIndex() const;
	QModelIndex transitionsRootIndex() const;
	QModelIndex itemToIndex(const CyberiadaAbstractItem* item) const;
	const CyberiadaAbstractItem* indexToItem(const QModelIndex& index) const;
	CyberiadaAbstractItem* indexToItem(const QModelIndex& index);
	bool        isTrivialIndex(const QModelIndex& index) const;
	bool		isInitialStateIndex(const QModelIndex& index) const;
	bool		isStateIndex(const QModelIndex& index) const;
	bool		isTransitionIndex(const QModelIndex& index) const;
	bool		isPropertyIndex(const QModelIndex& index) const;
	const CyberiadaSMItem* idToItem(const QString& id) const;
	CyberiadaSMItem* idToItem(const QString& id);

	// LOGGING
	void dump() const;
	
signals:
	void dataChanged(const QModelIndex & topLeft, const QModelIndex & bottomRight);
	void modelAboutToBeReset();
	void modelReset();

private:
	void emitParentsChanged(const CyberiadaAbstractItem* item);
	CyberiadaSMItem* convertNode(const CyberiadaNode* node,
								 CyberiadaAbstractItem* parent = NULL);
	CyberiadaTransitionItem* convertEdge(const CyberiadaEdge* edge,
										 CyberiadaAbstractItem* parent = NULL);
	void addChildNodes(const CyberiadaNode* parent, CyberiadaAbstractItem* parent_item, bool toplevel);
	void addToMap(const QString& id, CyberiadaSMItem* item);
	QString generateID() const;
	
	void move(CyberiadaAbstractItem* item, CyberiadaAbstractItem* target_item);
	void initTrees();
	void cleanupTrees();
	//void dumpRecursively(const CyberiadaSMItem* root, const QString& indent) const;
	
	QString							   	  sm_name, sm_version;
	CyberiadaRootItem                     *root;
	CyberiadaVisibleItem                  *sm_root, *states_root, *trans_root;
	QMap<QString, CyberiadaSMItem*>       states_map;
	QString							   	  cyberiadaStateMimeType;
	QIcon                              	  emptyIcon;
	QIcon							   	  smRootIcon, stateRootIcon, transRootIcon;
	QIcon                              	  stateIcon, initialStateIcon, transIcon, actionIcon, commentIcon, stateLinkIcon;
	QIcon                                 textpropIcon, textpropnoneditIcon, numpropIcon;
	QIcon                                 geompropIcon, pointpropIcon, rectpropIcon, pathpropIcon;
};

#endif
