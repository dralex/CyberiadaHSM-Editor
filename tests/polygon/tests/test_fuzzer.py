# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the catalog, coverage and fuzzer tests
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
from polygon import fuzzer as F
from polygon import register as R
from polygon import session as S
from tests.helpers import CONFIG, DIAGRAMS, ENV, GOOD, needs_editor


def load(name):
    dump = D.parse_dump((GOOD / (name + "-output.txt")).read_text())
    dump.stack = D.Stack(count=2, index=1, clean=False)
    return dump


class CatalogTest(unittest.TestCase):
    def test_catalog(self):
        cat = CAT.Catalog()
        self.assertEqual(len(cat.by_form(CAT.FORM_MODEL)), 30)
        self.assertEqual(len(cat.by_form(CAT.FORM_GESTURE)), 16)
        self.assertEqual(len(cat.by_form(CAT.FORM_GESTURE_FORM)), 15)
        self.assertIn("| `new-state <parent> [x y w h] <name>` |", cat.table(CAT.FORM_MODEL))
        self.assertIn(("reparent", "simple"), cat.cells())

    def test_tool_model(self):
        ts = CAT.tools()
        self.assertEqual(len(ts), 17)
        self.assertEqual(len(CAT.creation_tools()), 13)
        self.assertEqual({t.name for t in ts if t.family == CAT.FAMILY_RECT},
                         {"new-sm", "new-state", "new-submachine-state"})

    def test_creation_and_manipulation_generate(self):
        cat = CAT.Catalog()
        cov = COV.Coverage(Path(tempfile.mkdtemp()) / "coverage.json")
        fuzzer = F.Fuzzer(cat, cov, 7)
        dump = load("geometry")   # a populated diagram: states, transitions
        # every creation tool emits a `tool <name>` gesture
        for tool in CAT.creation_tools():
            r = fuzzer.emit_creation(tool, dump)
            self.assertIsNotNone(r, tool.name)
            self.assertTrue(r[0][0].startswith("tool "))
        # the transition draw and the select-tool manipulations
        self.assertIsNotNone(fuzzer.emit_transition(dump))
        for name in ("drag-state", "resize-state", "click-delete", "add-point", "move-endpoint"):
            self.assertIsNotNone(fuzzer.generate(name, dump), name)

    def test_seed_determinism(self):
        cat = CAT.Catalog()
        dump = load("lift")
        a = F.Fuzzer(cat, COV.Coverage(Path(tempfile.mkdtemp()) / "c.json"), 3)
        b = F.Fuzzer(cat, COV.Coverage(Path(tempfile.mkdtemp()) / "c.json"), 3)
        self.assertEqual([a.next(dump) for _ in range(10)], [b.next(dump) for _ in range(10)])


class CoverageTest(unittest.TestCase):
    def test_weights(self):
        with tempfile.TemporaryDirectory() as tmp:
            cov = COV.Coverage(Path(tmp) / "coverage.json")
            self.assertEqual(cov.weight("move", "simple"), 1.0)
            cov.record("move", "simple", None)
            cov.record("move", "simple", "move", fired=True)
            self.assertEqual(cov.count("move", "simple"), 2)
            self.assertEqual(cov.pairs["move>move"], 1)
            self.assertGreater(cov.weight("move", "simple"), cov.weight("move", "composite") / 3)
            self.assertLess(cov.weight("move", "simple") - COV.FIRED_BONUS / 3, 1.0)
            cov.save()
            again = COV.Coverage(Path(tmp) / "coverage.json")
            self.assertEqual(again.cells, cov.cells)
            self.assertIn(("polyline", "transition"), again.untried(CAT.Catalog()))


@needs_editor
class FuzzSessionTest(unittest.TestCase):
    def test_short_session(self):
        with tempfile.TemporaryDirectory() as tmp:
            cat = CAT.Catalog()
            cov = COV.Coverage(Path(tmp) / "coverage.json")
            reg = R.Register(Path(tmp) / "problems")
            producer = F.Fuzzer(cat, cov, 11)
            session = S.Session(ENV, CONFIG, DIAGRAMS / "hierarchy.graphml", producer, reg, cov,
                                Path(tmp) / "s", producer_name="fuzzer", seed=11, minimize=False)
            session.run(6)
            self.assertEqual(len(session.rounds), 6)
            self.assertTrue((Path(tmp) / "s" / "session.json").exists())
            self.assertTrue((Path(tmp) / "s" / "script").exists())
            self.assertGreaterEqual(sum(1 for r in session.rounds if r.accepted), 3)
            self.assertTrue(cov.cells)


if __name__ == "__main__":
    unittest.main()
