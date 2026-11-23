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

#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStackedWidget>
#include <QScrollBar>
#include <QSplitter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QUndoStack>
#include <QUrl>

#include "text_document.h"
#include "markdown_tools.h"
#include "cyberiada_constants.h"
#include "code_editor.h"
#include "code_highlighter.h"
#include "markdown_objects.h"
#include "markdown_outline_model.h"
#include "markdown_view.h"
#include "markdown_pdf.h"
#include "document_mode.h"
#include "settings_manager.h"

// one edit of the text: the undo stack of the tab drives the text's own history
class TextEditStep : public QUndoCommand {
public:
    TextEditStep(CodeEditor* editor, const QString& name):
        QUndoCommand(name.isEmpty() ? QObject::tr("typing") : name), editor(editor) {}
    // the cursor goes to the change, as the Undo of the editor itself puts it
    void undo() override {
        QTextCursor cursor = editor->textCursor();
        editor->document()->undo(&cursor);
        editor->setTextCursor(cursor);
    }
    // the push redoes the step the text has just done itself
    void redo() override {
        if (pushed) {
            QTextCursor cursor = editor->textCursor();
            editor->document()->redo(&cursor);
            editor->setTextCursor(cursor);
        }
        pushed = true;
    }
private:
    CodeEditor* editor;
    bool pushed = false;
};

TextDocument::TextDocument(const QString& mode, QObject* parent):
    EditorDocument(parent), m_mode(mode)
{
    m_undo = new QUndoStack(this);
    m_editor = new CodeEditor();
    m_document = m_editor->document();
    m_document->setUndoRedoEnabled(true);
    connect(m_document, &QTextDocument::undoCommandAdded, this,
            [this]() { m_undo->push(new TextEditStep(m_editor, m_stepName)); });
    m_rendered = new MarkdownView();
    m_page = new QStackedWidget();
    m_page->addWidget(m_editor);
    m_page->addWidget(m_rendered);
    // the marks follow the text and the rendered page (EDIT-FIND-8)
    connect(m_document, &QTextDocument::contentsChanged, this, &TextDocument::scheduleFind);
    connect(m_rendered->document(), &QTextDocument::contentsChanged, this, &TextDocument::scheduleFind);
    connect(&SettingsManager::instance(), &SettingsManager::textSchemeChanged, this, &TextDocument::applyScheme);
    // a JSON text is checked a while after its last edit (EDIT-MODE-26)
    m_checkTimer = new QTimer(this);
    m_checkTimer->setSingleShot(true);
    m_checkTimer->setInterval(JSON_CHECK_DELAY_MS);
    connect(m_checkTimer, &QTimer::timeout, this, &TextDocument::checkText);
    connect(m_document, &QTextDocument::contentsChanged, this, &TextDocument::scheduleCheck);
    connect(m_editor, &CodeEditor::errorClicked, this, &TextDocument::showError);
    connect(m_editor, &CodeEditor::linkActivated, this, &TextDocument::openLink);
    connect(m_rendered, &MarkdownView::linkActivated, this, &TextDocument::openLink);
    connect(m_editor, &CodeEditor::imageOffered, this, &TextDocument::imageOffered);
    connect(m_editor, &CodeEditor::formulaActivated, this, &TextDocument::editFormulaRequested);
    // a break of the fill as you type is a step of its own (EDIT-MD-21)
    connect(m_editor, &CodeEditor::editStepNamed, this, [this](const QString& name) { m_stepName = name; });
    // the preview follows the edits a while after the last one, and the cursor (EDIT-MD-11)
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(MARKDOWN_PREVIEW_DELAY_MS);
    connect(m_previewTimer, &QTimer::timeout, this, &TextDocument::renderPreview);
    connect(m_document, &QTextDocument::contentsChanged, this, [this]() {
        if (showsPreview()) m_previewTimer->start();
    });
    connect(m_editor, &QPlainTextEdit::cursorPositionChanged, this, &TextDocument::followCursor);
    connect(&SettingsManager::instance(), &SettingsManager::markdownPreviewChanged, this, &TextDocument::showPage);
    // the outline is read at most once a second after the first change (EDIT-MD-14)
    m_outlineTimer = new QTimer(this);
    m_outlineTimer->setSingleShot(true);
    m_outlineTimer->setInterval(MARKDOWN_OUTLINE_INTERVAL_MS);
    connect(m_outlineTimer, &QTimer::timeout, this, &TextDocument::readOutline);
    connect(m_document, &QTextDocument::contentsChanged, this, &TextDocument::markOutline);
    // the mode line follows the clean state, the file, the mode and the flags (EDIT-MODE-31)
    connect(m_undo, &QUndoStack::cleanChanged, this, &TextDocument::updateModeLine);
    connect(this, &EditorDocument::titleChanged, this, &TextDocument::updateModeLine);
    connect(this, &EditorDocument::modeChanged, this, &TextDocument::updateModeLine);
    connect(this, &EditorDocument::readOnlyChanged, this, &TextDocument::updateModeLine);
    applyMode();
    updateModeLine();
}

