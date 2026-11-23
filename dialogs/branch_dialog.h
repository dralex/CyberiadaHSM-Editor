/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The new branch dialog: its name, its start and the switch
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

#ifndef CYBERIADA_BRANCH_DIALOG
#define CYBERIADA_BRANCH_DIALOG

#include <functional>
#include <QDialog>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

// New Branch... (EDIT-GIT-20): the name, the start - the current commit or a local branch - and
// Switch to it; the branch is made by the caller, a name git refuses keeps the dialog open
class BranchDialog: public QDialog {
Q_OBJECT
public:
    // create: runs git, false with git's message; start empty for the current commit
    using Create = std::function<bool(const QString& name, const QString& start, bool switchTo, QString* error)>;

    BranchDialog(const QStringList& branches, const Create& create, QWidget* parent = nullptr);

    QString      name() const;
    // the local branch chosen, empty for the current commit
    QString      start() const;
    bool         switchTo() const;
    QLineEdit*   nameEdit() const { return m_name; }
    QComboBox*   startBox() const { return m_start; }
    QCheckBox*   switchBox() const { return m_switch; }
    QPushButton* createButton() const { return m_createButton; }
    QString      errorText() const;

private:
    void         tryCreate();

    Create       m_create;
    QLineEdit*   m_name;
    QComboBox*   m_start;
    QCheckBox*   m_switch;
    QLabel*      m_error;
    QPushButton* m_createButton;
};

#endif
