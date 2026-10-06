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

#ifndef MULTILINETEXTDIALOG_H
#define MULTILINETEXTDIALOG_H

#include <QDialog>

#include "code_highlighter.h"

class QPlainTextEdit;

// a plain multiline editor for a behaviour or a comment body (EDIT-TEXT-10);
// the text is highlighted as the role asks, by the document language
class MultilineTextDialog : public QDialog {
    Q_OBJECT

public:
    MultilineTextDialog(const QString& title, const QString& label, const QString& text,
                        CodeRole role = codeRoleNone, const CodeLanguage* language = nullptr,
                        QWidget* parent = nullptr);

    QString text() const;

private:
    QPlainTextEdit* edit;
};

#endif // MULTILINETEXTDIALOG_H
