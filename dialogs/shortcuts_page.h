/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Shortcuts page of the Preferences
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

#ifndef CYBERIADA_SHORTCUTS_PAGE
#define CYBERIADA_SHORTCUTS_PAGE

#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QWidget>

class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class ShortcutMap;

// The keys of the actions by their menus (EDIT-KEYS-2/3, docs/SHORTCUTS.md): the page
// edits a copy, apply() gives it to the map
class ShortcutsPage: public QWidget {
Q_OBJECT
public:
    ShortcutsPage(ShortcutMap* map, QWidget* parent = nullptr);

    // the keys of the map shown, the edits dropped
    void                load();
    void                apply();
    // the keys of the copy
    QList<QKeySequence> keys(const QString& id) const { return m_keys.value(id); }
    // the row of an action made current; false for an unknown one
    bool                select(const QString& id);

private slots:
    void                filter(const QString& text);
    void                rowChanged();
    void                keyChanged();
    void                assign();
    void                add();
    void                clear();
    void                reset();
    void                resetAll();

private:
    QString             currentId() const;
    QString             name(const QString& id) const;
    // the key joins or replaces the keys of the current row; another holder is asked for
    void                take(bool replace);
    // a key of the row is no more a key of any other one
    void                release(const QKeySequence& key, const QString& except);
    void                showRow(const QString& id);

    ShortcutMap*                          m_map;
    QHash<QString, QList<QKeySequence> >  m_keys;
    QHash<QString, QTreeWidgetItem*>      m_rows;
    QLineEdit*                            m_filter;
    QTreeWidget*                          m_tree;
    QKeySequenceEdit*                     m_key;
    QLabel*                               m_holder;
    QPushButton*                          m_assign;
    QPushButton*                          m_add;
    QPushButton*                          m_clear;
    QPushButton*                          m_reset;
};

#endif
