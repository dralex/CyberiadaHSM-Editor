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

#include <QAction>
#include "shortcut_map.h"
#include "settings_manager.h"

namespace {

struct DefaultKeys {
    const char*                 id;
    // the keys as portable text, then the platform keys of the standard one
    const char*                 keys;
    QKeySequence::StandardKey   standard;
};

const DefaultKeys DEFAULT_KEYS[] = {
    { "actionNew",              "Ctrl+N",               QKeySequence::UnknownKey },
    { "actionOpen",             "Ctrl+O",               QKeySequence::UnknownKey },
    { "actionSave",             "Ctrl+S",               QKeySequence::UnknownKey },
    { "actionSaveAs",           "Ctrl+Shift+S",         QKeySequence::UnknownKey },
    { "actionExport",           "Ctrl+E",               QKeySequence::UnknownKey },
    { "actionClose",            "Ctrl+W",               QKeySequence::UnknownKey },
    { "actionExit",             "Ctrl+Q",               QKeySequence::UnknownKey },
    { "actionUndo",             "Ctrl+Z",               QKeySequence::UnknownKey },
    { "actionRedo",             "Ctrl+Shift+Z; Ctrl+Y", QKeySequence::UnknownKey },
    { "actionCut",              "Ctrl+X",               QKeySequence::UnknownKey },
    { "actionCopy",             "Ctrl+C",               QKeySequence::UnknownKey },
    { "actionPaste",            "Ctrl+V",               QKeySequence::UnknownKey },
    { "actionDeleteElement",    "Del",                  QKeySequence::UnknownKey },
    { "actionFind",             "",                     QKeySequence::Find },
    { "actionFindNext",         "F3",                   QKeySequence::FindNext },
    { "actionFindPrevious",     "Shift+F3",             QKeySequence::FindPrevious },
    { "actionGoToLine",         "Alt+G",                QKeySequence::UnknownKey },
    { "actionSelectTool",       "Esc",                  QKeySequence::UnknownKey },
    { "actionPan",              "H",                    QKeySequence::UnknownKey },
    { "actionZoomTool",         "Z",                    QKeySequence::UnknownKey },
    { "actionNewState",         "S",                    QKeySequence::UnknownKey },
    { "actionNewTransition",    "T",                    QKeySequence::UnknownKey },
    { "actionNewComment",       "C",                    QKeySequence::UnknownKey },
    { "actionNewFormalComment", "Shift+C",              QKeySequence::UnknownKey },
    { "actionNewInitial",       "I",                    QKeySequence::UnknownKey },
    { "actionNewFinal",         "F",                    QKeySequence::UnknownKey },
    { "actionNewChoise",        "X",                    QKeySequence::UnknownKey },
    { "actionZoomIn",           "Ctrl++; Ctrl+=",       QKeySequence::UnknownKey },
    { "actionZoomOut",          "Ctrl+-",               QKeySequence::UnknownKey },
    { "actionFitContent",       "Ctrl+0",               QKeySequence::UnknownKey },
    { "actionGridVisibility",   "Ctrl+G",               QKeySequence::UnknownKey },
    { "actionSnapMode",         "Ctrl+Shift+G",         QKeySequence::UnknownKey },
    { "actionNextDocument",     "",                     QKeySequence::NextChild },
    { "actionPreviousDocument", "",                     QKeySequence::PreviousChild },
    { "actionPreferences",      "Ctrl+I",               QKeySequence::UnknownKey },
    { "actionDocumentProperties", "Alt+Return",         QKeySequence::UnknownKey },
    { "actionCheck",            "F7",                   QKeySequence::UnknownKey },
    { "actionSessionStep",      "F10",                  QKeySequence::UnknownKey },
    { "actionSessionRun",       "F5",                   QKeySequence::UnknownKey },
    { "actionSessionPause",     "F6",                   QKeySequence::UnknownKey },
    { "actionSessionStop",      "Shift+F5",             QKeySequence::UnknownKey },
};

const size_t DEFAULT_KEYS_COUNT = sizeof(DEFAULT_KEYS) / sizeof(DEFAULT_KEYS[0]);

// a key some row of the table writes
bool writtenKey(const QKeySequence& key)
{
    for (size_t i = 0; i < DEFAULT_KEYS_COUNT; i++) {
        if (shortcutKeysOf(DEFAULT_KEYS[i].keys).contains(key)) return true;
    }
    return false;
}

} // namespace

