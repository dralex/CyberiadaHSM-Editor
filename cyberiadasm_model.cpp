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
#include <QRegularExpression>
#include <QDebug>

#include "cyberiadasm_model.h"
#include "cyberiadasm_undo.h"
#include "settings_manager.h"
#include "myassert.h"
#include "cyberiada_constants.h"

CyberiadaSMModel::CyberiadaSMModel(QObject *parent):
	QAbstractItemModel(parent)
{
	root = NULL;
	undo = new QUndoStack(this);
	undo->setUndoLimit(100);
	undoDepth = 0;
	fileFormat = Cyberiada::formatCyberiada10;
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

bool CyberiadaSMModel::loadDocument(const QString& path, bool reconstruct, bool reconstruct_sm, bool strict)
{
	Cyberiada::LocalDocument* new_doc = NULL;

	lastLoadError.clear();
	try {
		new_doc = new Cyberiada::LocalDocument();
		new_doc->open(path.toStdString(), Cyberiada::formatDetect, Cyberiada::geometryFormatQt,
					  reconstruct, reconstruct_sm, false, false, false, strict);
	} catch (const Cyberiada::XMLException& e) {
		lastLoadError = tr("XML grapml error:\n") + QString(e.str().c_str());
	} catch (const Cyberiada::CybMLException& e) {
		lastLoadError = tr("Wrong format of the Cyberiada grapml file:\n") + QString(e.str().c_str());
	} catch (const Cyberiada::Exception& e) {
		lastLoadError = tr("Cannot load state machine graph:\n") + QString(e.str().c_str());
	}

	if (!lastLoadError.isEmpty()) {
		if (new_doc) {
			delete new_doc;
		}
		return false;
	}

	beginResetModel();
	if (root) {
		delete root;
	}
	root = new_doc;
	filePath = QString(root->get_file_path().c_str());
	fileFormat = root->get_file_format();
	endResetModel();
	undo->clear();
	return true;
}

bool CyberiadaSMModel::readOnly() const
{
	return SettingsManager::instance().getInspectorMode();
}

// the snapshot is the Cyberiada 1.0 encoding of the document in memory; an
// absent or empty document encodes as an empty string
std::string CyberiadaSMModel::snapshot() const
{
	if (!root) return std::string();
	std::string buffer;
	try {
		root->encode(buffer, Cyberiada::formatCyberiada10);
	} catch (const Cyberiada::Exception& e) {
		qWarning() << "cannot snapshot the document:" << e.str().c_str();
		return std::string();
	}
	return buffer;
}

void CyberiadaSMModel::beginUndoStep(const QString& text)
{
	if (undoDepth++ == 0) {
		undoText = text;
		undoBefore = snapshot();
	} else if (undoText.isEmpty()) {
		// the gesture is named by its first mutation
		undoText = text;
	}
}

void CyberiadaSMModel::endUndoStep()
{
	if (undoDepth == 0) return;
	if (--undoDepth > 0) return;
	std::string after = snapshot();
	if (after == undoBefore) return;
	undo->push(new DocumentStep(this, undoText.isEmpty() ? tr("edit") : undoText, undoBefore, after));
}

void CyberiadaSMModel::restoreSnapshot(const std::string& snapshot)
{
	beginResetModel();
	if (!root) {
		root = new Cyberiada::LocalDocument();
	}
	if (snapshot.empty()) {
		root->reset();
	} else {
		// the format argument is the input hint as well as the detected result
		Cyberiada::DocumentFormat f = Cyberiada::formatDetect;
		Cyberiada::String format_str;
		try {
			root->decode(snapshot, f, format_str, Cyberiada::geometryFormatQt);
		} catch (const Cyberiada::Exception& e) {
			// the snapshot came from this very document: a failure is a defect
			qWarning() << "cannot restore the document:" << e.str().c_str();
		}
	}
	// the decode resets the file identity with the rest of the document
	root->set_file(filePath.toStdString(), fileFormat);
	endResetModel();
}

// the mutations of one call form one step unless a gesture is open
class UndoScope {
public:
	UndoScope(CyberiadaSMModel* model, const QString& text): model(model) { model->beginUndoStep(text); }
	~UndoScope() { model->endUndoStep(); }
private:
	CyberiadaSMModel* model;
};

// the written document declares its geometry (7.1): the editor writes the
// exact sizes or none at all; the yEd formats carry no metainformation
void CyberiadaSMModel::declareGeometry(Cyberiada::DocumentFormat f, bool skip_geometry)
{
	if (!root || f != Cyberiada::formatCyberiada10) return;
	Cyberiada::DocumentGeometryDeclaration g = skip_geometry ?
		Cyberiada::geometryDeclarationNone : Cyberiada::geometryDeclarationFull;
	if (root->meta().get_geometry() == g) return;
	root->meta().set_geometry(g);
	root->update_metainfo_element();
	QModelIndex comment_index = elementToIndex(root->get_meta_element());
	emit dataChanged(comment_index, comment_index);
	emit dataChanged(documentIndex(), documentIndex());
}

void CyberiadaSMModel::saveDocument(bool round)
{
	if (readOnly()) return;
	if (root && !root->get_file_path().empty()) {
		declareGeometry(root->get_file_format(), false);
		root->save(round);
		undo->setClean();
	}
}

void CyberiadaSMModel::saveAsDocument(const QString& path, Cyberiada::DocumentFormat f,
									  bool round, bool skip_geometry, bool check_initial,
									  bool strict_actions, bool skip_empty_behavior)
{
	if (root) {
		declareGeometry(f, skip_geometry);
		root->save_as(path.toStdString(), f, round, skip_geometry, check_initial,
					  strict_actions, skip_empty_behavior);
		filePath = QString(root->get_file_path().c_str());
		fileFormat = root->get_file_format();
		undo->setClean();
	}
}

void CyberiadaSMModel::saveAsDocument(const QString& path, Cyberiada::DocumentFormat f, bool round)
{
	if (root) {
		declareGeometry(f, false);
		root->save_as(path.toStdString(), f, round);
		filePath = QString(root->get_file_path().c_str());
		fileFormat = root->get_file_format();
		undo->setClean();
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
	if (readOnly()) return false;
	if(index.isValid() && role == Qt::EditRole && index.column() == 0) {
		return updateTitle(index, value.toString());
	}
	return false;
}

bool CyberiadaSMModel::updateID(const QModelIndex& index, const QString& new_value)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("id"));
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
	Cyberiada::ID old_id = element->get_id();
	element->set_id(new_id);
	// transitions reference elements by id; keep them consistent
	// (comment subject ids are not updated yet)
	Cyberiada::StateMachineList sms = root->get_state_machines();
	for (Cyberiada::StateMachineList::iterator i = sms.begin(); i != sms.end(); i++) {
		std::vector<Cyberiada::Transition*> transitions = (*i)->get_transitions();
		for (std::vector<Cyberiada::Transition*>::iterator j = transitions.begin();
			 j != transitions.end(); j++) {
			Cyberiada::Transition* t = *j;
			if (t->source_element_id() == old_id || t->target_element_id() == old_id) {
				t->update(t->source_element_id() == old_id ? new_id : t->source_element_id(),
						  t->target_element_id() == old_id ? new_id : t->target_element_id());
				QModelIndex ti = elementToIndex(t);
				emit dataChanged(ti, ti);
			}
		}
	}
	emit dataChanged(index, index);
	return true;
}

