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

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QStyle>
#include <QToolButton>

#include "find_bar.h"
#include "cyberiada_constants.h"

// the width of the field in average characters
static const int FIND_FIELD_CHARS = 32;

FindBar::FindBar(QWidget* parent):
    QWidget(parent)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(new QLabel(tr("Find:"), this));
    m_field = new QLineEdit(this);
    m_field->setClearButtonEnabled(true);
    m_field->setMinimumWidth(m_field->fontMetrics().averageCharWidth() * FIND_FIELD_CHARS);
    // the field keeps its width whatever the counter says
    m_field->setSizePolicy(QSizePolicy::Preferred, m_field->sizePolicy().verticalPolicy());
    m_field->installEventFilter(this);
    m_fieldPalette = m_field->palette();
    layout->addWidget(m_field);
    m_case = addOption("Aa", tr("Match case"));
    m_words = addOption("W", tr("Whole words"));
    m_regexp = addOption(".*", tr("Regular expression"));
    m_previous = new QToolButton(this);
    m_previous->setArrowType(Qt::UpArrow);
    m_previous->setAutoRaise(true);
    m_previous->setToolTip(tr("Find Previous"));
    layout->addWidget(m_previous);
    m_next = new QToolButton(this);
    m_next->setArrowType(Qt::DownArrow);
    m_next->setAutoRaise(true);
    m_next->setToolTip(tr("Find Next"));
    layout->addWidget(m_next);
    m_counter = new QLabel(this);
    layout->addWidget(m_counter);
    layout->addStretch();
    m_close = new QToolButton(this);
    m_close->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    m_close->setAutoRaise(true);
    m_close->setToolTip(tr("Close"));
    layout->addWidget(m_close);

    connect(m_field, &QLineEdit::textChanged, this, &FindBar::emitQuery);
    connect(m_previous, &QToolButton::clicked, this, &FindBar::previousRequested);
    connect(m_next, &QToolButton::clicked, this, &FindBar::nextRequested);
    connect(m_close, &QToolButton::clicked, this, &FindBar::dismiss);
}

QToolButton* FindBar::addOption(const QString& text, const QString& tip)
{
    QToolButton* button = new QToolButton(this);
    button->setText(text);
    button->setToolTip(tip);
    button->setCheckable(true);
    button->setAutoRaise(true);
    layout()->addWidget(button);
    connect(button, &QToolButton::toggled, this, &FindBar::emitQuery);
    return button;
}

FindQuery FindBar::query() const
{
    FindQuery q;
    q.text = m_field->text();
    q.matchCase = m_case->isChecked();
    q.wholeWords = m_words->isChecked();
    q.regexp = m_regexp->isChecked();
    return q;
}

void FindBar::start(const QString& seed)
{
    show();
    if (!seed.isEmpty()) m_field->setText(seed);
    m_field->selectAll();
    m_field->setFocus();
}

void FindBar::setResult(int current, int total, bool more)
{
    bool none = !m_field->text().isEmpty() && total == 0;
    if (m_field->text().isEmpty()) {
        m_counter->clear();
    } else if (none) {
        m_counter->setText(tr("No matches"));
    } else {
        m_counter->setText(tr("%1 of %2").arg(current + 1).arg(QString::number(total) + (more ? "+" : "")));
    }
    // a tint dark enough for black text whatever the theme
    QPalette palette = m_fieldPalette;
    if (none) {
        palette.setColor(QPalette::Base, QColor(FIND_NO_MATCH_COLOR));
        palette.setColor(QPalette::Text, Qt::black);
    }
    m_field->setPalette(palette);
}

void FindBar::setSearchable(bool on)
{
    setEnabled(on);
    if (!on) setResult(-1, 0, false);
}

QString FindBar::counterText() const
{
    return m_counter->text();
}

void FindBar::emitQuery()
{
    emit queryChanged(query());
}

void FindBar::dismiss()
{
    hide();
    emit closed();
}

bool FindBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_field && event->type() == QEvent::KeyPress) {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if (key->modifiers() & Qt::ShiftModifier) emit previousRequested();
            else emit nextRequested();
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            dismiss();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool FindBar::event(QEvent* event)
{
    if (event->type() == QEvent::ShortcutOverride &&
        static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        event->accept();
        return true;
    }
    return QWidget::event(event);
}

void FindBar::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        dismiss();
        return;
    }
    QWidget::keyPressEvent(event);
}
