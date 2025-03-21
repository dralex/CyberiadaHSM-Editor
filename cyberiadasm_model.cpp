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
#include <QMessageBox>

#include "cyberiadasm_model.h"
#include "myassert.h"
#include "cyberiada_constants.h"

CyberiadaSMModel::CyberiadaSMModel(QObject *parent):
	QAbstractItemModel(parent)
{
	root = NULL;
	icons[Cyberiada::elementRoot] = QIcon(":/Icons/images/sm-root.png");
	icons[Cyberiada::elementSM] = QIcon(":/Icons/images/sm.png");
	icons[Cyberiada::elementSimpleState] = QIcon(":/Icons/images/state.png");
	icons[Cyberiada::elementCompositeState] = QIcon(":/Icons/images/state-comp.png"); 
	icons[Cyberiada::elementComment] = QIcon(":/Icons/images/comment.png");
	icons[Cyberiada::elementFormalComment] = QIcon(":/Icons/images/comment-machine.png");
	icons[Cyberiada::elementInitial] = QIcon(":/Icons/images/init-state.png");
	icons[Cyberiada::elementFinal] = QIcon(":/Icons/images/final-state.png");
	icons[Cyberiada::elementChoice] = QIcon(":/Icons/images/choice.png");
	icons[Cyberiada::elementTerminate] = QIcon(":/Icons/images/terminate.png");
	icons[Cyberiada::elementTransition] = QIcon(":/Icons/images/trans.png");;

	cyberiadaStateMimeType = CYBERIADA_MIME_TYPE_STATE;
}

CyberiadaSMModel::~CyberiadaSMModel()
{
	if (root) {
		delete root;
	}
}

void CyberiadaSMModel::reset()
{
	beginResetModel();
	if (root) {
		root->reset();
	}	
	endResetModel();	
}

void CyberiadaSMModel::loadDocument(const QString& path, bool reconstruct, bool reconstruct_sm)
{	
	Cyberiada::LocalDocument* new_doc = NULL;

	bool error = false;
	try {
		new_doc = new Cyberiada::LocalDocument();
		new_doc->open(path.toStdString(), Cyberiada::formatDetect, Cyberiada::geometryFormatQt,
					  reconstruct, reconstruct_sm);
	} catch (const Cyberiada::XMLException& e) {
		QMessageBox::critical(NULL, tr("Load State Machine"),
							  tr("XML grapml error:\n") + QString(e.str().c_str()));
		error = true;
	} catch (const Cyberiada::CybMLException& e) {
		QMessageBox::critical(NULL, tr("Load State Machine"),
							  tr("Wrong format of the Cyberiada grapml file:\n") + QString(e.str().c_str()));
		error = true;
	} catch (const Cyberiada::Exception& e) {
		QMessageBox::critical(NULL, tr("Load State Machine"),
							  tr("Cannot load state machine graph:\n") + QString(e.str().c_str()));
		error = true;
	}

	if (error && new_doc) {
		delete new_doc;
		new_doc = NULL;
	}

	if (new_doc) {
		beginResetModel();
		if (root) {
			delete root;
		}
		root = new_doc;
		endResetModel();
	}
}

void CyberiadaSMModel::saveDocument(bool round)
{
	if (root && !root->get_file_path().empty()) {
		root->save();
	}
}

void CyberiadaSMModel::saveAsDocument(const QString& path, Cyberiada::DocumentFormat f, bool round)
{
	if (root) {
		root->save_as(path.toStdString(), f, round);
	}
}

