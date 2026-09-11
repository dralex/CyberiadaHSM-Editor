# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the register tests
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

from polygon import oracles
from polygon import register as R
from tests.helpers import CONFIG, DIAGRAMS, ENV, needs_editor


class RegisterTest(unittest.TestCase):
    def test_dedupe_and_cases(self):
        with tempfile.TemporaryDirectory() as tmp:
            reg = R.Register(Path(tmp) / "problems")
            f = oracles.Finding("oracle", "save-reopen:x", "the note")
            p1, new1 = reg.add(f, DIAGRAMS / "hierarchy.graphml", "new-state G0 A\n", title="t")
            p2, new2 = reg.add(f, DIAGRAMS / "hierarchy.graphml", "other\n")
            self.assertTrue(new1)
            self.assertFalse(new2)
            self.assertIs(p1, p2)
            self.assertEqual(p1.hits, 2)
            self.assertEqual(p1.id, "P-1")
            self.assertTrue((Path(tmp) / "problems" / "P-1" / "start.graphml").exists())
            self.assertEqual((Path(tmp) / "problems" / "P-1" / "script").read_text(), "new-state G0 A\n")
            g = oracles.Finding("crash", "signal:SIGSEGV", "boom")
            p3, _ = reg.add(g, DIAGRAMS / "hierarchy.graphml", "x\n")
            self.assertEqual(p3.id, "P-2")
            cases = (Path(tmp) / "problems" / R.CASES_NAME).read_text()
            self.assertIn("add_polygon_case(P-1)\n", cases)
            self.assertIn("add_polygon_case(P-2)\n", cases)
            again = R.Register(Path(tmp) / "problems")
            self.assertEqual([p.id for p in again.problems], ["P-1", "P-2"])
            again.problems[0].status = R.STATUS_FIXED
            again.save()
            self.assertNotIn("P-1", (Path(tmp) / "problems" / R.CASES_NAME).read_text())


@needs_editor
class MinimizeTest(unittest.TestCase):
    def test_minimize_and_check(self):
        with tempfile.TemporaryDirectory() as tmp:
            reg = R.Register(Path(tmp) / "problems")
            script = "new-state G0 A\nnew-state G0 B\nnew-state G0 C\n"
            added, result = R.register_script(reg, ENV, CONFIG, DIAGRAMS / "hierarchy.graphml",
                                              script, "count state 8\n", title="too many states")
            self.assertEqual(added, [])   # a semantic finding is not a defect
            added, result = R.register_script(reg, ENV, CONFIG, DIAGRAMS / "hierarchy.graphml",
                                              script, "count state 8\n", title="too many states",
                                              kinds=None)
            self.assertEqual([p.kind for p, _ in added], [oracles.KIND_SEMANTIC])
            problem = added[0][0]
            self.assertEqual((Path(tmp) / "problems" / problem.id / "script").read_text(), "new-state G0 A\n")
            holds, _ = reg.check(ENV, CONFIG, problem)
            self.assertTrue(holds)


if __name__ == "__main__":
    unittest.main()
