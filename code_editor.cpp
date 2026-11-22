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
// the continuation sign: its part of the row, the pen as a part of the fringe, the arrow head
static const qreal FRINGE_SIGN_PART = 0.6;
static const qreal FRINGE_PEN_DIVISOR = 8.0;
static const qreal FRINGE_HEAD_DIVISOR = 3.0;
// the space above and below the text of the error bar, in pixels
static const int ERROR_BAR_PADDING = 3;
// the mode line: the space above and below its text, the spaces between its parts
static const int MODE_LINE_PADDING = 3;
static const int MODE_LINE_GAP_SPACES = 3;

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

// the state, the position, the mode, the fill and the selection under the text (EDIT-MODE-31)
class ModeLineBar : public QWidget {
public:
    ModeLineBar(CodeEditor* editor): QWidget(editor), editor(editor)
    {
        setObjectName("modeLine");
        setMouseTracking(true);
        hide();
    }
protected:
    void paintEvent(QPaintEvent* event) override { editor->paintModeLine(event); }
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) editor->modeLineClicked(event->pos());
    }
    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (editor->modeLineFillRect().contains(event->pos())) setCursor(Qt::PointingHandCursor);
        else unsetCursor();
    }
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::ToolTip) {
            QHelpEvent* help = static_cast<QHelpEvent*>(event);
            QString tip = editor->modeLineToolTip(help->pos());
            if (tip.isEmpty()) QToolTip::hideText();
            else QToolTip::showText(help->globalPos(), tip, this);
            return true;
        }
        return QWidget::event(event);
    }
private:
    CodeEditor* editor;
};

CodeEditor::CodeEditor(QWidget* parent):
    QPlainTextEdit(parent), lineNumbers(new LineNumberArea(this)), errorBar(new ErrorBar(this)),
    fringe(new WrapFringe(this)), modeLine(new ModeLineBar(this))
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
    // the mode line follows the cursor, the fill and View > Mode Line (EDIT-MODE-31)
    connect(this, &QPlainTextEdit::cursorPositionChanged, modeLine, QOverload<>::of(&QWidget::update));
    connect(this, &QPlainTextEdit::selectionChanged, modeLine, QOverload<>::of(&QWidget::update));
    connect(&SettingsManager::instance(), &SettingsManager::textFillChanged, modeLine,
            QOverload<>::of(&QWidget::update));
    connect(&SettingsManager::instance(), &SettingsManager::modeLineChanged, this, &CodeEditor::updateMargin);
    applyFontMetrics();
    applyWrap();
}

void CodeEditor::applyFontMetrics()
{
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * EDITOR_TAB_SPACES);
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
    setViewportMargins(lineNumberAreaWidth(), 0, fringeWidth(), errorBarHeight() + modeLineHeight());
    placeErrorBar();
    placeModeLine();
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

void CodeEditor::setModeLineHead(const QString& state, const QString& name, const QString& mode)
{
    bool shown = modeLineShown();
    m_hasModeLine = true;
    m_modeState = state;
    m_modeName = name;
    m_modeMode = mode;
    if (modeLineShown() != shown) updateMargin();
    modeLine->update();
}

bool CodeEditor::modeLineShown() const
{
    return m_hasModeLine && SettingsManager::instance().getShowModeLine();
}

int CodeEditor::modeLineHeight() const
{
    return modeLineShown() ? fontMetrics().height() + 2 * MODE_LINE_PADDING : 0;
}

// under the error bar, the fringe column too, left of a vertical scroll bar
void CodeEditor::placeModeLine()
{
    QRect cr = contentsRect();
    QRect view = viewport()->geometry();
    modeLine->setGeometry(QRect(cr.left(), view.bottom() + 1 + errorBarHeight(),
                                view.right() + 1 + fringeWidth() - cr.left(), modeLineHeight()));
    modeLine->setVisible(modeLineShown());
    modeLine->raise();
}

int CodeEditor::cursorColumn() const
{
    QTextCursor cursor = textCursor();
    return textColumns(cursor.block().text().left(cursor.positionInBlock())) + 1;
}

