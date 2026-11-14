/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The text diff tab: two texts side by side, line by line
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

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QSplitter>
#include <QTextBlock>
#include <QVBoxLayout>

#include "text_diff_document.h"
#include "text_diff_view.h"
#include "code_highlighter.h"
#include "cyberiada_constants.h"

using namespace TextDiff;

// a side: its title over its text, the target with the ruler at its right
static QWidget* sidePane(QLabel* label, QWidget* text, QWidget* ruler = nullptr)
{
    QWidget* pane = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(pane);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(label);
    QHBoxLayout* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(text, 1);
    if (ruler) row->addWidget(ruler);
    layout->addLayout(row, 1);
    return pane;
}

// the highlighter of a side: the language of the mode, Markdown by its elements
static CodeHighlighter* highlighterOf(QTextDocument* document, const QString& language)
{
    if (language == MODE_MARKDOWN) {
        return new CodeHighlighter(document, codeRoleMarkup, nullptr, codeMarkupMarkdown);
    }
    if (const CodeLanguage* lang = CodeStyle::instance().findId(language)) {
        return new CodeHighlighter(document, codeRoleFormalComment, lang);
    }
    return nullptr;
}

TextDiffDocument::TextDiffDocument(const QString& language, QObject* parent):
    ComparisonDocument(parent), m_language(language)
{
    m_basePane = new DiffTextPane();
    m_targetPane = new DiffTextPane();
    m_basePane->setObjectName("textDiffBase");
    m_targetPane->setObjectName("textDiffTarget");
    m_focused = m_targetPane;
    m_ruler = new OverviewRuler();
    m_baseLabel = new QLabel();
    m_targetLabel = new QLabel();
    m_page = new QSplitter(Qt::Horizontal);
    m_page->addWidget(sidePane(m_baseLabel, m_basePane));
    m_page->addWidget(sidePane(m_targetLabel, m_targetPane, m_ruler));
    // the rows are aligned: one scroll for both sides (EDIT-MODE-21)
    for (DiffTextPane* from : { m_basePane, m_targetPane }) {
        DiffTextPane* to = from == m_basePane ? m_targetPane : m_basePane;
        connect(from->verticalScrollBar(), &QScrollBar::valueChanged,
                to->verticalScrollBar(), &QScrollBar::setValue);
        connect(from->horizontalScrollBar(), &QScrollBar::valueChanged,
                to->horizontalScrollBar(), &QScrollBar::setValue);
        connect(from, &DiffTextPane::focused, this, [this, from]() { paneFocused(from); });
    }
    m_ruler->setPane(m_targetPane);
    connect(m_ruler, &OverviewRuler::rowClicked, this, &TextDiffDocument::scrollToRow);
    m_baseHighlighter = highlighterOf(m_basePane->document(), m_language);
    m_targetHighlighter = highlighterOf(m_targetPane->document(), m_language);
    // a language a plugin brought may go with a scan: its highlighters go first
    connect(&CodeStyle::instance(), &CodeStyle::languagesChanged, this, [this]() {
        delete m_baseHighlighter;
        delete m_targetHighlighter;
        m_baseHighlighter = highlighterOf(m_basePane->document(), m_language);
        m_targetHighlighter = highlighterOf(m_targetPane->document(), m_language);
    });
}

// the page goes first: the highlighters belong to the documents of its sides
TextDiffDocument::~TextDiffDocument()
{
    delete m_baseHighlighter;
    delete m_targetHighlighter;
    delete m_page;
}

QWidget* TextDiffDocument::page() const
{
    return m_page;
}

void TextDiffDocument::setIgnoreWhitespace(bool on)
{
    if (m_options.ignoreWhitespace == on) return;
    m_options.ignoreWhitespace = on;
    m_current = -1;
    compare();
}

void TextDiffDocument::loadSides()
{
    m_baseLabel->setText(tr("Base: %1").arg(m_base.label));
    m_targetLabel->setText(tr("Target: %1").arg(m_target.label));
    m_baseLines = splitLines(QString::fromUtf8(m_baseBuffer.data(), int(m_baseBuffer.size())));
    m_targetLines = splitLines(QString::fromUtf8(m_targetBuffer.data(), int(m_targetBuffer.size())));
    compare();
}

void TextDiffDocument::compare()
{
    m_result = diffLines(m_baseLines, m_targetLines, m_options);
    fillPanes();
    // the matches are of the old rows
    if (!m_findQuery.isEmpty()) runFind(0, false);
    emit compared();
}

