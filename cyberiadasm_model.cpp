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
#include <QFile>

#include <cmath>
#include <algorithm>

#include "cyberiadasm_model.h"
#include "cyberiadasm_undo.h"
#include "settings_manager.h"
#include "gesture_log.h"
#include "myassert.h"
#include "cyberiada_constants.h"

// the session log helpers: a model mutation is written as its batch verb (see
// batch_script.cpp) so a recorded session replays as a test. logAction is a
// no-op while a mouse gesture is in flight (the gesture records the same edit)
// and while logging is off, so these are cheap.
namespace {
	// escape a trailing text field into one physical line (\\ and \n)
	QString logEsc(const QString& s)
	{
		QString r;
		r.reserve(s.size());
		for (int i = 0; i < s.size(); i++) {
			QChar c = s.at(i);
			if (c == QChar('\\')) r += "\\\\";
			else if (c == QChar('\n')) r += "\\n";
			else r += c;
		}
		return r;
	}
	QString logNum(double v) { return QString::number(v, 'g', 10); }
	QString logRect(const Cyberiada::Rect& r)
	{
		return logNum(r.x) + " " + logNum(r.y) + " " + logNum(r.width) + " " + logNum(r.height);
	}
	QString logPt(const Cyberiada::Point& p) { return logNum(p.x) + " " + logNum(p.y); }
	QString qid(const Cyberiada::Element* e) { return QString::fromStdString(e->get_id()); }
	// the CyberiadaML action notation the batch parser reads back
	QString logActionText(const Cyberiada::Action& a)
	{
		QString behaviour = QString::fromStdString(a.get_behavior());
		if (a.get_type() == Cyberiada::actionEntry) return "entry/ " + behaviour;
		if (a.get_type() == Cyberiada::actionExit) return "exit/ " + behaviour;
		QString head = QString::fromStdString(a.get_trigger());
		QString guard = QString::fromStdString(a.get_guard());
		if (!guard.isEmpty()) head += " [" + guard + "]";
		return head + "/ " + behaviour;
	}
}