void TextDocument::updateModeLine()
{
    QString state = !isEditable() ? "%%" : isClean() ? "--" : "**";
    m_editor->setModeLineHead(state, title(), modeName(m_mode));
}

// the page (and the editor with it) belongs to no widget parent
TextDocument::~TextDocument()
{
    delete m_findPlace;
    delete m_errorPlace;
    delete m_highlighter;
    delete m_page;
}

QWidget* TextDocument::page() const
{
    return m_page;
}

bool TextDocument::setMode(const QString& mode)
{
    if (!isTextMode(mode)) return false;
    if (mode == m_mode) return true;
    m_mode = mode;
    applyMode();
    emit modeChanged();
    return true;
}

void TextDocument::refreshLanguage()
{
    if (m_mode == MODE_TEXT || m_mode == MODE_MARKDOWN) return;
    if (!CodeStyle::instance().findId(m_mode)) setMode(MODE_TEXT);
    else applyMode();
}

void TextDocument::applyMode()
{
    delete m_highlighter;
    m_highlighter = nullptr;
    // only the text of a Markdown takes the typed replacements (EDIT-AUTO-2/3)
    m_editor->setAutoreplaceKind(m_mode == MODE_MARKDOWN ? autoreplaceMarkdown : autoreplaceNone);
    m_editor->setFillKind(fillKind());
    // a dropped or pasted image goes to Insert Image (EDIT-MD-2)
    m_editor->setImageDrops(m_mode == MODE_MARKDOWN);
    // the source of a language as its code, Markdown by its elements (EDIT-TEXT-7/11)
    if (m_mode == MODE_MARKDOWN) {
        m_highlighter = new CodeHighlighter(m_document, codeRoleMarkup, nullptr, codeMarkupMarkdown);
        m_highlighter->setOpenLinks(true);
    } else if (const CodeLanguage* lang = CodeStyle::instance().findId(m_mode)) {
        m_highlighter = new CodeHighlighter(m_document, codeRoleFormalComment, lang);
    }
    applyScheme();
    showPage();
    checkText();
    applyOutline();
}

void TextDocument::applyOutline()
{
    if (m_mode == MODE_MARKDOWN && !m_outline) {
        m_outline = new MarkdownOutlineModel(this);
        markOutline();
    } else if (m_mode != MODE_MARKDOWN && m_outline) {
        m_outlineTimer->stop();
        m_outlineDirty = false;
        delete m_outline;
        m_outline = nullptr;
    }
}

void TextDocument::markOutline()
{
    if (!m_outline) return;
    m_outlineDirty = true;
    // the next changes do not delay the reading: a text typed on is read every second
    if (m_outlineShown && !m_outlineTimer->isActive()) m_outlineTimer->start();
}

void TextDocument::setOutlineShown(bool on)
{
    m_outlineShown = on;
    if (!on) m_outlineTimer->stop();
    else if (m_outlineDirty) readOutline();
}

void TextDocument::readOutline()
{
    m_outlineTimer->stop();
    if (!m_outline) return;
    m_outline->refresh(title(), text(), baseDir());
    m_outlineDirty = false;
    emit outlineRead();
}

