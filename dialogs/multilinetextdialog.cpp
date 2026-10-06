/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Multiline Text Dialog
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

#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QLabel>
#include <QDialogButtonBox>
#include <QTextCursor>

#include "multilinetextdialog.h"
#include "fontmanager.h"

MultilineTextDialog::MultilineTextDialog(const QString& title, const QString& label,
                                         const QString& text, CodeRole role,
                                         const CodeLanguage* language, QWidget* parent):
    QDialog(parent)
{
    setWindowTitle(title);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(label, this));

    edit = new QPlainTextEdit(this);
    edit->setFont(FontManager::instance().font(fontRoleStateAction));
    // highlighted as the code of the role, by the document language (EDIT-TEXT-10)
    if (role != codeRoleNone) {
        new CodeHighlighter(edit->document(), role, language);
    }
    edit->setPlainText(text);
    QTextCursor cursor = edit->textCursor();
    cursor.movePosition(QTextCursor::End);
    edit->setTextCursor(cursor);
    layout->addWidget(edit);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    edit->setFocus();
}

QString MultilineTextDialog::text() const
{
    return edit->toPlainText();
}
