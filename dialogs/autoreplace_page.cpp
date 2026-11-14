/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Autoreplace page of the Preferences
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

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
#include "autoreplace_page.h"
#include "settings_manager.h"

static const int COLUMN_FROM = 0;
static const int COLUMN_TO = 1;
static const int COLUMNS = 2;

AutoreplacePage::AutoreplacePage(QWidget* parent):
    QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    m_enabled = new QCheckBox(tr("Replace as you type"), this);
    m_enabled->setObjectName("autoreplaceEnabled");
    layout->addWidget(m_enabled);

    // the check box of a row switches its rule
    m_table = new QTableWidget(0, COLUMNS, this);
    m_table->setObjectName("autoreplaceTable");
    m_table->setHorizontalHeaderLabels(QStringList() << tr("Replace") << tr("With"));
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->hide();
    layout->addWidget(m_table);

    QHBoxLayout* buttons = new QHBoxLayout;
    QPushButton* defaults = new QPushButton(tr("Restore defaults"), this);
    defaults->setObjectName("autoreplaceDefaults");
    buttons->addStretch(1);
    buttons->addWidget(defaults);
    layout->addLayout(buttons);

    connect(m_table, &QTableWidget::itemChanged, this, &AutoreplacePage::cellChanged);
    connect(defaults, &QPushButton::clicked, this, &AutoreplacePage::restoreDefaults);
    connect(m_enabled, &QCheckBox::toggled, m_table, &QWidget::setEnabled);
    load();
}

void AutoreplacePage::load()
{
    SettingsManager& sm = SettingsManager::instance();
    m_enabled->setChecked(sm.getAutoreplaceEnabled());
    m_table->setEnabled(m_enabled->isChecked());
    show(sm.getAutoreplaceRules());
}

void AutoreplacePage::apply()
{
    SettingsManager& sm = SettingsManager::instance();
    sm.setAutoreplaceEnabled(m_enabled->isChecked());
    sm.setAutoreplaceRules(rules());
}

bool AutoreplacePage::isEnabledChecked() const
{
    return m_enabled->isChecked();
}

QString AutoreplacePage::pattern(int row) const
{
    QTableWidgetItem* from = m_table->item(row, COLUMN_FROM);
    return from ? from->text() : QString();
}

AutoreplaceRules AutoreplacePage::rules() const
{
    AutoreplaceRules rules;
    for (int row = 0; row < m_table->rowCount(); row++) {
        QString from = pattern(row);
        if (from.isEmpty()) continue;
        QTableWidgetItem* to = m_table->item(row, COLUMN_TO);
        rules << AutoreplaceRule(from, to ? to->text() : QString(),
                                 m_table->item(row, COLUMN_FROM)->checkState() == Qt::Checked);
    }
    return rules;
}

void AutoreplacePage::addRow(const AutoreplaceRule& rule)
{
    int row = m_table->rowCount();
    m_table->insertRow(row);
    QTableWidgetItem* from = new QTableWidgetItem(rule.from);
    from->setFlags(from->flags() | Qt::ItemIsUserCheckable);
    from->setCheckState(rule.enabled ? Qt::Checked : Qt::Unchecked);
    m_table->setItem(row, COLUMN_FROM, from);
    m_table->setItem(row, COLUMN_TO, new QTableWidgetItem(rule.to));
}

// the rules and a free row last, where a new one is written
void AutoreplacePage::show(const AutoreplaceRules& rules)
{
    QSignalBlocker block(m_table);
    m_table->setRowCount(0);
    for (const AutoreplaceRule& rule : rules) addRow(rule);
    addRow(AutoreplaceRule());
}

void AutoreplacePage::cellChanged(QTableWidgetItem* item)
{
    if (item->column() != COLUMN_FROM) return;
    QSignalBlocker block(m_table);
    int row = item->row();
    QString from = item->text();
    // a pattern is written once: the first row with it is shown
    for (int other = 0; !from.isEmpty() && other < m_table->rowCount(); other++) {
        if (other == row || pattern(other) != from) continue;
        item->setText(QString());
        m_table->setCurrentCell(other, COLUMN_FROM);
        from.clear();
    }
    int last = m_table->rowCount() - 1;
    // a row without a pattern is dropped, the free one is kept last
    if (from.isEmpty() && row != last) m_table->removeRow(row);
    else if (!from.isEmpty() && row == last) addRow(AutoreplaceRule());
}

void AutoreplacePage::restoreDefaults()
{
    show(defaultAutoreplaceRules());
}
