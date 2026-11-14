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

#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include "shortcuts_page.h"
#include "shortcut_map.h"

static const int COLUMN_ACTION = 0;
static const int COLUMN_KEYS = 1;

static QString nativeKeys(const QList<QKeySequence>& keys)
{
    return QKeySequence::listToString(keys, QKeySequence::NativeText);
}

ShortcutsPage::ShortcutsPage(ShortcutMap* map, QWidget* parent):
    QWidget(parent), m_map(map)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    m_filter = new QLineEdit(this);
    m_filter->setObjectName("shortcutFilter");
    m_filter->setPlaceholderText(tr("Filter"));
    m_filter->setClearButtonEnabled(true);
    layout->addWidget(m_filter);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName("shortcutTree");
    m_tree->setHeaderLabels(QStringList() << tr("Action") << tr("Keys"));
    m_tree->header()->setSectionResizeMode(COLUMN_ACTION, QHeaderView::ResizeToContents);
    m_tree->setRootIsDecorated(false);
    layout->addWidget(m_tree);

    // the actions under their menus, in the order of the menus
    QHash<QString, QTreeWidgetItem*> groups;
    QFont bold = m_tree->font();
    bold.setBold(true);
    for (const QString& id : m_map->ids()) {
        QString path = m_map->path(id);
        QTreeWidgetItem* group = groups.value(path);
        if (!group) {
            group = new QTreeWidgetItem(m_tree, QStringList() << path);
            group->setFlags(Qt::ItemIsEnabled);
            group->setFirstColumnSpanned(true);
            group->setFont(COLUMN_ACTION, bold);
            groups.insert(path, group);
        }
        QTreeWidgetItem* row = new QTreeWidgetItem(group, QStringList() << m_map->label(id));
        row->setData(COLUMN_ACTION, Qt::UserRole, id);
        m_rows.insert(id, row);
    }
    m_tree->expandAll();

    QHBoxLayout* edit = new QHBoxLayout;
    m_key = new QKeySequenceEdit(this);
    m_key->setObjectName("shortcutKeyEdit");
    m_assign = new QPushButton(tr("Assign"), this);
    m_assign->setObjectName("shortcutAssign");
    m_add = new QPushButton(tr("Add"), this);
    m_add->setObjectName("shortcutAdd");
    m_clear = new QPushButton(tr("Clear"), this);
    m_clear->setObjectName("shortcutClear");
    m_reset = new QPushButton(tr("Reset"), this);
    m_reset->setObjectName("shortcutReset");
    QPushButton* all = new QPushButton(tr("Reset all"), this);
    all->setObjectName("shortcutResetAll");
    edit->addWidget(m_key, 1);
    edit->addWidget(m_assign);
    edit->addWidget(m_add);
    edit->addWidget(m_clear);
    edit->addWidget(m_reset);
    edit->addWidget(all);
    layout->addLayout(edit);
    m_holder = new QLabel(this);
    m_holder->setObjectName("shortcutHolder");
    layout->addWidget(m_holder);

    connect(m_filter, &QLineEdit::textChanged, this, &ShortcutsPage::filter);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, &ShortcutsPage::rowChanged);
    connect(m_key, &QKeySequenceEdit::keySequenceChanged, this, &ShortcutsPage::keyChanged);
    connect(m_assign, &QPushButton::clicked, this, &ShortcutsPage::assign);
    connect(m_add, &QPushButton::clicked, this, &ShortcutsPage::add);
    connect(m_clear, &QPushButton::clicked, this, &ShortcutsPage::clear);
    connect(m_reset, &QPushButton::clicked, this, &ShortcutsPage::reset);
    connect(all, &QPushButton::clicked, this, &ShortcutsPage::resetAll);
    load();
}

void ShortcutsPage::load()
{
    for (const QString& id : m_map->ids()) {
        m_keys[id] = m_map->keys(id);
        showRow(id);
    }
    rowChanged();
}

void ShortcutsPage::apply()
{
    for (const QString& id : m_map->ids()) m_map->setKeys(id, m_keys.value(id));
}

bool ShortcutsPage::select(const QString& id)
{
    QTreeWidgetItem* row = m_rows.value(id);
    if (row) m_tree->setCurrentItem(row);
    return row;
}

QString ShortcutsPage::currentId() const
{
    QTreeWidgetItem* row = m_tree->currentItem();
    return row ? row->data(COLUMN_ACTION, Qt::UserRole).toString() : QString();
}

