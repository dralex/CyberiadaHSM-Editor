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

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStackedWidget>
#include <QTextCursor>
#include <QTextDocument>
#include <QUndoStack>

#include "text_document.h"
#include "code_editor.h"
#include "code_highlighter.h"
#include "markdown_view.h"
#include "markdown_pdf.h"

// one edit of the text: the undo stack of the tab drives the text's own history
class TextEditStep : public QUndoCommand {
public:
    TextEditStep(QTextDocument* document, const QString& name):
        QUndoCommand(name.isEmpty() ? QObject::tr("typing") : name), document(document) {}
    void undo() override { document->undo(); }
    // the push redoes the step the text has just done itself
    void redo() override {
        if (pushed) document->redo();
        pushed = true;
    }
private:
    QTextDocument* document;
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
            [this]() { m_undo->push(new TextEditStep(m_document, m_stepName)); });
    m_rendered = new MarkdownView();
    m_page = new QStackedWidget();
    m_page->addWidget(m_editor);
    m_page->addWidget(m_rendered);
    applyMode();
}

// the page (and the editor with it) belongs to no widget parent
TextDocument::~TextDocument()
{
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
    // the source of a language as its code, Markdown by its elements (EDIT-TEXT-7/11)
    if (m_mode == MODE_MARKDOWN) {
        m_highlighter = new CodeHighlighter(m_document, codeRoleMarkup, nullptr, codeMarkupMarkdown);
    } else if (const CodeLanguage* lang = CodeStyle::instance().findId(m_mode)) {
        m_highlighter = new CodeHighlighter(m_document, codeRoleFormalComment, lang);
    }
    showPage();
}

void TextDocument::showPage()
{
    if (showsRendered()) {
        // the images are found next to the file (EDIT-MODE-14)
        m_rendered->setMarkdownText(text(), baseDir());
        m_page->setCurrentWidget(m_rendered);
    } else {
        m_page->setCurrentWidget(m_editor);
    }
}

bool TextDocument::showsRendered() const
{
    return m_mode == MODE_MARKDOWN && readOnlyFlag();
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
    return true;
}

QString TextDocument::title() const
{
    return hasFile() ? QFileInfo(m_path).fileName() : tr("untitled");
}

bool TextDocument::isClean() const
{
    return m_undo->isClean();
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
    emit titleChanged();
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
    return true;
}

bool TextDocument::saveAs(const QString& path, QString* error)
{
    dropGitFact();
    if (!write(path, error)) return false;
    m_path = QFileInfo(path).absoluteFilePath();
    m_undo->setClean();
    showPage();
    emit titleChanged();
    return true;
}
