# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the runner and oracle tests
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

from polygon import dump as D
from polygon import oracles
from polygon import runner
from tests.helpers import CONFIG, DIAGRAMS, ENV, SCRIPTS, needs_editor


@needs_editor
class RunnerTest(unittest.TestCase):
    def test_dump_run(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = runner.run(ENV, DIAGRAMS / "hierarchy.graphml", script=SCRIPTS / "add-elements.script",
                                dump=True, stack=True, workdir=tmp)
            self.assertEqual(result.exit, runner.EXIT_OK)
            self.assertFalse(result.crashed)
            dump = D.parse_dump(result.stdout)
            self.assertEqual(len(dump.document.states()), 10)
            self.assertEqual(dump.stack.index, 5)

    def test_script_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            script = runner.write_script(Path(tmp) / "bad.script", "new-state G0 A\nrename nope B\n")
            result = runner.run(ENV, DIAGRAMS / "hierarchy.graphml", script=script, dump=True, workdir=tmp)
            self.assertEqual(result.exit, runner.EXIT_SCRIPT)
            line, message = result.script_error()
            self.assertEqual(line, 2)
            self.assertTrue(message)

    def test_load_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = runner.run(ENV, DIAGRAMS / "broken-xml.graphml", dump=True, workdir=tmp)
            self.assertEqual(result.exit, runner.EXIT_LOAD)


@needs_editor
class OracleTest(unittest.TestCase):
    def evaluate(self, diagram, script, facts="", prefix=0):
        with tempfile.TemporaryDirectory() as tmp:
            round_ = oracles.Round(ENV, CONFIG, DIAGRAMS / (diagram + ".graphml"), tmp)
            return round_.evaluate((SCRIPTS / (script + ".script")).read_text(), prefix, facts)

    def test_model_script_passes(self):
        result = self.evaluate("hierarchy", "add-elements", "state Working parent G0\ncount state 10\nundo-depth 5")
        self.assertIsNone(result.script_error)
        self.assertEqual([(f.kind, f.signature) for f in result.findings], [])

    def test_gesture_script_passes(self):
        # a state made by the script, dragged by its body, clicked and
        # deleted (the geometry corpus diagrams do not round-trip a save,
        # a registered finding, so the test builds its own state)
        script = "new-state G0 100 100 200 100 Dragged\npress 180 80\ndrag 210 100\n" \
                 "release 210 100\nclick 210 100\ndelete-selected\n"
        with tempfile.TemporaryDirectory() as tmp:
            round_ = oracles.Round(ENV, CONFIG, DIAGRAMS / "hierarchy.graphml", tmp)
            result = round_.evaluate(script, 0, "absent n2\ncount state 8\nundo-depth 3")
        self.assertEqual([(f.kind, f.signature) for f in result.findings], [])

    def test_failed_expectation(self):
        result = self.evaluate("hierarchy", "add-elements", "count state 3")
        self.assertEqual([f.kind for f in result.findings], [oracles.KIND_SEMANTIC])
        self.assertEqual(result.findings[0].signature, "expect:count state #")

    def test_script_error_is_feedback_not_finding(self):
        with tempfile.TemporaryDirectory() as tmp:
            round_ = oracles.Round(ENV, CONFIG, DIAGRAMS / "hierarchy.graphml", tmp)
            result = round_.evaluate("new-state G0 A\nrename nope B\n", prefix_lines=1)
            self.assertEqual(result.script_error[0], 2)
            self.assertEqual(result.findings, [])
            # the same error inside the accepted prefix is a replay failure
            result = round_.evaluate("new-state G0 A\nrename nope B\n", prefix_lines=2)
            self.assertEqual([f.kind for f in result.findings], [oracles.KIND_ORACLE])
            self.assertTrue(result.findings[0].signature.startswith("replay:"))

    def test_format_profile_skips_the_transition_type(self):
        # a new transition is external in memory and local after a save or an
        # undo snapshot: a property of the format, no finding
        script = "new-state G0 0 0 200 100 A\nnew-state G0 300 0 200 100 B\nnew-transition G0 n0 n1 GO\n"
        empty = ENV.polygon / "corpus" / "empty.graphml"
        with tempfile.TemporaryDirectory() as tmp:
            round_ = oracles.Round(ENV, CONFIG, empty, tmp)
            result = round_.evaluate(script, 0, "transition n0 n1")
            self.assertEqual([(f.kind, f.signature) for f in result.findings], [])
            round_.ignore = set()
            result = round_.evaluate(script, 0, "")
            self.assertEqual(sorted(f.signature for f in result.findings),
                             ["redo-all:transition:type", "save-reopen:transition:type"])

    def test_normalize(self):
        self.assertEqual(oracles.normalize("line 12: rect (1.5; -2) x"), "line #: rect (#; #) x")


if __name__ == "__main__":
    unittest.main()
