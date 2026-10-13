/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
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

#ifndef META_CHOICE_MENU_H
#define META_CHOICE_MENU_H

#include <functional>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

class QAction;
class QActionGroup;
class QMenu;

// a submenu choosing one metainformation value of the active document
// (EDIT-TEXT-9/12): the fixed entries, then the unknown values met in this
// session and the current unknown value
class MetaChoiceMenu : public QObject {
    Q_OBJECT

public:
    struct Entry {
        QString text;
        QString value;   // empty: the default entry (the parameter removed)
    };
    // the entry value of a document value; known is false for an unknown one
    typedef std::function<QString(const QString& value, bool* known)> Resolver;

    MetaChoiceMenu(QMenu* menu, const QList<Entry>& entries, Resolver resolve,
                   QObject* parent = nullptr);

    // check the entry of the active document's value
    void sync(const QString& value);
    // an unknown value stays in the menu until the editor exits
    void noteUnknown(const QString& value);
    void setEnabled(bool on);

signals:
    // another entry was chosen; the owner writes it to the document
    void chosen(const QString& value);

private slots:
    void onTriggered(QAction* action);

private:
    QAction* addEntry(const QString& text, const QString& value);

    QMenu* menu;
    QActionGroup* group;
    QAction* separator;
    Resolver resolve;
    QString currentValue;
    QStringList sessionValues;
    QList<QAction*> extraActions;
};

#endif // META_CHOICE_MENU_H
