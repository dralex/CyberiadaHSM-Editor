# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the dump parser tests
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
from tests.helpers import GOOD


def load(name):
    return D.parse_dump((GOOD / (name + "-output.txt")).read_text())


class DumpTest(unittest.TestCase):
    def test_every_good_dump_parses(self):
        files = sorted(GOOD.glob("*-output.txt"))
        self.assertTrue(files)
        for path in files:
            if "-text-" in path.name or "-stack-" in path.name:
                continue
            with self.subTest(path.name):
                dump = D.parse_dump(path.read_text())
                self.assertIsNotNone(dump.document)
                ids = dump.document.by_id()
                for item_id in dump.scene_items():
                    self.assertIn(item_id, ids)

    def test_lift_structure(self):
        doc = load("lift").document
        self.assertEqual([m.name for m in doc.machines()], ["Lift"])
        states = doc.states()
        self.assertEqual(len(states), 5)
        self.assertEqual(len(doc.transitions()), 8)
        moving = doc.find("moving")
        self.assertEqual(moving.kind, D.KIND_COMPOSITE)
        self.assertEqual([c.name for c in moving.children], ["MovingUp", "MovingDown"])
        self.assertEqual(moving.actions[0].notation(), "entry/ lamp_on()")
        t1 = doc.find("t1")
        self.assertEqual((t1.source, t1.target), ("idle", "up"))
        self.assertEqual(t1.action.notation(), "CALL [call_floor > floor]/ target = call_floor")
        self.assertEqual(doc.find("t0").action.notation(), "/ floor = 1")

    def test_multiline_and_subjects(self):
        doc = load("comments").document
        self.assertEqual(doc.find("n2").body, "Updated note\nwith a second line")
        doc = load("subjects").document
        self.assertEqual(doc.find("n2").subjects[0], ("name", "n0", "Parent"))
        self.assertEqual(doc.find("n2").subjects[1], ("data", "n1::n0", "init"))

    def test_scene_geometry(self):
        dump = load("geometry")
        items = dump.scene_items()
        self.assertEqual(items["node-0-1"].abs_rect, (800.0, 150.0, 150.0, 150.0))
        self.assertEqual(items["node-0-0-2"].abs_rect, (500.0, 150.0, 150.0, 150.0))
        self.assertEqual(items["node-0-0-2"].parent.id, "node-0-0")

    def test_stack_and_sections(self):
        text = (GOOD / "add-elements-stack-output.txt").read_text()
        self.assertEqual(D.parse_stack(D.sections(text)["stack"]).index, 5)
        text = "== document\nx\n== scene\ny\n== stack\ncount: 1\nindex: 1\nclean: no\n"
        self.assertEqual(D.sections(text), {"document": "x", "scene": "y",
                                            "stack": "count: 1\nindex: 1\nclean: no"})

    def test_compare(self):
        a = load("lift").text
        b = a.replace("file: 'diagrams/lift.graphml'", "file: '/tmp/x.graphml'")
        self.assertIsNone(D.compare(a, b))
        b = a.replace("standard version: '1.0', ", "standard version: '1.0', geometry: 'full', ")
        self.assertIsNone(D.compare(a, b))
        b = a.replace("name: 'MovingUp'", "name: 'MovingDown'", 1)
        diff = D.compare(a, b)
        self.assertEqual(diff.tag, "simple-state:name")
        self.assertIn("name='MovingUp'", diff.expected)
        self.assertIn("name='MovingDown'", diff.got)
        b = a.replace("type: loc, source: 'idle', target: 'up'", "type: ext, source: 'idle', target: 'up'")
        self.assertEqual(D.compare(a, b).tag, "transition:type")
        # the format does not preserve the type: no difference when ignored
        self.assertIsNone(D.compare(a, b, ignore={"transition:type"}))
        c = b.replace("name: 'MovingUp'", "name: 'MovingDown'", 1)
        self.assertEqual(D.compare(a, c, ignore={"transition:type"}).tag, "simple-state:name")
        self.assertIsNotNone(D.compare(a, load("hierarchy").text))
        self.assertIsNone(D.compare(load("geometry").text, load("geometry").text))

    def test_describe(self):
        text = D.describe(load("lift").document)
        self.assertIn('State machine "Lift": 5 states, 1 initial pseudostate, 8 transitions.', text)
        self.assertIn('composite state "Moving"; entry/ lamp_on(); it holds:', text)
        self.assertIn('  - "Idle" -> "MovingUp": CALL [call_floor > floor]/ target = call_floor', text)
        self.assertIn('  - the initial pseudostate -> "Idle": / floor = 1', text)
        self.assertNotIn("moving", text.replace("Moving", ""))

    def test_structural_diff(self):
        lift = load("lift").document
        self.assertEqual(D.structural_diff(lift, lift), [])
        diff = D.structural_diff(lift, load("hierarchy").document)
        self.assertTrue(any("MovingUp" in d or "Moving" in d for d in diff))
        self.assertTrue(any(d.startswith("transition expected") for d in diff))


if __name__ == "__main__":
    unittest.main()


class TextSectionTest(unittest.TestCase):
    def test_parse_text_lines(self):
        d = D.parse_dump((GOOD / "geometry-text-output.txt").read_text())
        self.assertEqual(len(d.texts), 8)
        title = d.text_of("node-0-1", "title")
        self.assertEqual(title.text, "node 0-1")
        self.assertTrue(all(t.family == "Cyberiada Mono" for t in d.texts))
        self.assertTrue(d.text_of("node-0", "title").bold)

    def test_multiline_plain(self):
        d = D.parse_dump((GOOD / "multiline-actions-text-output.txt").read_text())
        action = next(t for t in d.texts if t.fact_role == "action")
        self.assertIn("\\n", action.text)
        self.assertIn("\n", action.plain())
        self.assertNotIn("\\n", action.plain())