// the action with its menus, as the holder of a key is named
QString ShortcutsPage::name(const QString& id) const
{
    return m_map->path(id) + " > " + m_map->label(id);
}

// the keys of the row, a changed row in bold (EDIT-KEYS-2)
void ShortcutsPage::showRow(const QString& id)
{
    QTreeWidgetItem* row = m_rows.value(id);
    if (!row) return;
    row->setText(COLUMN_KEYS, nativeKeys(m_keys.value(id)));
    QFont font = m_tree->font();
    font.setBold(m_keys.value(id) != m_map->defaultKeys(id));
    row->setFont(COLUMN_ACTION, font);
    row->setFont(COLUMN_KEYS, font);
}

void ShortcutsPage::filter(const QString& text)
{
    for (int g = 0; g < m_tree->topLevelItemCount(); g++) {
        QTreeWidgetItem* group = m_tree->topLevelItem(g);
        bool any = false;
        for (int i = 0; i < group->childCount(); i++) {
            QTreeWidgetItem* row = group->child(i);
            bool shown = group->text(COLUMN_ACTION).contains(text, Qt::CaseInsensitive) ||
                         row->text(COLUMN_ACTION).contains(text, Qt::CaseInsensitive) ||
                         row->text(COLUMN_KEYS).contains(text, Qt::CaseInsensitive);
            row->setHidden(!shown);
            any = any || shown;
        }
        group->setHidden(!any);
    }
}

void ShortcutsPage::rowChanged()
{
    QString id = currentId();
    bool row = !id.isEmpty();
    QList<QKeySequence> keys = m_keys.value(id);
    m_key->setEnabled(row);
    m_key->setKeySequence(keys.isEmpty() ? QKeySequence() : keys.first());
    m_clear->setEnabled(row && !keys.isEmpty());
    m_reset->setEnabled(row && keys != m_map->defaultKeys(id));
    keyChanged();
}

// the holder of the key typed is named (EDIT-KEYS-3)
void ShortcutsPage::keyChanged()
{
    QString id = currentId();
    QKeySequence key = m_key->keySequence();
    bool own = m_keys.value(id).contains(key);
    bool typed = !id.isEmpty() && !key.isEmpty();
    m_assign->setEnabled(typed && m_keys.value(id) != (QList<QKeySequence>() << key));
    m_add->setEnabled(typed && !own);
    QString holder;
    if (typed) {
        for (const QString& other : m_map->ids()) {
            if (other != id && m_keys.value(other).contains(key)) holder = other;
        }
    }
    m_holder->setText(holder.isEmpty() ? QString()
                      : tr("%1 is the key of \"%2\".").arg(key.toString(QKeySequence::NativeText), name(holder)));
    m_holder->setProperty("holder", holder);
}

void ShortcutsPage::release(const QKeySequence& key, const QString& except)
{
    for (const QString& other : m_map->ids()) {
        if (other != except && m_keys[other].removeAll(key)) showRow(other);
    }
}

void ShortcutsPage::take(bool replace)
{
    QString id = currentId();
    QKeySequence key = m_key->keySequence();
    if (id.isEmpty() || key.isEmpty()) return;
    QString holder = m_holder->property("holder").toString();
    if (!holder.isEmpty()) {
        QString question = tr("%1 is the key of \"%2\". Give it to \"%3\"?")
                           .arg(key.toString(QKeySequence::NativeText), name(holder), name(id));
        if (QMessageBox::question(this, tr("Shortcuts"), question) != QMessageBox::Yes) return;
        release(key, id);
    }
    if (replace) m_keys[id].clear();
    if (!m_keys[id].contains(key)) m_keys[id] << key;
    showRow(id);
    rowChanged();
}

void ShortcutsPage::assign()
{
    take(true);
}

void ShortcutsPage::add()
{
    take(false);
}

void ShortcutsPage::clear()
{
    QString id = currentId();
    if (id.isEmpty()) return;
    m_keys[id].clear();
    showRow(id);
    rowChanged();
}

// the default keys come back from the rows that took them
void ShortcutsPage::reset()
{
    QString id = currentId();
    if (id.isEmpty()) return;
    m_keys[id] = m_map->defaultKeys(id);
    for (const QKeySequence& key : m_keys[id]) release(key, id);
    showRow(id);
    rowChanged();
}

void ShortcutsPage::resetAll()
{
    for (const QString& id : m_map->ids()) {
        m_keys[id] = m_map->defaultKeys(id);
        showRow(id);
    }
    rowChanged();
}
