/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The parts of a text diff tab: the sides and the overview ruler
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

#ifndef CYBERIADA_TEXT_DIFF_VIEW
#define CYBERIADA_TEXT_DIFF_VIEW

#include <QVector>
#include <QWidget>

#include "code_editor.h"

// a side of a text diff tab: a row a block, read-only; a row is numbered by the line
// of its own text, a filler row not (EDIT-MODE-20)
class DiffTextPane: public CodeEditor {
    Q_OBJECT
public:
    DiffTextPane(QWidget* parent = nullptr);

    // the line of each row from 0, -1 for a filler
    void    setLineNumbers(const QVector<int>& lines) { m_lines = lines; update(); }
    QString lineLabel(int block) const override;

signals:
    void    focused();

protected:
    void    focusInEvent(QFocusEvent* event) override;

private:
    QVector<int> m_lines;
};

// where the changes lie in the whole text, beside the target side; a click scrolls
// both sides there (EDIT-MODE-21)
class OverviewRuler: public QWidget {
    Q_OBJECT
public:
    OverviewRuler(QWidget* parent = nullptr);

    // the TextDiff::RowKind of each row
    void    setRows(const QVector<int>& kinds);
    // the side it stands beside: a text shorter than its view maps row by row
    void    setPane(CodeEditor* pane) { m_pane = pane; }
    QSize   sizeHint() const override;

signals:
    void    rowClicked(int row);

protected:
    void    paintEvent(QPaintEvent* event) override;
    void    mousePressEvent(QMouseEvent* event) override;

private:
    // the rows the height stands for: all of them, at least those the view shows
    int     scaleRows() const;

    QVector<int> m_kinds;
    CodeEditor*  m_pane = nullptr;
};

#endif
