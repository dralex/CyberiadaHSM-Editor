/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The rendered page of a read-only Markdown tab
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

#ifndef CYBERIADA_MARKDOWN_VIEW
#define CYBERIADA_MARKDOWN_VIEW

#include <QTextBrowser>

// the Markdown with its formulas and images (EDIT-MODE-14, docs/MARKDOWN.md)
class MarkdownView: public QTextBrowser {
Q_OBJECT
public:
    explicit MarkdownView(QWidget* parent = nullptr);

    // the text rendered again, its image paths relative to baseDir (empty: none)
    void setMarkdownText(const QString& text, const QString& baseDir);
    QString baseDir() const { return m_baseDir; }

    // the GitHub anchor of a heading: lower case, the spaces as '-', no punctuation
    static QString headingAnchor(const QString& heading);
    // scroll to the heading of the anchor; false when there is none
    bool scrollToHeading(const QString& anchor);

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void openLink(const QUrl& url);

private:
    QString m_baseDir;
};

#endif
