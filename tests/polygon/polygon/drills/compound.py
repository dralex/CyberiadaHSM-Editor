# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the compound, boundary and history-deep drills
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

"""The compound drill interleaves the features over the whole accumulated
structure. Each round grounds the shadow model from the real dump, then
predicts exactly one operation's delta; the model equivalence check verifies
the editor produced that delta and nothing else. This is where the cross-
feature invariants live: a reparent must keep the state's incident
transitions, a delete must carry its subtree and incident transitions, an
action must survive a reparent."""

from .. import dump as D
from .base import Drill


class CompoundDrill(Drill):
    name = "compound"

    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        self.tracks_model = True
        self.boundary = False       # set by the boundary sweep

    # --- structure helpers on the grounded model --------------------------

    def _subtree(self, e):
        out, stack = set(), [e]
        while stack:
            x = stack.pop()
            out.add(x)
            stack.extend(x.children)
        return out

    def _placement(self):
        if self.boundary:
            # extremes: tiny, huge, negative-origin
            return self.rng.choice([(0, 0, 8, 8), (10, 10, 1600, 1200),
                                    (-400, -300, 200, 150), (20, 20, 40, 40)])
        return (30, 30, 160, 110)

    # --- the operations (each mutates the model and returns editor lines) --

    def _depth(self, e):
        d, n = 0, e
        while n.parent is not None:
            d, n = d + 1, n.parent
        return d

    def _add_state(self, dump, machine, states):
        if self.boundary and states:
            # drive the nesting depth: always dig into the deepest state
            container = max(states, key=self._depth)
        else:
            container = self.rng.choice(states + [None]) if states else None
        parent_id = container.dump_id if container is not None else machine.id
        name = self.fresh("K")
        x, y, w, h = self._placement()
        # spread the siblings so the editor does not stack them (a real overlap
        # of two states is a NODE-6 defect; two states placed at the same coords
        # is the producer's doing, not the editor's)
        siblings = (container.children if container is not None
                    else [e for e in self.model.elements if e.parent is None])
        n = sum(1 for c in siblings if c.kind in D.STATE_KINDS)
        x += n * (w + 40)
        self.model.add(D.KIND_SIMPLE, name, container)
        return ["new-state %s %d %d %d %d %s" % (parent_id, x, y, w, h, name)]

    def _add_transition(self, dump, machine, states):
        a, b = self.rng.sample(states, 2)
        self.model.add_transition(a, b)
        return ["new-transition %s %s %s EV%d" % (machine.id, a.dump_id, b.dump_id, self.step)]

    def _add_action(self, dump, machine, states):
        s = self.rng.choice(states)
        notation = "EV%d/ act%d()" % (self.step, self.counter)
        self.counter += 1
        s.actions.append(notation)
        return ["new-action %s %s" % (s.dump_id, notation)]

    def _reparent(self, dump, machine, states):
        movable = [s for s in states if s.dump_id]
        self.rng.shuffle(movable)
        for child in movable:
            forbidden = self._subtree(child)
            targets = [s for s in states if s not in forbidden and s is not child.parent]
            if not targets:
                continue
            dest = self.rng.choice(targets + [None])   # None = up to the machine
            dest_id = dest.dump_id if dest is not None else machine.id
            self.model.reparent(child, dest)
            return ["reparent %s %s" % (child.dump_id, dest_id)]
        return None

    def _delete(self, dump, machine, states):
        s = self.rng.choice(states)
        self.model.remove(s)
        return ["delete %s" % s.dump_id]

    def _paste(self, dump, machine, states):
        # unpredictable ids/names: apply it, let the generic oracles guard this
        # round, and re-ground the model next round. No equivalence facts here.
        s = self.rng.choice(states)
        item = dump.scene_items().get(s.dump_id)
        if item is None:
            return None
        cx, cy = item.abs_rect[0] + item.abs_rect[2] / 2, item.abs_rect[1] + item.abs_rect[3] / 2
        self._skip_model = True
        return ["click %d %d" % (round(cx), round(cy)), "copy", "paste"]

    def emit(self, dump):
        machine = self.machine(dump.document)
        if machine is None:
            return None
        self.model.import_dump(dump)
        states = self.model.states()
        self._skip_model = False
        # grow a baseline before the interleaving has material to work on
        if len(states) < 3:
            lines = self._add_state(dump, machine, states)
            return lines, "state", self.merged(dump)
        if self.boundary:
            # extremes: deep nesting, long action lists, many transitions
            ops = [self._add_state, self._add_state, self._add_action,
                   self._add_action, self._add_transition]
            op = self.rng.choice(ops)
            lines = op(dump, machine, states)
            return (lines or self._add_state(dump, machine, states)), "state", self.merged(dump)
        ops = [self._add_state, self._add_state, self._add_transition,
               self._add_action, self._reparent, self._paste]
        if len(states) > 4:
            ops.append(self._delete)
        if len(self.model.transitions) < 6:
            ops.append(self._add_transition)
        op = self.rng.choice(ops)
        lines = op(dump, machine, states)
        if lines is None:
            lines = self._add_state(dump, machine, states)
        exp = "" if self._skip_model else self.merged(dump)
        return lines, "state", exp