QModelIndex TextDocument::outlineRowAtCursor() const
{
    if (!m_outline || showsRendered()) return QModelIndex();
    QTextCursor cursor = m_editor->textCursor();
    return m_outline->indexAt(cursor.blockNumber() + 1, cursor.positionInBlock() + 1);
}

QModelIndex TextDocument::showOutlineRow(const QPersistentModelIndex& row, const OutlineEntry& named, bool focus)
{
    if (!m_outline) return QModelIndex();
    if (m_outlineDirty) readOutline();
    // the row the reading kept, else what it named moved (EDIT-MD-16)
    QModelIndex shown = row;
    if (!shown.isValid()) shown = m_outline->addedNamesake(named);
    const OutlineEntry* entry = m_outline->entry(shown);
    if (!entry) return QModelIndex();
    if (showsRendered()) {
        if (entry->kind != OutlineEntry::outlineHeading || !m_rendered->scrollToHeading(entry->anchor)) {
            m_rendered->scrollToSourceLine(entry->line);
        }
        if (focus) m_rendered->setFocus();
    } else {
        goToLine(entry->line, entry->column, focus);
    }
    return shown;
}

bool TextDocument::isJson() const
{
    return m_mode == LANGUAGE_JSON;
}

void TextDocument::scheduleCheck()
{
    if (isJson()) m_checkTimer->start();
}

void TextDocument::checkText()
{
    m_checkTimer->stop();
    m_jsonCheck = isJson() ? checkJson(text()) : JsonCheck();
    delete m_errorPlace;
    m_errorPlace = nullptr;
    QString message;
    if (!m_jsonCheck.ok) {
        m_errorPlace = new QTextCursor(m_document);
        m_errorPlace->setPosition(m_jsonCheck.position);
        m_errorPlace->setPosition(m_jsonCheck.position + m_jsonCheck.length, QTextCursor::KeepAnchor);
        message = tr("Line %1, column %2: %3").arg(m_jsonCheck.line).arg(m_jsonCheck.column)
            .arg(m_jsonCheck.message);
    }
    m_editor->setError(m_jsonCheck.ok ? 0 : m_jsonCheck.line, message);
    markFind();
}

void TextDocument::showError()
{
    if (!m_errorPlace) return;
    QTextCursor cursor(m_document);
    cursor.setPosition(m_errorPlace->selectionStart());
    m_editor->setTextCursor(cursor);
    m_editor->centerCursor();
    m_editor->setFocus();
}

void TextDocument::applyScheme()
{
    const TextScheme& scheme = SettingsManager::instance().currentTextScheme();
    m_editor->setScheme(scheme);
    if (m_highlighter) m_highlighter->setScheme(scheme);
}

void TextDocument::showPage()
{
    bool preview = showsPreview();
    if (preview && !m_previewPage) {
        m_previewPage = new QSplitter(Qt::Horizontal);
        m_preview = new MarkdownView();
        m_previewPage->addWidget(m_preview);
        m_page->addWidget(m_previewPage);
        connect(m_preview, &MarkdownView::linkActivated, this, &TextDocument::openLink);
    }
    // the editor moves beside the preview and back, so the stack holds it alone otherwise
    if (preview && m_editor->parentWidget() != m_previewPage) {
        bool focused = m_editor->hasFocus();
        m_previewPage->insertWidget(0, m_editor);
        m_previewPage->setSizes(QList<int>() << 1 << 1);
        // the stack hid it as a page it did not show
        m_editor->show();
        if (focused) m_editor->setFocus();
    } else if (!preview && m_previewPage && m_editor->parentWidget() == m_previewPage) {
        m_page->insertWidget(0, m_editor);
    }
    if (showsRendered()) {
        // the images are found next to the file (EDIT-MODE-14)
        m_rendered->setMarkdownText(text(), baseDir());
        m_page->setCurrentWidget(m_rendered);
    } else if (preview) {
        m_previewTimer->stop();
        renderPreview();
        m_page->setCurrentWidget(m_previewPage);
    } else {
        m_page->setCurrentWidget(m_editor);
    }
    scheduleFind();
}

