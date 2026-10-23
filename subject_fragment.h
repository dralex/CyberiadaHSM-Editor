/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The fragments of the comment subjects: the fields of a target's text
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

#ifndef CYBERIADA_SUBJECT_FRAGMENT_H
#define CYBERIADA_SUBJECT_FRAGMENT_H

#include <QString>
#include <QVector>

#include <cyberiada/cyberiadamlpp.h>

namespace SubjectFragment {

// a field of a target's text a fragment lies in (EDIT-STRUCT-12)
enum FieldKind {
    fieldName = 0,
    fieldTrigger,
    fieldGuard,
    fieldBehaviour,
    fieldBody
};

struct Field {
    FieldKind kind = fieldName;
    int action = -1;    // the state action; -1 for the name, the body and a transition action
    QString text;
};

// the non-empty fields a fragment of the type may lie in, in the search order:
// a state's name or its actions (event, guard, behaviour each), a transition's
// label either way, a comment's name or body, a vertex's name
QVector<Field> fields(const Cyberiada::Element* target, Cyberiada::CommentSubjectType type);

// the commented occurrence: the first field holding the fragment and the offset in it
struct Place {
    Field field;
    int offset = -1;
    bool found() const { return offset >= 0; }
};
Place locate(const Cyberiada::Element* target, Cyberiada::CommentSubjectType type, const QString& fragment);

// the fragment a new name/data subject starts with: the first field whole
QString defaultFragment(const Cyberiada::Element* target, Cyberiada::CommentSubjectType type);

// the text of an action as drawn and where its fields start in it (-1: not drawn)
struct DrawnAction {
    QString text;
    int trigger = -1;
    int guard = -1;
    int behaviour = -1;
    int start(FieldKind kind) const;
};
// a state action: "entry/ behaviour", "EVENT [guard] / behaviour" (EDIT-TEXT-2/6)
DrawnAction stateAction(const Cyberiada::Action& action);
// a transition label: "EVENT [guard] / behaviour"
DrawnAction transitionLabel(const Cyberiada::Action& action);

}

#endif
