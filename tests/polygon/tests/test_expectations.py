# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the expectation tests
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

import unittest

from polygon import dump as D
from polygon import expectations as X
from tests.helpers import GOOD


def load(name):
    return D.parse_dump((GOOD / (name + "-output.txt")).read_text())


class ExpectationTest(unittest.TestCase):
    def test_add_elements(self):
        dump = load("add-elements")
        dump.stack = D.parse_stack((GOOD / "add-elements-stack-output.txt").read_text().split("== stack\n")[1])
        holds = """
        state Working parent G0
        state Nested new state parent n1
        count state 10
        count initial 1
        count final 1
        count comment 1
        kind n0 composite
        kind n0::n0 simple
        absent nope
        undo-depth 5
        """
        self.assertEqual(X.evaluate(holds, dump), [])
        fails = X.evaluate("state Missing parent G0\ncount state 3\nkind n0 simple\nabsent n0\nundo-depth 2\nfoo bar", dump)
        self.assertEqual([f for f, _ in fails],
                         ["state Missing parent G0", "count state 3", "kind n0 simple",
                          "absent n0", "undo-depth 2", "foo bar"])

    def test_geometry_facts(self):
        dump = load("geometry")
        self.assertEqual(X.evaluate("rect-inside node-0-0-1 node-0-0\nno-overlap node-0-0-1 node-0-0-2\n"
                                    "transition node-0-0-1 node-0-0-2\naction edge-2 0 LABEL/", dump), [])
        self.assertEqual(len(X.evaluate("rect-inside node-0-0 node-0-0-1\nno-overlap node-0-0 node-0-0-1\n"
                                        "transition node-0-0-2 node-0-0-1", dump)), 3)

    def test_lift_actions(self):
        dump = load("lift")
        self.assertEqual(X.evaluate("action idle 0 entry/ lamp_off()\naction t1 0 CALL [call_floor > floor]/ target = call_floor", dump), [])
        self.assertEqual(len(X.evaluate("action idle 1 exit/ x\naction idle 0 entry/ lamp_on()", dump)), 2)


if __name__ == "__main__":
    unittest.main()
