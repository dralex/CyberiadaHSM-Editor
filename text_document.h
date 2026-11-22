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

#include <QImage>
#include <QPersistentModelIndex>

#include "editor_document.h"
#include "json_text.h"
#include "text_fill.h"
#include "markdown_outline.h"

class QSplitter;
class QStackedWidget;
class QTextCursor;
class QTextDocument;
class QTimer;
class CodeEditor;
class MarkdownView;
class MarkdownOutlineModel;
class CodeHighlighter;

// A text tab (docs/MODES.md, Text tabs): the text in UTF-8, highlighted by its
// mode; a read-only Markdown tab shows the rendered document (EDIT-MODE-13/14), a
// JSON tab marks its first error (EDIT-MODE-26).
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
    // a line as a paragraph of its own, blank lines around it, one undo step of the label
    // (EDIT-MD-2/12): before the line `before` from 1 (past the end after the last one), at 0
    // after the paragraph of the cursor or on its blank line; the cursor at its end; false
    // when the text is not editable
    bool             insertBlock(const QString& line, int before, const QString& label);
    // the text at the cursor, the selection replaced, one undo step of the label (EDIT-MD-9);
    // false when the text is not editable
    bool             insertInline(const QString& text, const QString& label);
    // a Markdown text fills as Markdown, a Plain text as plain, the other modes never
    // (EDIT-MD-18/23)
    TextFillKind     fillKind() const;
    // the writable source of a text that fills: Fill and Unfill act (EDIT-MD-20)
    bool             canFill() const;
    // the units of the cursor or the selection filled at the text width, or each joined into
    // one line, one undo step, the selection kept at its characters; the units of the lines
    // first..last from 1, the cursor after them; false when the text cannot fill or has not
    // the lines, true for a text that stays the same (EDIT-MD-20)
    bool             fill(bool unfill);
    bool             fillLines(int first, int last, bool unfill);
    // the heading of the anchor shown: its source line, or scrolled to on a rendered page;
    // false when no heading has it (EDIT-MD-10)
    bool             showHeading(const QString& anchor);
    // write the text to the file, which becomes the document's
    bool             saveAs(const QString& path, QString* error);
    QString          text() const;
    void             setText(const QString& text);
    CodeEditor*      editor() const { return m_editor; }
    // the rendered view of a read-only Markdown tab is shown
    bool             showsRendered() const;
    // the source and its rendered page side by side: a writable Markdown tab with View >
    // Preview (EDIT-MD-11)
    bool             showsPreview() const;
    // the preview page, null until it is first shown
    MarkdownView*    preview() const { return m_preview; }
    // the lines of the source from 1 (EDIT-KEYS-7): the cursor to the start of the
    // line, scrolled into view and focused unless not focus; false for a line the text has not
    bool             goToLine(int line, int column = 1, bool focus = true);
    int              lineCount() const;
    int              currentLine() const;
    // the column of the cursor from 1, a tab to the next stop (EDIT-MODE-31)
    int              currentColumn() const;
    // the directory of the file, where the Markdown images are found; empty without a file
    QString          baseDir() const;
    // the rendered Markdown text saved as a PDF (EDIT-MODE-18)
    bool             exportPdf(const QString& path, QString* error) const;
    // an empty unmodified tab a file may take
    bool             isUntitled() const { return !hasFile() && isClean() && text().isEmpty(); }
    // the built-in JSON language (EDIT-MODE-26)
    bool             isJson() const;
    // the last check of a JSON tab, ok in another mode; the edits of the last
    // JSON_CHECK_DELAY_MS are not checked yet
    const JsonCheck& jsonCheck() const { return m_jsonCheck; }
    // check the text now (EDIT-MODE-26)
    void             checkText();
    // a link target of the Markdown source opened (EDIT-MODE-28): a #anchor on its heading's
    // line, a file the editor opens by openFileRequested with its #fragment (EDIT-MD-10), another
    // file or a site by the system
    void             openLink(const QString& target);

    // the outline of a Markdown tab, null in another mode (EDIT-MD-13)
    MarkdownOutlineModel* outline() const { return m_outline; }
    // the text changed since the outline was last read
    bool             outlineOutOfDate() const { return m_outlineDirty; }
    // the dock shows the outline: read at once when out of date, then at most once a second
    // after a change; a hidden one is only marked out of date (EDIT-MD-14)
    void             setOutlineShown(bool on);
    void             readOutline();
    // the row of the cursor's place in the source, invalid on a rendered page (EDIT-MD-15)
    QModelIndex      outlineRowAtCursor() const;
    // the place of a row made current or entered, the page focused with focus (EDIT-MD-15/16):
    // the outline read first when out of date; the row the reading kept, else the only namesake
    // it added; the row shown, invalid when what it named is gone
    QModelIndex      showOutlineRow(const QPersistentModelIndex& row, const OutlineEntry& named, bool focus);

    // the shown page is searched, the editor or the rendered view (EDIT-FIND-5)
    bool             canFind() const override { return true; }
    void             setFindQuery(const FindQuery& query, bool navigate) override;
    void             moveFind(int step) override;
    void             clearFind(bool keep) override;
    QString          findSeed() const override;
    const FindRanges& findMatches() const { return m_findMatches; }
    int              findCurrent() const { return m_findCurrent; }

