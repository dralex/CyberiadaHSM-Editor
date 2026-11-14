/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * A comparison tab: two sides from their origins and the list of their changes
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

#include <QUndoStack>

#include "comparison_document.h"

ComparisonDocument::ComparisonDocument(QObject* parent):
    EditorDocument(parent)
{
    m_undo = new QUndoStack(this);
}

QString ComparisonDocument::title() const
{
    return m_target.label + QString::fromUtf8(" ↔ ") + m_base.label;
}

void ComparisonDocument::setSides(const std::string& base, const std::string& target)
{
    m_baseBuffer = base;
    m_targetBuffer = target;
    // the rows are new: none is current
    m_current = -1;
    loadSides();
    emit titleChanged();
}

bool ComparisonDocument::setCurrentChange(int index)
{
    if (index < 0 || index >= changes().size()) return false;
    m_current = index;
    showChange(index);
    emit currentChangeChanged(index);
    return true;
}

bool ComparisonDocument::moveChange(int step)
{
    int count = changes().size();
    if (count == 0) return false;
    // from no row the first or the last one; the ends stop
    int index = m_current < 0 ? (step > 0 ? 0 : count - 1) : m_current + step;
    return setCurrentChange(index);
}
