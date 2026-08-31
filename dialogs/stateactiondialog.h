/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The State Action Dialog
 *
 * Copyright (C) 2025 Anastasia Viktorova <viktorovaa.04@gmail.com>
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

#ifndef STATEACTIONDIALOG_H
#define STATEACTIONDIALOG_H
#include <QDialog>

class QPlainTextEdit;

class StateActionDialog : public QDialog {
    Q_OBJECT

public:
    StateActionDialog(const QString& keyword, QWidget* parent = nullptr);

    // parse the entered text; on success the behaviour is available
    bool parseInput();
    QString getBehaviour() const;

private slots:
    void slotAccept();

private:
    QString keyword;
    QString behaviour;
    QPlainTextEdit* actionEdit;
};

#endif // STATEACTIONDIALOG_H
