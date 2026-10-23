/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Comment Fragment Dialog
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

#ifndef FRAGMENTDIALOG_H
#define FRAGMENTDIALOG_H

#include <QDialog>
#include <QVector>

#include <cyberiada/cyberiadamlpp.h>

#include "code_highlighter.h"
#include "subject_fragment.h"

class QPlainTextEdit;
class QGridLayout;
class QComboBox;
class QLabel;
class QPushButton;

// the fragment of a comment subject picked in its target's text: one read-only box
// per field, a selection in one box at a time; the fragment and the type stay
// editable and are checked by the subject rules (EDIT-TOOL-16)
class FragmentDialog : public QDialog {
    Q_OBJECT

public:
    FragmentDialog(const Cyberiada::Element* target, Cyberiada::CommentSubjectType type,
                   const QString& fragment, const CodeLanguage* language = nullptr,
                   QWidget* parent = nullptr);

    Cyberiada::CommentSubjectType subjectType() const;
    QString fragment() const;

    // the field boxes in the search order, the box marked as the underlined one
    // (-1: none), the OK state and the note - for the tests
    int fieldCount() const { return boxes.size(); }
    QPlainTextEdit* fieldBox(int i) const;
    const SubjectFragment::Field& field(int i) const;
    int markedBox() const { return marked; }
    bool acceptable() const;
    QString note() const;
    QPlainTextEdit* fragmentEditor() const { return fragmentEdit; }
    QComboBox* typeSelector() const { return typeBox; }

private slots:
    void onSelectionChanged();
    void onFragmentEdited();
    void validate();

private:
    struct Box {
        QPlainTextEdit* edit = nullptr;
        SubjectFragment::Field field;
        QString caption;
    };
    int boxOf(const SubjectFragment::Field& field) const;
    void addRow(QGridLayout* grid, const QString& label, const QVector<int>& row);
    QPlainTextEdit* makeBox(const SubjectFragment::Field& field, const CodeLanguage* language);

    const Cyberiada::Element* target;
    bool transition;
    QVector<Box> boxes;
    QPlainTextEdit* fragmentEdit;
    QComboBox* typeBox;
    QLabel* noteLabel;
    QPushButton* okButton;
    bool selecting = false;
    // the selection the fragment came from: its box and offset (-1: typed)
    int selectedBox = -1;
    int selectedOffset = -1;
    int marked = -1;
};

#endif // FRAGMENTDIALOG_H