QString shortcutKeysText(const QList<QKeySequence>& keys)
{
    return QKeySequence::listToString(keys, QKeySequence::PortableText);
}

QList<QKeySequence> shortcutKeysOf(const QString& text)
{
    QList<QKeySequence> keys;
    for (const QKeySequence& key : QKeySequence::listFromString(text, QKeySequence::PortableText)) {
        if (!key.isEmpty() && !keys.contains(key)) keys << key;
    }
    return keys;
}

QList<QKeySequence> defaultShortcutKeys(const QString& id)
{
    QList<QKeySequence> keys;
    for (size_t i = 0; i < DEFAULT_KEYS_COUNT; i++) {
        if (id != DEFAULT_KEYS[i].id) continue;
        keys = shortcutKeysOf(DEFAULT_KEYS[i].keys);
        if (DEFAULT_KEYS[i].standard == QKeySequence::UnknownKey) break;
        for (const QKeySequence& key : QKeySequence::keyBindings(DEFAULT_KEYS[i].standard)) {
            if (!keys.contains(key) && !writtenKey(key)) keys << key;
        }
        break;
    }
    return keys;
}

ShortcutMap::ShortcutMap(QObject* parent):
    QObject(parent)
{
}

void ShortcutMap::add(QAction* action, const QString& path)
{
    QString id = action->objectName();
    if (id.isEmpty() || m_entries.contains(id)) return;
    Entry entry;
    entry.action = action;
    entry.path = path;
    entry.defaults = defaultShortcutKeys(id);
    bool stored = false;
    QString text = SettingsManager::instance().getShortcut(id, &stored);
    entry.keys = stored ? shortcutKeysOf(text) : entry.defaults;
    action->setShortcuts(entry.keys);
    m_order << id;
    m_entries.insert(id, entry);
}

QAction* ShortcutMap::action(const QString& id) const
{
    return m_entries.value(id).action;
}

QString ShortcutMap::path(const QString& id) const
{
    return m_entries.value(id).path;
}

QString ShortcutMap::label(const QString& id) const
{
    QAction* a = action(id);
    if (!a) return QString();
    // "&&" is a written ampersand
    QString text = a->text();
    QString label;
    for (int i = 0; i < text.length(); i++) {
        if (text[i] == '&' && i + 1 < text.length()) i++;
        label += text[i];
    }
    return label;
}

QList<QKeySequence> ShortcutMap::defaultKeys(const QString& id) const
{
    return m_entries.value(id).defaults;
}

QList<QKeySequence> ShortcutMap::keys(const QString& id) const
{
    return m_entries.value(id).keys;
}

bool ShortcutMap::isChanged(const QString& id) const
{
    QHash<QString, Entry>::const_iterator it = m_entries.constFind(id);
    return it != m_entries.constEnd() && it->keys != it->defaults;
}

bool ShortcutMap::apply(const QString& id, const QList<QKeySequence>& keys)
{
    QHash<QString, Entry>::iterator it = m_entries.find(id);
    if (it == m_entries.end() || it->keys == keys) return false;
    it->keys = keys;
    it->action->setShortcuts(keys);
    // only a change is stored (EDIT-KEYS-4)
    if (keys == it->defaults) SettingsManager::instance().removeShortcut(id);
    else SettingsManager::instance().setShortcut(id, shortcutKeysText(keys));
    return true;
}

void ShortcutMap::setKeys(const QString& id, const QList<QKeySequence>& keys)
{
    if (apply(id, keys)) emit changed();
}

void ShortcutMap::reset(const QString& id)
{
    setKeys(id, defaultKeys(id));
}

void ShortcutMap::resetAll()
{
    bool any = false;
    for (const QString& id : m_order) any = apply(id, defaultKeys(id)) || any;
    if (any) emit changed();
}

QString ShortcutMap::holder(const QKeySequence& key, const QString& except) const
{
    for (const QString& id : m_order) {
        if (id != except && m_entries[id].keys.contains(key)) return id;
    }
    return QString();
}
