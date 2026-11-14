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
#include <QMouseEvent>
#include <QPainter>
#include <QTextBlock>

#include "code_editor.h"
#include "cyberiada_constants.h"
#include "text_scheme.h"

// the gap between the numbers and the text, in pixels
static const int LINE_NUMBER_PADDING = 6;
// the tab stop in spaces
static const int TAB_SPACES = 4;
// the space above and below the text of the error bar, in pixels
static const int ERROR_BAR_PADDING = 3;

class LineNumberArea : public QWidget {
public:
    LineNumberArea(CodeEditor* editor): QWidget(editor), editor(editor) {}
    QSize sizeHint() const override { return QSize(editor->lineNumberAreaWidth(), 0); }
protected:
    void paintEvent(QPaintEvent* event) override { editor->paintLineNumbers(event); }
private:
    CodeEditor* editor;
};

class ErrorBar : public QWidget {
public:
    ErrorBar(CodeEditor* editor): QWidget(editor), editor(editor)
    {
        setCursor(Qt::PointingHandCursor);
        hide();
    }
protected:
    void paintEvent(QPaintEvent* event) override { editor->paintErrorBar(event); }
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) emit editor->errorClicked();
    }
private:
    CodeEditor* editor;
};

CodeEditor::CodeEditor(QWidget* parent):
    QPlainTextEdit(parent), lineNumbers(new LineNumberArea(this)), errorBar(new ErrorBar(this))
{
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * TAB_SPACES);
    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateMargin);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumbers);
    updateMargin();
}

void CodeEditor::setScheme(const TextScheme& scheme)
{
    // the margin paints with AlternateBase and PlaceholderText
    static const struct {
        QPalette::ColorRole palette;
        TextSchemeRole scheme;
    } roles[] = {
        { QPalette::Base,            textSchemeBackground },
        { QPalette::Text,            textSchemeText },
        { QPalette::Highlight,       textSchemeSelection },
        { QPalette::HighlightedText, textSchemeSelectionText },
        { QPalette::AlternateBase,   textSchemeLineNumbersBackground },
        { QPalette::PlaceholderText, textSchemeLineNumbers }
    };
    QPalette p = palette();
    for (size_t i = 0; i < sizeof(roles) / sizeof(roles[0]); i++) {
        p.setColor(roles[i].palette, scheme.color(roles[i].scheme));
    }
    setPalette(p);
}

int CodeEditor::lineNumberAreaWidth() const
{
    int digits = QString::number(qMax(1, blockCount())).length();
    return 2 * LINE_NUMBER_PADDING + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void CodeEditor::updateMargin()
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, errorBarHeight());
    placeErrorBar();
}

void CodeEditor::setError(int line, const QString& message)
{
    if (line == m_errorLine && message == m_errorMessage) return;
    m_errorLine = line;
    m_errorMessage = message;
    errorBar->setToolTip(message);
    errorBar->setVisible(!message.isEmpty());
    updateMargin();
    errorBar->update();
    lineNumbers->update();
}

int CodeEditor::errorBarHeight() const
{
    return m_errorMessage.isEmpty() ? 0 : fontMetrics().height() + 2 * ERROR_BAR_PADDING;
}

// the bottom margin under the viewport, left of a vertical scroll bar
void CodeEditor::placeErrorBar()
{
    QRect cr = contentsRect();
    QRect view = viewport()->geometry();
    errorBar->setGeometry(QRect(cr.left(), view.bottom() + 1, view.right() + 1 - cr.left(), errorBarHeight()));
    errorBar->raise();
}

void CodeEditor::paintErrorBar(QPaintEvent* event)
{
    QPainter painter(errorBar);
    painter.fillRect(event->rect(), QColor(TEXT_ERROR_BAR_COLOR));
    painter.setPen(Qt::white);
    QRect text = errorBar->rect().adjusted(LINE_NUMBER_PADDING, 0, -LINE_NUMBER_PADDING, 0);
    painter.drawText(text, Qt::AlignLeft | Qt::AlignVCenter,
                     fontMetrics().elidedText(m_errorMessage, Qt::ElideRight, text.width()));
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
    placeErrorBar();
}

void CodeEditor::paintLineNumbers(QPaintEvent* event)
{
    QPainter painter(lineNumbers);
    painter.fillRect(event->rect(), palette().color(QPalette::AlternateBase));
    QTextBlock block = firstVisibleBlock();
    int number = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    QColor numbers = palette().color(QPalette::PlaceholderText);
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            painter.setPen(number + 1 == m_errorLine ? QColor(TEXT_ERROR_COLOR) : numbers);
            painter.drawText(0, top, lineNumbers->width() - LINE_NUMBER_PADDING,
                             fontMetrics().height(), Qt::AlignRight, lineLabel(number));
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
