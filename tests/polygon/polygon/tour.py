# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the deterministic tool tour
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

"""A no-LLM producer that walks the (tool, pattern) coverage matrix and emits
the gestures itself, reusing the fuzzer's tool emitters. It is the free,
guaranteed-coverage workhorse of the tour."""

from . import fuzzer as F
from . import toolcover as TC


class DeterministicTour:
    def __init__(self, catalog, coverage, toolcover, seed):
        self.fuzzer = F.Fuzzer(catalog, coverage, seed)
        self.toolcover = toolcover
        self.rng = self.fuzzer.rng

    def next(self, dump, exclude=()):
        """Pick the next untried (action, pattern) cell whose gesture applies,
        emit it with the pattern variation, record the cell."""
        cells = self.toolcover.untried() or self.toolcover.all_cells()
        for action, pattern in cells:   # already in prerequisite order
            if action in exclude:
                continue
            step = self.emit(action, pattern, dump)
            if step is None:
                continue
            lines, kind = step
            self.toolcover.record(action, pattern)
            return lines, "tour:%s:%s" % (action, pattern), kind
        return None

    def emit(self, action, pattern, dump):
        base = self.fuzzer.generate(action, dump)
        if base is None:
            return None
        lines, kind = list(base[0]), base[1]
        if pattern == "several":
            more = self.fuzzer.generate(action, dump)
            if more is not None:
                lines = lines + more[0]
        elif pattern == "then-undo":
            lines = lines + ["undo", "redo"]
        elif pattern == "combined" and dump.document is not None:
            # follow a creation with a comment placed on the canvas
            follow = self.fuzzer.generate("new-comment", dump)
            if follow is not None:
                lines = lines + follow[0]
        # "single", "in-container", "extreme" use the base emitter as is
        return lines, kind
