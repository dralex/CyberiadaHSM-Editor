/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The page of a text tab: the plain text editor with the line numbers
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

#ifndef CYBERIADA_CODE_EDITOR
#define CYBERIADA_CODE_EDITOR

#include <QCursor>
#include <QImage>
#include <QPlainTextEdit>
#include <QRectF>
#include <QVector>
#include "autoreplace.h"
#include "text_fill.h"

struct TextScheme;

// The text of a text tab (docs/MODES.md, Text tabs): the line numbers in a margin, an
// error bar under the text; Undo and Redo belong to the window, so the undo stack of the
// tab is the one the edits go through (EDIT-MODE-13)
class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    CodeEditor(QWidget* parent = nullptr);

    int  lineNumberAreaWidth() const;
    void paintLineNumbers(QPaintEvent* event);
    // the number shown at a block from 0: its own number; a diff side has its lines
    virtual QString lineLabel(int block) const { return QString::number(block + 1); }
    // what the text is for the typed replacements (EDIT-AUTO-2/3): none unless set
    void setAutoreplaceKind(AutoreplaceKind kind) { m_kind = kind; }
    AutoreplaceKind autoreplaceKind() const { return m_kind; }
    // what the text fills as when Fill as you type is on (EDIT-MD-21): none unless set
    void setFillKind(TextFillKind kind) { m_fillKind = kind; }
    TextFillKind fillKind() const { return m_fillKind; }
    // the page, the selection and the margin in the colors of the scheme (EDIT-COLOR-1)
    void setScheme(const TextScheme& scheme);
    // an error of the text (EDIT-MODE-26): the number of its line in red, the message in a
    // bar under the text; line 0 and an empty message clear them
    void setError(int line, const QString& message);
    int  errorLine() const { return m_errorLine; }
    QString errorMessage() const { return m_errorMessage; }
    // the bar, 0 high without a message
    int  errorBarHeight() const;
    void paintErrorBar(QPaintEvent* event);
    // the link target under a point of the viewport: the anchor of the highlighter's format
    // under the glyph there, empty when none (EDIT-MODE-28)
    QString linkAt(const QPoint& pos) const;
    // the cursor a block over the character, one space of the font wide, else a line; the
    // editors follow the setting, the scheme preview the choice shown (EDIT-MODE-29)
    void setBlockCursor(bool on);
    bool blockCursor() const { return m_blockCursor; }
    // a dropped or pasted image is offered instead of its text (EDIT-MD-2): a Markdown text
    void setImageDrops(bool on) { m_imageDrops = on; }
    bool imageDrops() const { return m_imageDrops; }
    // the long lines wrapped at the width, at any character, a sign in a right fringe at the end
    // of each row that continues; else scrolled (EDIT-MODE-30). The editors follow the setting,
    // the scheme preview the choice shown; a text diff side is never wrapped (its rows align)
    void setWrapLines(bool on);
    bool wrapLines() const { return m_wrap && m_wrapAllowed; }
    void setWrapAllowed(bool on);
    // the fringe, 0 wide without the wrapping
    int  fringeWidth() const;
    void paintFringe(QPaintEvent* event);
    // the rows of the visible blocks that continue on the next row, in viewport coordinates
    QVector<QRectF> continuationRows() const;

signals:
    // the bar is clicked
    void errorClicked();
    // a link target is Ctrl+clicked
    void linkActivated(const QString& target);
    // an image file or a clipboard image dropped or pasted at the cursor, after the event
    void imageOffered(const QString& path, const QImage& image);
    // the name of the undo step of the edit that follows, empty after it (EDIT-MD-21)
    void editStepNamed(const QString& name);

protected:
    bool event(QEvent* event) override;
    bool viewportEvent(QEvent* event) override;
    bool canInsertFromMimeData(const QMimeData* source) const override;
    void insertFromMimeData(const QMimeData* source) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void updateMargin();
    void placeErrorBar();
    void updateLineNumbers(const QRect& rect, int dy);

private:
    // the tab stop and the margins, measured in the font
    void applyFontMetrics();
    void applyCursor();
    void applyWrap();
    // a Space or a Return typed past the text width breaks the line, an undo step of its own
    void autoFill();
    void placeLineNumbers();
    void placeFringe();
    // the hand over a link target while Ctrl is held, the editor's pointer elsewhere
    void updateLinkPointer(const QPoint& pos, bool ctrl);

    QWidget* lineNumbers;
    QWidget* errorBar;
    QWidget* fringe;
    int m_errorLine = 0;
    QString m_errorMessage;
    AutoreplaceKind m_kind = autoreplaceNone;
    TextFillKind m_fillKind = textFillNone;
    Autoreplacer m_replacer;
    // the target a Ctrl+press started on, opened when released over it
    QString m_pressedLink;
    bool m_linkPointer = false;
    QCursor m_textPointer;
    bool m_imageDrops = false;
    bool m_blockCursor = false;
    bool m_wrap = false;
    bool m_wrapAllowed = true;
};

#endif
