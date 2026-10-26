/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Plugin Dock: the running plugins, the Diagnostics and an output tab per plugin
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
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class PluginRunner;
class PluginSessionControls;
class PluginSessionTab;
class CyberiadaSMEditorDocument;
struct PluginDiagnostic;

// under the canvas, hidden until the first run (EDIT-PLUG-32): one row per
// running plugin with its time and Cancel, the findings of the last run with
// findings (EDIT-PLUG-15), an output tab per plugin with one block per run
// (EDIT-PLUG-17), closable, cleared and saved from its menu (EDIT-PLUG-35); a
// session has its controls in the run row and a session tab (EDIT-PLUG-39, 42)
class PluginDock : public QDockWidget {
    Q_OBJECT

public:
    PluginDock(QWidget* parent = nullptr);

    void          runStarted(PluginRunner* run, CyberiadaSMEditorDocument* doc);
    void          runFinished(PluginRunner* run, CyberiadaSMEditorDocument* doc);

    QTreeWidget*  diagnostics() const { return diagnosticsList; }
    // the output tab of a plugin; nullptr while it is closed
    QPlainTextEdit* output(const QString& pluginId) const;
    QTabWidget*   tabs() const { return tabWidget; }
    // the context menu of an open output tab, else nullptr; the caller owns it
    QMenu*        createOutputMenu(const QString& pluginId);
    void          clearOutput(const QString& pluginId);
    bool          saveOutput(const QString& pluginId, const QString& path, QString* error = nullptr) const;
    // the Cancel button of a running plugin, the Stop of a session (the tests press it)
    QPushButton*  cancelButton(PluginRunner* run) const;
    // the session parts of a running session, else nullptr
    PluginSessionControls* sessionControls(PluginRunner* run) const;
    PluginSessionTab* sessionTab(PluginRunner* run) const;
    // the findings of a running session replace the shown ones (EDIT-PLUG-49)
    void          showDiagnostics(const QList<PluginDiagnostic>& diagnostics, CyberiadaSMEditorDocument* doc);

signals:
    void          diagnosticActivated(CyberiadaSMEditorDocument* doc, const QString& elementId);

private slots:
    void          slotTick();
    void          slotItemActivated(QTreeWidgetItem* item, int column);
    void          slotDocumentChanged();
    void          slotTabCloseRequested(int index);
    void          slotTabMenu(const QPoint& pos);

private:
    struct Row {
        QWidget*  widget;
        QLabel*   label = nullptr;
        QPushButton* cancel = nullptr;
        PluginSessionControls* controls = nullptr;
        PluginSessionTab* tab = nullptr;
        QString   title;
    };

    struct OutputTab {
        QPlainTextEdit* text = nullptr;
        QString   name;
        // the folder of the document of the last run, empty for an untitled one
        QString   folder;
    };

    QPlainTextEdit* outputTab(const QString& pluginId);
    QString       pluginIdOf(const QWidget* tab) const;
    void          append(const QString& pluginId, const QString& text, bool isError);
    void          askClear(const QString& pluginId);
    void          askSave(const QString& pluginId);
    void          markStale();

    QVBoxLayout*  runsLayout;
    QTabWidget*   tabWidget;
    QTreeWidget*  diagnosticsList;
    QMap<QString, OutputTab> outputs;
    QMap<PluginRunner*, Row> rows;
    QTimer        ticker;
    QPointer<CyberiadaSMEditorDocument> diagnosticsDocument;
    QMetaObject::Connection documentChanged;
    bool          stale = false;
};

#endif
