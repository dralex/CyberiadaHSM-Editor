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

#include <QApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBlock>
#include <QTreeWidget>
#include <QUndoStack>
#include <QVBoxLayout>

#include "plugin_dock.h"
#include "plugin_runner.h"
#include "cyberiadasm_editor_document.h"

#define TICK_MS              1000
#define OUTPUT_BLOCK_LIMIT   20000

enum DiagnosticColumn {
    columnSeverity = 0,
    columnRule,
    columnElement,
    columnLine,
    columnMessage,
    columnsCount
};

// the protocol token shown in the interface language
static QString severityText(const QString& severity)
{
    if (severity == "error") return PluginDock::tr("error", "severity");
    if (severity == "warning") return PluginDock::tr("warning", "severity");
    if (severity == "info") return PluginDock::tr("info", "severity");
    return severity;
}

PluginDock::PluginDock(QWidget* parent):
    QDockWidget(tr("Plugins"), parent)
{
    setObjectName("pluginDock");
    QWidget* body = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    runsLayout = new QVBoxLayout();
    layout->addLayout(runsLayout);

    tabWidget = new QTabWidget(body);
    diagnosticsList = new QTreeWidget(tabWidget);
    diagnosticsList->setColumnCount(columnsCount);
    diagnosticsList->setHeaderLabels(QStringList() << tr("Severity") << tr("Rule") << tr("Element")
                                                   << tr("Line") << tr("Message"));
    diagnosticsList->setRootIsDecorated(false);
    diagnosticsList->header()->setStretchLastSection(true);
    outputText = new QPlainTextEdit(tabWidget);
    outputText->setReadOnly(true);
    outputText->setMaximumBlockCount(OUTPUT_BLOCK_LIMIT);
    tabWidget->addTab(diagnosticsList, tr("Diagnostics"));
    tabWidget->addTab(outputText, tr("Output"));
    layout->addWidget(tabWidget);
    setWidget(body);

    ticker.setInterval(TICK_MS);
    connect(&ticker, &QTimer::timeout, this, &PluginDock::slotTick);
    connect(diagnosticsList, &QTreeWidget::itemActivated, this, &PluginDock::slotItemActivated);
    connect(diagnosticsList, &QTreeWidget::itemClicked, this, &PluginDock::slotItemActivated);
}

QPushButton* PluginDock::cancelButton(PluginRunner* run) const
{
    return rows.contains(run) ? rows.value(run).cancel : nullptr;
}

void PluginDock::runStarted(PluginRunner* run)
{
    Row row;
    row.title = QString("%1: %2").arg(run->plugin().descriptor.name, run->action().label);
    row.widget = new QWidget(widget());
    QHBoxLayout* h = new QHBoxLayout(row.widget);
    h->setContentsMargins(4, 0, 4, 0);
    row.label = new QLabel(tr("%1: running").arg(row.title), row.widget);
    row.cancel = new QPushButton(tr("Cancel"), row.widget);
    h->addWidget(row.label, 1);
    h->addWidget(row.cancel);
    runsLayout->addWidget(row.widget);
    rows.insert(run, row);
    QPointer<PluginRunner> guard(run);
    connect(row.cancel, &QPushButton::clicked, this, [guard]() { if (guard) guard->cancel(); });
    connect(run, &PluginRunner::output, this, &PluginDock::slotOutput);
    append(QString("== %1").arg(row.title), false);
    for (const PluginMetaNote& note: run->result().metaNotes) {
        switch (note.kind) {
        case PluginMetaNote::metaSet:
            append(tr("%1 is set to %2").arg(note.key, note.value), false);
            break;
        case PluginMetaNote::metaKept:
            append(tr("%1 is kept as %2; the plugin declares %3").arg(note.key, note.value, note.declared),
                   false);
            break;
        default:
            append(tr("%1 is not set: the document has no state machine or is read-only").arg(note.key),
                   false);
            break;
        }
    }
    ticker.start();
    // the dock comes with the first run; the batch mode never shows it
    if (!qApp->property("batchMode").toBool()) show();
}

