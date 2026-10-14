/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Plugin Dock: the running plugins, the Diagnostics and the Output
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

#ifndef PLUGIN_DOCK_H
#define PLUGIN_DOCK_H

#include <QDockWidget>
#include <QMap>
#include <QPointer>
#include <QTimer>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class PluginRunner;
class CyberiadaSMEditorDocument;

// under the canvas, hidden until the first run (EDIT-PLUG-32): one row per
// running plugin with its time and Cancel, the findings of the last run with
// findings (EDIT-PLUG-15), one Output block per run (EDIT-PLUG-17)
class PluginDock : public QDockWidget {
    Q_OBJECT

public:
    PluginDock(QWidget* parent = nullptr);

    void          runStarted(PluginRunner* run);
    void          runFinished(PluginRunner* run, CyberiadaSMEditorDocument* doc);

    QTreeWidget*  diagnostics() const { return diagnosticsList; }
    QPlainTextEdit* output() const { return outputText; }
    QTabWidget*   tabs() const { return tabWidget; }
    // the Cancel button of a running plugin (the tests press it)
    QPushButton*  cancelButton(PluginRunner* run) const;

signals:
    void          diagnosticActivated(CyberiadaSMEditorDocument* doc, const QString& elementId);

private slots:
    void          slotOutput(const QString& text, bool isError);
    void          slotTick();
    void          slotItemActivated(QTreeWidgetItem* item, int column);
    void          slotDocumentChanged();

private:
    struct Row {
        QWidget*  widget;
        QLabel*   label;
        QPushButton* cancel;
        QString   title;
    };

    void          append(const QString& text, bool isError);
    void          markStale();

    QVBoxLayout*  runsLayout;
    QTabWidget*   tabWidget;
    QTreeWidget*  diagnosticsList;
    QPlainTextEdit* outputText;
    QMap<PluginRunner*, Row> rows;
    QTimer        ticker;
    QPointer<CyberiadaSMEditorDocument> diagnosticsDocument;
    QMetaObject::Connection documentChanged;
    bool          stale = false;
};

#endif
