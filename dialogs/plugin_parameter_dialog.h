/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Plugin Parameter Dialog, generated from the parameters (docs/PLUGIN-API.md)
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

#ifndef PLUGIN_PARAMETER_DIALOG_H
#define PLUGIN_PARAMETER_DIALOG_H

#include <QDialog>
#include <QList>
#include <QMap>

#include "plugin_values.h"

class QDialogButtonBox;
class QPushButton;

// one widget per parameter type; a text parameter is a large box on top, the
// accept button waits for every required value (EDIT-PLUG-7)
class PluginParameterDialog : public QDialog {
    Q_OBJECT

public:
    PluginParameterDialog(const QString& title, const QList<const PluginParameter*>& parameters,
                          const PluginValues& values, const QString& acceptText,
                          QWidget* parent = nullptr);

    PluginValues values() const;
    // the editor widget of a parameter (the tests address it)
    QWidget*     editor(const QString& parameterId) const { return editors.value(parameterId); }
    QPushButton* acceptButton() const { return runButton; }

private slots:
    void         updateAccept();

private:
    QWidget*     createEditor(const PluginParameter* parameter, const QString& value);
    QString      value(const PluginParameter* parameter) const;

    QList<const PluginParameter*> parameters;
    QMap<QString, QWidget*>       editors;
    QPushButton*                  runButton = nullptr;
};

#endif