CyberiadaSMModel::CyberiadaSMModel(QObject *parent):
	QAbstractItemModel(parent)
{
	root = NULL;
	undo = new QUndoStack(this);
	undo->setUndoLimit(100);
	undoDepth = 0;
	undoBeforeOk = true;
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
	icons[Cyberiada::elementShallowHistory] = QIcon(":/Icons/images/shallow-history.png");
	icons[Cyberiada::elementDeepHistory] = QIcon(":/Icons/images/deep-history.png");
	icons[Cyberiada::elementSubmachineState] = QIcon(":/Icons/images/state-submachine.png");
	icons[Cyberiada::elementEntryPoint] = QIcon(":/Icons/images/entry-point.png");
	icons[Cyberiada::elementExitPoint] = QIcon(":/Icons/images/exit-point.png");
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
std::string CyberiadaSMModel::snapshot(bool* ok) const
{
	if (ok) *ok = true;
	if (!root) return std::string();
	std::string buffer;
	try {
		root->encode(buffer, Cyberiada::formatCyberiada10);
	} catch (const Cyberiada::Exception& e) {
		// a failed encode returns "": the caller must not record it as an undo
		// boundary, or restoreSnapshot("") would reset the document
		qWarning() << "cannot snapshot the document:" << e.str().c_str();
		if (ok) *ok = false;
		return std::string();
	}
	return buffer;
}

void CyberiadaSMModel::beginUndoStep(const QString& text)
{
	if (undoDepth++ == 0) {
		undoText = text;
		undoBefore = snapshot(&undoBeforeOk);
	} else if (undoText.isEmpty()) {
		// the gesture is named by its first mutation
		undoText = text;
	}
}

void CyberiadaSMModel::endUndoStep()
{
	if (undoDepth == 0) return;
	if (--undoDepth > 0) return;
	bool afterOk = true;
	std::string after = snapshot(&afterOk);
	// a failed snapshot at either boundary must not become an undo step: it
	// would restore "" and reset the whole document
	if (!undoBeforeOk || !afterOk) return;
	if (after == undoBefore) return;
	undo->push(new DocumentStep(this, undoText.isEmpty() ? tr("edit") : undoText, undoBefore, after));
}

void CyberiadaSMModel::resetUndoGesture()
{
	// a lost release (a modal menu ate it, or a leaked grab) can leave a scope open;
	// force it closed so the next top-level gesture is captured as its own step
	undoDepth = 0;
	undoText.clear();
	undoBefore.clear();
	undoBeforeOk = true;
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
	GestureLog::instance().logAction("update-id " + QString::fromStdString(old_id) + " " +
									 QString::fromStdString(new_id));
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
	// the document meta comment (CGML_META) is not editable
	if (root && element == root->get_meta_element()) return false;
	Cyberiada::Name new_name(new_value.toStdString());
	// the states of one level are told apart by name; the vertices have none
	if (isState(element) && (new_value.trimmed().isEmpty() || siblingStateNamed(element, new_name))) {
		return false;
	}
	element->set_name(new_name);
	GestureLog::instance().logAction("rename " + qid(element) + " " + logEsc(new_value));
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::setColor(const QModelIndex& index, const QString& color)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("color"));
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (!Cyberiada::element_set_color(element, color.toStdString())) return false;
	GestureLog::instance().logAction("set-color " + qid(element) + " " + logEsc(color));
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

// another outgoing transition of the choice already carries the 'else' guard
// (the default branch of a choice is unique)
static bool choiceHasElse(const Cyberiada::LocalDocument* root, const Cyberiada::Element* choice,
						  const Cyberiada::Element* exclude)
{
	if (!root || !choice) return false;
	Cyberiada::ConstStateMachineList sms = root->get_state_machines();
	for (Cyberiada::ConstStateMachineList::const_iterator s = sms.begin(); s != sms.end(); s++) {
		std::vector<const Cyberiada::Transition*> trans = (*s)->get_transitions();
		for (std::vector<const Cyberiada::Transition*>::const_iterator t = trans.begin(); t != trans.end(); t++) {
			if (static_cast<const Cyberiada::Element*>(*t) == exclude) continue;
			if ((*t)->source_element_id() == choice->get_id() &&
				(*t)->get_action().get_guard() == "else") {
				return true;
			}
		}
	}
	return false;
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
		Cyberiada::Transition* trans = static_cast<Cyberiada::Transition*>(element);
		Cyberiada::Element* src = idToElement(trans->source_element_id().c_str());
		if (src && src->get_type() == Cyberiada::elementChoice) {
			// an outgoing edge of a choice carries only a guard or the default
			// 'else', never a trigger or a behaviour, and only one edge is 'else'
			if (!new_trigger.trimmed().isEmpty() || !new_behaviour.trimmed().isEmpty()) return false;
			QString guard = new_guard.trimmed();
			if (guard.isEmpty()) return false;
			if (guard == "else" && choiceHasElse(root, src, element)) return false;
			trans->get_action().update(std::string(), guard.toStdString(), std::string());
		} else {
			// a transition action may have no trigger (initial, completion)
			if (new_trigger.isEmpty() && new_guard.isEmpty() && new_behaviour.trimmed().isEmpty()) return false;
			trans->get_action().update(new_trigger.toStdString(), new_guard.toStdString(),
									   normalizedBehaviour(new_behaviour).toStdString());
		}
	} else {
		return false;
	}
	{
		// read back the stored action so the notation matches exactly
		int idx = action_index;
		QString text;
		if (element->get_type() == Cyberiada::elementSimpleState ||
			element->get_type() == Cyberiada::elementCompositeState) {
			Cyberiada::State* st = static_cast<Cyberiada::State*>(element);
			if (idx >= 0 && (size_t)idx < st->get_actions().size())
				text = logActionText(st->get_actions().at(idx));
		} else {
			idx = 0;
			text = logActionText(static_cast<Cyberiada::Transition*>(element)->get_action());
		}
		GestureLog::instance().logAction("update-action " + qid(element) + " " +
										 QString::number(idx) + " " + logEsc(text));
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
		if (trigger.isEmpty() && guard.isEmpty() && behaviour.trimmed().isEmpty()) return false;
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
	{
		QString text;
		if (element->get_type() == Cyberiada::elementSimpleState ||
			element->get_type() == Cyberiada::elementCompositeState) {
			Cyberiada::State* st = static_cast<Cyberiada::State*>(element);
			if (!st->get_actions().empty()) text = logActionText(st->get_actions().back());
		} else {
			text = logActionText(static_cast<Cyberiada::Transition*>(element)->get_action());
		}
		GestureLog::instance().logAction("new-action " + qid(element) + " " + logEsc(text));
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
	{
		int idx = element->get_type() == Cyberiada::elementTransition ? 0 : action_index;
		GestureLog::instance().logAction("delete-action " + qid(element) + " " + QString::number(idx));
	}
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::updateGeometry(const QModelIndex& index, const Cyberiada::Point& point, bool record)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("geometry"));
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (!element->has_point_geometry()) return false;
	Cyberiada::Vertex* v = static_cast<Cyberiada::Vertex*>(element);
	if (v->has_geometry()) {
		Cyberiada::Point cur = v->get_geometry_point();
		if (std::fabs(cur.x - point.x) < 0.001 && std::fabs(cur.y - point.y) < 0.001) return true;
	}
	v->update_geometry(point);
	// a derived re-base is reproduced on replay, so it records no gesture
	if (record) GestureLog::instance().logAction("move " + qid(element) + " " + logPt(point));
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::updateGeometry(const QModelIndex& index, const Cyberiada::Rect& rect, bool record)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("geometry"));
	Cyberiada::Element* element = indexToElement(index);
    if (!element) return false;
    if (!element->has_rect_geometry()) return false;
    // an update that leaves the rect unchanged is not a user gesture: skip it so
    // it pushes no undo step and records no (possibly id-less, unreplayable) move
    // - a scene item may re-apply its own geometry while it is being built
    Cyberiada::Rect current;
    if (element->get_type() == Cyberiada::elementChoice) {
        current = static_cast<Cyberiada::ChoicePseudostate*>(element)->get_geometry_rect();
    } else if (element->get_type() == Cyberiada::elementComment ||
               element->get_type() == Cyberiada::elementFormalComment) {
        current = static_cast<Cyberiada::Comment*>(element)->get_geometry_rect();
    } else {
        current = static_cast<Cyberiada::ElementCollection*>(element)->get_geometry_rect();
    }
    if (current.valid && rect.valid && current.almost_equal(rect)) return true;
    // a border set on a geometry-less document (format "none") is otherwise lost:
    // its snapshot omits geometry, so the change neither saves nor undoes. Adopt
    // the Qt format so the rect is serialised, undoable and persisted.
    if (rect.valid && root && root->get_geometry_format() == Cyberiada::geometryFormatNone) {
        root->set_geometry(Cyberiada::geometryFormatQt);
    }
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
	// a derived grow (growToFitChildren) is reproduced on replay by re-creating
	// the child, so it must not record a gesture of its own
	if (record) GestureLog::instance().logAction("move " + qid(element) + " " + logRect(rect));
	emit dataChanged(index, index);
	// a grow (the rect got larger) may reach the element's siblings: push them
	// clear so a container never overlaps a peer (NODE-6). A plain move (same
	// size) or a shrink pushes nothing.
	if (current.valid && rect.valid &&
		(rect.width > current.width + 0.01 || rect.height > current.height + 0.01)) {
		pushSiblingsClear(element, current, rect);
	}
	return true;
}

