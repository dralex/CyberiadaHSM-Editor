/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The State Action Dialog
 *
 * Copyright (C) 2025 Anastasia Viktorova <viktorovaa.04@gmail.com>
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

#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QTextCursor>

#include "stateactiondialog.h"
#include "fontmanager.h"
#include "cyberiadasm_editor_transition_item.h"

StateActionDialog::StateActionDialog(const QString& keyword, QWidget* parent):
    QDialog(parent), mode(Mode::EntryExit), keyword(keyword)
{
    // the keyword is part of the text: the behaviour continues on the same
    // line or, as in the document format, on the next one
    setupUi(tr("New action"), tr("Action:"), keyword + "/");
}

StateActionDialog::StateActionDialog(Mode mode, QWidget* parent):
    QDialog(parent), mode(mode)
{
    setupUi(tr("New internal transition"),
            tr("Internal transition (EVENT [guard] / behaviour):"), QString());
}

void StateActionDialog::setupUi(const QString& title, const QString& label, const QString& prefill)
{
    setWindowTitle(title);
    auto* layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(label, this));

    actionEdit = new QPlainTextEdit(this);
    actionEdit->setFont(FontManager::instance().font(fontRoleStateAction));
    actionEdit->setPlainText(prefill);
    QTextCursor cursor = actionEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    actionEdit->setTextCursor(cursor);
    layout->addWidget(actionEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &StateActionDialog::slotAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    actionEdit->setFocus();
}

bool StateActionDialog::parseInput()
{
    QString text = actionEdit->toPlainText();
    if (mode == Mode::Transition) {
        // reuse the edge-label parser; an internal transition needs an event
        TransitionAction::parseLabel(text, trigger, guard, behaviour);
        return !trigger.isEmpty();
    }
    if (!text.startsWith(keyword + "/")) return false;
    // the same-line space or the document-style newline after the keyword is
    // the separator, not the behaviour; the model normalizes the interior
    behaviour = text.mid(keyword.length() + 1).trimmed();
    return true;
}

QString StateActionDialog::getTrigger() const
{
    return trigger;
}

QString StateActionDialog::getGuard() const
{
    return guard;
}

QString StateActionDialog::getBehaviour() const
{
    return behaviour;
}

void StateActionDialog::slotAccept()
{
    if (!parseInput()) {
        if (mode == Mode::Transition) {
            QMessageBox::warning(this, tr("New internal transition"),
                                 tr("An internal transition needs an event."));
        } else {
            QMessageBox::warning(this, tr("New action"),
                                 tr("The action must start with '%1/'").arg(keyword));
        }
        return;
    }
    accept();
}