bool TextDocument::showsPreview() const
{
    return m_mode == MODE_MARKDOWN && !readOnlyFlag() && SettingsManager::instance().getMarkdownPreview();
}

void TextDocument::renderPreview()
{
    if (!m_preview || !showsPreview()) return;
    int scroll = m_preview->verticalScrollBar()->value();
    m_preview->setMarkdownText(text(), baseDir());
    m_preview->verticalScrollBar()->setValue(scroll);
}

void TextDocument::followCursor()
{
    if (!m_preview || !showsPreview()) return;
    int block = m_editor->textCursor().blockNumber();
    if (block == m_previewBlock) return;
    m_previewBlock = block;
    m_preview->scrollToSourceLine(block + 1);
}

bool TextDocument::showsRendered() const
{
    return m_mode == MODE_MARKDOWN && readOnlyFlag();
}

bool TextDocument::goToLine(int line, int column, bool focus)
{
    QTextBlock block = m_document->findBlockByNumber(line - 1);
    if (!block.isValid()) return false;
    QTextCursor cursor(block);
    // a column past the end of the line stops at its end
    cursor.setPosition(block.position() + qBound(0, column - 1, block.length() - 1));
    m_editor->setTextCursor(cursor);
    m_editor->centerCursor();
    if (focus) m_editor->setFocus();
    return true;
}

void TextDocument::openLink(const QString& target)
{
    QUrl url(target, QUrl::TolerantMode);
    if (url.scheme().isEmpty() && url.path().isEmpty() && url.hasFragment()) {
        goToHeading(url.fragment(QUrl::FullyDecoded));
        return;
    }
    // a file as the rendered page finds an image: from the directory of the tab's file
    QString path = markdownImagePath(target, baseDir());
    if (!path.isEmpty()) {
        if (isDocumentFile(path)) emit openFileRequested(path, url.hasFragment() ? url.fragment(QUrl::FullyDecoded) : QString());
        else QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        return;
    }
    // a relative path of an untitled tab names nothing
    if (!url.isRelative()) QDesktopServices::openUrl(url);
}

bool TextDocument::insertInline(const QString& text, const QString& label)
{
    if (!isEditable()) return false;
    QTextCursor cursor = m_editor->textCursor();
    m_stepName = label;
    cursor.beginEditBlock();
    cursor.insertText(text);
    cursor.endEditBlock();
    m_stepName.clear();
    m_editor->setTextCursor(cursor);
    m_editor->ensureCursorVisible();
    return true;
}

bool TextDocument::replaceRange(int begin, int end, const QString& text, const QString& label)
{
    if (!isEditable() || begin < 0 || end < begin || end >= m_document->characterCount()) return false;
    QTextCursor cursor(m_document);
    cursor.setPosition(begin);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    m_stepName = label;
    cursor.beginEditBlock();
    cursor.insertText(text);
    cursor.endEditBlock();
    m_stepName.clear();
    m_editor->setTextCursor(cursor);
    m_editor->ensureCursorVisible();
    return true;
}

TextFillKind TextDocument::fillKind() const
{
    if (m_mode == MODE_MARKDOWN) return textFillMarkdown;
    if (m_mode == MODE_TEXT) return textFillPlain;
    return textFillNone;
}

bool TextDocument::canFill() const
{
    return fillKind() != textFillNone && isEditable() && !showsRendered();
}

bool TextDocument::fill(bool unfill)
{
    QTextCursor cursor = m_editor->textCursor();
    return fillRange(cursor.anchor(), cursor.position(), unfill, true);
}

bool TextDocument::fillLines(int first, int last, bool unfill)
{
    if (first < 1 || last < first || last > lineCount()) return false;
    QTextBlock end = m_document->findBlockByNumber(last - 1);
    return fillRange(m_document->findBlockByNumber(first - 1).position(), end.position() + end.length() - 1,
                     unfill, false);
}

