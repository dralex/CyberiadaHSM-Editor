/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Color Schemes Panel of the Preferences
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

#ifndef CYBERIADA_TEXT_SCHEME_PANEL
#define CYBERIADA_TEXT_SCHEME_PANEL

#include <QWidget>
#include "text_scheme.h"

class QComboBox;
class QPushButton;
class QToolButton;
class CodeEditor;
class CodeHighlighter;

// The color schemes of the text editors (EDIT-COLOR-3/4, docs/COLOR-SCHEMES.md): the
// panel edits a copy, apply() stores it and makes the scheme shown current
class TextSchemePanel: public QWidget {
Q_OBJECT
public:
    explicit TextSchemePanel(QWidget* parent = nullptr);

    // the settings shown, the edits dropped
    void               load();
    void               apply();
    const TextSchemes& schemes() const { return m_schemes; }
    QString            shownId() const;
    // false for an id no scheme has
    bool               showScheme(const QString& id);
    // a color of the scheme shown, as a swatch sets it
    void               setColor(int role, const QColor& color);
    // Copy... and Rename... with the name given; false when it is empty or taken
    bool               copyScheme(const QString& name);
    bool               renameScheme(const QString& name);
    void               deleteScheme();
    void               restoreScheme();
    CodeEditor*        preview() const { return m_preview; }

private slots:
    void               schemeChosen();
    void               swatchClicked();
    void               copyClicked();
    void               renameClicked();

private:
    // the list with the scheme of the id shown
    void               fill(const QString& id);
    // the swatches, the buttons and the preview of the scheme shown
    void               showColors();
    // a name no other scheme has in any case
    bool               isFree(const QString& name, const QString& ownId) const;
    // a name asked; empty when cancelled or taken
    QString            askName(const QString& title, const QString& proposed, const QString& ownId);
    TextScheme*        shown();

    TextSchemes        m_schemes;
    QComboBox*         m_list;
    QPushButton*       m_copy;
    QPushButton*       m_rename;
    QPushButton*       m_delete;
    QPushButton*       m_restore;
    QToolButton*       m_swatches[textSchemeRolesCount];
    CodeEditor*        m_preview;
    CodeHighlighter*   m_highlighter;
};

#endif
