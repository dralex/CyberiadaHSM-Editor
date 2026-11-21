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
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QUrl>
#include <QMouseEvent>
#include <QPainter>
#include <QTextLayout>
#include <QPainterPath>
#include <QTextBlock>
#include <QToolTip>

#include "code_editor.h"
#include "cyberiada_constants.h"
#include "fontmanager.h"
#include "markdown_tools.h"
#include "settings_manager.h"
#include "text_scheme.h"

// the gap between the numbers and the text, in pixels
static const int LINE_NUMBER_PADDING = 6;
// the tab stop in spaces
static const int TAB_SPACES = 4;
// the continuation sign: its part of the row, the pen as a part of the fringe, the arrow head
static const qreal FRINGE_SIGN_PART = 0.6;
static const qreal FRINGE_PEN_DIVISOR = 8.0;
static const qreal FRINGE_HEAD_DIVISOR = 3.0;
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

// the right margin of a wrapped text: a sign at the end of each row that continues (EDIT-MODE-30)
class WrapFringe : public QWidget {
public:
    WrapFringe(CodeEditor* editor): QWidget(editor), editor(editor) { hide(); }
protected:
    void paintEvent(QPaintEvent* event) override { editor->paintFringe(event); }
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
    QPlainTextEdit(parent), lineNumbers(new LineNumberArea(this)), errorBar(new ErrorBar(this)),
    fringe(new WrapFringe(this))
{
    // the text editor font, not the diagram one (EDIT-MODE-27)
    setFont(FontManager::instance().editorFont());
    setLineWrapMode(QPlainTextEdit::NoWrap);
    // the pointer follows the link targets under it (EDIT-MODE-28)
    viewport()->setMouseTracking(true);
    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateMargin);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumbers);
    connect(&SettingsManager::instance(), &SettingsManager::editorFontChanged, this,
            [this]() { setFont(FontManager::instance().editorFont()); });
    m_blockCursor = SettingsManager::instance().getEditorBlockCursor();
    connect(&SettingsManager::instance(), &SettingsManager::editorCursorChanged, this,
            [this]() { setBlockCursor(SettingsManager::instance().getEditorBlockCursor()); });
    // a row breaks at any character, as the continuation lines of Emacs (EDIT-MODE-30)
    setWordWrapMode(QTextOption::WrapAnywhere);
    m_wrap = SettingsManager::instance().getEditorWrap();
    connect(&SettingsManager::instance(), &SettingsManager::editorWrapChanged, this,
            [this]() { setWrapLines(SettingsManager::instance().getEditorWrap()); });
    applyFontMetrics();
    applyWrap();
}

void CodeEditor::applyFontMetrics()
{
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * TAB_SPACES);
    updateMargin();
    placeLineNumbers();
    applyCursor();
}

void CodeEditor::setBlockCursor(bool on)
{
    m_blockCursor = on;
    applyCursor();
}

void CodeEditor::applyCursor()
{
    // a cursor wider than one pixel is drawn inverting what is under it: the character shows
    setCursorWidth(m_blockCursor ? qMax(1, fontMetrics().horizontalAdvance(' ')) : 1);
    viewport()->update();
}

void CodeEditor::changeEvent(QEvent* event)
{
    QPlainTextEdit::changeEvent(event);
    if (event->type() == QEvent::FontChange) applyFontMetrics();
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
    setViewportMargins(lineNumberAreaWidth(), 0, fringeWidth(), errorBarHeight());
    placeErrorBar();
    placeFringe();
}

void CodeEditor::setWrapLines(bool on)
{
    m_wrap = on;
    applyWrap();
}

void CodeEditor::setWrapAllowed(bool on)
{
    m_wrapAllowed = on;
    applyWrap();
}

void CodeEditor::applyWrap()
{
    bool on = wrapLines();
    setLineWrapMode(on ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    fringe->setVisible(on);
    updateMargin();
}

int CodeEditor::fringeWidth() const
{
    return wrapLines() ? fontMetrics().horizontalAdvance(QLatin1Char('9')) + LINE_NUMBER_PADDING : 0;
}

// the right margin beside the viewport, left of a vertical scroll bar
void CodeEditor::placeFringe()
{
    QRect view = viewport()->geometry();
    fringe->setGeometry(QRect(view.right() + 1, view.top(), fringeWidth(), view.height()));
}

QVector<QRectF> CodeEditor::continuationRows() const
{
    QVector<QRectF> rows;
    if (!wrapLines()) return rows;
    QTextBlock block = firstVisibleBlock();
    QPointF offset = contentOffset();
    int height = viewport()->height();
    while (block.isValid()) {
        QRectF box = blockBoundingGeometry(block).translated(offset);
        if (box.top() > height) break;
        QTextLayout* layout = block.layout();
        // every row of a block but its last continues
        for (int i = 0; block.isVisible() && layout && i + 1 < layout->lineCount(); i++) {
            QRectF line = layout->lineAt(i).rect();
            rows << QRectF(0, box.top() + line.top(), fringeWidth(), line.height());
        }
        block = block.next();
    }
    return rows;
}

void CodeEditor::paintFringe(QPaintEvent* event)
{
    QPainter painter(fringe);
    painter.fillRect(event->rect(), palette().color(QPalette::AlternateBase));
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(palette().color(QPalette::PlaceholderText));
    pen.setWidthF(qMax(1.0, fringeWidth() / FRINGE_PEN_DIVISOR));
    painter.setPen(pen);
    // a hook arrow: down along the right, then to the left, as the continuation sign of Emacs
    for (const QRectF& row : continuationRows()) {
        qreal size = qMin(row.width(), row.height()) * FRINGE_SIGN_PART;
        QRectF box(row.center().x() - size / 2, row.center().y() - size / 2, size, size);
        QPainterPath hook;
        hook.moveTo(box.right(), box.top());
        hook.lineTo(box.right(), box.bottom());
        hook.lineTo(box.left(), box.bottom());
        qreal head = size / FRINGE_HEAD_DIVISOR;
        hook.moveTo(box.left() + head, box.bottom() - head);
        hook.lineTo(box.left(), box.bottom());
        hook.lineTo(box.left() + head, box.bottom() + head);
        painter.drawPath(hook);
    }
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
    // the fringe follows the rows the same way
    if (fringe->isVisible()) {
        if (dy) fringe->scroll(0, dy);
        else fringe->update(0, rect.y(), fringe->width(), rect.height());
    }
    if (rect.contains(viewport()->rect())) updateMargin();
}

void CodeEditor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    placeLineNumbers();
    placeErrorBar();
    placeFringe();
}

