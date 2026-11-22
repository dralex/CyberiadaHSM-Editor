/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The outline of a Markdown text: its headings and its images in the order of the text
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

#include "markdown_outline.h"

#include <QFileInfo>

#include <algorithm>

#include "markdown_tools.h"

QString OutlineEntry::label() const
{
    if (isImage() && title.isEmpty()) return QFileInfo(target).fileName();
    return title;
}

QString OutlineEntry::key() const
{
    // the kinds never match each other
    return (isImage() ? QString("!") : QString("#")) + name();
}

QString OutlineEntry::name() const
{
    return isImage() ? target : title;
}

bool OutlineEntry::operator==(const OutlineEntry& other) const
{
    return kind == other.kind && level == other.level && line == other.line &&
        column == other.column && title == other.title && anchor == other.anchor &&
        target == other.target && link == other.link && svg == other.svg &&
        graphml == other.graphml && upToDate == other.upToDate;
}

QList<OutlineEntry> markdownOutline(const QString& text, const QString& baseDir, DiagramFileCache* cache)
{
    QList<OutlineEntry> entries;
    for (const MarkdownHeading& heading : markdownHeadings(text)) {
        OutlineEntry e;
        e.kind = OutlineEntry::outlineHeading;
        e.level = heading.level;
        e.line = heading.line;
        e.column = 1;
        e.title = heading.title;
        e.anchor = heading.anchor;
        entries << e;
    }
    for (const MarkdownImage& image : markdownImages(text)) {
        OutlineEntry e;
        e.kind = OutlineEntry::outlineImage;
        e.line = image.line;
        e.column = image.column;
        e.title = image.alt;
        e.target = image.target;
        e.link = image.link;
        DiagramImage diagram;
        if (diagramOfImage(image, baseDir, &diagram, cache)) {
            e.kind = OutlineEntry::outlineDiagram;
            e.svg = diagram.svg;
            e.graphml = diagram.graphml;
            e.upToDate = diagram.upToDate;
        }
        entries << e;
    }
    // by the line, a heading before the images of its line, the images by the column
    std::stable_sort(entries.begin(), entries.end(), [](const OutlineEntry& a, const OutlineEntry& b) {
        if (a.line != b.line) return a.line < b.line;
        if (a.isImage() != b.isImage()) return !a.isImage();
        return a.column < b.column;
    });
    return entries;
}