signals:
    // a file a link names that the editor opens in a tab (EDIT-MODE-28)
    void             openFileRequested(const QString& path, const QString& fragment);
    // an image dropped on or pasted into a Markdown source (EDIT-MD-2)
    void             imageOffered(const QString& path, const QImage& image);
    // the outline read from the text (EDIT-MD-14)
    void             outlineRead();

protected:
    void             applyReadOnlyFlag(bool on) override;

private:
    void             applyMode();
    // the source editor in the current color scheme (EDIT-COLOR-1/5)
    void             applyScheme();
    void             showPage();
    // the preview rendered again, its scroll kept; it follows the block of the cursor
    void             renderPreview();
    void             followCursor();
    bool             write(const QString& path, QString* error);
    // the editor or the rendered view, the one the stack shows
    QTextDocument*   shownDocument() const;
    QTextCursor      shownCursor() const;
    void             setShownCursor(const QTextCursor& cursor);
    // the start of the current match as the edits move it; -1 without one
    int              findPlace() const;
    void             setFindCurrent(int index);
    // search the shown page, the first match at or after place current; navigate
    // moves the cursor to it, a search after an edit does not (EDIT-FIND-8)
    void             runFind(int place, bool navigate);
    void             scheduleFind();
    void             markFind();
    void             showFindMatch();
    // a JSON text edited: checked after JSON_CHECK_DELAY_MS
    void             scheduleCheck();
    // the bar clicked: the cursor at the error
    void             showError();
    // the cursor on the source line of the heading an anchor names; false when none
    bool             goToHeading(const QString& anchor);
    bool             fillRange(int anchor, int position, bool unfill, bool select);
    // the state, the name and the mode on the mode line of the source (EDIT-MODE-31)
    void             updateModeLine();
    // the outline made or dropped with the Markdown mode
    void             applyOutline();
    // a change: the outline out of date, its reading started when shown and not waiting already
    void             markOutline();

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
    FindQuery        m_findQuery;
    FindRanges       m_findMatches;
    int              m_findCurrent = -1;
    bool             m_findMore = false;
    bool             m_findPending = false;
    QTextCursor*     m_findPlace = nullptr;
    QTimer*          m_checkTimer;
    // the editor stands in the splitter beside the preview while it is shown, else in the stack
    QSplitter*       m_previewPage = nullptr;
    MarkdownView*    m_preview = nullptr;
    QTimer*          m_previewTimer;
    // the source block the preview last followed
    int              m_previewBlock = -1;
    JsonCheck        m_jsonCheck;
    MarkdownOutlineModel* m_outline = nullptr;
    QTimer*          m_outlineTimer;
    bool             m_outlineDirty = false;
    bool             m_outlineShown = false;
    // the underlined token as the edits move it, null without an error
    QTextCursor*     m_errorPlace = nullptr;
};

#endif
