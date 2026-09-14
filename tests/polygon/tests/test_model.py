# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the drill shadow model tests
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

"""The shadow model builds the expected structure and its equivalence facts
must pass on a faithful dump and catch every injected divergence."""

import unittest

from polygon import dump as D
from polygon import expectations as EX
from polygon.drills.model import Model
from tests.helpers import GOOD


def load(name):
    return D.parse_dump((GOOD / (name + "-output.txt")).read_text())


def hierarchy_model():
    """A model mirroring good/hierarchy-output.txt."""
    m = Model()
    m.set_machine("SM")
    p0 = m.add(D.KIND_COMPOSITE, "Parent 0")
    m.add(D.KIND_SIMPLE, "State 0-0", p0)
    sub = m.add(D.KIND_COMPOSITE, "Subparent 0-1", p0)
    m.add(D.KIND_SIMPLE, "State 0-1-0", sub)
    m.add(D.KIND_SIMPLE, "State 0-1-1", sub)
    p1 = m.add(D.KIND_COMPOSITE, "Parent 1")
    m.add(D.KIND_SIMPLE, "State 1-0", p1)
    m.add(D.KIND_SIMPLE, "State 1-1", p1)
    return m


def failures(model, dump):
    """The facts the model asserts that do NOT hold on the dump."""
    bad = []
    for line in model.check_facts(dump):
        fact = EX.Fact(line)
        reason = EX.check(fact, dump)
        if reason is not None:
            bad.append((line, reason))
    return bad


class ModelTest(unittest.TestCase):
    def test_faithful_dump_passes(self):
        dump = load("hierarchy")
        self.assertEqual(failures(hierarchy_model(), dump), [])

    def test_resolves_nested_ids(self):
        dump = load("hierarchy")
        m = hierarchy_model()
        deep = [e for e in m.elements if e.name == "State 0-1-1"][0]
        self.assertEqual(m.resolve(dump, deep), "n0::n1::n1")

    def test_moved_child_is_caught(self):
        dump = load("hierarchy")
        m = hierarchy_model()
        child = [e for e in m.elements if e.name == "State 0-0"][0]
        other = [e for e in m.elements if e.name == "Parent 1"][0]
        m.reparent(child, other)          # the editor still has it under Parent 0
        self.assertTrue(failures(m, dump))

    def test_dropped_state_is_caught(self):
        dump = load("hierarchy")
        m = hierarchy_model()
        m.remove([e for e in m.elements if e.name == "State 1-1"][0])
        self.assertTrue(any("count state" in line for line, _ in failures(m, dump)))

    def test_extra_state_is_caught(self):
        dump = load("hierarchy")
        m = hierarchy_model()
        m.add(D.KIND_SIMPLE, "Ghost", None)
        self.assertTrue(failures(m, dump))

    def test_wrong_action_is_caught(self):
        dump = load("hierarchy")
        m = hierarchy_model()
        [e for e in m.elements if e.name == "State 0-0"][0].actions.append("entry/ boom")
        self.assertTrue(any("action" in line for line, _ in failures(m, dump)))

    def test_import_dump_round_trips(self):
        # a model grounded from a dump must describe that same dump exactly
        for name in ("hierarchy", "add-transition", "actions"):
            with self.subTest(name):
                dump = load(name)
                m = Model()
                m.import_dump(dump)
                self.assertEqual(failures(m, dump), [])

    def test_import_then_mutation_is_caught(self):
        # grounding then predicting a delta that the dump does NOT have fails
        dump = load("hierarchy")
        m = Model()
        m.import_dump(dump)
        a = m.states()[0]
        b = m.states()[1]
        m.add_transition(a, b)            # the hierarchy dump has no transitions
        self.assertTrue(failures(m, dump))

    def test_phantom_transition_is_caught(self):
        dump = load("hierarchy")
        m = hierarchy_model()
        a = [e for e in m.elements if e.name == "State 0-0"][0]
        b = [e for e in m.elements if e.name == "State 1-0"][0]
        m.add_transition(a, b)            # the dump has no transitions
        self.assertTrue(any("transition" in line for line, _ in failures(m, dump)))


class RectFactTest(unittest.TestCase):
    """The new exact-geometry fact in expectations.py."""

    def test_exact_rect_passes_and_perturbation_fails(self):
        dump = load("geometry")
        item = dump.scene_items()["node-0-0-1"]
        x, y, w, h = item.abs_rect
        ok = EX.Fact("rect node-0-0-1 %g %g %g %g" % (x, y, w, h))
        self.assertIsNone(EX.check(ok, dump))
        off = EX.Fact("rect node-0-0-1 %g %g %g %g" % (x + 40, y, w, h))
        self.assertIsNotNone(EX.check(off, dump))

    def test_within_tolerance_passes(self):
        dump = load("geometry")
        item = dump.scene_items()["node-0-0-1"]
        x, y, w, h = item.abs_rect
        near = EX.Fact("rect node-0-0-1 %g %g %g %g" % (x + 1, y - 1, w, h))
        self.assertIsNone(EX.check(near, dump))


if __name__ == "__main__":
    unittest.main()
