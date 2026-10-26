/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Plugin Session View: the session controls and the session tab of the dock
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

#include <QActionGroup>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "plugin_session_view.h"
#include "plugin_runner.h"
#include "plugin_session.h"
#include "cyberiadasm_editor_document.h"
#include "dialogs/multilinetextdialog.h"

#define SECONDS_IN_MINUTE  60
#define MS_IN_SECOND       1000
#define TIME_DECIMALS      3
// the name a saved log gets: <document name><suffix>
#define LOG_FILE_SUFFIX    ".log"

enum HistoryColumn {
    columnTime = 0,
    columnEvent,
    columnOrigin,
    columnOutcome,
    columnFired,
    historyColumns
};

// the protocol tokens shown in the interface language
static QString statusText(const QString& status)
{
    if (status == "running") return PluginSessionControls::tr("running", "session status");
    if (status == "paused") return PluginSessionControls::tr("paused", "session status");
    if (status == "waiting") return PluginSessionControls::tr("waiting", "session status");
    if (status == "finished") return PluginSessionControls::tr("finished", "session status");
    return status;
}

static QString verdictText(const QString& verdict)
{
    if (verdict == "accepted") return PluginSessionControls::tr("accepted", "session verdict");
    if (verdict == "rejected") return PluginSessionControls::tr("rejected", "session verdict");
    return verdict;
}

PluginSessionControls::PluginSessionControls(PluginRunner* run, const QString& runTitle, QWidget* parent):
    QWidget(parent), session(run->session()), title(runTitle)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    QHBoxLayout* row = new QHBoxLayout();
    label = new QLabel(this);
    stepTool = new QToolButton(this);
    stepTool->setPopupMode(QToolButton::MenuButtonPopup);
    units = new QMenu(stepTool);
    stepTool->setMenu(units);
    runTool = new QToolButton(this);
    runTool->setPopupMode(QToolButton::MenuButtonPopup);
    speeds = new QMenu(runTool);
    runTool->setMenu(speeds);
    pause = new QPushButton(tr("Pause"), this);
    restart = new QPushButton(tr("Restart"), this);
    stop = new QPushButton(tr("Stop"), this);
    row->addWidget(label, 1);
    row->addWidget(stepTool);
    row->addWidget(runTool);
    row->addWidget(pause);
    row->addWidget(restart);
    row->addWidget(stop);
    layout->addLayout(row);

    answers = new QWidget(this);
    QVBoxLayout* bar = new QVBoxLayout(answers);
    bar->setContentsMargins(0, 0, 0, 0);
    question = new QLabel(answers);
    question->setWordWrap(true);
    bar->addWidget(question);
    QHBoxLayout* line = new QHBoxLayout();
    answerLine = new QLineEdit(answers);
    choiceLayout = new QHBoxLayout();
    QPushButton* answer = new QPushButton(tr("Answer"), answers);
    QPushButton* decline = new QPushButton(tr("Decline"), answers);
    line->addWidget(answerLine, 1);
    line->addLayout(choiceLayout);
    line->addWidget(answer);
    line->addWidget(decline);
    bar->addLayout(line);
    layout->addWidget(answers);
    answers->hide();

    QPointer<PluginSession> s = session;
    connect(stepTool, &QToolButton::clicked, this, [s]() { if (s) s->step(); });
    connect(runTool, &QToolButton::clicked, this, [s]() { if (s) s->run(s->lastSpeed()); });
    connect(pause, &QPushButton::clicked, this, [s]() { if (s) s->pause(); });
    connect(restart, &QPushButton::clicked, this, [s]() { if (s) s->restart(); });
    connect(stop, &QPushButton::clicked, this, [s]() { if (s) s->stop(); });
    connect(answer, &QPushButton::clicked, this, [this, s]() { if (s) s->answer(answerLine->text()); });
    connect(answerLine, &QLineEdit::returnPressed, this, [this, s]() { if (s) s->answer(answerLine->text()); });
    connect(decline, &QPushButton::clicked, this, [s]() { if (s) s->decline(); });
    if (session) {
        connect(session, &PluginSession::modelReceived, this, &PluginSessionControls::slotState);
        connect(session, &PluginSession::stateChanged, this, &PluginSessionControls::slotState);
        connect(session, &PluginSession::questionsChanged, this, &PluginSessionControls::slotQuestions);
    }
    slotState();
}