static bool isState(const Cyberiada::Element* element)
{
	return element->get_type() == Cyberiada::elementSimpleState ||
		element->get_type() == Cyberiada::elementCompositeState;
}

// another state of the same level already carries the name
static bool siblingStateNamed(const Cyberiada::Element* element, const Cyberiada::Name& name)
{
	const Cyberiada::ElementCollection* parent =
		dynamic_cast<const Cyberiada::ElementCollection*>(element->get_parent());
	if (!parent || !parent->has_children()) return false;
	Cyberiada::ConstElementList children = parent->get_children();
	for (Cyberiada::ConstElementList::const_iterator i = children.begin(); i != children.end(); i++) {
		if (*i != element && isState(*i) && (*i)->get_name() == name) return true;
	}
	return false;
}

bool CyberiadaSMModel::updateTitle(const QModelIndex& index, const QString& new_value)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("rename"));
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	Cyberiada::Name new_name(new_value.toStdString());
	// the states of one level are told apart by name; the vertices have none
	if (isState(element) && (new_value.trimmed().isEmpty() || siblingStateNamed(element, new_name))) {
		return false;
	}
	element->set_name(new_name);
	emit dataChanged(index, index);
	return true;
}

// blank lines separate the action blocks in the document text format, so a
// stored behaviour must not contain them - the saved file would not load back
static QString normalizedBehaviour(const QString& behaviour)
{
	QString s = behaviour;
	s.replace(QRegularExpression("[ \t\r]*\n([ \t\r]*\n)+"), "\n");
	return s.trimmed();
}

