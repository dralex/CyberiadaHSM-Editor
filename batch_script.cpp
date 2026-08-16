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

#include "batch_script.h"
#include "cyberiadasm_model.h"

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
	QStringList rest = tokens.mid(from);
	return rest.join(" ");
}

static bool runCommand(CyberiadaSMModel* model, const QStringList& tokens, QString* error)
{
	const QString& cmd = tokens.first();
	double v[4];

	if (cmd == "new-state" || cmd == "new-comment" ||
		cmd == "new-initial" || cmd == "new-final") {
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
		} else if (cmd == "new-comment") {
			QString body = restOfLine(tokens, 2);
			if (body.isEmpty()) { *error = "new-comment requires a body"; return false; }
			return model->newComment(collection, body.toStdString()) != NULL;
		} else {
			Cyberiada::Point p;
			if (toNumbers(tokens, 2, 2, v)) {
				p = Cyberiada::Point(v[0], v[1]);
			}
			if (cmd == "new-initial") {
				return model->newInitial(collection, p) != NULL;
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
		Cyberiada::Action action(restOfLine(tokens, 4).toStdString());
		return model->newTransition(sm, Cyberiada::transitionExternal, source, target, action) != NULL;
	}

	// the remaining commands address an existing element by id
	if (cmd != "rename" && cmd != "move" && cmd != "reparent" && cmd != "delete") {
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
	}

	*error = "unknown command '" + cmd + "'";
	return false;
}

bool runEditScript(CyberiadaSMModel* model, const QString& path, QString* error)
{
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
		try {
			ok = runCommand(model, tokens, &message);
			if (!ok && message.isEmpty()) {
				message = "command failed";
			}
		} catch (const Cyberiada::Exception& e) {
			message = QString(e.str().c_str());
		}
		if (!ok) {
			*error = QString("line %1: %2").arg(lineno).arg(message);
			return false;
		}
	}
	return true;
}
