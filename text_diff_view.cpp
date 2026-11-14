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

#include <QMouseEvent>
#include <QPainter>

#include "text_diff_view.h"
#include "text_diff.h"
#include "cyberiada_constants.h"

DiffTextPane::DiffTextPane(QWidget* parent):
    CodeEditor(parent)
{
    setReadOnly(true);
    // the selection stays visible in the side the focus left: Copy acts on it
    setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
}

QString DiffTextPane::lineLabel(int block) const
{
    if (block < 0 || block >= m_lines.size() || m_lines[block] < 0) return QString();
    return QString::number(m_lines[block] + 1);
}

void DiffTextPane::focusInEvent(QFocusEvent* event)
{
    CodeEditor::focusInEvent(event);
    emit focused();
}

OverviewRuler::OverviewRuler(QWidget* parent):
    QWidget(parent)
{
    setFixedWidth(TEXT_DIFF_RULER_WIDTH);
    setCursor(Qt::PointingHandCursor);
}

void OverviewRuler::setRows(const QVector<int>& kinds)
{
    m_kinds = kinds;
    update();
}

int OverviewRuler::scaleRows() const
{
    int shown = 0;
    if (m_pane) {
        int line = m_pane->fontMetrics().lineSpacing();
        if (line > 0) shown = m_pane->viewport()->height() / line;
    }
    return qMax(m_kinds.size(), shown);
}

QSize OverviewRuler::sizeHint() const
{
    return QSize(TEXT_DIFF_RULER_WIDTH, 0);
}

// a mark per row that is not equal, at least a pixel high
void OverviewRuler::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::AlternateBase));
    if (m_kinds.isEmpty()) return;
    const double step = double(height()) / scaleRows();
    for (int row = 0; row < m_kinds.size(); row++) {
        const char* color = nullptr;
        switch (m_kinds[row]) {
        case TextDiff::rowRemoved: color = DIFF_REMOVED_COLOR; break;
        case TextDiff::rowAdded:   color = DIFF_ADDED_COLOR; break;
        case TextDiff::rowChanged: color = DIFF_CHANGED_COLOR; break;
        default:                   continue;
        }
        painter.fillRect(QRectF(0, row * step, width(), qMax(step, 1.0)), QColor(color));
    }
}

void OverviewRuler::mousePressEvent(QMouseEvent* event)
{
    if (m_kinds.isEmpty() || height() <= 0) return;
    int row = int(double(event->pos().y()) / height() * scaleRows());
    emit rowClicked(qBound(0, row, m_kinds.size() - 1));
}