bool CyberiadaSMModel::updateAction(const QModelIndex& index,
									int action_index, const QString& new_trigger, const QString& new_guard,
									const QString& new_behaviour)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("action"));
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (element->get_type() == Cyberiada::elementSimpleState || element->get_type() == Cyberiada::elementCompositeState) {
		Cyberiada::State* state = static_cast<Cyberiada::State*>(element);
		std::vector<Cyberiada::Action>& actions = state->get_actions();
		if (action_index < 0 || action_index >= actions.size()) {
			return false;
		}
		Cyberiada::Action& a = actions[action_index];
		std::string behaviour = normalizedBehaviour(new_behaviour).toStdString();
        if (a.get_type() != Cyberiada::actionTransition) {
			a.update(behaviour);
        } else {
			if (new_trigger.length() == 0) return false;
			a.update(new_trigger.toStdString(), new_guard.toStdString(), behaviour);
		}
    } else if (element->get_type() == Cyberiada::elementTransition) {
		if (new_trigger.length() == 0) return false;
		Cyberiada::Transition* trans = static_cast<Cyberiada::Transition*>(element);
		trans->get_action().update(new_trigger.toStdString(), new_guard.toStdString(),
								   normalizedBehaviour(new_behaviour).toStdString());
	} else {
		return false;
	}
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::newAction(const QModelIndex& index, Cyberiada::ActionType type, const QString& trigger, const QString& guard,
								 const QString& behaviour)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("action"));
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (element->get_type() == Cyberiada::elementSimpleState || element->get_type() == Cyberiada::elementCompositeState) {
		Cyberiada::State* state = static_cast<Cyberiada::State*>(element);
		std::vector<Cyberiada::Action>& actions = state->get_actions();
		std::string new_behaviour = normalizedBehaviour(behaviour).toStdString();
		if (type == Cyberiada::actionTransition) { 
			if (trigger.length() == 0) return false;
			actions.push_back(Cyberiada::Action(trigger.toStdString(), guard.toStdString(), new_behaviour));
		} else {
			actions.push_back(Cyberiada::Action(type, new_behaviour));
		}
	} else if (element->get_type() == Cyberiada::elementTransition) {
		if (trigger.length() == 0) return false;
		Cyberiada::Transition* trans = static_cast<Cyberiada::Transition*>(element);
		if (trans->has_action()) {
			// should edit available action
			return false;
		}
		trans->get_action().update(trigger.toStdString(), guard.toStdString(),
								   normalizedBehaviour(behaviour).toStdString());
	} else {
		return false;
	}
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::deleteAction(const QModelIndex& index, int action_index)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("action"));
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
	if (readOnly()) return false;
	UndoScope scope(this, tr("geometry"));
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
	if (readOnly()) return false;
	UndoScope scope(this, tr("geometry"));
	Cyberiada::Element* element = indexToElement(index);
    if (!element) return false;
    if (!element->has_rect_geometry()) return false;
    if (element->get_type() == Cyberiada::elementChoice) {
		Cyberiada::ChoicePseudostate* choice = static_cast<Cyberiada::ChoicePseudostate*>(element);
		choice->update_geometry(rect);
	} else if (element->get_type() == Cyberiada::elementComment || element->get_type() == Cyberiada::elementFormalComment) {
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
	if (readOnly()) return false;
	UndoScope scope(this, tr("geometry"));
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (element->get_type() != Cyberiada::elementTransition) return false;
	Cyberiada::Transition* trans = static_cast<Cyberiada::Transition*>(element);
    // TODO
    trans->update(source, target);
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::updateGeometry(const QModelIndex& index, const Cyberiada::Polyline& pl)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("geometry"));
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (element->get_type() != Cyberiada::elementTransition) return false;
	Cyberiada::Transition* trans = static_cast<Cyberiada::Transition*>(element);
	// TODO
    trans->update(pl);
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::updateGeometry(const QModelIndex &index, const Cyberiada::ID &source, const Cyberiada::ID &target)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("geometry"));
    Cyberiada::Element* element = indexToElement(index);
    if (!element) return false;
    if (element->get_type() != Cyberiada::elementTransition) return false;
    Cyberiada::Transition* trans = static_cast<Cyberiada::Transition*>(element);
    // TODO
    if (root->find_element_by_id(source) == NULL || root->find_element_by_id(target) == NULL) {
        // the id isn't available in the document
        return false;
    }
    trans->update(source, target);
    emit dataChanged(index, index);
    return true;
}