void TextDiffDocument::fillPanes()
{
    QStringList left, right;
    QVector<int> leftLines, rightLines, kinds;
    for (const Row& row : m_result.rows) {
        left << (row.base >= 0 ? m_baseLines[row.base] : QString());
        right << (row.target >= 0 ? m_targetLines[row.target] : QString());
        leftLines << row.base;
        rightLines << row.target;
        kinds << row.kind;
    }
    int value = m_targetPane->verticalScrollBar()->value();
    m_basePane->setLineNumbers(leftLines);
    m_targetPane->setLineNumbers(rightLines);
    m_basePane->setPlainText(left.join('\n'));
    m_targetPane->setPlainText(right.join('\n'));
    m_ruler->setRows(kinds);
    markPanes();
    // a refresh keeps the place
    m_targetPane->verticalScrollBar()->setValue(value);
}

// the background of a row, the whole width
static QTextEdit::ExtraSelection rowMark(QTextDocument* document, int row, const QBrush& brush)
{
    QTextEdit::ExtraSelection mark;
    mark.cursor = QTextCursor(document->findBlockByNumber(row));
    mark.format.setBackground(brush);
    mark.format.setProperty(QTextFormat::FullWidthSelection, true);
    return mark;
}

static QTextEdit::ExtraSelection spanMark(QTextDocument* document, int row, const Span& span)
{
    QTextEdit::ExtraSelection mark;
    QTextBlock block = document->findBlockByNumber(row);
    mark.cursor = QTextCursor(document);
    mark.cursor.setPosition(block.position() + span.from);
    mark.cursor.setPosition(block.position() + span.to, QTextCursor::KeepAnchor);
    mark.format.setBackground(QColor(TEXT_DIFF_SPAN_COLOR));
    return mark;
}

void TextDiffDocument::markPanes()
{
    m_baseMarks.clear();
    m_targetMarks.clear();
    QTextDocument* left = m_basePane->document();
    QTextDocument* right = m_targetPane->document();
    const QBrush filler(QColor(TEXT_DIFF_FILLER_COLOR), Qt::BDiagPattern);
    for (int r = 0; r < m_result.rows.size(); r++) {
        const Row& row = m_result.rows[r];
        switch (row.kind) {
        case rowEqual:
            break;
        case rowRemoved:
            m_baseMarks << rowMark(left, r, QColor(TEXT_DIFF_REMOVED_COLOR));
            m_targetMarks << rowMark(right, r, filler);
            break;
        case rowAdded:
            m_baseMarks << rowMark(left, r, filler);
            m_targetMarks << rowMark(right, r, QColor(TEXT_DIFF_ADDED_COLOR));
            break;
        case rowChanged:
            m_baseMarks << rowMark(left, r, QColor(TEXT_DIFF_CHANGED_COLOR));
            m_targetMarks << rowMark(right, r, QColor(TEXT_DIFF_CHANGED_COLOR));
            for (const Span& span : row.baseSpans) m_baseMarks << spanMark(left, r, span);
            for (const Span& span : row.targetSpans) m_targetMarks << spanMark(right, r, span);
            break;
        }
    }
    // the matches over the row marks of their side (EDIT-MODE-24)
    QList<QTextEdit::ExtraSelection> found;
    if (m_findPane) {
        for (int i = 0; i < m_findMatches.size(); i++) {
            QTextEdit::ExtraSelection mark;
            mark.cursor = QTextCursor(m_findPane->document());
            mark.cursor.setPosition(m_findMatches[i].from);
            mark.cursor.setPosition(m_findMatches[i].to, QTextCursor::KeepAnchor);
            mark.format.setBackground(QColor(i == m_findCurrent ? FIND_CURRENT_COLOR : FIND_MATCH_COLOR));
            mark.format.setForeground(Qt::black);
            found << mark;
        }
    }
    m_basePane->setExtraSelections(m_baseMarks + (m_findPane == m_basePane ? found : QList<QTextEdit::ExtraSelection>()));
    m_targetPane->setExtraSelections(m_targetMarks + (m_findPane == m_targetPane ? found : QList<QTextEdit::ExtraSelection>()));
}

QString TextDiffDocument::lineRange(int line, int count)
{
    if (count <= 0) return QStringLiteral(".");
    if (count == 1) return QString::number(line + 1);
    return QString("%1-%2").arg(line + 1).arg(line + count);
}

