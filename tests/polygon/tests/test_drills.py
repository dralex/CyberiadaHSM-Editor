# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the drill class tests
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

import tempfile
import unittest
from pathlib import Path

from polygon import catalog as CAT
from polygon import coverage as COV
from polygon import dump as D
from polygon import oracles
from polygon.drills import nesting, features, compound
from tests.helpers import GOOD

ALL_DRILLS = [nesting.NestingDrill, nesting.PseudostateDrill,
              features.TransitionPointDrill, features.RebindDrill,
              features.ChoiceDrill, features.ActionDrill,
              features.ResizeDrill, features.CopyPasteDrill,
              compound.CompoundDrill]


def dump_with_stack(name):
    d = D.parse_dump((GOOD / (name + "-output.txt")).read_text())
    d.stack = D.Stack(2, 1, False)
    return d


class DrillTest(unittest.TestCase):
    def test_each_drill_emits_valid_steps(self):
        cov = COV.Coverage(Path(tempfile.mkdtemp()) / "c.json")
        empty = dump_with_stack("hierarchy")
        for cls in ALL_DRILLS:
            with self.subTest(cls.name):
                d = cls(CAT.Catalog(), cov, 3, budget=6)
                for _ in range(6):
                    step = d.next(empty)
                    if step is None:
                        break
                    lines, verb, kind = step[0], step[1], step[2]
                    self.assertTrue(verb.startswith("drill:" + cls.name))
                    self.assertTrue(all(l.split()[0] for l in lines))

    def test_nesting_builds_a_chain(self):
        cov = COV.Coverage(Path(tempfile.mkdtemp()) / "c.json")
        d = nesting.NestingDrill(CAT.Catalog(), cov, 1, budget=6)
        doc = dump_with_stack("geometry")
        first = d.next(doc)
        self.assertIn("new-state", first[0][0])
        self.assertTrue(d.names)

    def test_compound_interleaves_and_tracks_the_model(self):
        cov = COV.Coverage(Path(tempfile.mkdtemp()) / "c.json")
        d = compound.CompoundDrill(CAT.Catalog(), cov, 5, budget=30)
        doc = dump_with_stack("hierarchy")
        verbs = set()
        for _ in range(30):
            step = d.next(doc)
            self.assertIsNotNone(step)
            for line in step[0]:
                verbs.add(line.split()[0])
        # the model was grounded from the 8-state hierarchy every round
        self.assertGreaterEqual(len(d.model.states()), 8)
        # more than one kind of operation was emitted (interleaving)
        self.assertTrue(len(verbs & {"new-state", "new-transition", "new-action",
                                     "reparent", "delete", "paste"}) >= 2)

    def test_boundary_reaches_extreme_rects(self):
        cov = COV.Coverage(Path(tempfile.mkdtemp()) / "c.json")
        d = compound.CompoundDrill(CAT.Catalog(), cov, 2, budget=20)
        d.boundary = True
        doc = dump_with_stack("hierarchy")
        widths = set()
        for _ in range(20):
            step = d.next(doc)
            for line in step[0]:
                if line.startswith("new-state"):
                    widths.add(int(line.split()[4]))   # the w field
        # tiny and huge widths both appear
        self.assertTrue(any(w <= 40 for w in widths) and any(w >= 1000 for w in widths))

    def test_invariant_kind_registers(self):
        # KIND_INVARIANT is a defect kind (registered), unlike KIND_SEMANTIC
        from polygon import register as R
        self.assertIn(oracles.KIND_INVARIANT, R.DEFECT_KINDS)
        self.assertNotIn(oracles.KIND_SEMANTIC, R.DEFECT_KINDS)


if __name__ == "__main__":
    unittest.main()