QString PluginSessionControls::unitName(const QString& unit) const
{
    if (unit == "event") return tr("event", "step unit");
    if (unit == "input") return tr("input", "step unit");
    if (unit == "time") return tr("time", "step unit");
    return unit;
}

QString PluginSessionControls::speedName(double speed) const
{
    if (speed <= 0) return tr("max", "run speed");
    return tr("x%1", "run speed").arg(speed);
}

void PluginSessionControls::tick(qint64 elapsedMs)
{
    elapsed = elapsedMs;
    updateLabel();
}

void PluginSessionControls::updateLabel()
{
    QStringList parts;
    if (session) {
        const PluginSession::State& st = session->state();
        if (session->phase() == PluginSession::phaseStarting) parts << tr("starting");
        else if (session->phase() == PluginSession::phaseStopping) parts << tr("stopping");
        else if (!st.status.isEmpty()) parts << statusText(st.status);
        if (!st.verdict.isEmpty()) parts << verdictText(st.verdict);
        if (!st.text.isEmpty()) parts << st.text;
        if (st.hasTime) parts << tr("t = %1 s").arg(st.time, 0, 'f', TIME_DECIMALS);
        if (st.position >= 0) parts << tr("#%1").arg(st.position);
    }
    qint64 s = elapsed / MS_IN_SECOND;
    parts << QString("%1:%2").arg(s / SECONDS_IN_MINUTE, 2, 10, QChar('0'))
                             .arg(s % SECONDS_IN_MINUTE, 2, 10, QChar('0'));
    label->setText(tr("%1: %2").arg(title, parts.join("  ")));
}

void PluginSessionControls::slotState()
{
    if (!session) return;
    const PluginSession::Model& m = session->model();
    // the buttons the model offers, enabled by the status (EDIT-PLUG-39)
    stepTool->setVisible(m.controls.contains("step"));
    runTool->setVisible(m.controls.contains("run"));
    pause->setVisible(m.controls.contains("pause"));
    restart->setVisible(m.controls.contains("restart"));
    if (units->actions().size() != m.steps.size()) {
        units->clear();
        QActionGroup* group = new QActionGroup(units);
        QPointer<PluginSession> s = session;
        for (const QString& unit: m.steps) {
            QAction* a = units->addAction(unitName(unit));
            a->setCheckable(true);
            a->setChecked(unit == session->lastUnit());
            group->addAction(a);
            connect(a, &QAction::triggered, this, [s, unit]() { if (s) s->step(unit); });
        }
    }
    if (speeds->actions().size() != m.speeds.size()) {
        speeds->clear();
        QActionGroup* group = new QActionGroup(speeds);
        QPointer<PluginSession> s = session;
        for (double speed: m.speeds) {
            QAction* a = speeds->addAction(speedName(speed));
            a->setCheckable(true);
            a->setChecked(speed == session->lastSpeed());
            group->addAction(a);
            connect(a, &QAction::triggered, this, [s, speed]() { if (s) s->run(speed); });
        }
    }
    stepTool->setText(tr("Step: %1").arg(unitName(session->lastUnit())));
    runTool->setText(tr("Run %1").arg(speedName(session->lastSpeed())));
    stepTool->setEnabled(session->canStep());
    runTool->setEnabled(session->canRun());
    pause->setEnabled(session->canPause());
    restart->setEnabled(session->canRestart());
    bool live = session->phase() == PluginSession::phaseStarting ||
                session->phase() == PluginSession::phaseRunning;
    stop->setEnabled(live);
    updateLabel();
}

void PluginSessionControls::slotQuestions()
{
    qDeleteAll(choices);
    choices.clear();
    if (!session || session->questions().isEmpty()) {
        answers->hide();
        return;
    }
    // one question at a time, the oldest first (EDIT-PLUG-43)
    const PluginSession::Question& q = session->questions().first();
    question->setText(tr("? %1").arg(q.text));
    answerLine->clear();
    QPointer<PluginSession> s = session;
    for (const QString& choice: q.choices) {
        QPushButton* b = new QPushButton(choice, answers);
        choiceLayout->addWidget(b);
        choices.append(b);
        connect(b, &QPushButton::clicked, this, [s, choice]() { if (s) s->answer(choice); });
    }
    answers->show();
    answerLine->setFocus();
    emit asking();
}