QVariant CyberiadaSMModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || index == rootIndex())
		return QVariant();

	const Cyberiada::Element *element;
	int column = index.column();
	
	switch(role) {
	case Qt::DisplayRole:
	case Qt::ToolTipRole:
	case Qt::EditRole:
		element = static_cast<const Cyberiada::Element*>(index.internalPointer());
		MY_ASSERT(element);		
		if (column == 0) {
			if (element->get_type() == Cyberiada::elementRoot) {
				const Cyberiada::Document* doc = static_cast<const Cyberiada::Document*>(element);
				MY_ASSERT(doc);
				return QString(doc->meta().get_string(CYBERIADA_META_NAME).c_str());
			} else if (element->get_type() == Cyberiada::elementTransition) {
				const Cyberiada::Transition* trans = static_cast<const Cyberiada::Transition*>(element);
				QString id = trans->source_element_id().c_str();
				const Cyberiada::Element* source = idToElement(id);
				MY_ASSERT(source);
				QString source_name = source->get_name().c_str();
				if (source_name.isEmpty()) {
					source_name = QString("[") + source->get_id().c_str() + "]";
				}
				id = trans->target_element_id().c_str();
				const Cyberiada::Element* target = idToElement(id);
				MY_ASSERT(target);
				QString target_name = target->get_name().c_str();
				if (target_name.isEmpty()) {
					target_name = QString("[") + target->get_id().c_str() + "]";
				}				
				return QString(source_name + " -> " + target_name);
			} else {
				QString name = element->get_name().c_str();
				if (name.isEmpty()) {
					name = QString("[") + element->get_id().c_str() + "]";
				}
				return name;
			}
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

QIcon CyberiadaSMModel::getElementIcon(Cyberiada::ElementType type) const
{
	if (icons.find(type) != icons.end()) {
		return icons[type];
	} else {
		return emptyIcon;
	}	
}

QIcon CyberiadaSMModel::getIndexIcon(const QModelIndex& index) const
{
	if (!index.isValid() || index == rootIndex()) return emptyIcon;
	const Cyberiada::Element *element = static_cast<const Cyberiada::Element*>(index.internalPointer());
	MY_ASSERT(element);
	return getElementIcon(element->get_type());
}

bool CyberiadaSMModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if(index.isValid() && role == Qt::EditRole && index.column() == 0) {
		return updateTitle(index, value.toString());
	}
	return false;
}

bool CyberiadaSMModel::updateID(const QModelIndex& index, const QString& new_value)
{
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (new_value.size() == 0) {
		// empty id is not allowed
		return false;
	}
	Cyberiada::ID new_id(new_value.toStdString());
	if (root->find_element_by_id(new_id) != NULL) {
		// the id is already available in the document
		return false;
	}
	element->set_id(new_id);
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::updateTitle(const QModelIndex& index, const QString& new_value)
{
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	Cyberiada::Name new_name(new_value.toStdString());
	element->set_name(new_name);
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::updateAction(const QModelIndex& index,
									int action_index, const QString& new_trigger, const QString& new_guard,
									const QString& new_behaviour)
{
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (element->get_type() == Cyberiada::elementSimpleState || element->get_type() == Cyberiada::elementCompositeState) {
		Cyberiada::State* state = static_cast<Cyberiada::State*>(element);
		std::vector<Cyberiada::Action>& actions = state->get_actions();
		if (action_index < 0 || action_index >= actions.size()) {
			return false;
		}
		Cyberiada::Action& a = actions[action_index];
		if (a.get_type() != Cyberiada::actionTransition) {
			a.update(new_behaviour.toStdString());
		} else {
			if (new_trigger.length() == 0) return false;
			a.update(new_trigger.toStdString(), new_guard.toStdString(), new_behaviour.toStdString());
		}
	} else if (element->get_type() == Cyberiada::elementTransition) {
		if (new_trigger.length() == 0) return false;
		Cyberiada::Transition* trans = static_cast<Cyberiada::Transition*>(element);
		trans->get_action().update(new_trigger.toStdString(), new_guard.toStdString(), new_behaviour.toStdString());
	} else {
		return false;
	}
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::newAction(const QModelIndex& index, Cyberiada::ActionType type, const QString& trigger, const QString& guard,
								 const QString& behaviour)
{
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (element->get_type() == Cyberiada::elementSimpleState || element->get_type() == Cyberiada::elementCompositeState) {
		Cyberiada::State* state = static_cast<Cyberiada::State*>(element);
		std::vector<Cyberiada::Action>& actions = state->get_actions();
		if (type == Cyberiada::actionTransition) { 
			if (trigger.length() == 0) return false;
			actions.push_back(Cyberiada::Action(trigger.toStdString(), guard.toStdString(), behaviour.toStdString()));
		} else {
			actions.push_back(Cyberiada::Action(type, behaviour.toStdString()));
		}
	} else if (element->get_type() == Cyberiada::elementTransition) {
		if (trigger.length() == 0) return false;
		Cyberiada::Transition* trans = static_cast<Cyberiada::Transition*>(element);
		if (trans->has_action()) {
			// should edit available action
			return false;
		}
		trans->get_action().update(trigger.toStdString(), guard.toStdString(), behaviour.toStdString());
	} else {
		return false;
	}
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::deleteAction(const QModelIndex& index, int action_index)
{
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (element->get_type() == Cyberiada::elementSimpleState || element->get_type() == Cyberiada::elementCompositeState) {
		Cyberiada::State* state = static_cast<Cyberiada::State*>(element);
		std::vector<Cyberiada::Action>& actions = state->get_actions();
		if (action_index < 0 || action_index >= actions.size()) {
			return false;
		}
		actions.erase(actions.begin() + static_cast<size_t>(action_index));
	} else if (element->get_type() == Cyberiada::elementTransition) {
		Cyberiada::Transition* trans = static_cast<Cyberiada::Transition*>(element);
		if (!trans->has_action()) {
			return false;
		}
		trans->get_action().clear();
	} else {
		return false;
	}
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::updateGeometry(const QModelIndex& index, const Cyberiada::Point& point)
{
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (!element->has_point_geometry()) return false;
	Cyberiada::Vertex* v = static_cast<Cyberiada::Vertex*>(element);
	v->update_geometry(point);
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::updateGeometry(const QModelIndex& index, const Cyberiada::Rect& rect)
{
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (!element->has_rect_geometry()) return false;
	if (element->get_type() == Cyberiada::elementComment || Cyberiada::elementFormalComment) {
		Cyberiada::Comment* comment = static_cast<Cyberiada::Comment*>(element);
		comment->update_geometry(rect);
	} else {
		Cyberiada::ElementCollection* ec = static_cast<Cyberiada::ElementCollection*>(element);
		ec->update_geometry(rect);
	}
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::updateGeometry(const QModelIndex& index, const Cyberiada::Point& source, const Cyberiada::Point& target)
{
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (element->get_type() != Cyberiada::elementTransition) return false;
	Cyberiada::Transition* trans = static_cast<Cyberiada::Transition*>(element);
	// TODO
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::updateGeometry(const QModelIndex& index, const Cyberiada::Polyline& pl)
{
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (element->get_type() != Cyberiada::elementTransition) return false;
	Cyberiada::Transition* trans = static_cast<Cyberiada::Transition*>(element);
	// TODO
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::updateCommentBody(const QModelIndex& index, const QString& body)
{
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::updateMetainformation(const QModelIndex& index, const QString& parameter, const QString& new_value)
{
	if (index != documentIndex()) {
		return false;
	}
	QModelIndex comment_index = elementToIndex(root->get_meta_element());
	emit dataChanged(comment_index, comment_index);
	emit dataChanged(index, index);
	return true;
}

Qt::ItemFlags CyberiadaSMModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags default_flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	if (isSMIndex(index)) {
		return Qt::ItemIsDropEnabled | default_flags;
	} else if (isStateIndex(index) || isInitialIndex(index)) {
		default_flags |= Qt::ItemIsDragEnabled | Qt::ItemIsEditable;
		if (isStateIndex(index)) {
			default_flags |= Qt::ItemIsDropEnabled;
		}
		return default_flags;
	}
	return default_flags;
}

bool CyberiadaSMModel::hasIndex(int row, int column, const QModelIndex & parent) const
{
	if (!parent.isValid()) {
		return row == 0;
	}
	if (parent == rootIndex()) {
		return row == 0;
	}
	if (column != 0) {
		return false;
	}
	const Cyberiada::Element *parent_element = static_cast<const Cyberiada::Element*>(parent.internalPointer());
	MY_ASSERT(parent_element);
	if(parent_element->has_children()) {
		return row >= 0 && row < int(parent_element->children_count());
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
	if (parent == rootIndex()) {
		return createIndex(row, column, (void*)root);
	}
	const Cyberiada::ElementCollection* parent_element = static_cast<const Cyberiada::ElementCollection*>(parent.internalPointer());
	MY_ASSERT(parent_element);
	const Cyberiada::Element* child_element = parent_element->get_element(size_t(row));
	MY_ASSERT(child_element);
	//qDebug() << "index result: child" << row << column << (void*)child_element;
	return createIndex(row, column, (void*)child_element);
}

QModelIndex CyberiadaSMModel::parent(const QModelIndex &index) const
{
	//qDebug() << "parent" << index.row() << index.column() << (void*)index.internalPointer();
	if (!index.isValid() || index == rootIndex()) {
		//qDebug() << "parent result: empty";
		return QModelIndex();
	}
	if (index == documentIndex()) {
		return rootIndex();
	}
	if (isSMIndex(index)) {
		//qDebug() << "parent result: root";
		return documentIndex();
	}
	const Cyberiada::Element* child_element = static_cast<const Cyberiada::Element*>(index.internalPointer());
	MY_ASSERT(child_element);
	const Cyberiada::Element* parent_element = child_element->get_parent();
	MY_ASSERT(parent_element);
	if (parent_element == root) {
		//qDebug() << "parent result: root2";		
		return documentIndex();
	}
	//qDebug() << "parent result" << parent_element->index() << 0 << (void*)parent_element;
	return createIndex(parent_element->index(), 0, (void*)parent_element);
}

int CyberiadaSMModel::rowCount(const QModelIndex &parent) const
{
	//qDebug() << "row count" << (void*)parent.internalPointer();
	const Cyberiada::Element* element;
	if (!parent.isValid()) {
		return 1;
	} else if (parent == rootIndex()) {
		if (root) {
			return 1;
		} else {
			return 0;
		}
	} else {
		element = static_cast<const Cyberiada::Element*>(parent.internalPointer());
	}
	MY_ASSERT(element);
	return element->children_count();
}

int CyberiadaSMModel::columnCount(const QModelIndex &) const
{
	return 1;
}

bool CyberiadaSMModel::hasChildren(const QModelIndex & parent) const
{
	return rowCount(parent) > 0;
}

QModelIndex CyberiadaSMModel::rootIndex() const
{
	return createIndex(0, 0, (void*)this);
}

QModelIndex CyberiadaSMModel::documentIndex() const
{
	if (root) {
		return createIndex(0, 0, (void*)root);
	} else {
		return QModelIndex();
	}
}

QModelIndex CyberiadaSMModel::firstSMIndex() const
{
	if (root) {
		std::vector<Cyberiada::StateMachine*> sms = root->get_state_machines();
		if (sms.size() == 0) {
			return QModelIndex();
		} else {
			return elementToIndex(sms.front());
		}
	} else {
		return QModelIndex();
	}
}

QModelIndex CyberiadaSMModel::elementToIndex(const Cyberiada::Element* element) const
{
	if (element == NULL) return QModelIndex();
	if (element->is_root()) {
		return documentIndex();
	} else {
		const Cyberiada::Element* parent = element->get_parent();
		MY_ASSERT(parent);
		return index(element->index(), 0, elementToIndex(parent));
	}
}

const Cyberiada::Element* CyberiadaSMModel::indexToElement(const QModelIndex& index) const
{
	if (!index.isValid()) return NULL;
	if (index == documentIndex()) return root;
	return static_cast<const Cyberiada::Element*>(index.internalPointer());
}

Cyberiada::Element* CyberiadaSMModel::indexToElement(const QModelIndex& index)
{
	if (!index.isValid()) return NULL;
	if (index == documentIndex()) return root;
	return static_cast<Cyberiada::Element*>(index.internalPointer());
}

const Cyberiada::Element* CyberiadaSMModel::idToElement(const QString& id) const
{
	MY_ASSERT(root);
	return root->find_element_by_id(id.toStdString());
}

Cyberiada::Element* CyberiadaSMModel::idToElement(const QString& id)
{
	MY_ASSERT(root);
	return root->find_element_by_id(id.toStdString());
}

const Cyberiada::LocalDocument* CyberiadaSMModel::rootDocument() const
{
	if (root) {
		return root;
	} else {
		return NULL;
	}
}

Cyberiada::LocalDocument* CyberiadaSMModel::rootDocument()
{
	if (root) {
		return root;
	} else {
		return NULL;
	}
}

bool CyberiadaSMModel::isStateIndex(const QModelIndex& index) const
{
	return isSimpleStateIndex(index) || isCompositeStateIndex(index);
}

bool CyberiadaSMModel::isSMIndex(const QModelIndex& index) const
{
	if (!index.isValid()) return false;
	const Cyberiada::Element* element = static_cast<const Cyberiada::Element*>(index.internalPointer());
	MY_ASSERT(element);
	return element->get_type() == Cyberiada::elementSM;
}

bool CyberiadaSMModel::isSimpleStateIndex(const QModelIndex& index) const
{
	if (!index.isValid()) return false;
	const Cyberiada::Element* element = static_cast<const Cyberiada::Element*>(index.internalPointer());
	MY_ASSERT(element);
	return element->get_type() == Cyberiada::elementSimpleState;
}

bool CyberiadaSMModel::isCompositeStateIndex(const QModelIndex& index) const
{
	if (!index.isValid()) return false;
	const Cyberiada::Element* element = static_cast<const Cyberiada::Element*>(index.internalPointer());
	MY_ASSERT(element);
	return element->get_type() == Cyberiada::elementCompositeState;
}

bool CyberiadaSMModel::isInitialIndex(const QModelIndex& index) const
{
	if (!index.isValid()) return false;
	const Cyberiada::Element* element = static_cast<const Cyberiada::Element*>(index.internalPointer());
	MY_ASSERT(element);
	return element->get_type() == Cyberiada::elementInitial;
}

bool CyberiadaSMModel::isTransitionIndex(const QModelIndex& index) const
{
	if (!index.isValid()) return false;
	const Cyberiada::Element* element = static_cast<const Cyberiada::Element*>(index.internalPointer());
	MY_ASSERT(element);
	return element->get_type() == Cyberiada::elementTransition;
}

Qt::DropActions CyberiadaSMModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

void CyberiadaSMModel::move(Cyberiada::Element*, Cyberiada::ElementCollection*)
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

bool CyberiadaSMModel::dropMimeData(const QMimeData *,
									Qt::DropAction, int, int,
									const QModelIndex &)
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
	return false;
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
		if (!isStateIndex(index) && !isInitialIndex(index)) continue;
		const Cyberiada::Element* element = indexToElement(index);
		MY_ASSERT(element);
		stream << element->get_id().c_str();
	}
	mimeData->setData(cyberiadaStateMimeType, encodedData);	
	return mimeData;
}
