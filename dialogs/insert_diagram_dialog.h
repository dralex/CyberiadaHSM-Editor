/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Insert Diagram dialog of a Markdown text
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

#ifndef INSERT_DIAGRAM_DIALOG_H
#define INSERT_DIAGRAM_DIALOG_H

#include <QDialog>
#include <QList>

class QComboBox;
class QFontComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;

// a diagram tab the dialog offers: its saved file is drawn
struct DiagramTab {
    QString title;
    QString path;
    bool    modified = false;
};

// Insert Diagram (EDIT-MD-5): an open diagram tab or a file, the folder it is copied to
// when it lies outside the directory of the text, the font, the alt text, the files written
// and the line inserted
class InsertDiagramDialog : public QDialog {
    Q_OBJECT

public:
    InsertDiagramDialog(const QList<DiagramTab>& tabs, const QString& baseDir, const QString& folder,
                        const QString& font, QWidget* parent = nullptr);

    // the diagram chosen, absolute
    QString source() const;
    // it lies outside the directory of the text, so it is copied
    bool    copies() const;
    // the diagram the line links: the copy or the source
    QString target() const;
    // the image beside the target
    QString svg() const;
    QString alt() const;
    QString font() const;
    // the folder to remember, empty when nothing is copied
    QString folder() const;
    QString insertedLine() const;

    // the controls, for the tests
    QRadioButton* tabChoice() const { return m_tabRadio; }
    QRadioButton* fileChoice() const { return m_fileRadio; }
    QComboBox*    tabBox() const { return m_tabBox; }
    QLineEdit*    fileEdit() const { return m_fileEdit; }
    QLineEdit*    folderEdit() const { return m_folderEdit; }
    QLineEdit*    altEdit() const { return m_altEdit; }
    QLabel*       noteLabel() const { return m_noteLabel; }
    QPushButton*  insertButton() const { return m_insertButton; }
    void          setFile(const QString& path);

private slots:
    void browse();
    void sourceChanged();
    void refresh();

private:
    QList<DiagramTab> m_tabs;
    QString        m_baseDir;
    QString        m_lastSource;
    bool           m_altTouched = false;
    QRadioButton*  m_tabRadio;
    QRadioButton*  m_fileRadio;
    QComboBox*     m_tabBox;
    QLineEdit*     m_fileEdit;
    QPushButton*   m_browseButton;
    QLineEdit*     m_folderEdit;
    QFontComboBox* m_fontBox;
    QLineEdit*     m_altEdit;
    QLabel*        m_writesLabel;
    QLabel*        m_lineLabel;
    QLabel*        m_noteLabel;
    QPushButton*   m_insertButton;
};

#endif // INSERT_DIAGRAM_DIALOG_H
