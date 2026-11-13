/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The text tab: a plain, a Markdown or a source text file
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

#ifndef CYBERIADA_TEXT_DOCUMENT
#define CYBERIADA_TEXT_DOCUMENT

#include "editor_document.h"

class QStackedWidget;
class QTextDocument;
class CodeEditor;
class MarkdownView;
class CodeHighlighter;

// A text tab (docs/MODES.md, Text tabs): the text in UTF-8, highlighted by its
// mode; a read-only Markdown tab shows the rendered document (EDIT-MODE-13/14).
// The undo stack mirrors the edits of the text, so the window's Undo and Redo
// and the clean state work as for a diagram
class TextDocument: public EditorDocument {
Q_OBJECT
public:
    TextDocument(const QString& mode, QObject* parent = nullptr);
    ~TextDocument();

    QString          mode() const override { return m_mode; }
    // another text mode: the text stays, the highlighting and the marker change
    bool             setMode(const QString& mode);
    // the languages changed: a language gone leaves Plain text (EDIT-PLUG-60)
    void             refreshLanguage();
    QString          title() const override;
    QString          filePath() const override { return m_path; }
    bool             isClean() const override;
    QWidget*         page() const override;
    QUndoStack*      undoStack() const override { return m_undo; }

    // read the file as UTF-8 with a clean history
    bool             load(const QString& path, QString* error);
    bool             save(QString* error) override;
    // and no plugin run locks the text (EDIT-PLUG-22)
    bool             isEditable() const override { return !isReadOnly() && !m_locked; }
    void             setLocked(bool on);
    bool             isLocked() const { return m_locked; }
    // the whole text replaced as one undo step of the label; false when it is the
    // same text (EDIT-PLUG-12/57)
    bool             replaceText(const QString& text, const QString& label);
    // write the text to the file, which becomes the document's
    bool             saveAs(const QString& path, QString* error);
    QString          text() const;
    void             setText(const QString& text);
    CodeEditor*      editor() const { return m_editor; }
    // the rendered view of a read-only Markdown tab is shown
    bool             showsRendered() const;
    // the directory of the file, where the Markdown images are found; empty without a file
    QString          baseDir() const;
    // the rendered Markdown text saved as a PDF (EDIT-MODE-18)
    bool             exportPdf(const QString& path, QString* error) const;
    // an empty unmodified tab a file may take
    bool             isUntitled() const { return !hasFile() && isClean() && text().isEmpty(); }

protected:
    void             applyReadOnlyFlag(bool on) override;

private:
    void             applyMode();
    void             showPage();
    bool             write(const QString& path, QString* error);

    QString          m_mode;
    QString          m_path;
    QTextDocument*   m_document;
    CodeEditor*      m_editor;
    MarkdownView*    m_rendered;
    QStackedWidget*  m_page;
    QUndoStack*      m_undo;
    CodeHighlighter* m_highlighter = nullptr;
    bool             m_locked = false;
    // the name of the next undo step, "typing" when empty
    QString          m_stepName;
};

#endif