bool CyberiadaSMModel::updateLabel(const QModelIndex& index, const Cyberiada::Point& label_point)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("label"));
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (element->get_type() != Cyberiada::elementTransition) return false;
	Cyberiada::Transition* trans = static_cast<Cyberiada::Transition*>(element);
	trans->update_label(label_point);
	{
		QString verb = "label " + qid(element);
		if (label_point.valid) verb += " " + logPt(label_point);
		GestureLog::instance().logAction(verb);
	}
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::updateLabel(const QModelIndex& index, const Cyberiada::Rect& label_rect)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("label"));
	Cyberiada::Element* element = indexToElement(index);
	if (!element) return false;
	if (element->get_type() != Cyberiada::elementTransition) return false;
	Cyberiada::Transition* trans = static_cast<Cyberiada::Transition*>(element);
	trans->update_label(label_rect);
	{
		QString verb = "label " + qid(element);
		if (label_rect.valid) verb += " " + logRect(label_rect);
		GestureLog::instance().logAction(verb);
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
	{
		QString verb = "polyline " + qid(element);
		for (Cyberiada::Polyline::const_iterator i = pl.begin(); i != pl.end(); i++) {
			verb += " " + logPt(*i);
		}
		GestureLog::instance().logAction(verb);
	}
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
    if (root->find_element_by_id(source) == NULL || root->find_element_by_id(target) == NULL) {
        // the id isn't available in the document
        return false;
    }
    trans->update(source, target);
    // the id encodes the endpoints (source-target); a retarget renames it so a
    // transition drawn as a loop no longer keeps its loop id
    Cyberiada::ID base = source + "-" + target;
    Cyberiada::ID newId = base;
    for (int n = 2; root->find_element_by_id(newId) != NULL &&
                    root->find_element_by_id(newId) != element; n++) {
        newId = base + "-" + std::to_string(n);
    }
    if (newId != element->get_id()) {
        element->set_id(newId);
    }
    emit dataChanged(index, index);
    return true;
}

// the half-extent the children of pc need from its centre, on each axis; the
// parent's own rect is excluded, so this is the bare content the border must
// hold. Children are stored parent-centre-relative, so |centre| + half-size is
// the reach; a point pseudostate reaches its vertex radius about its point.
void CyberiadaSMModel::childrenHalfExtent(const Cyberiada::ElementCollection* pc,
										  double& halfW, double& halfH) const
{
	halfW = 0.0;
	halfH = 0.0;
	if (!pc) return;
	Cyberiada::ConstElementList kids = pc->get_children();
	for (Cyberiada::ConstElementList::const_iterator i = kids.begin(); i != kids.end(); i++) {
		const Cyberiada::ElementCollection* c = dynamic_cast<const Cyberiada::ElementCollection*>(*i);
		if (c && c->has_geometry()) {
			Cyberiada::Rect cr = c->get_geometry_rect();
			halfW = std::max(halfW, std::fabs(cr.x) + cr.width / 2.0);
			halfH = std::max(halfH, std::fabs(cr.y) + cr.height / 2.0);
			continue;
		}
		const Cyberiada::Vertex* v = dynamic_cast<const Cyberiada::Vertex*>(*i);
		if (v && v->has_point_geometry() && v->has_geometry()) {
			Cyberiada::Point vp = v->get_geometry_point();
			halfW = std::max(halfW, std::fabs(double(vp.x)) + double(VERTEX_POINT_RADIUS));
			halfH = std::max(halfH, std::fabs(double(vp.y)) + double(VERTEX_POINT_RADIUS));
		}
	}
}

// the children's bounding box relative to pc's centre (each edge), or all-zero if none
void CyberiadaSMModel::childrenExtent(const Cyberiada::ElementCollection* pc,
									  double& left, double& right, double& top, double& bottom) const
{
	left = right = top = bottom = 0.0;
	bool any = false;
	if (!pc) return;
	Cyberiada::ConstElementList kids = pc->get_children();
	for (Cyberiada::ConstElementList::const_iterator i = kids.begin(); i != kids.end(); i++) {
		double l, r, t, b;
		const Cyberiada::ElementCollection* c = dynamic_cast<const Cyberiada::ElementCollection*>(*i);
		if (c && c->has_geometry()) {
			Cyberiada::Rect cr = c->get_geometry_rect();
			l = cr.x - cr.width / 2.0; r = cr.x + cr.width / 2.0;
			t = cr.y - cr.height / 2.0; b = cr.y + cr.height / 2.0;
		} else {
			const Cyberiada::Vertex* v = dynamic_cast<const Cyberiada::Vertex*>(*i);
			if (!(v && v->has_point_geometry() && v->has_geometry())) continue;
			Cyberiada::Point vp = v->get_geometry_point();
			l = vp.x - VERTEX_POINT_RADIUS; r = vp.x + VERTEX_POINT_RADIUS;
			t = vp.y - VERTEX_POINT_RADIUS; b = vp.y + VERTEX_POINT_RADIUS;
		}
		if (!any) { left = l; right = r; top = t; bottom = b; any = true; }
		else { left = std::min(left, l); right = std::max(right, r);
			   top = std::min(top, t); bottom = std::max(bottom, b); }
	}
}

// shift every direct child of pc by (dx, dy) so it holds its absolute place when
// the parent centre moves during a directional grow
void CyberiadaSMModel::rebaseChildren(Cyberiada::ElementCollection* pc, double dx, double dy)
{
	if (!pc || (std::fabs(dx) < 1e-6 && std::fabs(dy) < 1e-6)) return;
	Cyberiada::ElementList kids = pc->get_children();
	for (Cyberiada::ElementList::const_iterator i = kids.begin(); i != kids.end(); i++) {
		Cyberiada::ElementCollection* c = dynamic_cast<Cyberiada::ElementCollection*>(*i);
		if (c && c->has_geometry()) {
			Cyberiada::Rect cr = c->get_geometry_rect();
			updateGeometry(elementToIndex(c), Cyberiada::Rect(cr.x + dx, cr.y + dy, cr.width, cr.height), false);
			continue;
		}
		Cyberiada::Vertex* v = dynamic_cast<Cyberiada::Vertex*>(*i);
		if (v && v->has_point_geometry() && v->has_geometry()) {
			Cyberiada::Point vp = v->get_geometry_point();
			updateGeometry(elementToIndex(v), Cyberiada::Point(vp.x + dx, vp.y + dy), false);
		}
	}
}

