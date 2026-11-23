/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Insert Formula dialog of a Markdown text
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

#ifndef INSERT_FORMULA_DIALOG_H
#define INSERT_FORMULA_DIALOG_H

#include <QChar>
#include <QDialog>
#include <QList>
#include <QPixmap>
#include <QTabWidget>

class QLabel;
class QToolButton;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QTimer;

// where a formula goes: the characters around it in its line (null at a line end), and a
// display one as a paragraph of its own (an insert without a selection)
struct FormulaPlace {
    QChar before;
    QChar after;
    bool  displayBlock = false;
};

// the palette of EDIT-MD-27: a tab of buttons per formulaPalette() tab, each drawn as its face
class FormulaPalette : public QTabWidget {
    Q_OBJECT

public:
    explicit FormulaPalette(QWidget* parent = nullptr);
    // the buttons of a tab in the order of its entries, for the tests
    QList<QToolButton*> buttons(int tab) const;

signals:
    void entryChosen(const QString& snippet);
};

// Insert Formula and Edit Formula (EDIT-MD-24/25): the TeX, the kind, the preview and the text
// written (docs/MARKDOWN.md, Insert Formula)
class InsertFormulaDialog : public QDialog {
    Q_OBJECT

public:
    InsertFormulaDialog(const QString& tex, bool display, const FormulaPlace& place, bool editing,
                        QWidget* parent = nullptr);

    QString tex() const;
    bool    display() const;
    // what Insert or Replace writes (EDIT-MD-26), empty while the TeX is blank
    QString insertedText() const;
    // the preview drawn now, not after the pause
    void    renderPreview();
    // the formula drawn, null while the TeX is blank
    QPixmap preview() const { return m_preview; }

    // the controls, for the tests
    FormulaPalette* palette() const { return m_palette; }
    QPlainTextEdit* texEdit() const { return m_texEdit; }
    QRadioButton*   inlineChoice() const { return m_inlineRadio; }
    QRadioButton*   displayChoice() const { return m_displayRadio; }
    QLabel*         previewLabel() const { return m_previewLabel; }
    QLabel*         errorLabel() const { return m_errorLabel; }
    QLabel*         lineLabel() const { return m_lineLabel; }
    QPushButton*    insertButton() const { return m_insertButton; }

private slots:
    void refresh();
    // an entry of the palette at the cursor of the TeX, one undo step of the field
    void insertSnippet(const QString& snippet);

private:
    FormulaPlace    m_place;
    FormulaPalette* m_palette;
    QPlainTextEdit* m_texEdit;
    QRadioButton*   m_inlineRadio;
    QRadioButton*   m_displayRadio;
    QLabel*         m_previewLabel;
    QLabel*         m_errorLabel;
    QLabel*         m_lineLabel;
    QPushButton*    m_insertButton;
    QTimer*         m_previewTimer;
    QPixmap         m_preview;
};

#endif // INSERT_FORMULA_DIALOG_H
