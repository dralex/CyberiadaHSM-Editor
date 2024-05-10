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
#include <cyberiada/cyberiadamlpp.h>

class CyberiadaSMModel: public QAbstractItemModel {
Q_OBJECT

public:
	CyberiadaSMModel(QObject *parent);
	~CyberiadaSMModel();

	// CORE FUNCTIONALITY
	void                                reset();
	void                                loadDocument(const QString& path);

	// DATA REPRESENTATION
	QVariant                            data(const QModelIndex &index, int role) const;	
	Qt::ItemFlags                       flags(const QModelIndex &index) const;
	bool                                hasIndex(int row, int column, const QModelIndex & parent = QModelIndex()) const;
	QModelIndex                         index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
	QModelIndex                         parent(const QModelIndex &index) const;
	int                                 rowCount(const QModelIndex &parent = QModelIndex()) const;
	int                                 columnCount(const QModelIndex &parent = QModelIndex()) const;
	bool                                hasChildren(const QModelIndex & parent = QModelIndex()) const;
	QIcon                               getIndexIcon(const QModelIndex& index) const;
	QIcon                               getElementIcon(Cyberiada::ElementType type) const;
	
	// EDITING
	bool                                setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole);

	// DRAG & DROP
	Qt::DropActions                     supportedDropActions() const;
	bool                                dropMimeData(const QMimeData *data,
													 Qt::DropAction action, int row, int column, const QModelIndex &parent);
	QMimeData*                          mimeData(const QModelIndexList &indexes) const;
	QStringList                         mimeTypes() const;

	// INDEXES
	QModelIndex                         rootIndex() const;
	QModelIndex                         documentIndex() const;
	QModelIndex                         elementToIndex(const Cyberiada::Element* element) const;
	const Cyberiada::Element*           indexToElement(const QModelIndex& index) const;
	Cyberiada::Element*                 indexToElement(const QModelIndex& index);
	const Cyberiada::Document*          rootDocument() const;
	bool                                isSMIndex(const QModelIndex& index) const;
	bool                                isInitialIndex(const QModelIndex& index) const;
	bool                                isStateIndex(const QModelIndex& index) const;
	bool                                isSimpleStateIndex(const QModelIndex& index) const;
	bool                                isCompositeStateIndex(const QModelIndex& index) const;
	bool                                isTransitionIndex(const QModelIndex& index) const;

signals:
	void                                dataChanged(const QModelIndex & topLeft, const QModelIndex & bottomRight);
	void                                modelAboutToBeReset();
	void                                modelReset();

private:
	void                                move(Cyberiada::Element* element, Cyberiada::ElementCollection* target_parent);
	
	Cyberiada::Document*                root;
	QString							   	cyberiadaStateMimeType;
	QIcon                              	emptyIcon;
	QMap<Cyberiada::ElementType, QIcon> icons;
};

#endif
