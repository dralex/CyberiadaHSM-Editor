/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Markdown into a document: the text, the formulas and the images
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

#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include "markdown_importer.h"
#include "markdown_objects.h"

namespace {

const char* const VOID_ELEMENTS =
    "area|base|br|col|embed|hr|img|input|link|meta|param|source|track|wbr";
const char* const TEXT_ELEMENTS =
    "a|abbr|b|big|cite|code|del|dfn|em|font|i|ins|kbd|mark|q|s|samp|small|span|"
    "strike|strong|sub|sup|tt|u|var";

void alignDisplayFormulas(QTextDocument* doc)
{
    QTextCursor cursor(doc);
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        if (b.text().trimmed() != QString(QChar::ObjectReplacementCharacter)) continue;
        for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it) {
            QTextCharFormat f = it.fragment().charFormat();
            if (f.objectType() == MarkdownMathObject && f.boolProperty(MarkdownDisplay)) {
                QTextBlockFormat bf = b.blockFormat();
                bf.setAlignment(Qt::AlignHCenter);
                cursor.setPosition(b.position());
                cursor.setBlockFormat(bf);
                break;
            }
        }
    }
}

} // namespace

bool isForeignTag(const QString& html)
{
    static const QRegularExpression tag("^</?([a-zA-Z][a-zA-Z0-9-]*)\\b[^>]*>$");
    static const QRegularExpression known(QString("^(?:%1|%2)$").arg(VOID_ELEMENTS, TEXT_ELEMENTS),
                                          QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m = tag.match(html);
    return m.hasMatch() && !known.match(m.captured(1)).hasMatch();
}

QString closeVoidTags(const QString& html)
{
    static const QRegularExpression re(QString("<((?:%1)\\b[^>]*?)\\s*/?>").arg(VOID_ELEMENTS),
                                       QRegularExpression::CaseInsensitiveOption);
    QString out = html;
    out.replace(re, "<\\1/>");
    return out;
}

void importMarkdown(QTextDocument* doc, const QString& text, const QString& baseDir)
{
    installMarkdownObjects(doc);
    // the document keeps no history of its own rendering
    bool undo = doc->isUndoRedoEnabled();
    doc->setUndoRedoEnabled(false);
    readMarkdownMd4c(doc, text, baseDir);
    alignDisplayFormulas(doc);
    doc->setUndoRedoEnabled(undo);
}
