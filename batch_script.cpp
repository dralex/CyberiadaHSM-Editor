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

#include "batch_script.h"
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"
#include "smeditor_window.h"
#include "cyberiada_constants.h"

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
			return model->updateGeometry(index, Cyberiada::Rect(v[0], v[1], v[2], v[3]));
		}
		if (element->get_type() == Cyberiada::elementChoice) {
			*error = "the choice requires <x y w h>";
			return false;
		}
		if (toNumbers(tokens, 2, 2, v)) {
			return model->updateGeometry(index, Cyberiada::Point(v[0], v[1]));
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
		cmd == "click" || cmd == "double-click" || cmd == "tool";
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
