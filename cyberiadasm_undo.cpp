/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The undo step: the document before and after one user gesture
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

#include "cyberiadasm_undo.h"
#include "cyberiadasm_model.h"

DocumentStep::DocumentStep(CyberiadaSMModel* model, const QString& text,
                           const std::string& before, const std::string& after):
    QUndoCommand(text), model(model), before(before), after(after), applied(true)
{
}

void DocumentStep::undo()
{
    model->restoreSnapshot(before);
    applied = false;
}

void DocumentStep::redo()
{
    if (applied) return;
    model->restoreSnapshot(after);
    applied = true;
}