bool TextDocument::fillRange(int anchor, int position, bool unfill, bool select)
{
    if (!canFill()) return false;
    TextFillEdit edit = textFill(text(), anchor, position, SettingsManager::instance().getTextWidth(), fillKind(),
                                 unfill);
    if (!edit.changed) return true;
    // the block begun at the cursor: an undo puts the cursor back there
    QTextCursor cursor = m_editor->textCursor();
    m_stepName = unfill ? tr("unfill paragraph") : tr("fill paragraph");
    cursor.beginEditBlock();
    cursor.setPosition(edit.from);
    cursor.setPosition(edit.to, QTextCursor::KeepAnchor);
    cursor.insertText(edit.text);
    cursor.endEditBlock();
    m_stepName.clear();
    cursor.setPosition(select ? edit.anchor : edit.cursor);
    cursor.setPosition(edit.cursor, QTextCursor::KeepAnchor);
    m_editor->setTextCursor(cursor);
    m_editor->ensureCursorVisible();
    return true;
}

bool TextDocument::showHeading(const QString& anchor)
{
    return showsRendered() ? m_rendered->scrollToHeading(anchor) : goToHeading(anchor);
}

bool TextDocument::goToHeading(const QString& anchor)
{
    // the anchors as the rendered view and the link check number them (EDIT-MD-8)
    for (const MarkdownHeading& heading : markdownHeadings(text())) {
        if (heading.anchor == anchor.toLower()) return goToLine(heading.line);
    }
    return false;
}

int TextDocument::lineCount() const
{
    return m_document->blockCount();
}

int TextDocument::currentLine() const
{
    return m_editor->textCursor().blockNumber() + 1;
}

int TextDocument::currentColumn() const
{
    return m_editor->cursorColumn();
}

QString TextDocument::baseDir() const
{
    return hasFile() ? QFileInfo(m_path).absolutePath() : QString();
}

bool TextDocument::exportPdf(const QString& path, QString* error) const
{
    return exportMarkdownPdf(text(), baseDir(), title(), path, error) > 0;
}

void TextDocument::applyReadOnlyFlag(bool on)
{
    m_editor->setReadOnly(on || m_locked);
    showPage();
}

void TextDocument::setLocked(bool on)
{
    m_locked = on;
    m_editor->setReadOnly(readOnlyFlag() || on);
    updateModeLine();
}

bool TextDocument::replaceText(const QString& text, const QString& label)
{
    if (text == this->text()) return false;
    m_stepName = label;
    QTextCursor cursor(m_document);
    cursor.beginEditBlock();
    cursor.select(QTextCursor::Document);
    cursor.insertText(text);
    cursor.endEditBlock();
    m_stepName.clear();
    showPage();
    checkText();
    return true;
}

static bool isBlankBlock(const QTextBlock& block)
{
    return block.text().trimmed().isEmpty();
}

bool TextDocument::insertBlock(const QString& line, int before, const QString& label)
{
    if (!isEditable()) return false;
    int count = m_document->blockCount();
    // the paragraph goes before the block `at`; count is after the last one
    int at = 0;
    if (before > 0) {
        at = qMin(before - 1, count);
    } else {
        QTextBlock block = m_editor->textCursor().block();
        if (isBlankBlock(block)) {
            at = block.blockNumber();
        } else {
            while (block.next().isValid() && !isBlankBlock(block.next())) block = block.next();
            at = block.blockNumber() + 1;
        }
    }
    bool blankBefore = at == 0 || isBlankBlock(m_document->findBlockByNumber(at - 1));
    bool blankAfter = at == count || isBlankBlock(m_document->findBlockByNumber(at));
    QString text = (blankBefore ? QString() : QString("\n")) + line + (blankAfter ? QString() : QString("\n"));
    m_stepName = label;
    QTextCursor cursor(m_document);
    cursor.beginEditBlock();
    if (at < count) {
        cursor.setPosition(m_document->findBlockByNumber(at).position());
        cursor.insertText(text + "\n");
    } else {
        cursor.movePosition(QTextCursor::End);
        cursor.insertText("\n" + text);
    }
    cursor.endEditBlock();
    m_stepName.clear();
    // at the end of the paragraph's last line
    QTextCursor end(m_document->findBlockByNumber(at + (blankBefore ? 0 : 1) + line.count('\n')));
    end.movePosition(QTextCursor::EndOfBlock);
    m_editor->setTextCursor(end);
    m_editor->ensureCursorVisible();
    showPage();
    checkText();
    return true;
}

