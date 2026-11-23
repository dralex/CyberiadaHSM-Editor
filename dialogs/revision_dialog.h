/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The revision of Compare with a Revision...
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

#ifndef CYBERIADA_REVISION_DIALOG
#define CYBERIADA_REVISION_DIALOG

#include <functional>
#include <QDialog>
#include <QVector>

#include "repository_status.h"

class QComboBox;
class QLabel;

// Compare with a Revision... (EDIT-GIT-13): HEAD and the last commits of the file offered, a
// branch, a tag or a hash typed; a revision the check refuses keeps the dialog open
class RevisionDialog: public QDialog {
Q_OBJECT
public:
    // true for a revision git knows, else false with the reason
    using Check = std::function<bool(const QString& revision, QString* error)>;

    RevisionDialog(const QString& file, const QVector<LogEntry>& commits, const Check& check,
                   QWidget* parent = nullptr);

    // the hash of a commit chosen from the list, else the text typed
    QString    revision() const;
    QComboBox* revisionBox() const { return m_revision; }
    QString    errorText() const;

private:
    void       tryAccept();

    Check      m_check;
    QComboBox* m_revision;
    QLabel*    m_error;
};

#endif