// pc just grew from oldRect to newRect (same parent-centre frame); shift the
// siblings it grew into outward by the per-side expansion so they no longer
// overlap it. Every sibling on a side moves by the same amount, so their mutual
// spacing is kept and no new sibling-sibling overlap appears; the caller's
// walk-up then grows pc's parent to re-contain the pushed siblings.
void CyberiadaSMModel::pushSiblingsClear(Cyberiada::Element* pc,
										 const Cyberiada::Rect& oldRect,
										 const Cyberiada::Rect& newRect)
{
	if (!pc) return;
	Cyberiada::ElementCollection* gp = dynamic_cast<Cyberiada::ElementCollection*>(pc->get_parent());
	if (!gp) return;   // a top-level pc has no siblings

	double oL = oldRect.x - oldRect.width / 2.0,  oR = oldRect.x + oldRect.width / 2.0;
	double oT = oldRect.y - oldRect.height / 2.0, oB = oldRect.y + oldRect.height / 2.0;
	double nL = newRect.x - newRect.width / 2.0,  nR = newRect.x + newRect.width / 2.0;
	double nT = newRect.y - newRect.height / 2.0, nB = newRect.y + newRect.height / 2.0;
	double eL = std::max(0.0, oL - nL), eR = std::max(0.0, nR - oR);
	double eT = std::max(0.0, oT - nT), eB = std::max(0.0, nB - oB);
	if (eL == 0.0 && eR == 0.0 && eT == 0.0 && eB == 0.0) return;

	Cyberiada::ElementList kids = gp->get_children();
	for (Cyberiada::ElementList::const_iterator i = kids.begin(); i != kids.end(); i++) {
		Cyberiada::Element* s = *i;
		if (s == pc) continue;
		const Cyberiada::ElementCollection* sc = dynamic_cast<const Cyberiada::ElementCollection*>(s);
		const Cyberiada::Vertex* sv = dynamic_cast<const Cyberiada::Vertex*>(s);
		double cx, cy, hw, hh;
		if (sc && sc->has_geometry()) {
			Cyberiada::Rect r = sc->get_geometry_rect();
			cx = r.x; cy = r.y; hw = r.width / 2.0; hh = r.height / 2.0;
		} else if (sv && sv->has_point_geometry() && sv->has_geometry()) {
			Cyberiada::Point p = sv->get_geometry_point();
			cx = p.x; cy = p.y; hw = hh = VERTEX_POINT_RADIUS;
		} else {
			continue;
		}
		double sl = cx - hw, sr = cx + hw, st = cy - hh, sb = cy + hh;
		// skip a pre-existing overlap (not ours) and a sibling pc never reached
		bool overOld = sl < oR && oL < sr && st < oB && oT < sb;
		bool overNew = sl < nR && nL < sr && st < nB && nT < sb;
		if (overOld || !overNew) continue;
		double dx = 0.0, dy = 0.0;
		if (sl >= oR)      dx = eR;
		else if (sr <= oL) dx = -eL;
		if (st >= oB)      dy = eB;
		else if (sb <= oT) dy = -eT;
		if (dx == 0.0 && dy == 0.0) continue;
		// push in the cleared direction, then settle to a slot clear of the other
		// siblings too - a uniform push can shove s into a third sibling (a cascade
		// overlap); freeChildCentre resolves it and is a no-op when the push sufficed
		Cyberiada::Point f = freeChildCentre(gp, s, hw * 2.0, hh * 2.0,
											 Cyberiada::Point(cx + dx, cy + dy));
		if (sc && sc->has_geometry()) {
			updateGeometry(elementToIndex(s), Cyberiada::Rect(f.x, f.y, hw * 2.0, hh * 2.0), false);
		} else {
			updateGeometry(elementToIndex(s), Cyberiada::Point(f.x, f.y), false);
		}
	}
}

static bool elementRect(const Cyberiada::Element* element, Cyberiada::Rect& out);

// find a centre in pc's child frame where a (w x h) box clears every existing
// child (skipping `skip` and transitions): keep `preferred` when it is already
// free, else the nearest free slot on a grid around it, else right of the content.
// paste/reparent place the incoming child clear of its new siblings (EDIT-NODE-6);
// the caller's growToFitChildren then grows pc to re-contain a slot that spilt out.
Cyberiada::Point CyberiadaSMModel::freeChildCentre(const Cyberiada::ElementCollection* pc,
                                                   const Cyberiada::Element* skip,
                                                   double w, double h,
                                                   const Cyberiada::Point& preferred) const
{
	if (w <= 0.0 || h <= 0.0) return preferred;   // no box to place
	struct Box { double l, r, t, b; };
	std::vector<Box> taken;
	double contentRight = 0.0;
	if (pc) {
		Cyberiada::ConstElementList kids = pc->get_children();
		for (Cyberiada::ConstElementList::const_iterator i = kids.begin(); i != kids.end(); i++) {
			if (*i == skip || (*i)->get_type() == Cyberiada::elementTransition) continue;
			double cx, cy, hw, hh;
			const Cyberiada::ElementCollection* c = dynamic_cast<const Cyberiada::ElementCollection*>(*i);
			const Cyberiada::Vertex* v = dynamic_cast<const Cyberiada::Vertex*>(*i);
			if (c && c->has_geometry()) {
				Cyberiada::Rect r = c->get_geometry_rect();
				if (r.width <= 0.0 || r.height <= 0.0) continue;
				cx = r.x; cy = r.y; hw = r.width / 2.0; hh = r.height / 2.0;
			} else if (v && v->has_point_geometry() && v->has_geometry()) {
				Cyberiada::Point p = v->get_geometry_point();
				cx = p.x; cy = p.y; hw = hh = VERTEX_POINT_RADIUS;
			} else {
				continue;
			}
			Box box = { cx - hw, cx + hw, cy - hh, cy + hh };
			taken.push_back(box);
			contentRight = taken.size() == 1 ? box.r : std::max(contentRight, box.r);
		}
	}
	if (taken.empty()) return preferred;   // nothing to clear, keep the position

	const double g = PLACEMENT_GAP, hw = w / 2.0, hh = h / 2.0;
	// a box centred at (cx, cy) sits clear of every taken box with a g gap
	auto clear = [&](double cx, double cy) {
		for (size_t k = 0; k < taken.size(); k++) {
			const Box& t = taken[k];
			if (cx - hw < t.r + g && t.l - g < cx + hw &&
				cy - hh < t.b + g && t.t - g < cy + hh) return false;
		}
		return true;
	};
	if (clear(preferred.x, preferred.y)) return preferred;

	// scan expanding rings around the preferred centre - the nearest free slot
	const double stepX = w + g, stepY = h + g;
	for (int ring = 1; ring <= 8; ring++) {
		for (int dr = -ring; dr <= ring; dr++) {
			for (int dc = -ring; dc <= ring; dc++) {
				int ar = dr < 0 ? -dr : dr, ac = dc < 0 ? -dc : dc;
				if ((ar > ac ? ar : ac) != ring) continue;   // ring perimeter only
				double cx = preferred.x + dc * stepX, cy = preferred.y + dr * stepY;
				if (clear(cx, cy)) return Cyberiada::Point(cx, cy);
			}
		}
	}
	// packed: place right of the content (x-clearance alone makes it non-overlapping)
	return Cyberiada::Point(contentRight + hw + g, preferred.y);
}

