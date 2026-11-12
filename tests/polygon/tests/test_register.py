# -----------------------------------------------------------------------------
# CyberiadaPro, the Cyberiada State Machine Editor
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

import os
import tempfile
import unittest
from pathlib import Path

from polygon import env as E
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

    def test_family_of(self):
        # a law problem reruns the standing laws, a semantic one the main run
        self.assertEqual(R.family_of("law:NODE-6:siblings n# and n# overlap"), "law")
        self.assertEqual(R.family_of("redo-all:simple-state:region"), "redo-all")
        self.assertEqual(R.family_of("expect:shown n# title alpha"), "main")


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


class EnvReproducibilityTest(unittest.TestCase):
    def test_discover_pins_the_build_library_path(self):
        # a manual run (platform var already set) must still get the build
        # tree's LD_LIBRARY_PATH, or a stale system library shadows the build;
        # the tree is the one of the inspected binary (an out-of-tree build)
        build = Path(tempfile.mkdtemp())
        (build / "tests").mkdir()
        (build / "tests" / "CTestTestfile.cmake").write_text(
            'set_tests_properties(t PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LD_LIBRARY_PATH=/x/lib")\n')
        saved = {k: os.environ.get(k) for k in (E.BINARY_ENV, "LD_LIBRARY_PATH")}
        try:
            os.environ[E.BINARY_ENV] = str(build / E.BINARY_NAME)
            os.environ["LD_LIBRARY_PATH"] = "/stale/lib"
            env = E.Env.discover()
        finally:
            for k, v in saved.items():
                if v is None:
                    os.environ.pop(k, None)
                else:
                    os.environ[k] = v
        self.assertEqual(env.binary, build / E.BINARY_NAME)
        self.assertEqual(env.environ["LD_LIBRARY_PATH"].split(os.pathsep), ["/x/lib", "/stale/lib"])

    def test_env_mismatch_only_fires_on_a_real_difference(self):
        reg = R.Register(Path(tempfile.mkdtemp()) / "problems")
        env = E.Env.discover()
        fp = "bin:aaaa libcyberiadaml:bbbb"
        same = R.Problem(id="P-1", kind="oracle", signature="x", title="t", env=fp)
        # a problem with no recorded runtime never warns (old entries)
        legacy = R.Problem(id="P-2", kind="oracle", signature="x", title="t")
        self.assertIsNone(reg.env_mismatch(env, legacy))

        class Fixed:
            def fingerprint(self):
                return fp
        self.assertIsNone(reg.env_mismatch(Fixed(), same))

        class Drifted:
            def fingerprint(self):
                return "bin:aaaa libcyberiadaml:cccc"
        msg = reg.env_mismatch(Drifted(), same)
        self.assertIn("libcyberiadaml", msg)
        self.assertIn("bbbb->cccc", msg)


@needs_editor
class FingerprintTest(unittest.TestCase):
    def test_fingerprint_names_the_binary_and_tracked_libs(self):
        fp = ENV.fingerprint()
        self.assertIn("bin:", fp)
        self.assertIn("libcyberiadaml:", fp)
        self.assertNotIn(":?", fp)      # every tracked library resolved


if __name__ == "__main__":
    unittest.main()
