/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The find bar under the document area
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

#ifndef CYBERIADA_FIND_BAR
#define CYBERIADA_FIND_BAR

#include <QPalette>
#include <QWidget>

#include "find_query.h"

class QLabel;
class QLineEdit;
class QToolButton;

// The find bar (EDIT-FIND-1..3, docs/FIND.md): the field, the options, the steps
// and the counter; the window passes the query to the active tab
class FindBar: public QWidget {
Q_OBJECT
public:
    explicit FindBar(QWidget* parent = nullptr);

    FindQuery    query() const;
    // shown with the focus in the field, all of it selected; a seed replaces the text
    void         start(const QString& seed);
    // the counter: current from 0, -1 without one; more: past FIND_MATCH_LIMIT
    void         setResult(int current, int total, bool more);
    // a tab that cannot search greys the bar (EDIT-FIND-9)
    void         setSearchable(bool on);

    QLineEdit*   field() const { return m_field; }
    QToolButton* caseButton() const { return m_case; }
    QToolButton* wordsButton() const { return m_words; }
    QToolButton* regexpButton() const { return m_regexp; }
    QToolButton* previousButton() const { return m_previous; }
    QToolButton* nextButton() const { return m_next; }
    QToolButton* closeButton() const { return m_close; }
    QString      counterText() const;

signals:
    void         queryChanged(const FindQuery& query);
    void         nextRequested();
    void         previousRequested();
    // Esc or the close button: the bar is hidden
    void         closed();

protected:
    // Enter, Shift+Enter and Esc in the field
    bool         eventFilter(QObject* watched, QEvent* event) override;
    // Esc is the bar's ahead of the action it is a key of (EDIT-KEYS-11)
    bool         event(QEvent* event) override;
    void         keyPressEvent(QKeyEvent* event) override;

private slots:
    void         emitQuery();
    void         dismiss();

private:
    QToolButton* addOption(const QString& text, const QString& tip);

    QLineEdit*   m_field;
    QToolButton* m_case;
    QToolButton* m_words;
    QToolButton* m_regexp;
    QToolButton* m_previous;
    QToolButton* m_next;
    QLabel*      m_counter;
    QToolButton* m_close;
    QPalette     m_fieldPalette;
};

#endif