bool CyberiadaSMModel::updateParent(const QModelIndex &index, const Cyberiada::ID &new_parent_id)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("reparent"));
    Cyberiada::Element* element = indexToElement(index);
    if (!element) return false;
    Cyberiada::ElementCollection* new_parent = dynamic_cast<Cyberiada::ElementCollection*>(idToElement(new_parent_id.c_str()));
    if (!new_parent) return false;
    if (parent(index) == elementToIndex(new_parent)) return true;
    if (element->get_type() == Cyberiada::elementInitial) {
        // TODO check
    }
    move(element, new_parent);
    return true;
}

bool CyberiadaSMModel::updateCommentBody(const QModelIndex& index, const QString& body)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("comment"));
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (element->get_type() != Cyberiada::elementComment &&
		element->get_type() != Cyberiada::elementFormalComment) return false;
	static_cast<Cyberiada::Comment*>(element)->set_body(body.toStdString());
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::updateMetainformation(const QModelIndex& index, const QString& parameter, const QString& new_value)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("metainformation"));
	if (!root || index != documentIndex()) {
		return false;
	}
	Cyberiada::DocumentMetainformation& meta = root->meta();
	Cyberiada::String name = parameter.toStdString();
	Cyberiada::String value = new_value.toStdString();
	if (name == CYBERIADA_META_STANDARD_VERSION) {
		meta.standard_version = value;
	} else if (name == CYBERIADA_META_TRANSITION_ORDER) {
		if (value == CYBERIADA_META_AO_EXIT) meta.transition_order = Cyberiada::transitionOrderExit;
		else if (value == CYBERIADA_META_AO_ACTION ||
				 value == CYBERIADA_META_AO_TRANSITION) meta.transition_order = Cyberiada::transitionOrderAction;
		else if (value == METAINFORMATION_VALUE_NONE) meta.transition_order = Cyberiada::transitionOrderNone;
		else return false;
	} else if (name == CYBERIADA_META_EVENT_PROPAGATION) {
		if (value == CYBERIADA_META_EP_PROPAGATE) meta.event_propagation = Cyberiada::docEventPropagationPropagate;
		else if (value == CYBERIADA_META_EP_BLOCK) meta.event_propagation = Cyberiada::docEventPropagationBlock;
		else if (value == METAINFORMATION_VALUE_NONE) meta.event_propagation = Cyberiada::docEventPropagationNone;
		else return false;
	} else if (name == CYBERIADA_META_GEOMETRY) {
		// "none" is a declaration here, the empty value removes the parameter
		if (value == CYBERIADA_META_GEOM_NONE) meta.set_geometry(Cyberiada::geometryDeclarationNone);
		else if (value == CYBERIADA_META_GEOM_SHORT) meta.set_geometry(Cyberiada::geometryDeclarationShort);
		else if (value == CYBERIADA_META_GEOM_FULL) meta.set_geometry(Cyberiada::geometryDeclarationFull);
		else if (value.empty()) meta.set_geometry(Cyberiada::geometryDeclarationAbsent);
		else return false;
	} else if (name == CYBERIADA_META_NAME) {
		// the document name and its metainformation entry are one thing
		root->set_name(value);
	} else {
		meta.set_string(name, value);
	}
	// re-serialize the meta comment; save() does not do it
	root->update_metainfo_element();
	QModelIndex comment_index = elementToIndex(root->get_meta_element());
	emit dataChanged(comment_index, comment_index);
	emit dataChanged(index, index);
	return true;
}

// the library keeps the transitions after the other children, so a new
// element lands before the first transition of the collection
static int newElementRow(Cyberiada::ElementCollection* parent)
{
    if (!parent) return 0;
    const Cyberiada::ElementList& children = parent->get_children();
    int row = 0;
    for (Cyberiada::ElementList::const_iterator i = children.begin(); i != children.end(); i++, row++) {
        if ((*i)->get_type() == Cyberiada::elementTransition) break;
    }
    return row;
}

