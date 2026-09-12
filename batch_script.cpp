/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The batch mode edit script interpreter
 *
 * Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
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

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QTextCursor>

#include "batch_script.h"
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"
#include "smeditor_window.h"
#include "cyberiada_constants.h"
#include "cyberiadasm_dump.h"
#include "editable_text_item.h"
#include "settings_manager.h"

static bool toNumbers(const QStringList& tokens, int from, int count, double* values)
{
	if (tokens.size() < from + count) return false;
	for (int i = 0; i < count; i++) {
		bool ok = false;
		values[i] = tokens.at(from + i).toDouble(&ok);
		if (!ok) return false;
	}
	return true;
}

static QString restOfLine(const QStringList& tokens, int from)
{
	QString text = tokens.mid(from).join(" ");
	// the trailing text field is one physical line; the escape \n embeds
	// a newline, \\ a backslash
	QString result;
	result.reserve(text.size());
	for (int i = 0; i < text.size(); i++) {
		if (text.at(i) == QChar('\\') && i + 1 < text.size()) {
			QChar next = text.at(i + 1);
			if (next == QChar('n')) { result += QChar('\n'); i++; continue; }
			if (next == QChar('\\')) { result += QChar('\\'); i++; continue; }
		}
		result += text.at(i);
	}
	return result;
}

// action text uses the CyberiadaML notation: 'entry/ behaviour',
// 'exit/ behaviour' or 'TRIGGER [guard]/ behaviour'; a transition may have
// no trigger ('/ behaviour': the initial or completion transition)
static bool parseActionText(const QString& text, bool transition, Cyberiada::ActionType* type,
							QString* trigger, QString* guard, QString* behaviour,
							QString* error)
{
	int slash = -1;
	int depth = 0;
	for (int i = 0; i < text.length(); i++) {
		QChar c = text.at(i);
		if (c == QChar('[')) depth++;
		else if (c == QChar(']')) depth--;
		else if (c == QChar('/') && depth == 0) { slash = i; break; }
	}
	if (slash < 0) {
		*error = "action text requires the 'trigger [guard]/ behaviour' notation";
		return false;
	}
	QString head = text.left(slash).trimmed();
	*behaviour = text.mid(slash + 1).trimmed();
	*trigger = head;
	guard->clear();
	int bracket = head.indexOf(QChar('['));
	if (bracket >= 0) {
		if (!head.endsWith("]")) { *error = "unbalanced guard brackets in the action text"; return false; }
		*trigger = head.left(bracket).trimmed();
		*guard = head.mid(bracket + 1, head.length() - bracket - 2).trimmed();
	}
	if (*trigger == "entry" || *trigger == "exit") {
		if (!guard->isEmpty()) { *error = "guards are not allowed for entry/exit activities"; return false; }
		*type = *trigger == "entry" ? Cyberiada::actionEntry : Cyberiada::actionExit;
		trigger->clear();
		return true;
	}
	*type = Cyberiada::actionTransition;
	if (trigger->isEmpty() && !transition) { *error = "the action trigger is required"; return false; }
	return true;
}

