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

#include "subject_fragment.h"

using namespace SubjectFragment;

static bool isState(const Cyberiada::Element* e)
{
    return e->get_type() == Cyberiada::elementSimpleState ||
        e->get_type() == Cyberiada::elementCompositeState ||
        e->get_type() == Cyberiada::elementSubmachineState;
}

static void addField(QVector<Field>& list, FieldKind kind, int action, const std::string& text)
{
    if (text.empty()) return;
    Field f;
    f.kind = kind;
    f.action = action;
    f.text = QString::fromStdString(text);
    list.append(f);
}

static void addActionFields(QVector<Field>& list, const Cyberiada::Action& a, int action)
{
    // entry/exit carry a behaviour only: their keyword is no field
    if (a.get_type() == Cyberiada::actionTransition) {
        addField(list, fieldTrigger, action, a.get_trigger());
        addField(list, fieldGuard, action, a.get_guard());
    }
    addField(list, fieldBehaviour, action, a.get_behavior());
}

QVector<Field> SubjectFragment::fields(const Cyberiada::Element* target, Cyberiada::CommentSubjectType type)
{
    QVector<Field> list;
    if (!target || type == Cyberiada::commentSubjectElement) return list;
    if (target->get_type() == Cyberiada::elementTransition) {
        // a transition has no name of its own: both kinds look into its label
        const Cyberiada::Transition* t = static_cast<const Cyberiada::Transition*>(target);
        if (t->has_action()) addActionFields(list, t->get_action(), -1);
        return list;
    }
    if (type == Cyberiada::commentSubjectName) {
        if (target->has_name()) addField(list, fieldName, -1, target->get_name());
        return list;
    }
    if (isState(target)) {
        const std::vector<Cyberiada::Action>& actions = static_cast<const Cyberiada::State*>(target)->get_actions();
        for (size_t i = 0; i < actions.size(); i++) addActionFields(list, actions[i], int(i));
    } else if (Cyberiada::element_is_comment(target)) {
        addField(list, fieldBody, -1, static_cast<const Cyberiada::Comment*>(target)->get_body());
    }
    return list;
}

Place SubjectFragment::locate(const Cyberiada::Element* target, Cyberiada::CommentSubjectType type,
                              const QString& fragment)
{
    Place place;
    if (fragment.isEmpty()) return place;
    for (const Field& f : fields(target, type)) {
        int offset = f.text.indexOf(fragment);
        if (offset >= 0) {
            place.field = f;
            place.offset = offset;
            break;
        }
    }
    return place;
}

QString SubjectFragment::defaultFragment(const Cyberiada::Element* target, Cyberiada::CommentSubjectType type)
{
    QVector<Field> list = fields(target, type);
    return list.isEmpty() ? QString() : list.first().text;
}

int DrawnAction::start(FieldKind kind) const
{
    switch (kind) {
    case fieldTrigger:   return trigger;
    case fieldGuard:     return guard;
    case fieldBehaviour: return behaviour;
    default:             return -1;
    }
}

DrawnAction SubjectFragment::stateAction(const Cyberiada::Action& action)
{
    DrawnAction d;
    QString behaviour = QString::fromStdString(action.get_behavior());
    // a single-line behaviour follows the header inline; a multi-line one starts
    // on its own line under it (EDIT-TEXT-2)
    bool ownLine = behaviour.contains('\n');
    if (action.get_type() == Cyberiada::actionTransition) {
        d.trigger = 0;
        d.text = QString::fromStdString(action.get_trigger());
        QString guard = QString::fromStdString(action.get_guard());
        if (!guard.isEmpty()) {
            d.text += " [";
            d.guard = d.text.length();
            d.text += guard + "]";
        }
        // a deferred event reads as EVENT / defer (EDIT-TEXT-6): no behaviour drawn
        if (action.get_propagation() == Cyberiada::eventPropagationDefer) {
            d.text += " / defer";
        } else if (!behaviour.isEmpty()) {
            d.text += ownLine ? " /\n" : " / ";
            d.behaviour = d.text.length();
            d.text += behaviour;
        }
        return d;
    }
    // TODO "exit", "entry" and "/" are constants from cyberiadamlpp
    if (action.get_type() == Cyberiada::actionEntry) d.text = "entry";
    else if (action.get_type() == Cyberiada::actionExit) d.text = "exit";
    if (!d.text.isEmpty()) d.text += ownLine ? "/\n" : "/ ";
    d.behaviour = d.text.length();
    d.text += behaviour;
    return d;
}

DrawnAction SubjectFragment::transitionLabel(const Cyberiada::Action& action)
{
    DrawnAction d;
    auto part = [&d](const QString& lead, const std::string& text, const QString& tail) {
        if (!d.text.isEmpty()) d.text += " ";
        d.text += lead;
        int start = d.text.length();
        d.text += QString::fromStdString(text) + tail;
        return start;
    };
    if (action.has_trigger()) d.trigger = part(QString(), action.get_trigger(), QString());
    if (action.has_guard()) d.guard = part("[", action.get_guard(), "]");
    if (action.has_behavior()) d.behaviour = part("/ ", action.get_behavior(), QString());
    return d;
}