// the states of one level are told apart by name, so a fresh one gets the
// base name, else the base with the smallest free numeric suffix
Cyberiada::Name CyberiadaSMModel::uniqueStateName(const Cyberiada::ElementCollection *parent,
                                                  const Cyberiada::Name &base) const
{
	if (!parent || !parent->has_children()) return base;
	Cyberiada::ConstElementList children = parent->get_children();
	for (int n = 0; ; n++) {
		Cyberiada::Name candidate = n == 0 ? base : base + " " + std::to_string(n);
		bool taken = false;
		for (Cyberiada::ConstElementList::const_iterator i = children.begin(); i != children.end(); i++) {
			if (isState(*i) && (*i)->get_name() == candidate) { taken = true; break; }
		}
		if (!taken) return candidate;
	}
}

Cyberiada::StateMachine *CyberiadaSMModel::newStateMachine(const Cyberiada::String &sm_name, const Cyberiada::Rect &r)
{
	if (readOnly()) return NULL;
	UndoScope scope(this, tr("new state machine"));
    if (root == NULL) {
        root = new Cyberiada::LocalDocument();
    }

    int row = rowCount(rootIndex());
    beginInsertRows(rootIndex(), row, row);
    Cyberiada::StateMachine* element = root->new_state_machine(sm_name, r);
    endInsertRows();

    return element;
}

Cyberiada::State *CyberiadaSMModel::newState(Cyberiada::ElementCollection *parent, const Cyberiada::String &state_name,
                                             const Cyberiada::Action &a, const Cyberiada::Rect &r, const Cyberiada::Rect &region,
                                             const Cyberiada::Color &color)
{
	if (readOnly()) return NULL;
	UndoScope scope(this, tr("new state"));
    if (root == NULL) {
        return nullptr;
    }

    int row = newElementRow(parent);
    beginInsertRows(elementToIndex(parent), row, row);
    Cyberiada::State* element = root->new_state(parent, state_name, a, r, region, color);
    endInsertRows();

    return element;
}

Cyberiada::InitialPseudostate *CyberiadaSMModel::newInitial(Cyberiada::ElementCollection *parent, const Cyberiada::Point &p)
{
	if (readOnly()) return NULL;
	UndoScope scope(this, tr("new element"));
    if (root == NULL) {
        return nullptr;
    }

    int row = newElementRow(parent);
    beginInsertRows(elementToIndex(parent), row, row);
    Cyberiada::InitialPseudostate* element = root->new_initial(parent, p);
    endInsertRows();

    return element;
}

Cyberiada::FinalState *CyberiadaSMModel::newFinal(Cyberiada::ElementCollection *parent, const Cyberiada::Point &p)
{
	if (readOnly()) return NULL;
	UndoScope scope(this, tr("new element"));
    if (root == NULL) {
        return nullptr;
    }

    int row = newElementRow(parent);
    beginInsertRows(elementToIndex(parent), row, row);
    Cyberiada::FinalState* element = root->new_final(parent, p);
    endInsertRows();

    return element;
}

Cyberiada::ChoicePseudostate *CyberiadaSMModel::newChoice(Cyberiada::ElementCollection *parent, const Cyberiada::Rect &r,
                                                          const Cyberiada::Color &color)
{
	if (readOnly()) return NULL;
	UndoScope scope(this, tr("new element"));
    if (root == NULL) {
        return nullptr;
    }

    int row = newElementRow(parent);
    beginInsertRows(elementToIndex(parent), row, row);
    Cyberiada::ChoicePseudostate* element = root->new_choice(parent, r, color);
    endInsertRows();

    return element;
}

Cyberiada::TerminatePseudostate *CyberiadaSMModel::newTerminate(Cyberiada::ElementCollection *parent, const Cyberiada::Point &p)
{
	if (readOnly()) return NULL;
	UndoScope scope(this, tr("new element"));
    if (root == NULL) {
        return nullptr;
    }

    int row = newElementRow(parent);
    beginInsertRows(elementToIndex(parent), row, row);
    Cyberiada::TerminatePseudostate* element = root->new_terminate(parent, p);
    endInsertRows();

    return element;
}

Cyberiada::Transition *CyberiadaSMModel::newTransition(Cyberiada::StateMachine *sm, Cyberiada::TransitionType ttype,
                                                       Cyberiada::Element *source, Cyberiada::Element *target,
                                                       const Cyberiada::Action &action, const Cyberiada::Polyline &pl,
                                                       const Cyberiada::Point &sp, const Cyberiada::Point &tp,
                                                       const Cyberiada::Point &label_point, const Cyberiada::Rect &label_rect,
                                                       const Cyberiada::Color &color)
{
	if (readOnly()) return NULL;
	UndoScope scope(this, tr("new transition"));
    if (root == NULL) {
        return nullptr;
    }

    int row = rowCount(elementToIndex(sm));
    beginInsertRows(elementToIndex(sm), row, row);
    Cyberiada::Transition* element = root->new_transition(sm, ttype, source, target, action, pl, sp, tp, label_point, label_rect, color);
    endInsertRows();

    return element;
}

