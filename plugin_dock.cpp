/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
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
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabBar>
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
// the name a saved output gets: <plugin id><suffix>
#define OUTPUT_FILE_SUFFIX   "-output.txt"

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
    tabWidget->addTab(diagnosticsList, tr("Diagnostics"));
    // the output tabs close, the Diagnostics stays
    tabWidget->setTabsClosable(true);
    tabWidget->tabBar()->setTabButton(0, QTabBar::LeftSide, nullptr);
    tabWidget->tabBar()->setTabButton(0, QTabBar::RightSide, nullptr);
    tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(tabWidget);
    setWidget(body);

    ticker.setInterval(TICK_MS);
    connect(&ticker, &QTimer::timeout, this, &PluginDock::slotTick);
    connect(diagnosticsList, &QTreeWidget::itemActivated, this, &PluginDock::slotItemActivated);
    connect(diagnosticsList, &QTreeWidget::itemClicked, this, &PluginDock::slotItemActivated);
    connect(tabWidget, &QTabWidget::tabCloseRequested, this, &PluginDock::slotTabCloseRequested);
    connect(tabWidget->tabBar(), &QWidget::customContextMenuRequested, this, &PluginDock::slotTabMenu);
}

QPlainTextEdit* PluginDock::output(const QString& pluginId) const
{
    return outputs.value(pluginId).text;
}

QPlainTextEdit* PluginDock::outputTab(const QString& pluginId)
{
    OutputTab& tab = outputs[pluginId];
    if (tab.text) return tab.text;
    // a closed tab comes back empty (EDIT-PLUG-35)
    tab.text = new QPlainTextEdit(tabWidget);
    tab.text->setReadOnly(true);
    tab.text->setMaximumBlockCount(OUTPUT_BLOCK_LIMIT);
    tab.text->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tab.text, &QWidget::customContextMenuRequested, this, [this, pluginId](const QPoint& pos) {
        QMenu* menu = createOutputMenu(pluginId);
        if (!menu) return;
        menu->exec(output(pluginId)->viewport()->mapToGlobal(pos));
        delete menu;
    });
    int index = tabWidget->addTab(tab.text, tr("%1 output").arg(tab.name));
    tabWidget->setTabToolTip(index, pluginId);
    return tab.text;
}

QString PluginDock::pluginIdOf(const QWidget* tab) const
{
    for (QMap<QString, OutputTab>::const_iterator i = outputs.begin(); i != outputs.end(); i++) {
        if (tab && i.value().text == tab) return i.key();
    }
    return QString();
}

void PluginDock::slotTabCloseRequested(int index)
{
    QString id = pluginIdOf(tabWidget->widget(index));
    if (id.isEmpty()) return;
    QPlainTextEdit* text = outputs[id].text;
    outputs[id].text = nullptr;
    tabWidget->removeTab(index);
    text->deleteLater();
}

void PluginDock::slotTabMenu(const QPoint& pos)
{
    QString id = pluginIdOf(tabWidget->widget(tabWidget->tabBar()->tabAt(pos)));
    if (id.isEmpty()) return;
    QMenu* menu = createOutputMenu(id);
    if (!menu) return;
    menu->exec(tabWidget->tabBar()->mapToGlobal(pos));
    delete menu;
}

QMenu* PluginDock::createOutputMenu(const QString& pluginId)
{
    QPlainTextEdit* text = output(pluginId);
    if (!text) return nullptr;
    QMenu* menu = text->createStandardContextMenu();
    menu->addSeparator();
    QAction* save = menu->addAction(tr("Save to File..."));
    QAction* clear = menu->addAction(tr("Clear..."));
    bool empty = text->document()->isEmpty();
    save->setEnabled(!empty);
    clear->setEnabled(!empty);
    connect(save, &QAction::triggered, this, [this, pluginId]() { askSave(pluginId); });
    connect(clear, &QAction::triggered, this, [this, pluginId]() { askClear(pluginId); });
    return menu;
}

void PluginDock::clearOutput(const QString& pluginId)
{
    if (QPlainTextEdit* text = output(pluginId)) text->clear();
}

