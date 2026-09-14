# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the drill base
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

"""The base of the drill class: a deterministic producer that escalates one
feature and checks its invariants. Concrete drills override steps()."""

import random

from .. import dump as D
from .. import fuzzer as F
from .model import Model


class Drill:
    """A producer (Session.next protocol): each round is one escalation step,
    returning (lines, verb, kind, expectations). expectations are the drill's
    invariants, raised as registrable findings when the session runs in
    invariant mode."""

    name = "drill"

    def __init__(self, catalog, coverage, seed, budget=20):
        self.fuzzer = F.Fuzzer(catalog, coverage, seed)
        self.rng = self.fuzzer.rng
        self.budget = budget
        self.step = 0
        self.counter = 0
        # the shadow model; a drill that maintains it in lock-step sets
        # tracks_model and gets full-document equivalence for free
        self.model = Model()
        self.tracks_model = False

    def fresh(self, prefix):
        self.counter += 1
        return "%s%d" % (prefix, self.counter)

    def merged(self, dump, own=""):
        """The drill's own invariant facts plus, when the drill tracks the
        shadow model, the model's full equivalence facts."""
        facts = [l for l in own.splitlines() if l.strip()]
        if self.tracks_model:
            facts += self.model.check_facts(dump)
        return "\n".join(facts)

    # --- helpers on the current dump --------------------------------------

    @staticmethod
    def states(doc):
        return [e for e in doc.walk() if e.is_state] if doc else []

    @staticmethod
    def machine(doc):
        ms = doc.machines() if doc else []
        return ms[0] if ms else None

    def container_point(self, dump):
        """(id, x, y) inside a random state or the machine, to place into."""
        items = [i for i in dump.scene_items().values()
                 if i.kind in (D.KIND_SM,) + D.STATE_KINDS and i.rect[2] > 80 and i.rect[3] > 80]
        item = self.rng.choice(items) if items else None
        if item is None:
            return None
        x, y, w, h = item.abs_rect
        return item.id, round(x + w * 0.5), round(y + h * 0.5)

    # --- the escalation ---------------------------------------------------

    def steps(self, dump):
        """Yield (lines, kind, expectations) escalation steps; overridden."""
        raise NotImplementedError

    def next(self, dump, exclude=()):
        if self.step >= self.budget:
            return None
        try:
            result = self.emit(dump)
        except StopIteration:
            return None
        if result is None:
            return None
        lines, kind, expectations = result
        verb = "drill:%s:%d" % (self.name, self.step)
        self.step += 1
        return lines, verb, kind, expectations

    def emit(self, dump):
        """One escalation step from the current dump; None to stop."""
        raise NotImplementedError
