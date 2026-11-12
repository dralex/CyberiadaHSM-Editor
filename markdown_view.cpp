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

#include <QAbstractTextDocumentLayout>
#include <QDesktopServices>
#include <QEvent>
#include <QHash>
#include <QScrollBar>
#include <QTextBlock>

#include "markdown_view.h"
#include "markdown_importer.h"
#include "markdown_objects.h"

MarkdownView::MarkdownView(QWidget* parent): QTextBrowser(parent)
{
    // the links are opened here: a heading inside, a file or a site outside
    setOpenLinks(false);
    connect(this, &QTextBrowser::anchorClicked, this, &MarkdownView::openLink);
}

void MarkdownView::setMarkdownText(const QString& text, const QString& baseDir)
{
    m_baseDir = baseDir;
    setMarkdownTextColor(document(), palette().color(QPalette::Text));
    importMarkdown(document(), text, baseDir);
}

QString MarkdownView::headingAnchor(const QString& heading)
{
    QString anchor;
    const QString text = heading.trimmed().toLower();
    for (const QChar& c : text) {
        if (c.isLetterOrNumber() || c == '-' || c == '_') anchor += c;
        else if (c.isSpace()) anchor += '-';
    }
    return anchor;
}

bool MarkdownView::scrollToHeading(const QString& anchor)
{
    // a repeated heading is anchor-1, anchor-2, ... as GitHub numbers them
    QHash<QString, int> seen;
    for (QTextBlock b = document()->begin(); b.isValid(); b = b.next()) {
        if (b.blockFormat().headingLevel() <= 0) continue;
        QString name = headingAnchor(b.text());
        int n = seen.value(name, 0);
        seen.insert(name, n + 1);
        if (n > 0) name += "-" + QString::number(n);
        if (name == anchor.toLower()) {
            setTextCursor(QTextCursor(b));
            verticalScrollBar()->setValue(int(document()->documentLayout()->blockBoundingRect(b).top()));
            return true;
        }
    }
    return false;
}

void MarkdownView::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange) {
        setMarkdownTextColor(document(), palette().color(QPalette::Text));
        viewport()->update();
    }
    QTextBrowser::changeEvent(event);
}

void MarkdownView::openLink(const QUrl& url)
{
    if (url.scheme().isEmpty() && url.path().isEmpty() && url.hasFragment()) {
        scrollToHeading(url.fragment(QUrl::FullyDecoded));
        return;
    }
    QUrl target = url;
    if (url.isRelative() && !m_baseDir.isEmpty())
        target = QUrl::fromLocalFile(m_baseDir + "/").resolved(url);
    QDesktopServices::openUrl(target);
}
