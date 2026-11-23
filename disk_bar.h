/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The bar of a tab whose file changed on disk
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

#ifndef CYBERIADA_DISK_BAR
#define CYBERIADA_DISK_BAR

#include <QWidget>

#include "editor_document.h"

class QLabel;
class QPushButton;

// The bar above the page of the active tab (EDIT-DOC-15/16, docs/GIT.md): what
// happened to its file and the answers; the window acts on them
class DiskBar: public QWidget {
Q_OBJECT
public:
    explicit DiskBar(QWidget* parent = nullptr);

    // the status of the file of the tab named; diskOk hides the bar
    void         setStatus(EditorDocument::DiskStatus status, const QString& name);
    QString      text() const;

    QPushButton* reloadButton() const { return m_reload; }
    QPushButton* keepButton() const { return m_keep; }
    QPushButton* compareButton() const { return m_compare; }
    QPushButton* closeButton() const { return m_close; }

signals:
    void         reloadRequested();
    void         keepRequested();
    void         compareRequested();
    void         closeRequested();

private:
    QLabel*      m_label;
    QPushButton* m_reload;
    QPushButton* m_keep;
    QPushButton* m_compare;
    QPushButton* m_close;
};

#endif
