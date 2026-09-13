# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the tool-coverage matrix
#
# Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see https://www.gnu.org/licenses/
#
# -----------------------------------------------------------------------------

"""The systematic coverage matrix over (tool, pattern) cells for the tour:
which tools have been exercised in which usage patterns."""

import json
from pathlib import Path

from . import catalog as CAT

# the usage patterns of a tool, in prose for the agent and as cell keys
PATTERNS = ("single", "several", "in-container", "extreme", "then-undo", "combined")

PATTERN_HINT = {
    "single": "place a single one",
    "several": "place several of them",
    "in-container": "place one inside a state or a state machine",
    "extreme": "use it at an extreme position (tiny, huge or negative coordinates)",
    "then-undo": "place one, then undo and redo it",
    "combined": "combine it with what you built (a transition, a comment, copy-paste)",
}


def tool_actions():
    """The names the tour covers: the creation tools, the transition draw, the
    clipboard, and the select-tool manipulations."""
    creation = [t.name for t in CAT.creation_tools()]
    return creation + ["draw-transition", "copy-paste", "cut-paste",
                       "drag-state", "resize-state", "double-click-action",
                       "edit-title", "edit-action", "edit-label",
                       "add-point", "move-point", "remove-point",
                       "move-endpoint", "click-delete"]


class ToolCoverage:
    def __init__(self, path):
        self.path = Path(path)
        self.cells = {}
        if self.path.exists():
            self.cells = json.loads(self.path.read_text()).get("cells", {})

    @staticmethod
    def key(action, pattern):
        return "%s|%s" % (action, pattern)

    def record(self, action, pattern):
        k = self.key(action, pattern)
        self.cells[k] = self.cells.get(k, 0) + 1

    def count(self, action, pattern):
        return self.cells.get(self.key(action, pattern), 0)

    def all_cells(self):
        return [(a, p) for a in tool_actions() for p in PATTERNS]

    def untried(self):
        return [(a, p) for a, p in self.all_cells() if self.count(a, p) == 0]

    def save(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.path.write_text(json.dumps({"cells": dict(sorted(self.cells.items()))}, indent=2) + "\n")

    def report(self):
        total = len(self.all_cells())
        done = sum(1 for a, p in self.all_cells() if self.count(a, p))
        lines = ["tool coverage: %d / %d cells (%.0f%%)" % (done, total, 100.0 * done / total)]
        for a in tool_actions():
            marks = "".join("#" if self.count(a, p) else "." for p in PATTERNS)
            lines.append("  %-20s %s" % (a, marks))
        lines.append("  patterns: " + " ".join(PATTERNS))
        return "\n".join(lines)