static bool runCommand(CyberiadaSMModel* model, const QStringList& tokens, QString* error)
{
	const QString& cmd = tokens.first();
	double v[4];

	if (cmd == "new-state" || cmd == "new-comment" || cmd == "new-formal-comment" ||
		cmd == "new-initial" || cmd == "new-final" ||
		cmd == "new-choice" || cmd == "new-terminate") {
		if (tokens.size() < 2) { *error = cmd + " requires a parent id"; return false; }
		Cyberiada::Element* parent = model->idToElement(tokens.at(1));
		Cyberiada::ElementCollection* collection = dynamic_cast<Cyberiada::ElementCollection*>(parent);
		if (!collection) { *error = "unknown parent id '" + tokens.at(1) + "'"; return false; }
		if (cmd == "new-state") {
			Cyberiada::Rect r;
			int name_from = 2;
			if (toNumbers(tokens, 2, 4, v)) {
				r = Cyberiada::Rect(v[0], v[1], v[2], v[3]);
				name_from = 6;
			}
			QString name = restOfLine(tokens, name_from);
			if (name.isEmpty()) { *error = "new-state requires a name"; return false; }
			return model->newState(collection, name.toStdString(), Cyberiada::Action(), r) != NULL;
		} else if (cmd == "new-comment" || cmd == "new-formal-comment") {
			QString body = restOfLine(tokens, 2);
			if (body.isEmpty()) { *error = cmd + " requires a body"; return false; }
			if (cmd == "new-comment") {
				return model->newComment(collection, body.toStdString()) != NULL;
			}
			return model->newFormalComment(collection, body.toStdString()) != NULL;
		} else if (cmd == "new-choice") {
			Cyberiada::Rect r;
			if (toNumbers(tokens, 2, 4, v)) {
				r = Cyberiada::Rect(v[0], v[1], v[2], v[3]);
			}
			return model->newChoice(collection, r) != NULL;
		} else {
			Cyberiada::Point p;
			if (toNumbers(tokens, 2, 2, v)) {
				p = Cyberiada::Point(v[0], v[1]);
			}
			if (cmd == "new-initial") {
				return model->newInitial(collection, p) != NULL;
			}
			if (cmd == "new-terminate") {
				return model->newTerminate(collection, p) != NULL;
			}
			return model->newFinal(collection, p) != NULL;
		}
	} else if (cmd == "new-transition") {
		if (tokens.size() < 4) { *error = "new-transition requires <sm> <source> <target>"; return false; }
		Cyberiada::StateMachine* sm = dynamic_cast<Cyberiada::StateMachine*>(model->idToElement(tokens.at(1)));
		if (!sm) { *error = "unknown state machine id '" + tokens.at(1) + "'"; return false; }
		Cyberiada::Element* source = model->idToElement(tokens.at(2));
		Cyberiada::Element* target = model->idToElement(tokens.at(3));
		if (!source) { *error = "unknown source id '" + tokens.at(2) + "'"; return false; }
		if (!target) { *error = "unknown target id '" + tokens.at(3) + "'"; return false; }
		// the trailing field: a bare trigger, or the full action notation
		QString text = restOfLine(tokens, 4);
		Cyberiada::Action action(text.toStdString());
		if (text.contains(QChar('/'))) {
			Cyberiada::ActionType type;
			QString trigger, guard, behaviour;
			if (!parseActionText(text, true, &type, &trigger, &guard, &behaviour, error)) return false;
			if (type != Cyberiada::actionTransition) {
				*error = "entry/exit actions are not allowed on transitions";
				return false;
			}
			action = Cyberiada::Action(trigger.toStdString(), guard.toStdString(), behaviour.toStdString());
		}
		return model->newTransition(sm, Cyberiada::transitionExternal, source, target, action) != NULL;
	} else if (cmd == "new-sm") {
		// the state machine has no parent; the coordinates are optional and
		// precede the name (like new-state), the machine of a from-scratch
		// session which otherwise has no verb
		Cyberiada::Rect r;
		int name_from = 1;
		if (toNumbers(tokens, 1, 4, v)) {
			r = Cyberiada::Rect(v[0], v[1], v[2], v[3]);
			name_from = 5;
		}
		QString name = restOfLine(tokens, name_from);
		if (name.isEmpty()) { *error = "new-sm requires a name"; return false; }
		return model->newStateMachine(name.toStdString(), r) != NULL;
	} else if (cmd == "undo") {
		model->undoStack()->undo();
		return true;
	} else if (cmd == "redo") {
		model->undoStack()->redo();
		return true;
	} else if (cmd == "update-meta") {
		if (tokens.size() < 3) { *error = "update-meta requires <parameter> <value>"; return false; }
		QString param = tokens.at(1);
		QString value = restOfLine(tokens, 2);
		if ((param == "transitionOrder" && value != "actionFirst" && value != "transitionFirst" &&
			 value != "exitFirst" && value != METAINFORMATION_VALUE_NONE) ||
			(param == "eventPropagation" && value != "propagate" && value != "block" &&
			 value != METAINFORMATION_VALUE_NONE)) {
			*error = "invalid " + param + " value '" + value + "'";
			return false;
		}
		return model->updateMetainformation(model->documentIndex(), param, value);
	}

	// the remaining commands address an existing element by id
	if (cmd != "rename" && cmd != "move" && cmd != "reparent" && cmd != "delete" &&
		cmd != "new-action" && cmd != "update-action" && cmd != "delete-action" &&
		cmd != "update-comment" && cmd != "update-id" && cmd != "polyline" &&
		cmd != "label" &&
		cmd != "new-subject" && cmd != "delete-subject") {
		*error = "unknown command '" + cmd + "'";
		return false;
	}
	if (tokens.size() < 2) { *error = cmd + " requires an element id"; return false; }
	Cyberiada::Element* element = model->idToElement(tokens.at(1));
	if (!element) { *error = "unknown element id '" + tokens.at(1) + "'"; return false; }
	QModelIndex index = model->elementToIndex(element);

	if (cmd == "rename") {
		QString title = restOfLine(tokens, 2);
		if (title.isEmpty()) { *error = "rename requires a title"; return false; }
		return model->updateTitle(index, title);
	} else if (cmd == "move") {
		if (toNumbers(tokens, 2, 4, v)) {
			if (!model->updateGeometry(index, Cyberiada::Rect(v[0], v[1], v[2], v[3]))) return false;
			model->growToFitChildren(element);
			return true;
		}
		if (element->get_type() == Cyberiada::elementChoice) {
			*error = "the choice requires <x y w h>";
			return false;
		}
		if (toNumbers(tokens, 2, 2, v)) {
			if (!model->updateGeometry(index, Cyberiada::Point(v[0], v[1]))) return false;
			model->growToFitChildren(element);
			return true;
		}
		*error = "move requires <x y> or <x y w h>";
		return false;
	} else if (cmd == "reparent") {
		if (tokens.size() != 3) { *error = "reparent requires <id> <new-parent-id>"; return false; }
		if (!model->idToElement(tokens.at(2))) {
			*error = "unknown parent id '" + tokens.at(2) + "'";
			return false;
		}
		return model->updateParent(index, tokens.at(2).toStdString());
	} else if (cmd == "delete") {
		return model->deleteElement(index);
	} else if (cmd == "update-comment") {
		if (element->get_type() != Cyberiada::elementComment &&
			element->get_type() != Cyberiada::elementFormalComment) {
			*error = "element '" + tokens.at(1) + "' is not a comment";
			return false;
		}
		QString body = restOfLine(tokens, 2);
		if (body.isEmpty()) { *error = "update-comment requires a body"; return false; }
		return model->updateCommentBody(index, body);
	} else if (cmd == "update-id") {
		if (tokens.size() != 3) { *error = "update-id requires <id> <new-id>"; return false; }
		if (model->idToElement(tokens.at(2))) {
			*error = "id '" + tokens.at(2) + "' is already used";
			return false;
		}
		return model->updateID(index, tokens.at(2));
	} else if (cmd == "label") {
		if (element->get_type() != Cyberiada::elementTransition) {
			*error = "element '" + tokens.at(1) + "' is not a transition";
			return false;
		}
		// with coordinates the label is pinned, without them it is reset to
		// the automatic placement (an invalid point)
		Cyberiada::Point p;
		if (tokens.size() > 2) {
			if (!toNumbers(tokens, 2, 2, v)) { *error = "label requires <x y> or no coordinates"; return false; }
			p = Cyberiada::Point(v[0], v[1]);
		}
		return model->updateLabel(index, p);
	} else if (cmd == "polyline") {
		if (element->get_type() != Cyberiada::elementTransition) {
			*error = "element '" + tokens.at(1) + "' is not a transition";
			return false;
		}
		int count = tokens.size() - 2;
		if (count % 2 != 0) { *error = "polyline requires x y coordinate pairs"; return false; }
		Cyberiada::Polyline pl;
		for (int i = 0; i < count; i += 2) {
			if (!toNumbers(tokens, 2 + i, 2, v)) {
				*error = "polyline requires numeric coordinates";
				return false;
			}
			pl.push_back(Cyberiada::Point(v[0], v[1]));
		}
		return model->updateGeometry(index, pl);
	} else if (cmd == "new-subject" || cmd == "delete-subject") {
		if (element->get_type() != Cyberiada::elementComment &&
			element->get_type() != Cyberiada::elementFormalComment) {
			*error = "element '" + tokens.at(1) + "' is not a comment";
			return false;
		}
		if (cmd == "delete-subject") {
			bool index_ok = false;
			int subject_index = 0;
			if (tokens.size() > 2) subject_index = tokens.at(2).toInt(&index_ok);
			if (!index_ok) { *error = "delete-subject requires a subject index"; return false; }
			const Cyberiada::Comment* comment = static_cast<const Cyberiada::Comment*>(element);
			if (subject_index < 0 ||
				(size_t)subject_index >= comment->get_subjects().size()) {
				*error = QString("subject index %1 out of range").arg(subject_index);
				return false;
			}
			return model->deleteCommentSubject(index, subject_index);
		}
		if (tokens.size() < 3) { *error = "new-subject requires <comment> <target>"; return false; }
		Cyberiada::Element* target = model->idToElement(tokens.at(2));
		if (!target) { *error = "unknown element id '" + tokens.at(2) + "'"; return false; }
		if (target->get_type() == Cyberiada::elementRoot ||
			target->get_type() == Cyberiada::elementSM) {
			*error = "'" + tokens.at(2) + "' cannot be the subject of a comment";
			return false;
		}
		Cyberiada::CommentSubjectType type = Cyberiada::commentSubjectElement;
		QString fragment;
		if (tokens.size() > 3) {
			if (tokens.at(3) == "name") type = Cyberiada::commentSubjectName;
			else if (tokens.at(3) == "data") type = Cyberiada::commentSubjectData;
			else { *error = "the subject type must be 'name' or 'data'"; return false; }
			fragment = restOfLine(tokens, 4);
			if (fragment.isEmpty()) { *error = "the subject fragment is required"; return false; }
		}
		return model->newCommentSubject(index, target, type, fragment);
	} else if (cmd == "new-action" || cmd == "update-action" || cmd == "delete-action") {
		bool is_state = element->get_type() == Cyberiada::elementSimpleState ||
			element->get_type() == Cyberiada::elementCompositeState;
		bool is_transition = element->get_type() == Cyberiada::elementTransition;
		if (!is_state && !is_transition) {
			*error = "element '" + tokens.at(1) + "' cannot have actions";
			return false;
		}
		const Cyberiada::State* state = is_state ? static_cast<const Cyberiada::State*>(element) : NULL;
		Cyberiada::Transition* trans = is_transition ? static_cast<Cyberiada::Transition*>(element) : NULL;

		// new-action appends; the other commands address the action by its
		// 0-based index in the state's action list (transitions hold a
		// single action, their index must be 0)
		int action_index = 0;
		int text_from = 2;
		if (cmd != "new-action") {
			bool index_ok = false;
			if (tokens.size() > 2) action_index = tokens.at(2).toInt(&index_ok);
			if (!index_ok) { *error = cmd + " requires an action index"; return false; }
			text_from = 3;
			if (is_state &&
				(action_index < 0 || (size_t)action_index >= state->get_actions().size())) {
				*error = QString("action index %1 out of range").arg(action_index);
				return false;
			}
			if (is_transition && action_index != 0) {
				*error = "a transition has a single action with index 0";
				return false;
			}
		}

		if (cmd == "delete-action") {
			if (is_transition && !trans->has_action()) { *error = "the transition has no action"; return false; }
			return model->deleteAction(index, action_index);
		}

		QString text = restOfLine(tokens, text_from);
		if (text.isEmpty()) { *error = cmd + " requires the action text"; return false; }
		Cyberiada::ActionType type;
		QString trigger, guard, behaviour;
		if (!parseActionText(text, is_transition, &type, &trigger, &guard, &behaviour, error)) return false;
		if (is_transition && type != Cyberiada::actionTransition) {
			*error = "entry/exit actions are not allowed on transitions";
			return false;
		}
		if (cmd == "new-action") {
			if (is_transition && trans->has_action()) {
				*error = "the transition already has an action";
				return false;
			}
			return model->newAction(index, type, trigger, guard, behaviour);
		}
		if (is_state && state->get_actions().at(action_index).get_type() != type) {
			*error = QString("the action text does not match the type of action %1").arg(action_index);
			return false;
		}
		return model->updateAction(index, action_index, trigger, guard, behaviour);
	}

	*error = "unknown command '" + cmd + "'";
	return false;
}

