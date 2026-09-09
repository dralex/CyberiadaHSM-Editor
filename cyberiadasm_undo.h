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

#ifndef CYBERIADA_SM_UNDO_HEADER
#define CYBERIADA_SM_UNDO_HEADER

#include <string>
#include <QUndoCommand>

class CyberiadaSMModel;

// one user gesture: the whole document encoded before and after it; undo
// and redo decode the snapshots back into the model, which resets its views
class DocumentStep: public QUndoCommand {
public:
    DocumentStep(CyberiadaSMModel* model, const QString& text,
                 const std::string& before, const std::string& after);
    void undo() override;
    void redo() override;

private:
    CyberiadaSMModel* model;
    std::string       before;
    std::string       after;
    // the push runs redo() once while the mutation is already applied
    bool              applied;
};

#endif
