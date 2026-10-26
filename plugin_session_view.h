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

#ifndef PLUGIN_SESSION_VIEW_H
#define PLUGIN_SESSION_VIEW_H

#include <QList>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <string>

class QComboBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class PluginRunner;
class PluginSession;
class CyberiadaSMEditorDocument;

// the run row of a session (EDIT-PLUG-39): the status, Step with its units, Run
// with its speeds, Pause, Restart and Stop; the answer bar under it (EDIT-PLUG-43)
class PluginSessionControls : public QWidget {
    Q_OBJECT

public:
    PluginSessionControls(PluginRunner* run, const QString& title, QWidget* parent = nullptr);

    void          tick(qint64 elapsedMs);
    QPushButton*  stopButton() const { return stop; }
    QToolButton*  stepButton() const { return stepTool; }
    QLineEdit*    answerEdit() const { return answerLine; }
    QWidget*      answerBar() const { return answers; }

signals:
    // a question is open: the dock is raised
    void          asking();

private slots:
    void          slotState();
    void          slotQuestions();

private:
    void          updateLabel();
    QString       unitName(const QString& unit) const;
    QString       speedName(double speed) const;

    QPointer<PluginSession> session;
    QString       title;
    qint64        elapsed = 0;
    QLabel*       label;
    QToolButton*  stepTool;
    QMenu*        units;
    QToolButton*  runTool;
    QMenu*        speeds;
    QPushButton*  pause;
    QPushButton*  restart;
    QPushButton*  stop;
    QWidget*      answers;
    QLabel*       question;
    QLineEdit*    answerLine;
    QHBoxLayout*  choiceLayout;
    QList<QPushButton*> choices;
};

// the tab "<document> session" (EDIT-PLUG-42, EDIT-PLUG-44): the event line, the
// stream entries, the output symbols and the history
class PluginSessionTab : public QWidget {
    Q_OBJECT

public:
    PluginSessionTab(PluginRunner* run, CyberiadaSMEditorDocument* doc, QWidget* parent = nullptr);

    bool          isRunning() const;
    QTreeWidget*  historyList() const { return history; }
    QComboBox*    eventLine() const { return events; }
    // the entries of the Session menu (EDIT-PLUG-46)
    void          focusEventLine();
    void          askLoadStream();
    void          askEditStream();
    void          askSaveLog();
    // the session has ended: the history stays, the entries go grey
    void          finish();

signals:
    void          elementActivated(CyberiadaSMEditorDocument* doc, const QString& elementId);
    void          message(const QString& text, bool isError);

private slots:
    void          slotState();
    void          slotHistory();
    void          slotPost();
    void          slotItemActivated(QTreeWidgetItem* item, int column);
    void          slotRequestDone(int id, bool ok, const QString& text);

private:
    QString       elementText(const QString& id) const;
    QString       elementName(const std::string& id) const;

    QPointer<PluginSession> session;
    QPointer<CyberiadaSMEditorDocument> document;
    QComboBox*    events;
    QPushButton*  post;
    QPushButton*  end;
    QPushButton*  load;
    QPushButton*  edit;
    QPushButton*  save;
    QLabel*       output;
    QTreeWidget*  history;
    QTimer        historyTimer;
};

#endif