void CodeEditor::placeLineNumbers()
{
    QRect cr = contentsRect();
    lineNumbers->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
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

QString CodeEditor::linkAt(const QPoint& pos) const
{
    // the nearest gap to the point: the glyph is on the point's side of it, in its line
    QTextCursor cursor = cursorForPosition(pos);
    QRect gap = cursorRect(cursor);
    if (pos.y() < gap.top() || pos.y() > gap.bottom()) return QString();
    QTextBlock block = cursor.block();
    int glyph = cursor.positionInBlock() - (pos.x() < gap.left() ? 1 : 0);
    if (glyph < 0 || glyph >= block.length() - 1) return QString();
    for (const QTextLayout::FormatRange& range : block.layout()->formats()) {
        if (range.format.isAnchor() && glyph >= range.start && glyph < range.start + range.length) {
            return range.format.anchorHref();
        }
    }
    return QString();
}

void CodeEditor::updateLinkPointer(const QPoint& pos, bool ctrl)
{
    bool hand = ctrl && !linkAt(pos).isEmpty();
    if (hand == m_linkPointer) return;
    m_linkPointer = hand;
    if (hand) {
        m_textPointer = viewport()->cursor();
        viewport()->setCursor(Qt::PointingHandCursor);
    } else {
        viewport()->setCursor(m_textPointer);
    }
}

// a link target names itself in a tooltip (EDIT-MODE-28)
bool CodeEditor::viewportEvent(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        QHelpEvent* help = static_cast<QHelpEvent*>(event);
        QString link = linkAt(help->pos());
        if (!link.isEmpty()) {
            QToolTip::showText(help->globalPos(), tr("%1\nCtrl+click to open").arg(link), viewport());
            return true;
        }
        QToolTip::hideText();
    }
    return QPlainTextEdit::viewportEvent(event);
}

void CodeEditor::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ControlModifier)) {
        m_pressedLink = linkAt(event->pos());
        if (!m_pressedLink.isEmpty()) {
            event->accept();
            return;
        }
    }
    QPlainTextEdit::mousePressEvent(event);
}

void CodeEditor::mouseMoveEvent(QMouseEvent* event)
{
    updateLinkPointer(event->pos(), event->modifiers() & Qt::ControlModifier);
    // a press on a target selects nothing
    if (!m_pressedLink.isEmpty()) return;
    QPlainTextEdit::mouseMoveEvent(event);
}

void CodeEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_pressedLink.isEmpty()) {
        QPlainTextEdit::mouseReleaseEvent(event);
        return;
    }
    QString link = m_pressedLink;
    m_pressedLink.clear();
    event->accept();
    if (event->button() == Qt::LeftButton && linkAt(event->pos()) == link) emit linkActivated(link);
}

void CodeEditor::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control) updateLinkPointer(viewport()->mapFromGlobal(QCursor::pos()), false);
    QPlainTextEdit::keyReleaseEvent(event);
}

void CodeEditor::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control) updateLinkPointer(viewport()->mapFromGlobal(QCursor::pos()), true);
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

// the first local image file of a drop or a paste, empty when none
static QString offeredImageFile(const QMimeData* source)
{
    for (const QUrl& url : source->urls()) {
        if (url.isLocalFile() && isInsertableImage(url.toLocalFile())) return url.toLocalFile();
    }
    return QString();
}

bool CodeEditor::canInsertFromMimeData(const QMimeData* source) const
{
    if (m_imageDrops && (source->hasImage() || !offeredImageFile(source).isEmpty())) return true;
    return QPlainTextEdit::canInsertFromMimeData(source);
}

void CodeEditor::insertFromMimeData(const QMimeData* source)
{
    if (m_imageDrops && !isReadOnly()) {
        QString path = offeredImageFile(source);
        QImage image;
        if (path.isEmpty() && source->hasImage()) image = qvariant_cast<QImage>(source->imageData());
        if (!path.isEmpty() || !image.isNull()) {
            // the dialog opens after the drop or the paste is over, the cursor at its place
            QMetaObject::invokeMethod(this, [this, path, image]() { emit imageOffered(path, image); },
                                      Qt::QueuedConnection);
            return;
        }
    }
    QPlainTextEdit::insertFromMimeData(source);
}
