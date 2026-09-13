# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the tool-coverage and deterministic-tour tests
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
from polygon import toolcover as TC
from polygon import tour as TOUR
from tests.helpers import GOOD


class ToolCoverageTest(unittest.TestCase):
    def test_matrix(self):
        with tempfile.TemporaryDirectory() as tmp:
            tc = TC.ToolCoverage(Path(tmp) / "t.json")
            n = len(tc.all_cells())
            self.assertEqual(n, len(TC.tool_actions()) * len(TC.PATTERNS))
            self.assertEqual(len(tc.untried()), n)
            tc.record("new-state", "single")
            self.assertEqual(tc.count("new-state", "single"), 1)
            self.assertEqual(len(tc.untried()), n - 1)
            tc.save()
            again = TC.ToolCoverage(Path(tmp) / "t.json")
            self.assertEqual(again.count("new-state", "single"), 1)
            self.assertIn("tool coverage:", again.report())


class DeterministicTourTest(unittest.TestCase):
    def test_emits_untried_cells(self):
        with tempfile.TemporaryDirectory() as tmp:
            cov = COV.Coverage(Path(tmp) / "c.json")
            tc = TC.ToolCoverage(Path(tmp) / "t.json")
            producer = TOUR.DeterministicTour(CAT.Catalog(), cov, tc, 3)
            dump = D.parse_dump((GOOD / "geometry-output.txt").read_text())
            dump.stack = D.Stack(2, 1, False)
            seen = set()
            for _ in range(30):
                step = producer.next(dump)
                if step is None:
                    break
                lines, verb, kind = step
                self.assertTrue(verb.startswith("tour:"))
                self.assertTrue(all(l.split()[0] for l in lines))
                seen.add(verb)
            # distinct cells were exercised and recorded
            self.assertGreater(len(seen), 5)
            self.assertTrue(tc.cells)


if __name__ == "__main__":
    unittest.main()