bool PluginDock::saveOutput(const QString& pluginId, const QString& path, QString* error) const
{
    QPlainTextEdit* text = output(pluginId);
    QFile file(path);
    QByteArray data = text ? text->toPlainText().toUtf8() : QByteArray();
    if (!data.isEmpty()) data += '\n';
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text) || file.write(data) != data.size()) {
        if (error) *error = tr("Cannot write %1: %2").arg(QDir::toNativeSeparators(path), file.errorString());
        return false;
    }
    return true;
}

void PluginDock::askClear(const QString& pluginId)
{
    QMessageBox::StandardButton answer =
        QMessageBox::question(this, tr("Clear the output"),
                              tr("Clear the %1 output?").arg(outputs.value(pluginId).name),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes) clearOutput(pluginId);
}

void PluginDock::askSave(const QString& pluginId)
{
    const OutputTab& tab = outputs[pluginId];
    QDir folder(tab.folder.isEmpty() ? QDir::homePath() : tab.folder);
    QString path = QFileDialog::getSaveFileName(this, tr("Save the %1 output").arg(tab.name),
                                                folder.filePath(pluginId + OUTPUT_FILE_SUFFIX),
                                                tr("Text files (*.txt)"));
    if (path.isEmpty()) return;
    QString error;
    if (!saveOutput(pluginId, path, &error)) QMessageBox::warning(this, tr("Save the output"), error);
}

QPushButton* PluginDock::cancelButton(PluginRunner* run) const
{
    return rows.contains(run) ? rows.value(run).cancel : nullptr;
}

void PluginDock::runStarted(PluginRunner* run, CyberiadaSMEditorDocument* doc)
{
    const PluginDescriptor& plugin = run->plugin().descriptor;
    QString id = plugin.id;
    OutputTab& tab = outputs[id];
    tab.name = plugin.name.isEmpty() ? id : plugin.name;
    tab.folder = doc && doc->hasFile() ? QFileInfo(doc->filePath()).absolutePath() : QString();
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
    connect(run, &PluginRunner::output, this, [this, id](const QString& text, bool isError) {
        QString t = text;
        if (t.endsWith('\n')) t.chop(1);
        if (!t.isEmpty()) append(id, t, isError);
    });
    append(id, doc ? QString("== %1 - %2").arg(row.title, doc->title()) : QString("== %1").arg(row.title),
           false);
    tabWidget->setCurrentWidget(output(id));
    for (const PluginMetaNote& note: run->result().metaNotes) {
        switch (note.kind) {
        case PluginMetaNote::metaSet:
            append(id, tr("%1 is set to %2").arg(note.key, note.value), false);
            break;
        case PluginMetaNote::metaKept:
            append(id, tr("%1 is kept as %2; the plugin declares %3").arg(note.key, note.value, note.declared),
                   false);
            break;
        default:
            append(id, tr("%1 is not set: the document has no state machine or is read-only").arg(note.key),
                   false);
            break;
        }
    }
    ticker.start();
    // the dock comes with the first run; the batch mode never shows it
    if (!qApp->property("batchMode").toBool()) show();
}

void PluginDock::append(const QString& pluginId, const QString& text, bool isError)
{
    QPlainTextEdit* edit = outputTab(pluginId);
    QTextCharFormat format;
    if (isError) format.setForeground(Qt::darkRed);
    QTextCursor cursor(edit->document());
    cursor.movePosition(QTextCursor::End);
    if (!edit->document()->isEmpty()) cursor.insertBlock();
    cursor.insertText(text, format);
    edit->ensureCursorVisible();
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
    QString id = run->plugin().descriptor.id;
    switch (r.state) {
    case PluginResult::stateDone:
        append(id, r.exitCode >= 0 ? tr("done, exit code %1").arg(r.exitCode) : tr("done"), false);
        break;
    case PluginResult::stateCancelled:
        append(id, tr("cancelled"), true);
        break;
    default:
        append(id, tr("failed: %1").arg(r.error), true);
        break;
    }
    if (r.state != PluginResult::stateDone) return;
    for (const PluginFileChange& f: r.files) {
        append(id, (f.isNew ? tr("new %1", "plugin file") : tr("changed %1", "plugin file")).arg(f.path),
               false);
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