// the mouse state of a script: the gestures go through the scene as a view
// would send them, one left button, between a press and a release
struct GestureState {
	bool activated = false;
	bool pressed = false;
	Qt::KeyboardModifiers mods = Qt::NoModifier;
};

static bool isGesture(const QString& cmd)
{
	return cmd == "press" || cmd == "drag" || cmd == "release" ||
		cmd == "click" || cmd == "double-click" || cmd == "tool" ||
		cmd == "type" || cmd == "key" || cmd == "select-all" || cmd == "commit" ||
		cmd == "edit-text" || cmd == "edit";
}

static bool parseModifiers(const QStringList& tokens, int from,
						   Qt::KeyboardModifiers* mods, QString* error);

// the text item in edit mode, if any
static EditableTextItem* editingItem(CyberiadaSMEditorScene* scene)
{
	EditableTextItem* text = dynamic_cast<EditableTextItem*>(scene->focusItem());
	if (text && (text->textInteractionFlags() & Qt::TextEditorInteraction)) return text;
	return NULL;
}

// a key press and release delivered through the scene to the focus item
static void sendKey(CyberiadaSMEditorScene* scene, int key, Qt::KeyboardModifiers mods,
					const QString& text = QString())
{
	QKeyEvent press(QEvent::KeyPress, key, mods, text);
	QApplication::sendEvent(scene, &press);
	QKeyEvent release(QEvent::KeyRelease, key, mods, text);
	QApplication::sendEvent(scene, &release);
}

