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

#ifndef CYBERIADA_AUTOREPLACE_PAGE
#define CYBERIADA_AUTOREPLACE_PAGE

#include <QWidget>
#include "autoreplace.h"

class QCheckBox;
class QTableWidget;
class QTableWidgetItem;

// The switch and the rules of the typed replacements (EDIT-AUTO-7,
// docs/AUTOREPLACE.md): the page edits a copy, apply() stores it
class AutoreplacePage: public QWidget {
Q_OBJECT
public:
    explicit AutoreplacePage(QWidget* parent = nullptr);

    // the settings shown, the edits dropped
    void             load();
    void             apply();
    // the rules of the table, the rows without a pattern left out
    AutoreplaceRules rules() const;
    bool             isEnabledChecked() const;

private slots:
    void             cellChanged(QTableWidgetItem* item);
    void             restoreDefaults();

private:
    void             show(const AutoreplaceRules& rules);
    void             addRow(const AutoreplaceRule& rule);
    QString          pattern(int row) const;

    QCheckBox*       m_enabled;
    QTableWidget*    m_table;
};

#endif