Cyberiada::Comment *CyberiadaSMModel::newComment(Cyberiada::ElementCollection *parent, const Cyberiada::String &body,
                                                 const Cyberiada::Rect &rect, const Cyberiada::Color &color, const Cyberiada::String &markup)
{
	if (readOnly()) return NULL;
	UndoScope scope(this, tr("new comment"));
    if (root == NULL) {
        return nullptr;
    }

    int row = newElementRow(parent);
    beginInsertRows(elementToIndex(parent), row, row);
    Cyberiada::Comment* element = root->new_comment(parent, body, rect, color, markup);
    endInsertRows();

    return element;
}

Cyberiada::Comment *CyberiadaSMModel::newFormalComment(Cyberiada::ElementCollection *parent, const Cyberiada::String &body,
                                                       const Cyberiada::Rect &rect, const Cyberiada::Color &color,
                                                       const Cyberiada::String &markup)
{
	if (readOnly()) return NULL;
	UndoScope scope(this, tr("new comment"));
    if (root == NULL) {
        return nullptr;
    }

    int row = newElementRow(parent);
    beginInsertRows(elementToIndex(parent), row, row);
    Cyberiada::Comment* element = root->new_formal_comment(parent, body, rect, color, markup);
    endInsertRows();

    return element;
}

bool CyberiadaSMModel::newCommentSubject(const QModelIndex& index, Cyberiada::Element* target,
                                         Cyberiada::CommentSubjectType type, const QString& fragment)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("subject"));
    Cyberiada::Element* element = indexToElement(index);
    if (!element) return false;
    if (element->get_type() != Cyberiada::elementComment &&
        element->get_type() != Cyberiada::elementFormalComment) return false;
    Cyberiada::Comment* comment = static_cast<Cyberiada::Comment*>(element);
    if (type == Cyberiada::commentSubjectElement) {
        root->add_comment_to_element(comment, target);
    } else if (type == Cyberiada::commentSubjectName) {
        root->add_comment_to_element_name(comment, target, fragment.toStdString());
    } else {
        root->add_comment_to_element_body(comment, target, fragment.toStdString());
    }
    emit dataChanged(index, index);
    return true;
}

bool CyberiadaSMModel::deleteCommentSubject(const QModelIndex& index, int subject_index)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("subject"));
    Cyberiada::Element* element = indexToElement(index);
    if (!element) return false;
    if (element->get_type() != Cyberiada::elementComment &&
        element->get_type() != Cyberiada::elementFormalComment) return false;
    Cyberiada::Comment* comment = static_cast<Cyberiada::Comment*>(element);
    if (subject_index < 0 || (size_t)subject_index >= comment->get_subjects().size()) return false;
    comment->remove_subject((size_t)subject_index);
    emit dataChanged(index, index);
    return true;
}

// the subjects hold raw element pointers - collect the subtree to be
// deleted so the referencing subjects can be stripped first
static void collectElements(Cyberiada::Element* element, Cyberiada::ElementList* elements)
{
    elements->push_back(element);
    if (element->has_children()) {
        Cyberiada::ElementCollection* collection = static_cast<Cyberiada::ElementCollection*>(element);
        const Cyberiada::ElementList& children = collection->get_children();
        for (Cyberiada::ElementList::const_iterator i = children.begin(); i != children.end(); i++) {
            collectElements(*i, elements);
        }
    }
}