static void typeText(CyberiadaSMEditorScene* scene, const QString& text)
{
	for (int i = 0; i < text.length(); i++) {
		QChar c = text.at(i);
		if (c == QChar('\n')) sendKey(scene, Qt::Key_Return, Qt::NoModifier, "\r");
		else sendKey(scene, 0, Qt::NoModifier, QString(c));
	}
}

static bool keyByName(const QString& name, int* key, QString* text, QString* error)
{
	static const struct { const char* name; int key; } names[] = {
		{"return", Qt::Key_Return}, {"enter", Qt::Key_Enter}, {"escape", Qt::Key_Escape},
		{"tab", Qt::Key_Tab}, {"backspace", Qt::Key_Backspace}, {"delete", Qt::Key_Delete},
		{"left", Qt::Key_Left}, {"right", Qt::Key_Right}, {"up", Qt::Key_Up},
		{"down", Qt::Key_Down}, {"home", Qt::Key_Home}, {"end", Qt::Key_End},
		{"space", Qt::Key_Space},
	};
	text->clear();
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		if (name == names[i].name) { *key = names[i].key; return true; }
	}
	if (name.length() == 1) {
		*key = name.toUpper().at(0).unicode();
		*text = name;
		return true;
	}
	*error = "unknown key '" + name + "'";
	return false;
}

