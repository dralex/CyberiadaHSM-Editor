/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Menu of a Metainformation Choice (Edit > Language, Edit > Markup)
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

#include <QAction>
#include <QActionGroup>
#include <QMenu>

#include "meta_choice_menu.h"

MetaChoiceMenu::MetaChoiceMenu(QMenu* menu, const QList<Entry>& entries, Resolver resolve,
                               QObject* parent):
    QObject(parent), menu(menu), resolve(resolve)
{
    group = new QActionGroup(this);
    group->setExclusive(true);
    for (const Entry& entry : entries) {
        addEntry(entry.text, entry.value);
    }
    separator = menu->addSeparator();
    separator->setVisible(false);
    connect(group, &QActionGroup::triggered, this, &MetaChoiceMenu::onTriggered);
}

QAction* MetaChoiceMenu::addEntry(const QString& text, const QString& value)
{
    QAction* action = menu->addAction(text);
    action->setCheckable(true);
    action->setData(value);
    group->addAction(action);
    return action;
}

void MetaChoiceMenu::noteUnknown(const QString& value)
{
    bool known = false;
    resolve(value, &known);
    if (value.trimmed().isEmpty() || known || sessionValues.contains(value)) return;
    sessionValues.append(value);
}

void MetaChoiceMenu::sync(const QString& value)
{
    currentValue = value;
    bool known = false;
    QString key = resolve(value, &known);
    if (!known) key = value;

    // the session entries and the transient one are rebuilt; the triggered
    // action may be among them, so it is released later
    for (QAction* action : extraActions) {
        group->removeAction(action);
        menu->removeAction(action);
        action->deleteLater();
    }
    extraActions.clear();
    QStringList extras = sessionValues;
    if (!known && !extras.contains(value)) extras.append(value);
    for (const QString& extra : extras) {
        extraActions.append(addEntry(tr("%1 (no highlighting)").arg(extra), extra));
    }
    separator->setVisible(!extras.isEmpty());

    const QList<QAction*> actions = group->actions();
    for (QAction* action : actions) {
        if (action->data().toString() == key) {
            action->setChecked(true);
            break;
        }
    }
}

void MetaChoiceMenu::setEnabled(bool on)
{
    menu->menuAction()->setEnabled(on);
}

void MetaChoiceMenu::onTriggered(QAction* action)
{
    QString value = action->data().toString();
    bool known = false;
    QString key = resolve(currentValue, &known);
    if (!known) key = currentValue;
    // the checked entry again (an alias included) changes nothing
    if (value == key) return;
    // a replaced unknown value stays selectable (EDIT-TEXT-9)
    noteUnknown(currentValue);
    emit chosen(value);
}