QString TextDocument::title() const
{
    return hasFile() ? QFileInfo(m_path).fileName() : tr("untitled");
}

bool TextDocument::isClean() const
{
    // a file gone counts as modified until it is saved again (EDIT-DOC-16)
    return m_undo->isClean() && diskStatus() != diskAbsent;
}

QString TextDocument::text() const
{
    return m_document->toPlainText();
}

void TextDocument::setText(const QString& text)
{
    // a new text has no history to step back through
    m_editor->setPlainText(text);
    m_undo->clear();
    showPage();
    checkText();
}

bool TextDocument::load(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = tr("Cannot read %1:\n%2").arg(path, file.errorString());
        return false;
    }
    // the path first: a rendered page finds its images next to the file
    m_path = QFileInfo(path).absoluteFilePath();
    setText(QString::fromUtf8(file.readAll()));
    rememberDisk();
    emit titleChanged();
    return true;
}

bool TextDocument::followFile(const QString& path, QString* error)
{
    Q_UNUSED(error);
    m_path = QFileInfo(path).absoluteFilePath();
    rememberDisk();
    markOutline();
    emit titleChanged();
    return true;
}

bool TextDocument::reload(QString* error)
{
    if (!hasFile()) return false;
    int position = m_editor->textCursor().position();
    int scroll = m_editor->verticalScrollBar()->value();
    if (!load(m_path, error)) return false;
    QTextCursor cursor = m_editor->textCursor();
    cursor.setPosition(qBound(0, position, m_document->characterCount() - 1));
    m_editor->setTextCursor(cursor);
    m_editor->verticalScrollBar()->setValue(scroll);
    return true;
}

bool TextDocument::write(const QString& path, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(text().toUtf8()) < 0 || !file.commit()) {
        if (error) *error = tr("Cannot write %1:\n%2").arg(path, file.errorString());
        return false;
    }
    return true;
}

bool TextDocument::save(QString* error)
{
    if (!hasFile()) return false;
    dropGitFact();
    if (!write(m_path, error)) return false;
    m_undo->setClean();
    rememberDisk();
    return true;
}

bool TextDocument::saveAs(const QString& path, QString* error)
{
    dropGitFact();
    if (!write(path, error)) return false;
    m_path = QFileInfo(path).absoluteFilePath();
    m_undo->setClean();
    rememberDisk();
    showPage();
    // the document row and the image paths follow the file
    markOutline();
    emit titleChanged();
    return true;
}

QTextDocument* TextDocument::shownDocument() const
{
    return showsRendered() ? m_rendered->document() : m_document;
}

QTextCursor TextDocument::shownCursor() const
{
    return showsRendered() ? m_rendered->textCursor() : m_editor->textCursor();
}

void TextDocument::setShownCursor(const QTextCursor& cursor)
{
    if (showsRendered()) {
        m_rendered->setTextCursor(cursor);
        m_rendered->ensureCursorVisible();
    } else {
        m_editor->setTextCursor(cursor);
        m_editor->ensureCursorVisible();
    }
}

void TextDocument::setFindQuery(const FindQuery& query, bool navigate)
{
    m_findQuery = query;
    int place = findPlace();
    runFind(place >= 0 ? place : shownCursor().selectionStart(), navigate);
}

void TextDocument::runFind(int place, bool navigate)
{
    m_findMatches.clear();
    m_findMore = false;
    if (!m_findQuery.isEmpty()) {
        m_findMatches = findRanges(shownDocument()->toPlainText(), m_findQuery.expression(),
                                   FIND_MATCH_LIMIT, &m_findMore);
    }
    int current = m_findMatches.isEmpty() ? -1 : 0;
    for (int i = 0; i < m_findMatches.size(); i++) {
        if (m_findMatches[i].from >= place) {
            current = i;
            break;
        }
    }
    setFindCurrent(current);
    markFind();
    if (navigate) showFindMatch();
    emit findResultChanged(m_findCurrent, m_findMatches.size(), m_findMore);
}