// the text item of an element by role: title, action <i>, label, body
static EditableTextItem* textItemByRole(CyberiadaSMEditorScene* scene, const QString& id,
										const QString& role, int index, QString* error)
{
	QGraphicsItem* item = scene->getMap().value(Cyberiada::ID(id.toStdString()), NULL);
	if (!item) { *error = "unknown id '" + id + "'"; return NULL; }
	if (!SettingsManager::instance().getShowText()) {
		*error = "the text is hidden: the text verbs need --text";
		return NULL;
	}
	std::vector<EditableTextItem*> texts = textItemsOf(item);
	int n = 0;
	for (size_t i = 0; i < texts.size(); i++) {
		FontRole fr = texts[i]->getFontRole();
		bool match = (role == "title" && fr == fontRoleStateTitle) ||
			(role == "action" && fr == fontRoleStateAction) ||
			(role == "label" && fr == fontRoleTransition) ||
			(role == "body" && (fr == fontRoleComment || fr == fontRoleFormalComment));
		if (!match) continue;
		if (n == index) return texts[i];
		n++;
	}
	*error = QString("%1 has no %2 text %3").arg(id, role).arg(index);
	return NULL;
}

// open the inline editor of an element's text by role and select the editable
// part; *from is the token index where an edit-text value would start
static EditableTextItem* openTextItem(CyberiadaSMEditorScene* scene, const QStringList& tokens,
									  int* from, QString* error)
{
	if (tokens.size() < 3) { *error = tokens.first() + " requires <id> <role> [<i>]"; return NULL; }
	const QString& role = tokens.at(2);
	int index = 0;
	*from = 3;
	if (role == "action") {
		bool ok = false;
		if (tokens.size() > 3) index = tokens.at(3).toInt(&ok);
		if (!ok) { *error = "the action role requires the action index"; return NULL; }
		*from = 4;
	} else if (role != "title" && role != "label" && role != "body") {
		*error = "unknown text role '" + role + "'";
		return NULL;
	}
	EditableTextItem* text = textItemByRole(scene, tokens.at(1), role, index, error);
	if (!text) return NULL;
	text->startEditing();
	QTextCursor cursor = text->textCursor();
	cursor.setPosition(text->protectedLength());
	cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
	text->setTextCursor(cursor);
	return text;
}

