/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Insert Link dialog of a Markdown text
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

#ifndef INSERT_LINK_DIALOG_H
#define INSERT_LINK_DIALOG_H

#include <functional>

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;

// Insert Link (EDIT-MD-9): the text, the target - a heading of this text or of another
// Markdown file, a file, an address - and the line inserted
class InsertLinkDialog : public QDialog {
    Q_OBJECT

public:
    // the text's file and its current text; openText gives the text of a file's open tab,
    // a null string without one; the selection is the link text proposed
    InsertLinkDialog(const QString& textPath, const QString& ownText, const QString& selection,
                     const std::function<QString(const QString&)>& openText, QWidget* parent = nullptr);

    // as written: #anchor, file.md#anchor, a relative path or an address
    QString target() const;
    QString text() const;
    // [text](target), empty while either is missing
    QString insertedLine() const;

    // the controls, for the tests
    QRadioButton* headingChoice() const { return m_headingRadio; }
    QRadioButton* fileChoice() const { return m_fileRadio; }
    QRadioButton* addressChoice() const { return m_addressRadio; }
    QComboBox*    textBox() const { return m_textBox; }
    QComboBox*    headingBox() const { return m_headingBox; }
    QLineEdit*    fileEdit() const { return m_fileEdit; }
    QLineEdit*    addressEdit() const { return m_addressEdit; }
    QLineEdit*    textEdit() const { return m_textEdit; }
    QLabel*       noteLabel() const { return m_noteLabel; }
    QPushButton*  insertButton() const { return m_insertButton; }
    // a Markdown text chosen as Browse chooses it: added to the list when outside it
    void          chooseText(const QString& path);
    // a file chosen as Browse chooses it, written relative to the text
    void          chooseFile(const QString& path);

private slots:
    void browseText();
    void browseFile();
    void textSourceChanged();
    void refresh();

private:
    // the absolute path of the chosen text, empty for this text
    QString chosenText() const;
    QString relative(const QString& path) const;

    QString       m_textPath;
    QString       m_baseDir;
    QString       m_ownText;
    std::function<QString(const QString&)> m_openText;
    bool          m_textTouched = false;
    QLineEdit*    m_textEdit;
    QRadioButton* m_headingRadio;
    QRadioButton* m_fileRadio;
    QRadioButton* m_addressRadio;
    QComboBox*    m_textBox;
    QPushButton*  m_browseTextButton;
    QComboBox*    m_headingBox;
    QLineEdit*    m_fileEdit;
    QPushButton*  m_browseFileButton;
    QLineEdit*    m_addressEdit;
    QLabel*       m_lineLabel;
    QLabel*       m_noteLabel;
    QPushButton*  m_insertButton;
};

#endif // INSERT_LINK_DIALOG_H