bool CyberiadaSMModel::deleteElement(const QModelIndex &index)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("delete"));
    Cyberiada::Element* child_element = indexToElement(index);
    if (!child_element) return false;
    Cyberiada::ElementCollection* parent_element = dynamic_cast<Cyberiada::ElementCollection*>(child_element->get_parent());
    MY_ASSERT(parent_element);

    // strip the comment subjects referencing the deleted elements
    Cyberiada::ElementList doomed;
    collectElements(child_element, &doomed);
    Cyberiada::StateMachineList sms = root->get_state_machines();
    for (Cyberiada::StateMachineList::iterator s = sms.begin(); s != sms.end(); s++) {
        Cyberiada::ElementList comments = (*s)->find_elements_by_type(Cyberiada::elementComment);
        Cyberiada::ElementList formal = (*s)->find_elements_by_type(Cyberiada::elementFormalComment);
        comments.insert(comments.end(), formal.begin(), formal.end());
        for (Cyberiada::ElementList::iterator c = comments.begin(); c != comments.end(); c++) {
            Cyberiada::Comment* comment = static_cast<Cyberiada::Comment*>(*c);
            bool changed = false;
            for (size_t i = comment->get_subjects().size(); i > 0; i--) {
                const Cyberiada::Element* target = comment->get_subjects().at(i - 1).get_element();
                for (Cyberiada::ElementList::const_iterator d = doomed.begin(); d != doomed.end(); d++) {
                    if (*d == target) {
                        comment->remove_subject(i - 1);
                        changed = true;
                        break;
                    }
                }
            }
            if (changed) {
                QModelIndex ci = elementToIndex(comment);
                emit dataChanged(ci, ci);
            }
        }
    }

    // delete the transitions attached to the doomed subtree - the saved
    // document must not keep edges with missing endpoints
    for (Cyberiada::StateMachineList::iterator s = sms.begin(); s != sms.end(); s++) {
        Cyberiada::ElementList transitions = (*s)->find_elements_by_type(Cyberiada::elementTransition);
        for (Cyberiada::ElementList::iterator t = transitions.begin(); t != transitions.end(); t++) {
            Cyberiada::Transition* tr = static_cast<Cyberiada::Transition*>(*t);
            bool attached = false, inside = false;
            for (Cyberiada::ElementList::const_iterator d = doomed.begin(); d != doomed.end(); d++) {
                if (*d == *t) { inside = true; break; }
                if ((*d)->get_id() == tr->source_element_id() ||
                    (*d)->get_id() == tr->target_element_id()) {
                    attached = true;
                }
            }
            if (!inside && attached) {
                deleteElement(elementToIndex(tr));
            }
        }
    }

    int row = child_element->index();
    beginRemoveRows(elementToIndex(parent_element), row, row);
    parent_element->remove_element(child_element->get_id());
    endRemoveRows();
    return true;
}