static bool runTextVerb(CyberiadaSMEditorScene* scene, const QStringList& tokens, QString* error)
{
	const QString& cmd = tokens.first();
	if (cmd == "commit") {
		EditableTextItem* text = editingItem(scene);
		if (!text) { *error = "no text is being edited"; return false; }
		text->clearFocus();
		return true;
	}
	if (cmd == "edit" || cmd == "edit-text") {
		int from;
		EditableTextItem* text = openTextItem(scene, tokens, &from, error);
		if (!text) return false;
		if (cmd == "edit") return true;   // leave the editor open for the keystroke verbs
		QString value = restOfLine(tokens, from);
		if (value.isEmpty()) sendKey(scene, Qt::Key_Delete, Qt::NoModifier);
		else typeText(scene, value);
		text->clearFocus();
		return true;
	}
	if (cmd == "key") {
		// a key reaches the scene focus item: a text editor or a focused dot
		if (tokens.size() < 2) { *error = "key requires a name"; return false; }
		int key;
		QString text;
		if (!keyByName(tokens.at(1), &key, &text, error)) return false;
		Qt::KeyboardModifiers mods;
		if (!parseModifiers(tokens, 2, &mods, error)) return false;
		if (mods & Qt::ControlModifier) text.clear();
		sendKey(scene, key, mods, text);
		return true;
	}
	// type and select-all drive the text editor in progress
	if (!editingItem(scene)) { *error = "no text is being edited"; return false; }
	if (cmd == "type") {
		typeText(scene, restOfLine(tokens, 1));
		return true;
	}
	if (cmd == "select-all") {
		sendKey(scene, Qt::Key_A, Qt::ControlModifier, "a");
		return true;
	}
	*error = "unknown text verb '" + cmd + "'";
	return false;
}

static bool parseModifiers(const QStringList& tokens, int from,
						   Qt::KeyboardModifiers* mods, QString* error)
{
	*mods = Qt::NoModifier;
	for (int i = from; i < tokens.size(); i++) {
		const QString& m = tokens.at(i);
		if (m == "ctrl") *mods |= Qt::ControlModifier;
		else if (m == "shift") *mods |= Qt::ShiftModifier;
		else if (m == "alt") *mods |= Qt::AltModifier;
		else { *error = "unknown modifier '" + m + "'"; return false; }
	}
	return true;
}

static void sendMouse(QGraphicsScene* scene, QEvent::Type type, const QPointF& pos,
					  Qt::MouseButtons buttons, Qt::KeyboardModifiers mods)
{
	QGraphicsSceneMouseEvent event(type);
	event.setScenePos(pos);
	event.setScreenPos(pos.toPoint());
	event.setButton(Qt::LeftButton);
	event.setButtons(buttons);
	event.setModifiers(mods);
	QApplication::sendEvent(scene, &event);
}

