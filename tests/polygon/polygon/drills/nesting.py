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

    def emit(self, dump):
        doc = dump.document
        machine = self.machine(doc)
        if machine is None:
            return None
        # every third step (once there is depth) exercises out-then-in
        if len(self.names) >= 1 and self.step % 3 == 2:
            inner = _id_by_name(doc, self.names[-1])
            if inner is None or inner.parent is None:
                return self._create(doc, machine)
            parent = inner.parent.id
            lines = ["reparent %s %s" % (inner.id, machine.id),
                     "reparent %s %s" % (inner.id, parent)]
            # the round trip must restore the nesting and conserve the count
            exp = "state %s parent %s\ncount state %d" % (inner.name, parent, len(self.states(doc)))
            return lines, "state", exp
        return self._create(doc, machine)

    def _create(self, doc, machine):
        name = self.fresh("N")
        parent = _id_by_name(doc, self.names[-1]).id if self.names and _id_by_name(doc, self.names[-1]) else machine.id
        self.names.append(name)
        # nested inside the deepest state, with a margin
        exp = "state %s parent %s" % (name, parent)
        return ["new-state %s 40 40 220 140 %s" % (parent, name)], "state", exp


class PseudostateDrill(Drill):
    """Place initial and final pseudostates inside and outside states, and the
    unique-initial-per-level step: two initials on one level must not both
    survive (a crash there is the P-1 hang, a surviving pair an invariant
    violation)."""

    name = "pseudostate"

    def emit(self, dump):
        doc = dump.document
        if doc is None:
            return None
        machine = self.machine(doc)
        if machine is None:
            return None
        states = self.states(doc)
        step = self.step
        if step == 0:
            # a state to place pseudostates into
            return ["new-state %s 60 60 300 200 Host" % machine.id], "state", ""
        host = states[0].id if states else machine.id
        if step == 1:
            return ["new-final %s" % host], "final", "count final 1"
        if step == 2:
            return ["new-initial %s" % host], "initial", "count initial 1"
        if step == 3:
            # the unique-initial rule: a second initial on the same level
            return ["new-initial %s" % host], "initial", "count initial 1"
        # escalate: initials and finals at the machine level too
        if step % 2 == 0:
            return ["new-final %s" % machine.id], "final", ""
        return ["new-initial %s" % machine.id], "initial", "count initial 1"
