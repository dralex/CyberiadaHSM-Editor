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

#ifndef CYBERIADA_TEXT_DIFF_DOCUMENT
#define CYBERIADA_TEXT_DIFF_DOCUMENT

#include <QTextEdit>

#include "comparison_document.h"
#include "text_diff.h"

class QLabel;
class QSplitter;
class CodeHighlighter;
class DiffTextPane;
class OverviewRuler;

// A text diff tab (docs/MODES.md, The text diff tab): the base on the left, the target
// on the right, a row per line or filler, highlighted by the mode of the source
// (EDIT-MODE-19..24)
class TextDiffDocument: public ComparisonDocument {
Q_OBJECT
public:
    // language: the text mode of the source tab
    TextDiffDocument(const QString& language, QObject* parent = nullptr);
    ~TextDiffDocument();

    QString          mode() const override { return MODE_TEXT_DIFF; }
    QWidget*         page() const override;
    const QString&   language() const { return m_language; }

    // Document > Ignore Whitespace (EDIT-MODE-23)
    bool             ignoreWhitespace() const { return m_options.ignoreWhitespace; }
    void             setIgnoreWhitespace(bool on);

    const TextDiff::Result& result() const { return m_result; }
    const QStringList& baseLines() const { return m_baseLines; }
    const QStringList& targetLines() const { return m_targetLines; }
    // a row per hunk (EDIT-MODE-22)
    QList<ChangeEntry> changes() const override;
    // the lines of a side of a hunk as shown: "13", "13-15", "." for none
    static QString   lineRange(int line, int count);

    DiffTextPane*    basePane() const { return m_basePane; }
    DiffTextPane*    targetPane() const { return m_targetPane; }
    // the side the focus was in last, the target before any focus (EDIT-MODE-24)
    DiffTextPane*    focusedPane() const { return m_focused; }

    bool             canFind() const override { return true; }
    void             setFindQuery(const FindQuery& query, bool navigate) override;
    void             moveFind(int step) override;
    void             clearFind(bool keep) override;
    QString          findSeed() const override;
    const FindRanges& findMatches() const { return m_findMatches; }
    int              findCurrent() const { return m_findCurrent; }

protected:
    void             loadSides() override;
    void             showChange(int index) override;

private:
    void             compare();
    void             fillPanes();
    // the row marks of both sides, the find marks over them on the searched side
    void             markPanes();
    void             scrollToRow(int row);
    void             runFind(int place, bool navigate);
    void             showFindMatch();
    void             paneFocused(DiffTextPane* pane);

    QString          m_language;
    TextDiff::Options m_options;
    TextDiff::Result m_result;
    QStringList      m_baseLines;
    QStringList      m_targetLines;
    QList<QTextEdit::ExtraSelection> m_baseMarks;
    QList<QTextEdit::ExtraSelection> m_targetMarks;
    DiffTextPane*    m_basePane;
    DiffTextPane*    m_targetPane;
    DiffTextPane*    m_focused;
    CodeHighlighter* m_baseHighlighter = nullptr;
    CodeHighlighter* m_targetHighlighter = nullptr;
    OverviewRuler*   m_ruler;
    QLabel*          m_baseLabel;
    QLabel*          m_targetLabel;
    QSplitter*       m_page;
    FindQuery        m_findQuery;
    FindRanges       m_findMatches;
    int              m_findCurrent = -1;
    bool             m_findMore = false;
    // the side the matches are of
    DiffTextPane*    m_findPane = nullptr;
};

#endif