static bool runGesture(CyberiadaSMEditorScene* scene, const QStringList& tokens,
					   GestureState* state, QString* error)
{
	const QString& cmd = tokens.first();
	if (!state->activated) {
		// the scene handles the gestures of an active window only
		QEvent activate(QEvent::WindowActivate);
		QApplication::sendEvent(scene, &activate);
		state->activated = true;
	}
	if (cmd == "type" || cmd == "key" || cmd == "select-all" || cmd == "commit" ||
		cmd == "edit-text" || cmd == "edit") {
		return runTextVerb(scene, tokens, error);
	}
	if (cmd == "tool") {
		if (tokens.size() != 2) { *error = "tool requires a name"; return false; }
		if (tokens.at(1) == "select") scene->setCurrentTool(ToolType::Select);
		else if (tokens.at(1) == "transition") scene->setCurrentTool(ToolType::Transition);
		else { *error = "unknown tool '" + tokens.at(1) + "'"; return false; }
		return true;
	}
	double v[2];
	if (!toNumbers(tokens, 1, 2, v)) { *error = cmd + " requires the x y coordinates"; return false; }
	QPointF pos(v[0], v[1]);
	if (cmd == "drag" || cmd == "release") {
		if (tokens.size() != 3) { *error = cmd + " takes the coordinates only"; return false; }
		if (!state->pressed) { *error = cmd + " without a press"; return false; }
		if (cmd == "drag") {
			sendMouse(scene, QEvent::GraphicsSceneMouseMove, pos, Qt::LeftButton, state->mods);
		} else {
			sendMouse(scene, QEvent::GraphicsSceneMouseRelease, pos, Qt::NoButton, state->mods);
			state->pressed = false;
		}
		return true;
	}
	Qt::KeyboardModifiers mods;
	if (!parseModifiers(tokens, 3, &mods, error)) return false;
	if (cmd == "double-click") {
		sendMouse(scene, QEvent::GraphicsSceneMouseDoubleClick, pos, Qt::LeftButton, mods);
		return true;
	}
	if (state->pressed) { *error = cmd + " while the button is pressed"; return false; }
	sendMouse(scene, QEvent::GraphicsSceneMousePress, pos, Qt::LeftButton, mods);
	if (cmd == "press") {
		state->pressed = true;
		state->mods = mods;
	} else {
		sendMouse(scene, QEvent::GraphicsSceneMouseRelease, pos, Qt::NoButton, mods);
	}
	return true;
}

bool runEditScript(CyberiadaSMEditorWindow* win, const QString& path, QString* error)
{
	CyberiadaSMModel* model = win->getModel();
	CyberiadaSMEditorScene* scene = win->getScene();
	GestureState gesture;
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		*error = "cannot open script " + path;
		return false;
	}
	QTextStream in(&file);
	int lineno = 0;
	while (!in.atEnd()) {
		QString line = in.readLine();
		lineno++;
		QString trimmed = line.trimmed();
		if (trimmed.isEmpty() || trimmed.startsWith("#")) continue;
		QStringList tokens = trimmed.split(QRegularExpression("\\s+"));
		QString message;
		bool ok = false;
		// every command is one undo step; undo/redo themselves are not, and
		// a gesture is bracketed by the scene between the press and the release
		const QString& cmd = tokens.first();
		bool step = cmd != "undo" && cmd != "redo" && !isGesture(cmd);
		if (step) model->beginUndoStep(cmd);
		try {
			if (isGesture(cmd)) {
				ok = runGesture(scene, tokens, &gesture, &message);
			} else if (cmd == "delete-selected") {
				if (scene->selectedItems().isEmpty()) {
					message = "nothing is selected";
				} else {
					win->actionDeleteElement->trigger();
					ok = true;
				}
			} else {
				ok = runCommand(model, tokens, &message);
			}
			if (!ok && message.isEmpty()) {
				message = "command failed";
			}
		} catch (const Cyberiada::Exception& e) {
			message = QString(e.str().c_str());
		}
		if (step) model->endUndoStep();
		if (!ok) {
			*error = QString("line %1: %2").arg(lineno).arg(message);
			return false;
		}
	}
	if (gesture.pressed) {
		*error = QString("line %1: the script ends with the button pressed").arg(lineno);
		return false;
	}
	return true;
}