bool CyberiadaSMModel::growToFitChildren(Cyberiada::Element* moved, bool directional)
{
	if (readOnly() || !moved) return false;
	// re-entrancy guard: the re-base below writes child geometry, which can loop
	// back here through the scene
	if (m_growing) return false;
	m_growing = true;
	bool grew = false;
	// walk up: a grown parent may in turn no longer fit its own parent. Stop at
	// a collection without a real rectangle - a rect-less SM keeps no border and
	// reconstructs it from its content, so it must not be given a stored rect.
	for (Cyberiada::Element* e = moved; e; ) {
		Cyberiada::ElementCollection* pc =
			dynamic_cast<Cyberiada::ElementCollection*>(e->get_parent());
		if (!pc || !pc->has_geometry()) break;
		Cyberiada::Rect pr = pc->get_geometry_rect();
		if (!std::isfinite(pr.x) || !std::isfinite(pr.y)) break;
		if (pr.width <= 0.0 || pr.height <= 0.0 || pc->get_id().empty()) break;
		double cl, cr, ct, cb;
		childrenExtent(pc, cl, cr, ct, cb);
		if (!(std::isfinite(cl) && std::isfinite(cr) && std::isfinite(ct) && std::isfinite(cb))) {
			e = pc;
			continue;
		}
		if (directional) {
			// extend each edge outward to include the content, keeping the edges
			// the content does not push, and re-base the children to hold their
			// absolute place - the directional counterpart of the drag grow
			double L = std::min(-pr.width / 2.0, cl),  R = std::max(pr.width / 2.0, cr);
			double T = std::min(-pr.height / 2.0, ct), B = std::max(pr.height / 2.0, cb);
			if (R - L != pr.width || B - T != pr.height) {
				double offX = (L + R) / 2.0, offY = (T + B) / 2.0;   // centre shift
				rebaseChildren(pc, -offX, -offY);                    // hold children absolute
				updateGeometry(elementToIndex(pc),
							   Cyberiada::Rect(pr.x + offX, pr.y + offY, R - L, B - T), false);
				grew = true;
			}
		} else {
			// contain the new child by growing about the current centre (children
			// keep their stored positions) - used on create, where the freshly
			// placed child is the only one that may fall outside
			double halfW = std::max(std::fabs(cl), std::fabs(cr));
			double halfH = std::max(std::fabs(ct), std::fabs(cb));
			double newW = std::max((double)pr.width, 2.0 * halfW);
			double newH = std::max((double)pr.height, 2.0 * halfH);
			if (newW > pr.width + 0.001 || newH > pr.height + 0.001) {
				updateGeometry(elementToIndex(pc), Cyberiada::Rect(pr.x, pr.y, newW, newH), false);
				grew = true;
			}
		}
		e = pc;
	}
	m_growing = false;
	return grew;
}

bool CyberiadaSMModel::updateParent(const QModelIndex &index, const Cyberiada::ID &new_parent_id)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("reparent"));
    Cyberiada::Element* element = indexToElement(index);
    if (!element) return false;
    Cyberiada::ElementCollection* new_parent = dynamic_cast<Cyberiada::ElementCollection*>(idToElement(new_parent_id.c_str()));
    if (!new_parent) return false;
    // reparenting into the element itself or one of its descendants would free
    // the target subtree mid-move (use-after-free in move()): refuse it
    for (const Cyberiada::Element* a = new_parent; a; a = a->get_parent()) {
        if (a == element) return false;
    }
    if (parent(index) == elementToIndex(new_parent)) return true;
    if (element->get_type() == Cyberiada::elementInitial) {
        // TODO check
    }
    // move() frees the original element, so capture the id before the call
    Cyberiada::ID moved_id = element->get_id();
    move(element, new_parent);
    // move() keeps the child's absolute position but does not grow the new parent
    // or clear its existing children; if the drop overlaps a sibling, relocate the
    // moved child to a free slot, then grow the parent so it stays inside (as paste)
    if (Cyberiada::Element* moved = idToElement(moved_id.c_str())) {
        Cyberiada::Rect r;
        if (elementRect(moved, r)) {
            Cyberiada::Point f = freeChildCentre(new_parent, moved, r.width, r.height,
                                                 Cyberiada::Point(r.x, r.y));
            if (f.x != r.x || f.y != r.y) {
                updateGeometry(elementToIndex(moved),
                               Cyberiada::Rect(f.x, f.y, r.width, r.height), false);
            }
        }
        growToFitChildren(moved);
    }
    GestureLog::instance().logAction("reparent " + QString::fromStdString(moved_id) + " " +
                                     QString::fromStdString(new_parent_id));
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
	// the document meta comment (CGML_META) is not editable
	if (root && element == root->get_meta_element()) return false;
	static_cast<Cyberiada::Comment*>(element)->set_body(body.toStdString());
	GestureLog::instance().logAction("update-comment " + qid(element) + " " + logEsc(body));
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
	GestureLog::instance().logAction("update-meta " + parameter + " " + logEsc(new_value));
	// re-serialize the meta comment; save() does not do it
	root->update_metainfo_element();
	QModelIndex comment_index = elementToIndex(root->get_meta_element());
	emit dataChanged(comment_index, comment_index);
	emit dataChanged(index, index);
	return true;
}