Qt::ItemFlags CyberiadaSMModel::flags(const QModelIndex &index) const
{
	if (!index.isValid()) {
		return Qt::NoItemFlags;
	}
	Qt::ItemFlags default_flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	if (readOnly()) {
		return default_flags;
	}
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

// the offset of a parent chain in the local geometry formats: the sum of
// the ancestor positions up to the state machine
static void ancestorsOffset(const Cyberiada::Element* parent, double& x, double& y)
{
    x = y = 0.0;
    for (const Cyberiada::Element* e = parent;
         e && e->get_type() != Cyberiada::elementSM && e->get_type() != Cyberiada::elementRoot;
         e = e->get_parent()) {
        if (e->has_rect_geometry()) {
            const Cyberiada::Rect& r = static_cast<const Cyberiada::ElementCollection*>(e)->get_geometry_rect();
            x += r.x;
            y += r.y;
        }
    }
}

static void shiftGeometry(Cyberiada::Element* element, double dx, double dy)
{
    Cyberiada::ElementType type = element->get_type();
    if (!element->has_geometry()) return;
    if (element->has_point_geometry()) {
        Cyberiada::Vertex* v = static_cast<Cyberiada::Vertex*>(element);
        Cyberiada::Point p = v->get_geometry_point();
        v->update_geometry(Cyberiada::Point(p.x + dx, p.y + dy));
    } else if (element->has_rect_geometry()) {
        if (type == Cyberiada::elementChoice) {
            Cyberiada::ChoicePseudostate* c = static_cast<Cyberiada::ChoicePseudostate*>(element);
            Cyberiada::Rect r = c->get_geometry_rect();
            c->update_geometry(Cyberiada::Rect(r.x + dx, r.y + dy, r.width, r.height));
        } else if (type == Cyberiada::elementComment || type == Cyberiada::elementFormalComment) {
            Cyberiada::Comment* c = static_cast<Cyberiada::Comment*>(element);
            Cyberiada::Rect r = c->get_geometry_rect();
            c->update_geometry(Cyberiada::Rect(r.x + dx, r.y + dy, r.width, r.height));
        } else {
            Cyberiada::ElementCollection* ec = static_cast<Cyberiada::ElementCollection*>(element);
            Cyberiada::Rect r = ec->get_geometry_rect();
            ec->update_geometry(Cyberiada::Rect(r.x + dx, r.y + dy, r.width, r.height));
        }
    }
}

void CyberiadaSMModel::move(Cyberiada::Element* element, Cyberiada::ElementCollection* target_parent)
{
    QModelIndex srcindex = elementToIndex(element);
	QModelIndex parentindex = parent(srcindex);	
	QModelIndex dstindex;
	
    if (target_parent != NULL) {
        dstindex = elementToIndex(target_parent);
	} else {
        // TODO
        // dstindex = statesRootIndex();
        dstindex = rootIndex();
	}

	MY_ASSERT(srcindex.isValid());

	int remove_index = srcindex.row();
    Cyberiada::ElementCollection* source_parent = dynamic_cast<Cyberiada::ElementCollection*>(element->get_parent());

    if (target_parent == NULL || source_parent == NULL) {
		return;
	}

    // remove_element() frees the original, so deep-copy the element into the
    // new parent first and remove the original afterwards; every comment
    // subject of the document that pointed into the subtree follows the copy
    Cyberiada::Element* copied = element->copy(target_parent);
    if (Cyberiada::ElementCollection* subtree = dynamic_cast<Cyberiada::ElementCollection*>(copied)) {
        root->rebind_subjects(*subtree);
    }

    // the geometry is parent-relative: keep the absolute position
    Cyberiada::DocumentGeometryFormat gf = root->get_geometry_format();
    if (gf == Cyberiada::geometryFormatQt || gf == Cyberiada::geometryFormatCyberiada10) {
        double ox, oy, nx, ny;
        ancestorsOffset(source_parent, ox, oy);
        ancestorsOffset(target_parent, nx, ny);
        shiftGeometry(copied, ox - nx, oy - ny);
    }

	beginRemoveRows(parentindex, remove_index, remove_index);
    source_parent->remove_element(element->get_id());
	endRemoveRows();

	int add_index = newElementRow(target_parent);
	beginInsertRows(dstindex, add_index, add_index);
    target_parent->add_element(copied);
    endInsertRows();

    QModelIndex newIndex = elementToIndex(copied);
    emit dataChanged(newIndex, newIndex);
    // use in case scene::updateItemsRecursively is not used in scene::slotModelDataChanged
    QModelIndex newTargetIndex = elementToIndex(target_parent);
    QModelIndex newSourceIndex = elementToIndex(source_parent);
    emit dataChanged(newTargetIndex, newTargetIndex);
    emit dataChanged(newSourceIndex, newSourceIndex);
}

bool CyberiadaSMModel::dropMimeData(const QMimeData *data,
                                    Qt::DropAction action,
                                    int row, int column,
                                    const QModelIndex &parent)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("reparent"));
    if(action == Qt::IgnoreAction) {
		return true;
	}
	if(column > 0) return false;
    Cyberiada::Element* target_element = static_cast<Cyberiada::Element*>(parent.internalPointer());
    Cyberiada::ElementCollection* target_element_col = dynamic_cast<Cyberiada::ElementCollection*>(target_element);
    if (target_element_col == NULL) {
        return false;
    }
    // TODO check
    if (parent != rootIndex()) {
        MY_ASSERT(target_element);
        // MY_ASSERT(isStateIndex(parent));
	}
	if(data->hasFormat(cyberiadaStateMimeType)) {
        QByteArray encodedData = data->data(cyberiadaStateMimeType);
		QDataStream stream(&encodedData, QIODevice::ReadOnly);
        QStringList elements;
		while (!stream.atEnd()) {
			QString path;
			stream >> path;
            elements.append(path);
		}
        foreach(QString id_str, elements) {
            Cyberiada::Element* source_element = static_cast<Cyberiada::Element*>(idToElement(id_str));
            MY_ASSERT(source_element);
            Cyberiada::Element* source_parent = source_element->get_parent();
            if (source_parent == target_element)
                return false;

            move(source_element, target_element_col);
		}
		return true;
	} else {
		return false;
    }
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
	foreach(QModelIndex index, indexes) {
		if (!isStateIndex(index) && !isInitialIndex(index)) continue;
		const Cyberiada::Element* element = indexToElement(index);
		MY_ASSERT(element);
        stream << QString(element->get_id().c_str());
	}
	mimeData->setData(cyberiadaStateMimeType, encodedData);	
	return mimeData;
}
