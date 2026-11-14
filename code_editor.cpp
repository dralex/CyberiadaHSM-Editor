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

#include <QContextMenuEvent>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QTextBlock>

#include "code_editor.h"

// the gap between the numbers and the text, in pixels
static const int LINE_NUMBER_PADDING = 6;
// the tab stop in spaces
static const int TAB_SPACES = 4;

class LineNumberArea : public QWidget {
public:
    LineNumberArea(CodeEditor* editor): QWidget(editor), editor(editor) {}
    QSize sizeHint() const override { return QSize(editor->lineNumberAreaWidth(), 0); }
protected:
    void paintEvent(QPaintEvent* event) override { editor->paintLineNumbers(event); }
private:
    CodeEditor* editor;
};

CodeEditor::CodeEditor(QWidget* parent):
    QPlainTextEdit(parent), lineNumbers(new LineNumberArea(this))
{
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * TAB_SPACES);
    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateMargin);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumbers);
    updateMargin();
}

int CodeEditor::lineNumberAreaWidth() const
{
    int digits = QString::number(qMax(1, blockCount())).length();
    return 2 * LINE_NUMBER_PADDING + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void CodeEditor::updateMargin()
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumbers(const QRect& rect, int dy)
{
    if (dy) lineNumbers->scroll(0, dy);
    else lineNumbers->update(0, rect.y(), lineNumbers->width(), rect.height());
    if (rect.contains(viewport()->rect())) updateMargin();
}

void CodeEditor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    lineNumbers->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::paintLineNumbers(QPaintEvent* event)
{
    QPainter painter(lineNumbers);
    painter.fillRect(event->rect(), palette().color(QPalette::AlternateBase));
    painter.setPen(palette().color(QPalette::PlaceholderText));
    QTextBlock block = firstVisibleBlock();
    int number = block.blockNumber() + 1;
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            painter.drawText(0, top, lineNumbers->width() - LINE_NUMBER_PADDING,
                             fontMetrics().height(), Qt::AlignRight, QString::number(number));
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        number++;
    }
}

static bool undoKey(QKeyEvent* key)
{
    return key->matches(QKeySequence::Undo) || key->matches(QKeySequence::Redo);
}

// the window's Undo and Redo take the shortcuts: the editor does not override them
bool CodeEditor::event(QEvent* event)
{
    if (event->type() == QEvent::ShortcutOverride && undoKey(static_cast<QKeyEvent*>(event))) {
        event->ignore();
        return false;
    }
    return QPlainTextEdit::event(event);
}

void CodeEditor::keyPressEvent(QKeyEvent* event)
{
    if (undoKey(event)) {
        event->ignore();
        return;
    }
    if (m_replacer.before(event, textCursor())) return;
    int revision = document()->revision();
    QPlainTextEdit::keyPressEvent(event);
    // only a key that typed is read against the rules (EDIT-AUTO-5)
    if (typingKey(event) && document()->revision() != revision) m_replacer.typed(textCursor(), m_kind);
}

void CodeEditor::inputMethodEvent(QInputMethodEvent* event)
{
    QPlainTextEdit::inputMethodEvent(event);
    if (!isReadOnly() && !event->commitString().isEmpty()) m_replacer.typed(textCursor(), m_kind);
}

// the standard menu without its own Undo and Redo, which would pass the tab by
void CodeEditor::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu();
    for (QAction* action : menu->actions()) {
        if (action->objectName() == "edit-undo" || action->objectName() == "edit-redo") {
            action->setVisible(false);
        }
    }
    menu->exec(event->globalPos());
    delete menu;
}