bool CyberiadaSMModel::removeMetainformation(const QModelIndex& index, const QString& parameter)
{
	if (readOnly()) return false;
	UndoScope scope(this, tr("metainformation"));
	if (!root || index != documentIndex()) {
		return false;
	}
	// only the free-form parameters are removable
	Cyberiada::String name = parameter.toStdString();
	if (name == CYBERIADA_META_STANDARD_VERSION || name == CYBERIADA_META_TRANSITION_ORDER ||
		name == CYBERIADA_META_EVENT_PROPAGATION || name == CYBERIADA_META_GEOMETRY ||
		name == CYBERIADA_META_NAME) {
		return false;
	}
	root->meta().remove_string(name);
	GestureLog::instance().logAction("remove-meta " + parameter);
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

    if (element) {
        QString verb = "new-sm";
        if (r.valid) verb += " " + logRect(r);
        verb += " " + logEsc(QString::fromStdString(sm_name));
        GestureLog::instance().logAction(verb);
    }
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
    // keep siblings clear (EDIT-NODE-6): a create asked at a taken spot - the GUI
    // pre-picks a free place, a script may pass overlapping coords - is relocated.
    // a default create carries no concrete rect yet (the scene sizes it later): skip
    Cyberiada::Rect er;
    if (element && elementRect(element, er) && er.valid && er.width > 0.0 && er.height > 0.0) {
        Cyberiada::Point f = freeChildCentre(parent, element, er.width, er.height,
                                             Cyberiada::Point(er.x, er.y));
        if (f.x != er.x || f.y != er.y) {
            updateGeometry(elementToIndex(element),
                           Cyberiada::Rect(f.x, f.y, er.width, er.height), false);
        }
    }
    // a child placed past the parent border grows the parent to contain it (about
    // the centre - a freshly created child keeps the siblings where they are)
    if (element) growToFitChildren(element, false);

    if (element) {
        QString verb = "new-state " + qid(parent);
        if (r.valid) verb += " " + logRect(r);
        verb += " " + logEsc(QString::fromStdString(state_name));
        GestureLog::instance().logAction(verb);
    }
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
    // a vertex placed past the parent border grows the parent to contain it (EDIT-NODE-1/2)
    if (element) growToFitChildren(element, false);

    if (element) {
        QString verb = "new-initial " + qid(parent);
        if (p.valid) verb += " " + logPt(p);
        GestureLog::instance().logAction(verb);
    }
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
    if (element) growToFitChildren(element, false);

    if (element) {
        QString verb = "new-final " + qid(parent);
        if (p.valid) verb += " " + logPt(p);
        GestureLog::instance().logAction(verb);
    }
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
    if (element) growToFitChildren(element, false);

    if (element) {
        QString verb = "new-choice " + qid(parent);
        if (r.valid) verb += " " + logRect(r);
        GestureLog::instance().logAction(verb);
    }
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
    if (element) growToFitChildren(element, false);

    if (element) {
        QString verb = "new-terminate " + qid(parent);
        if (p.valid) verb += " " + logPt(p);
        GestureLog::instance().logAction(verb);
    }
    return element;
}

Cyberiada::HistoryPseudostate *CyberiadaSMModel::newShallowHistory(Cyberiada::ElementCollection *parent, const Cyberiada::Point &p)
{
	if (readOnly()) return NULL;
	UndoScope scope(this, tr("new element"));
    if (root == NULL) {
        return nullptr;
    }

    int row = newElementRow(parent);
    beginInsertRows(elementToIndex(parent), row, row);
    Cyberiada::HistoryPseudostate* element = root->new_shallow_history(parent, p);
    endInsertRows();
    if (element) growToFitChildren(element, false);

    if (element) {
        QString verb = "new-shallow-history " + qid(parent);
        if (p.valid) verb += " " + logPt(p);
        GestureLog::instance().logAction(verb);
    }
    return element;
}

Cyberiada::HistoryPseudostate *CyberiadaSMModel::newDeepHistory(Cyberiada::ElementCollection *parent, const Cyberiada::Point &p)
{
	if (readOnly()) return NULL;
	UndoScope scope(this, tr("new element"));
    if (root == NULL) {
        return nullptr;
    }

    int row = newElementRow(parent);
    beginInsertRows(elementToIndex(parent), row, row);
    Cyberiada::HistoryPseudostate* element = root->new_deep_history(parent, p);
    endInsertRows();
    if (element) growToFitChildren(element, false);

    if (element) {
        QString verb = "new-deep-history " + qid(parent);
        if (p.valid) verb += " " + logPt(p);
        GestureLog::instance().logAction(verb);
    }
    return element;
}

Cyberiada::SubmachineState *CyberiadaSMModel::newSubmachineState(Cyberiada::ElementCollection *parent, const Cyberiada::ID &reference, const Cyberiada::Rect &r)
{
	if (readOnly()) return NULL;
	UndoScope scope(this, tr("new element"));
    if (root == NULL) {
        return nullptr;
    }

    // the reference is set later in the properties; default to another machine
    Cyberiada::ID ref = reference;
    if (ref.empty()) {
        Cyberiada::ElementList sms = root->find_elements_by_type(Cyberiada::elementSM);
        if (!sms.empty()) ref = sms.front()->get_id();
        if (ref.empty()) ref = "submachine";
    }

    int row = newElementRow(parent);
    beginInsertRows(elementToIndex(parent), row, row);
    Cyberiada::SubmachineState* element = root->new_submachine_state(parent, ref, Cyberiada::Name(), r);
    endInsertRows();
    if (element) growToFitChildren(element, false);

    if (element) {
        GestureLog::instance().logAction("new-submachine-state " + qid(parent));
    }
    return element;
}

Cyberiada::ConnectionPoint *CyberiadaSMModel::newEntryPoint(Cyberiada::ElementCollection *parent, const Cyberiada::Point &p)
{
	if (readOnly()) return NULL;
	UndoScope scope(this, tr("new element"));
    if (root == NULL) {
        return nullptr;
    }

    int row = newElementRow(parent);
    beginInsertRows(elementToIndex(parent), row, row);
    Cyberiada::ConnectionPoint* element = root->new_entry(parent, p);
    endInsertRows();
    if (element) growToFitChildren(element, false);

    if (element) {
        QString verb = "new-entry-point " + qid(parent);
        if (p.valid) verb += " " + logPt(p);
        GestureLog::instance().logAction(verb);
    }
    return element;
}