PluginSessionTab::PluginSessionTab(PluginRunner* run, CyberiadaSMEditorDocument* doc, QWidget* parent):
    QWidget(parent), session(run->session()), document(doc)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    QHBoxLayout* row = new QHBoxLayout();
    events = new QComboBox(this);
    events->setEditable(true);
    events->setInsertPolicy(QComboBox::NoInsert);
    events->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    post = new QPushButton(tr("Post"), this);
    end = new QPushButton(this);
    load = new QPushButton(tr("Load Stream..."), this);
    edit = new QPushButton(tr("Edit Stream..."), this);
    save = new QPushButton(tr("Save Log..."), this);
    row->addWidget(new QLabel(tr("Event"), this));
    row->addWidget(events, 1);
    row->addWidget(post);
    row->addWidget(end);
    row->addStretch();
    row->addWidget(load);
    row->addWidget(edit);
    row->addWidget(save);
    layout->addLayout(row);
    output = new QLabel(this);
    layout->addWidget(output);
    history = new QTreeWidget(this);
    history->setColumnCount(historyColumns);
    history->setHeaderLabels(QStringList() << tr("Time") << tr("Event") << tr("Origin") << tr("Outcome")
                                           << tr("Fired"));
    history->setRootIsDecorated(false);
    history->header()->setStretchLastSection(true);
    layout->addWidget(history, 1);

    historyTimer.setSingleShot(true);
    historyTimer.setInterval(RUN_MARK_INTERVAL_MS);
    connect(&historyTimer, &QTimer::timeout, this, &PluginSessionTab::slotHistory);
    connect(post, &QPushButton::clicked, this, &PluginSessionTab::slotPost);
    connect(events->lineEdit(), &QLineEdit::returnPressed, this, &PluginSessionTab::slotPost);
    connect(load, &QPushButton::clicked, this, &PluginSessionTab::askLoadStream);
    connect(edit, &QPushButton::clicked, this, &PluginSessionTab::askEditStream);
    connect(save, &QPushButton::clicked, this, &PluginSessionTab::askSaveLog);
    connect(history, &QTreeWidget::itemActivated, this, &PluginSessionTab::slotItemActivated);
    connect(history, &QTreeWidget::itemClicked, this, &PluginSessionTab::slotItemActivated);
    QPointer<PluginSession> s = session;
    connect(end, &QPushButton::clicked, this, [s]() {
        if (s) s->post(s->model().end);
    });
    if (session) {
        connect(session, &PluginSession::modelReceived, this, &PluginSessionTab::slotState);
        connect(session, &PluginSession::stateChanged, this, &PluginSessionTab::slotState);
        connect(session, &PluginSession::historyChanged, this, [this]() {
            if (!historyTimer.isActive()) historyTimer.start();
        });
        connect(session, &PluginSession::requestDone, this, &PluginSessionTab::slotRequestDone);
    }
    slotState();
}

bool PluginSessionTab::isRunning() const
{
    return session && session->phase() != PluginSession::phaseEnded;
}

void PluginSessionTab::slotState()
{
    if (!session) return;
    const PluginSession::Model& m = session->model();
    const PluginSession::State& st = session->state();
    bool offered = !m.events.isEmpty() || !m.end.isEmpty();
    events->setVisible(offered);
    post->setVisible(offered);
    end->setVisible(!m.end.isEmpty());
    end->setText(m.end);
    load->setVisible(m.stream);
    edit->setVisible(m.stream);
    save->setVisible(m.save);
    output->setVisible(m.output);
    output->setText(tr("Output: %1").arg(st.output.join(' ')));
    // the events the machine reacts to now, else the ones the model offers
    QStringList names = st.events.isEmpty() ? m.events : st.events;
    QStringList shown;
    for (int i = 0; i < events->count(); i++) shown << events->itemText(i);
    if (shown != names) {
        QString text = events->currentText();
        events->clear();
        events->addItems(names);
        events->setEditText(text);
    }
    events->setEnabled(session->canPost());
    post->setEnabled(session->canPost());
    end->setEnabled(session->canPost());
    load->setEnabled(session->canStream());
    edit->setEnabled(session->canStream());
    save->setEnabled(session->canSave());
}

