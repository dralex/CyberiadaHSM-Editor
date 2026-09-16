# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the nesting and pseudostate drills
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

"""Nesting (states in and out, deeper) and pseudostate (initial/final in and
out, the unique-initial-per-level rule) drills."""

from .. import dump as D
from .base import Drill


def _id_by_name(doc, name):
    for e in doc.walk():
        if e.is_state and e.name == name:
            return e
    return None


class NestingDrill(Drill):
    """Create a state, nest it deeper by reparenting, move it out then back in
    (reversibility), escalating the depth. Ids are resolved from the dump by
    name each round, so the chain is robust to the library's id scheme."""

    name = "nesting"

    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        self.names = []      # created state names, outer to inner
        self.nodes = {}      # name -> model element
        self.tracks_model = True

    def emit(self, dump):
        doc = dump.document
        machine = self.machine(doc)
        if machine is None:
            return None
        if self.model.machine_name is None:
            self.model.set_machine(machine.name)
        # every third step (once there is depth) exercises out-then-in
        if len(self.names) >= 1 and self.step % 3 == 2:
            inner = _id_by_name(doc, self.names[-1])
            if inner is None or inner.parent is None:
                return self._create(dump, doc, machine)
            parent = inner.parent.id
            lines = ["reparent %s %s" % (inner.id, machine.id),
                     "reparent %s %s" % (inner.id, parent)]
            # the round trip leaves the model untouched: the whole document must
            # come back identical (checked by the merged model equivalence facts)
            return lines, "state", self.merged(dump)
        return self._create(dump, doc, machine)

    def _create(self, dump, doc, machine):
        name = self.fresh("N")
        parent_name = self.names[-1] if self.names else None
        parent = _id_by_name(doc, parent_name).id if parent_name and _id_by_name(doc, parent_name) else machine.id
        self.names.append(name)
        self.nodes[name] = self.model.add(D.KIND_SIMPLE, name, self.nodes.get(parent_name))
        # nested inside the deepest state, with a margin
        return ["new-state %s 40 40 220 140 %s" % (parent, name)], "state", self.merged(dump)


class PseudostateDrill(Drill):
    """Place initial and final pseudostates inside and outside states, and the
    unique-initial-per-level step: two initials on one level must not both
    survive (a crash there is the P-1 hang, a surviving pair an invariant
    violation)."""

    name = "pseudostate"

    def emit(self, dump):
        doc = dump.document
        machine = self.machine(doc)
        if machine is None:
            return None
        states = [e for e in doc.walk() if e.is_state]
        step = self.step
        if step == 0:
            return ["new-state %s 60 60 300 220 Host" % machine.id], "state", ""
        host = states[0].id if states else machine.id
        if step == 1:
            # a final and an initial placed inside the host by the tool gesture
            hx, hy = self._host_point(dump, host)
            return (["tool new-initial", "click %d %d" % (hx, hy)], "initial", "")
        if step == 2:
            hx, hy = self._host_point(dump, host, dy=60)
            return (["tool new-final", "click %d %d" % (hx, hy)], "final", "")
        if step == 3:
            # the unique-initial probe: a second initial in the host by gesture.
            # the editor must reject or refuse it; a hang here is the P-1 defect,
            # caught by the crash oracle (no invariant fact - it is a gesture,
            # a possible no-op that the tool may silently drop)
            hx, hy = self._host_point(dump, host, dy=-40)
            return (["tool new-initial", "click %d %d" % (hx, hy)], "initial", "")
        # escalate: pseudostates at the machine level and in more states
        if step % 3 == 0:
            return ["new-state %s 60 60 260 180 %s" % (machine.id, self.fresh("S"))], "state", ""
        target = self.rng.choice(states).id if states else machine.id
        verb = self.rng.choice(["new-final", "new-initial", "new-terminate",
                                "new-shallow-history", "new-deep-history"])
        px, py = self._host_point(dump, target, dy=self.rng.choice([-40, 0, 40]))
        return (["tool %s" % verb, "click %d %d" % (px, py)], "state", "")

    def _host_point(self, dump, host_id, dy=0):
        item = dump.scene_items().get(host_id)
        if item is None:
            m = dump.document.machines()[0]
            item = dump.scene_items().get(m.id)
        x, y, w, h = item.abs_rect
        return round(x + w * 0.5), round(y + h * 0.5 + dy)