Cyberiada::ConnectionPoint *CyberiadaSMModel::newExitPoint(Cyberiada::ElementCollection *parent, const Cyberiada::Point &p)
{
	if (readOnly()) return NULL;
	UndoScope scope(this, tr("new element"));
    if (root == NULL) {
        return nullptr;
    }

    int row = newElementRow(parent);
    beginInsertRows(elementToIndex(parent), row, row);
    Cyberiada::ConnectionPoint* element = root->new_exit(parent, p);
    endInsertRows();
    if (element) growToFitChildren(element, false);

    if (element) {
        QString verb = "new-exit-point " + qid(parent);
        if (p.valid) verb += " " + logPt(p);
        GestureLog::instance().logAction(verb);
    }
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

    if (element) {
        QString verb = "new-transition " + qid(sm) + " " + qid(source) + " " + qid(target);
        // append the action notation only for a non-empty action, so an
        // ordinary drawn edge stays a bare new-transition
        if (!action.get_trigger().empty() || !action.get_guard().empty() ||
            !action.get_behavior().empty()) {
            verb += " " + logEsc(logActionText(action));
        }
        GestureLog::instance().logAction(verb);
    }
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

    if (element) {
        GestureLog::instance().logAction("new-comment " + qid(parent) + " " +
                                         logEsc(QString::fromStdString(body)));
    }
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

    if (element) {
        GestureLog::instance().logAction("new-formal-comment " + qid(parent) + " " +
                                         logEsc(QString::fromStdString(body)));
    }
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
    {
        QString verb = "new-subject " + qid(element) + " " + qid(target);
        if (type == Cyberiada::commentSubjectName) verb += " name " + logEsc(fragment);
        else if (type == Cyberiada::commentSubjectData) verb += " data " + logEsc(fragment);
        GestureLog::instance().logAction(verb);
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
    GestureLog::instance().logAction("delete-subject " + qid(element) + " " +
                                     QString::number(subject_index));
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

    // the top-level delete cascades to the attached transitions; the replay
    // delete cascades the same way, so only the outermost call is logged
    if (deleteDepth == 0) {
        GestureLog::instance().logAction("delete " + qid(child_element));
    }
    deleteDepth++;

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
    deleteDepth--;
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

QString CyberiadaSMModel::editorView() const
{
	if (!root) return QString();
	return QString::fromStdString(root->meta().get_string("cyberiadaEditorView"));
}

void CyberiadaSMModel::setEditorView(const QString& value)
{
	if (!root || readOnly()) return;
	root->meta().set_string("cyberiadaEditorView", value.toStdString());
	root->update_metainfo_element();
}

bool CyberiadaSMModel::writeSnapshotFile(const QString& path) const
{
	if (!root) return false;
	std::string buffer;
	try {
		root->encode(buffer, Cyberiada::formatCyberiada10);
	} catch (const Cyberiada::Exception& e) {
		qWarning() << "cannot snapshot the document for the session log:" << e.str().c_str();
		return false;
	}
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
	f.write(buffer.data(), buffer.size());
	f.close();
	return true;
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

static double elementRectWidth(const Cyberiada::Element* element)
{
    if (!element->has_rect_geometry()) return 0.0;
    Cyberiada::ElementType type = element->get_type();
    if (type == Cyberiada::elementChoice) {
        return static_cast<const Cyberiada::ChoicePseudostate*>(element)->get_geometry_rect().width;
    } else if (type == Cyberiada::elementComment || type == Cyberiada::elementFormalComment) {
        return static_cast<const Cyberiada::Comment*>(element)->get_geometry_rect().width;
    }
    return static_cast<const Cyberiada::ElementCollection*>(element)->get_geometry_rect().width;
}

// the rect of a rect-geometry element (choice/comment/collection); false otherwise
static bool elementRect(const Cyberiada::Element* element, Cyberiada::Rect& out)
{
    if (!element->has_rect_geometry()) return false;
    Cyberiada::ElementType type = element->get_type();
    if (type == Cyberiada::elementChoice) {
        out = static_cast<const Cyberiada::ChoicePseudostate*>(element)->get_geometry_rect();
    } else if (type == Cyberiada::elementComment || type == Cyberiada::elementFormalComment) {
        out = static_cast<const Cyberiada::Comment*>(element)->get_geometry_rect();
    } else {
        out = static_cast<const Cyberiada::ElementCollection*>(element)->get_geometry_rect();
    }
    return true;
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

// pre-order (parent before children) walk of a subtree into a flat list
static void collectSubtree(Cyberiada::Element* e, std::vector<Cyberiada::Element*>& out)
{
    out.push_back(e);
    if (Cyberiada::ElementCollection* c = dynamic_cast<Cyberiada::ElementCollection*>(e)) {
        const Cyberiada::ElementList& kids = c->get_children();
        for (Cyberiada::ElementList::const_iterator i = kids.begin(); i != kids.end(); i++) {
            collectSubtree(*i, out);
        }
    }
}

// a fresh vertex-style id, qualified by a nested parent, unique in the document
// (mirrors Document::generate_vertex_id, which is private)
static Cyberiada::ID freshVertexId(const Cyberiada::Document* root, const Cyberiada::Element* parent)
{
    std::string base;
    if (parent && parent->get_type() != Cyberiada::elementRoot &&
        parent->get_type() != Cyberiada::elementSM) {
        base = parent->get_id() + "::";
    }
    for (int n = 0; ; n++) {
        Cyberiada::ID cand = base + "n" + std::to_string(n);
        if (!root->find_element_by_id(cand)) return cand;
    }
}

// a fresh transition id (source-target, then source-target#N) unique in the document
static Cyberiada::ID freshTransitionId(const Cyberiada::Document* root,
                                       const Cyberiada::ID& s, const Cyberiada::ID& t)
{
    Cyberiada::ID base = s + "-" + t;
    Cyberiada::ID cand = base;
    for (int n = 0; root->find_element_by_id(cand); n++) {
        cand = base + "#" + std::to_string(n);
    }
    return cand;
}

// offset a transition's stored points so a copy does not overlap its original
static void shiftTransition(Cyberiada::Transition* t, double dx, double dy)
{
    Cyberiada::Point sp = t->has_geometry_source_point() ? t->get_source_point() : Cyberiada::Point();
    Cyberiada::Point tp = t->has_geometry_target_point() ? t->get_target_point() : Cyberiada::Point();
    if (sp.valid) sp = Cyberiada::Point(sp.x + dx, sp.y + dy);
    if (tp.valid) tp = Cyberiada::Point(tp.x + dx, tp.y + dy);
    if (sp.valid || tp.valid) t->update(sp, tp);
    if (t->has_polyline()) {
        Cyberiada::Polyline pl = t->get_geometry_polyline();
        for (size_t i = 0; i < pl.size(); i++) pl[i] = Cyberiada::Point(pl[i].x + dx, pl[i].y + dy);
        t->update(pl);
    }
    if (t->has_geometry_label_point()) {
        Cyberiada::Point lp = t->get_label_point();
        t->update_label(Cyberiada::Point(lp.x + dx, lp.y + dy));
    } else if (t->has_geometry_label_rect()) {
        Cyberiada::Rect lr = t->get_label_rect();
        t->update_label(Cyberiada::Rect(lr.x + dx, lr.y + dy, lr.width, lr.height));
    }
}

Cyberiada::Element* CyberiadaSMModel::pasteElement(Cyberiada::ElementCollection* parent,
                                                   const Cyberiada::Element* src)
{
    if (readOnly() || !parent || !src || !root) return NULL;
    if (src->get_type() == Cyberiada::elementSM) return NULL;   // never paste a State Machine

    const double PASTE_OFFSET = 20.0;
    UndoScope scope(this, tr("paste"));
    beginResetModel();

    // deep clone (keeps ids/names) into the target, then add it so the id
    // generators and remap see the whole subtree
    Cyberiada::Element* copied = src->copy(parent);
    parent->add_element(copied);
    if (Cyberiada::ElementCollection* subtree = dynamic_cast<Cyberiada::ElementCollection*>(copied)) {
        root->rebind_subjects(*subtree);
    }

    std::vector<Cyberiada::Element*> all;
    collectSubtree(copied, all);

    // fresh ids for every node, recording old -> new (pre-order, so a parent's
    // new id is set before its children qualify against it)
    std::map<Cyberiada::ID, Cyberiada::ID> idmap;
    for (size_t i = 0; i < all.size(); i++) {
        if (all[i]->get_type() == Cyberiada::elementTransition) continue;
        Cyberiada::ID oldId = all[i]->get_id();
        Cyberiada::ID newId = freshVertexId(root, all[i]->get_parent());
        all[i]->set_id(newId);
        idmap[oldId] = newId;
    }
    // remap the transitions: endpoints inside the pasted set follow the copies,
    // endpoints outside keep their original ids (a parallel transition)
    for (size_t i = 0; i < all.size(); i++) {
        if (all[i]->get_type() != Cyberiada::elementTransition) continue;
        Cyberiada::Transition* t = static_cast<Cyberiada::Transition*>(all[i]);
        Cyberiada::ID s = t->source_element_id(), tg = t->target_element_id();
        if (idmap.count(s)) s = idmap[s];
        if (idmap.count(tg)) tg = idmap[tg];
        t->update(s, tg);
        t->set_id(freshTransitionId(root, s, tg));
    }

    // an internal transition (both endpoints inside the copied subtree) is stored
    // at the state-machine level, not in the subtree, so collectSubtree never sees
    // it; clone each such transition onto the copy so the paste keeps its own wiring
    if (Cyberiada::StateMachine* sm = root->get_parent_sm(copied)) {
        std::vector<Cyberiada::Transition*> internal;
        const Cyberiada::ElementList& sm_children = sm->get_children();
        for (Cyberiada::ElementList::const_iterator i = sm_children.begin(); i != sm_children.end(); i++) {
            if ((*i)->get_type() != Cyberiada::elementTransition) continue;
            Cyberiada::Transition* t = static_cast<Cyberiada::Transition*>(*i);
            if (idmap.count(t->source_element_id()) && idmap.count(t->target_element_id())) {
                internal.push_back(t);   // snapshot before adding the copies
            }
        }
        for (size_t i = 0; i < internal.size(); i++) {
            Cyberiada::Transition* t = static_cast<Cyberiada::Transition*>(internal[i]->copy(sm));
            Cyberiada::ID s = idmap[internal[i]->source_element_id()];
            Cyberiada::ID tg = idmap[internal[i]->target_element_id()];
            t->update(s, tg);
            t->set_id(freshTransitionId(root, s, tg));
            sm->add_element(t);
        }
    }

    // a unique name for the pasted element among its new siblings
    if (isState(copied)) {
        copied->set_name(uniqueStateName(parent, copied->get_name()));
    }

    // shift the copy a little off the original (the subtree follows a node shift;
    // a standalone transition keeps its endpoints, so shift its points instead)
    if (copied->get_type() == Cyberiada::elementTransition) {
        shiftTransition(static_cast<Cyberiada::Transition*>(copied), PASTE_OFFSET, PASTE_OFFSET);
    } else {
        // clear the source's box so the copy does not overlap it (a sibling
        // overlap is a NODE-6 violation); shift a rect element by its own width
        double dx = PASTE_OFFSET, dy = PASTE_OFFSET;
        double w = elementRectWidth(copied);
        if (w > 0.0) { dx = w + PASTE_OFFSET; dy = 0.0; }
        shiftGeometry(copied, dx, dy);
        // that offset clears the source; if the shifted spot still overlaps another
        // existing child of the target parent, relocate the copy to a free slot
        Cyberiada::Rect r;
        if (elementRect(copied, r)) {
            Cyberiada::Point f = freeChildCentre(parent, copied, r.width, r.height,
                                                 Cyberiada::Point(r.x, r.y));
            if (f.x != r.x || f.y != r.y) shiftGeometry(copied, f.x - r.x, f.y - r.y);
        }
        // the copy may fall past the parent border; grow the parent to hold it
        growToFitChildren(copied);
    }

    endResetModel();
    GestureLog::instance().logAction("paste " + qid(copied));
    return copied;
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
