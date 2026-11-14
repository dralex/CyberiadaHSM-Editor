/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The keys of the window actions
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

#ifndef CYBERIADA_SHORTCUT_MAP
#define CYBERIADA_SHORTCUT_MAP

#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

class QAction;

// the keys as the portable text of the settings, joined by "; "
QString             shortcutKeysText(const QList<QKeySequence>& keys);
QList<QKeySequence> shortcutKeysOf(const QString& text);
// the default keys of an action by its name (docs/SHORTCUTS.md); a key written in
// the table is never also a platform key of another action
QList<QKeySequence> defaultShortcutKeys(const QString& id);

// The keys of the built-in actions (EDIT-KEYS-1..4, docs/SHORTCUTS.md): the default
// ones and the changes of the user, which alone are stored
class ShortcutMap: public QObject {
Q_OBJECT
public:
    explicit ShortcutMap(QObject* parent = nullptr);

    // register the action under its objectName with the stored or the default keys;
    // an unnamed or a known one is skipped
    void                add(QAction* action, const QString& path);
    // in the order of the registration
    QStringList         ids() const { return m_order; }
    bool                contains(const QString& id) const { return m_entries.contains(id); }
    QAction*            action(const QString& id) const;
    // the menus over the action, "Edit > Tools"
    QString             path(const QString& id) const;
    // the text of the action without its mnemonic
    QString             label(const QString& id) const;
    QList<QKeySequence> defaultKeys(const QString& id) const;
    QList<QKeySequence> keys(const QString& id) const;
    bool                isChanged(const QString& id) const;
    void                setKeys(const QString& id, const QList<QKeySequence>& keys);
    void                reset(const QString& id);
    void                resetAll();
    // the action that holds the key, except one; empty when it is free
    QString             holder(const QKeySequence& key, const QString& except = QString()) const;

signals:
    void                changed();

private:
    struct Entry {
        QAction*            action;
        QString             path;
        QList<QKeySequence> defaults;
        QList<QKeySequence> keys;
    };
    bool                apply(const QString& id, const QList<QKeySequence>& keys);

    QStringList          m_order;
    QHash<QString, Entry> m_entries;
};

#endif