QVector<CodeEditor::ModeLinePart> CodeEditor::modeLineParts() const
{
    QVector<ModeLinePart> parts;
    if (!modeLineShown()) return parts;
    SettingsManager& sm = SettingsManager::instance();
    QTextCursor cursor = textCursor();
    auto add = [&parts](const QString& text, bool bold, bool dim, bool fill) {
        ModeLinePart part;
        part.text = text;
        part.bold = bold;
        part.dim = dim;
        part.fill = fill;
        parts << part;
    };
    add(m_modeState, false, false, false);
    add(m_modeName, true, false, false);
    add(QString("(%1,%2)").arg(cursor.blockNumber() + 1).arg(cursorColumn()), false, false, false);
    add(m_modeMode, false, false, false);
    // the fill of a Markdown or a Plain text: bold while on, dim while off (EDIT-MD-22)
    if (m_fillKind != textFillNone) {
        bool on = sm.getFillAsYouType();
        add(tr("Fill %1").arg(sm.getTextWidth()), on, !on, true);
    }
    // the characters of a selection, a line break one
    if (cursor.hasSelection()) add(tr("%1 selected").arg(cursor.selectedText().toUcs4().size()), false, false, false);
    int x = LINE_NUMBER_PADDING;
    int gap = fontMetrics().horizontalAdvance(' ') * MODE_LINE_GAP_SPACES;
    for (ModeLinePart& part : parts) {
        QFont f = font();
        f.setBold(part.bold);
        int width = QFontMetrics(f).horizontalAdvance(part.text);
        part.rect = QRect(x, 0, width, modeLineHeight());
        x += width + gap;
    }
    return parts;
}

QString CodeEditor::modeLineText() const
{
    QStringList texts;
    for (const ModeLinePart& part : modeLineParts()) texts << part.text;
    return texts.join(QString(MODE_LINE_GAP_SPACES, ' '));
}

QRect CodeEditor::modeLineFillRect() const
{
    for (const ModeLinePart& part : modeLineParts()) {
        if (part.fill) return part.rect;
    }
    return QRect();
}

QString CodeEditor::modeLineToolTip(const QPoint& pos) const
{
    if (!modeLineFillRect().contains(pos)) return QString();
    SettingsManager& sm = SettingsManager::instance();
    return sm.getFillAsYouType() ? tr("Fill as you type at %1 characters is on: click to switch it off").arg(sm.getTextWidth())
                                 : tr("Fill as you type at %1 characters is off: click to switch it on").arg(sm.getTextWidth());
}

void CodeEditor::modeLineClicked(const QPoint& pos)
{
    if (!modeLineFillRect().contains(pos)) return;
    SettingsManager& sm = SettingsManager::instance();
    sm.setFillAsYouType(!sm.getFillAsYouType());
}

// the line-number colors of the scheme, a rule above, the text color (EDIT-COLOR-1)
void CodeEditor::paintModeLine(QPaintEvent* event)
{
    QPainter painter(modeLine);
    painter.fillRect(event->rect(), palette().color(QPalette::AlternateBase));
    painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.drawLine(0, 0, modeLine->width(), 0);
    int right = modeLine->width() - LINE_NUMBER_PADDING;
    for (const ModeLinePart& part : modeLineParts()) {
        if (part.rect.left() >= right) break;
        QFont f = font();
        f.setBold(part.bold);
        painter.setFont(f);
        painter.setPen(palette().color(part.dim ? QPalette::PlaceholderText : QPalette::Text));
        QRect place = part.rect;
        place.setRight(qMin(place.right(), right));
        painter.drawText(place, Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetrics(f).elidedText(part.text, Qt::ElideRight, place.width() + 1));
    }
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
    placeModeLine();
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
    bool typed = document()->revision() != revision;
    // only a key that typed is read against the rules (EDIT-AUTO-5)
    if (typingKey(event) && typed) m_replacer.typed(textCursor(), m_kind);
    // then a Space or a Return breaks a line past the width (EDIT-MD-21)
    bool fillKey = event->text() == " " || ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
                                            !(event->modifiers() & ~Qt::KeypadModifier));
    if (typed && fillKey) autoFill();
}

void CodeEditor::autoFill()
{
    SettingsManager& sm = SettingsManager::instance();
    if (m_fillKind == textFillNone || isReadOnly() || !sm.getFillAsYouType() || textCursor().hasSelection()) return;
    // the block begun at the cursor: an undo puts the cursor back there
    QTextCursor edit = textCursor();
    bool began = false;
    // a line still past the width breaks again, each break further on
    int done = -1;
    TextFillBreak at;
    while (textAutoFillBreak(toPlainText(), textCursor().position(), sm.getTextWidth(), m_fillKind, &at) &&
           at.from > done) {
        if (!began) {
            emit editStepNamed(tr("fill"));
            edit.beginEditBlock();
            began = true;
        }
        edit.setPosition(at.from);
        edit.setPosition(at.to, QTextCursor::KeepAnchor);
        edit.insertText("\n" + at.prefix);
        done = at.from + at.prefix.length();
    }
    if (!began) return;
    edit.endEditBlock();
    emit editStepNamed(QString());
    // the typed pattern before the break is not given back by Backspace (EDIT-AUTO-6)
    m_replacer.forget();
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