void PluginSessionTab::finish()
{
    slotHistory();
    const QList<QWidget*> entries = QList<QWidget*>() << events << post << end << load << edit << save;
    for (QWidget* w: entries) w->setEnabled(false);
}

QString PluginSessionTab::elementName(const std::string& id) const
{
    const Cyberiada::Element* el = document ? document->model()->idToElement(QString::fromStdString(id)) : nullptr;
    if (!el || el->get_name().empty()) return QString::fromStdString(id);
    return QString::fromStdString(el->get_name());
}

// a transition as source -> target, any other element by its name
QString PluginSessionTab::elementText(const QString& id) const
{
    const Cyberiada::Element* el = document ? document->model()->idToElement(id) : nullptr;
    if (el && el->get_type() == Cyberiada::elementTransition) {
        const Cyberiada::Transition* t = static_cast<const Cyberiada::Transition*>(el);
        return QString("%1 -> %2").arg(elementName(t->source_element_id()), elementName(t->target_element_id()));
    }
    return elementName(id.toStdString());
}

void PluginSessionTab::slotHistory()
{
    if (!session) return;
    history->clear();
    QList<QTreeWidgetItem*> items;
    for (const PluginSession::HistoryRow& r: session->history()) {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        if (r.hasTime) item->setText(columnTime, QString::number(r.time, 'f', TIME_DECIMALS));
        item->setText(columnEvent, r.parameters.isEmpty() ? r.event : r.event + " " + r.parameters);
        item->setText(columnOrigin, r.origin);
        item->setText(columnOutcome, r.pending ? tr("pending") : r.outcome);
        QStringList fired;
        for (const QString& id: r.fired) fired << elementText(id);
        item->setText(columnFired, fired.join(", "));
        item->setToolTip(columnFired, r.fired.join(", "));
        if (!r.fired.isEmpty()) item->setData(columnFired, Qt::UserRole, r.fired.first());
        items.append(item);
    }
    history->addTopLevelItems(items);
    history->scrollToBottom();
}

void PluginSessionTab::slotPost()
{
    if (!session) return;
    QString error;
    if (session->post(events->currentText(), &error)) {
        events->setEditText(QString());
    } else {
        emit message(tr("the event is not posted: %1").arg(error), true);
    }
}

void PluginSessionTab::slotItemActivated(QTreeWidgetItem* item, int)
{
    QString id = item ? item->data(columnFired, Qt::UserRole).toString() : QString();
    if (!id.isEmpty() && document) emit elementActivated(document, id);
}

void PluginSessionTab::slotRequestDone(int, bool ok, const QString& text)
{
    if (!ok) emit message(tr("the plugin refused: %1").arg(text), true);
}

void PluginSessionTab::focusEventLine()
{
    if (events->isVisible() && events->isEnabled()) events->setFocus();
}

void PluginSessionTab::askLoadStream()
{
    if (!session || !session->canStream()) return;
    QString folder = document && document->hasFile() ? QFileInfo(document->filePath()).absolutePath()
                                                     : QDir::homePath();
    QString path = QFileDialog::getOpenFileName(this, tr("Load a stream"), folder,
                                                tr("Streams (*.log *.txt);;All files (*)"));
    if (!path.isEmpty()) session->loadStream(path);
}

void PluginSessionTab::askEditStream()
{
    if (!session || !session->canStream()) return;
    MultilineTextDialog dialog(tr("Edit the stream"), tr("Stream"), session->streamText(), codeRoleNone,
                               nullptr, this);
    if (dialog.exec() == QDialog::Accepted) session->editStream(dialog.text());
}

void PluginSessionTab::askSaveLog()
{
    if (!session || !session->canSave()) return;
    QString name = document && document->hasFile() ? QFileInfo(document->filePath()).completeBaseName()
                                                    : QString("session");
    QString folder = document && document->hasFile() ? QFileInfo(document->filePath()).absolutePath()
                                                     : QDir::homePath();
    QString path = QFileDialog::getSaveFileName(this, tr("Save the log"),
                                                QDir(folder).filePath(name + LOG_FILE_SUFFIX),
                                                tr("Streams (*.log *.txt);;All files (*)"));
    if (!path.isEmpty()) session->saveLog(path);
}