void TextDocument::scheduleFind()
{
    if (m_findQuery.isEmpty() || m_findPending) return;
    m_findPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_findPending = false;
        if (m_findQuery.isEmpty()) return;
        runFind(qMax(findPlace(), 0), false);
    });
}

void TextDocument::markFind()
{
    QList<QTextEdit::ExtraSelection> marks;
    QTextDocument* document = shownDocument();
    // the JSON error under the find marks (EDIT-MODE-26)
    if (m_errorPlace && m_errorPlace->document() == document && m_errorPlace->hasSelection()) {
        QTextEdit::ExtraSelection error;
        error.cursor = *m_errorPlace;
        error.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        error.format.setUnderlineColor(QColor(TEXT_ERROR_COLOR));
        marks.append(error);
    }
    for (int i = 0; i < m_findMatches.size(); i++) {
        QTextEdit::ExtraSelection mark;
        mark.cursor = QTextCursor(document);
        mark.cursor.setPosition(m_findMatches[i].from);
        mark.cursor.setPosition(m_findMatches[i].to, QTextCursor::KeepAnchor);
        mark.format.setBackground(QColor(i == m_findCurrent ? FIND_CURRENT_COLOR : FIND_MATCH_COLOR));
        mark.format.setForeground(Qt::black);
        marks.append(mark);
    }
    // the hidden page keeps no marks of an older search
    if (showsRendered()) {
        m_rendered->setExtraSelections(marks);
        m_editor->setExtraSelections(QList<QTextEdit::ExtraSelection>());
    } else {
        m_editor->setExtraSelections(marks);
        m_rendered->setExtraSelections(QList<QTextEdit::ExtraSelection>());
    }
}

// the cursor at the match without a selection: a selection is drawn over the marks
void TextDocument::showFindMatch()
{
    if (m_findCurrent < 0) return;
    QTextCursor cursor(shownDocument());
    // the end, then the start: both are scrolled to when the view holds them
    cursor.setPosition(m_findMatches[m_findCurrent].to);
    setShownCursor(cursor);
    cursor.setPosition(m_findMatches[m_findCurrent].from);
    setShownCursor(cursor);
}

void TextDocument::moveFind(int step)
{
    int count = m_findMatches.size();
    if (count > 0) {
        setFindCurrent(((m_findCurrent + step) % count + count) % count);
        markFind();
        showFindMatch();
    }
    emit findResultChanged(m_findCurrent, count, m_findMore);
}

void TextDocument::clearFind(bool keep)
{
    if (keep && m_findCurrent >= 0) {
        QTextCursor cursor(shownDocument());
        cursor.setPosition(m_findMatches[m_findCurrent].from);
        cursor.setPosition(m_findMatches[m_findCurrent].to, QTextCursor::KeepAnchor);
        setShownCursor(cursor);
    }
    m_findQuery = FindQuery();
    m_findMatches.clear();
    setFindCurrent(-1);
    m_findMore = false;
    markFind();
    if (keep) (showsRendered() ? static_cast<QWidget*>(m_rendered) : m_editor)->setFocus();
}

void TextDocument::setFindCurrent(int index)
{
    m_findCurrent = index;
    delete m_findPlace;
    m_findPlace = nullptr;
    if (index < 0) return;
    m_findPlace = new QTextCursor(shownDocument());
    m_findPlace->setPosition(m_findMatches[index].from);
}

int TextDocument::findPlace() const
{
    // a place on the other page (the read-only flag switched) does not count
    if (!m_findPlace || m_findPlace->document() != shownDocument()) return -1;
    return m_findPlace->position();
}

QString TextDocument::findSeed() const
{
    return findSeedOf(shownCursor().selectedText());
}