void PluginDock::slotOutput(const QString& text, bool isError)
{
    QString t = text;
    if (t.endsWith('\n')) t.chop(1);
    if (!t.isEmpty()) append(t, isError);
}

void PluginDock::append(const QString& text, bool isError)
{
    QTextCharFormat format;
    if (isError) format.setForeground(Qt::darkRed);
    QTextCursor cursor(outputText->document());
    cursor.movePosition(QTextCursor::End);
    if (!outputText->document()->isEmpty()) cursor.insertBlock();
    cursor.insertText(text, format);
    outputText->ensureCursorVisible();
}

void PluginDock::slotTick()
{
    for (QMap<PluginRunner*, Row>::const_iterator i = rows.begin(); i != rows.end(); i++) {
        qint64 s = i.key()->elapsedMs() / 1000;
        i.value().label->setText(tr("%1: running %2:%3").arg(i.value().title)
                                 .arg(s / 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0')));
    }
}

void PluginDock::runFinished(PluginRunner* run, CyberiadaSMEditorDocument* doc)
{
    if (rows.contains(run)) {
        Row row = rows.take(run);
        row.widget->deleteLater();
    }
    if (rows.isEmpty()) ticker.stop();
    const PluginResult& r = run->result();
    switch (r.state) {
    case PluginResult::stateDone:
        append(r.exitCode >= 0 ? tr("done, exit code %1").arg(r.exitCode) : tr("done"), false);
        break;
    case PluginResult::stateCancelled:
        append(tr("cancelled"), true);
        break;
    default:
        append(tr("failed: %1").arg(r.error), true);
        break;
    }
    if (r.state != PluginResult::stateDone) return;
    for (const PluginFileChange& f: r.files) {
        append((f.isNew ? tr("new %1", "plugin file") : tr("changed %1", "plugin file")).arg(f.path), false);
    }
    if (r.diagnostics.isEmpty() && run->action().result != PluginAction::resultDiagnostics) return;

    // the findings of this run replace the previous ones
    diagnosticsList->clear();
    disconnect(documentChanged);
    diagnosticsDocument = doc;
    stale = false;
    for (const PluginDiagnostic& d: r.diagnostics) {
        QTreeWidgetItem* item = new QTreeWidgetItem(diagnosticsList);
        item->setText(columnSeverity, severityText(d.severity));
        item->setText(columnRule, d.rule);
        item->setText(columnElement, d.element.isEmpty() ? QString() :
                      (d.linked ? d.element : tr("%1 (absent)").arg(d.element)));
        if (d.line > 0) item->setText(columnLine, QString::number(d.line));
        item->setText(columnMessage, d.message);
        if (d.linked) item->setData(columnElement, Qt::UserRole, d.element);
    }
    tabWidget->setTabText(tabWidget->indexOf(diagnosticsList),
                          tr("Diagnostics %1").arg(r.diagnostics.size()));
    if (!r.diagnostics.isEmpty()) tabWidget->setCurrentWidget(diagnosticsList);
    if (doc) {
        documentChanged = connect(doc->model()->undoStack(), &QUndoStack::indexChanged,
                                  this, &PluginDock::slotDocumentChanged);
    }
}

void PluginDock::slotDocumentChanged()
{
    markStale();
}

void PluginDock::markStale()
{
    // the findings describe the document as it was (EDIT-PLUG-15)
    if (stale) return;
    stale = true;
    disconnect(documentChanged);
    for (int i = 0; i < diagnosticsList->topLevelItemCount(); i++) {
        QTreeWidgetItem* item = diagnosticsList->topLevelItem(i);
        for (int c = 0; c < columnsCount; c++) item->setForeground(c, QBrush(Qt::gray));
    }
}

void PluginDock::slotItemActivated(QTreeWidgetItem* item, int)
{
    QString id = item ? item->data(columnElement, Qt::UserRole).toString() : QString();
    if (id.isEmpty() || !diagnosticsDocument) return;
    emit diagnosticActivated(diagnosticsDocument.data(), id);
}