QList<ChangeEntry> TextDiffDocument::changes() const
{
    QList<ChangeEntry> list;
    for (const Hunk& hunk : m_result.hunks) {
        ChangeEntry entry;
        entry.element = QString::fromUtf8("%1 → %2").arg(lineRange(hunk.baseLine, hunk.baseCount),
                                                        lineRange(hunk.targetLine, hunk.targetCount));
        if (hunk.baseCount > 0 && hunk.targetCount > 0) {
            entry.change = tr("changed %1 → %2").arg(hunk.baseCount).arg(hunk.targetCount);
            entry.kind = ChangeEntry::changeChanged;
        } else if (hunk.baseCount > 0) {
            entry.change = tr("removed %1").arg(hunk.baseCount);
            entry.kind = ChangeEntry::changeRemoved;
        } else {
            entry.change = tr("added %1").arg(hunk.targetCount);
            entry.kind = ChangeEntry::changeAdded;
        }
        list << entry;
    }
    return list;
}

// a few rows above the row, the cursors of both sides at it
void TextDiffDocument::scrollToRow(int row)
{
    for (DiffTextPane* pane : { m_basePane, m_targetPane }) {
        QTextCursor cursor(pane->document()->findBlockByNumber(row));
        pane->setTextCursor(cursor);
    }
    m_targetPane->verticalScrollBar()->setValue(qMax(0, row - TEXT_DIFF_CONTEXT_ROWS));
}

void TextDiffDocument::showChange(int index)
{
    scrollToRow(m_result.hunks.at(index).firstRow);
}

void TextDiffDocument::paneFocused(DiffTextPane* pane)
{
    if (m_focused == pane) return;
    m_focused = pane;
    // an open search goes with the focus
    if (!m_findQuery.isEmpty()) runFind(pane->textCursor().selectionStart(), false);
}

void TextDiffDocument::setFindQuery(const FindQuery& query, bool navigate)
{
    m_findQuery = query;
    int place = m_findCurrent >= 0 && m_findPane == m_focused ? m_findMatches[m_findCurrent].from
                                                              : m_focused->textCursor().selectionStart();
    runFind(place, navigate);
}

void TextDiffDocument::runFind(int place, bool navigate)
{
    m_findPane = m_focused;
    m_findMatches.clear();
    m_findMore = false;
    if (!m_findQuery.isEmpty()) {
        m_findMatches = findRanges(m_findPane->toPlainText(), m_findQuery.expression(),
                                   FIND_MATCH_LIMIT, &m_findMore);
    }
    m_findCurrent = m_findMatches.isEmpty() ? -1 : 0;
    for (int i = 0; i < m_findMatches.size(); i++) {
        if (m_findMatches[i].from >= place) {
            m_findCurrent = i;
            break;
        }
    }
    markPanes();
    if (navigate) showFindMatch();
    emit findResultChanged(m_findCurrent, m_findMatches.size(), m_findMore);
}

// the cursor at the match without a selection: a selection is drawn over the marks
void TextDiffDocument::showFindMatch()
{
    if (m_findCurrent < 0 || !m_findPane) return;
    QTextCursor cursor(m_findPane->document());
    cursor.setPosition(m_findMatches[m_findCurrent].to);
    m_findPane->setTextCursor(cursor);
    m_findPane->ensureCursorVisible();
    cursor.setPosition(m_findMatches[m_findCurrent].from);
    m_findPane->setTextCursor(cursor);
    m_findPane->ensureCursorVisible();
}

void TextDiffDocument::moveFind(int step)
{
    int count = m_findMatches.size();
    if (count > 0) {
        m_findCurrent = ((m_findCurrent + step) % count + count) % count;
        markPanes();
        showFindMatch();
    }
    emit findResultChanged(m_findCurrent, count, m_findMore);
}

void TextDiffDocument::clearFind(bool keep)
{
    if (keep && m_findCurrent >= 0 && m_findPane) {
        QTextCursor cursor(m_findPane->document());
        cursor.setPosition(m_findMatches[m_findCurrent].from);
        cursor.setPosition(m_findMatches[m_findCurrent].to, QTextCursor::KeepAnchor);
        m_findPane->setTextCursor(cursor);
    }
    DiffTextPane* pane = m_findPane;
    m_findQuery = FindQuery();
    m_findMatches.clear();
    m_findCurrent = -1;
    m_findMore = false;
    m_findPane = nullptr;
    markPanes();
    if (keep && pane) pane->setFocus();
}

QString TextDiffDocument::findSeed() const
{
    return findSeedOf(m_focused->textCursor().selectedText());
}
