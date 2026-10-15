# -----------------------------------------------------------------------------
# CyberiadaPro, the Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the generated texts, the briefs, the composer, the ledger
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

import json
import re
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

from polygon import brief as B
from polygon import catalog as CAT
from polygon import composer as M
from polygon import dump as D
from polygon import prompt as P
from tests.helpers import ENV, GOOD, POLYGON

GRAPHML = "{http://graphml.graphdrawing.org/xmlns}"
DIAGRAMS = POLYGON / "diagrams"


def prompt_of(diagram, name):
    """The body of the PROMPT <name> comment of a mission diagram."""
    root = ET.parse(diagram).getroot()
    for node in root.iter(GRAPHML + "node"):
        data = {d.get("key"): (d.text or "") for d in node.findall(GRAPHML + "data")}
        if data.get("dName") == "PROMPT " + name:
            return data.get("dData", "")
    return None


class PromptTest(unittest.TestCase):
    def setUp(self):
        self.cat = CAT.Catalog()

    def test_family_protocols(self):
        create = P.protocol(CAT.FAMILY_CREATE, self.cat)
        self.assertIn("`new-state", create)
        self.assertNotIn("`new-transition", create)
        self.assertIn("Raw gestures are accepted", create)
        gesture = P.protocol(CAT.FAMILY_GESTURE, self.cat)
        self.assertIn("`press", gesture)
        self.assertIn("new-comment", gesture)       # the creation tools are named
        self.assertNotIn("pan", gesture.split("| command |")[0].split("The tools:")[1])

    def test_expect_vocabulary(self):
        self.assertIn("rect-inside", P.EXPECT)
        self.assertIn("undo-depth", P.EXPECT)

    def test_task(self):
        mission = M.Mission(M.STRESS, 1, "stress", Path("empty.graphml"), domain="a lift controller",
                            budget=(30, 70), theme={"name": "t", "hint": "h"})
        text = P.task(mission)
        self.assertIn("a lift controller", text)
        self.assertIn("30 to 70 commands", text)
        self.assertIn("Theme: t - h", text)
        self.assertIn("state machine", text)         # the model card

    def test_drawing_rules_cite_real_ids(self):
        rules = prompt_of(DIAGRAMS / "draw.graphml", "draw")
        self.assertIsNotNone(rules)
        spec = (ENV.root / "docs" / "EDITOR-SPEC.md").read_text()
        cited = set()
        for group in re.findall(r"\[([A-Z]+-[0-9]+(?:\.\.[0-9]+)?(?:, [A-Z]+-[0-9]+(?:\.\.[0-9]+)?)*)\]", rules):
            for item in group.split(", "):
                area, _, numbers = item.partition("-")
                if ".." in numbers:
                    a, b = numbers.split("..")
                    cited.update("%s-%d" % (area, n) for n in range(int(a), int(b) + 1))
                else:
                    cited.add(item)
        self.assertTrue(cited)
        for req in sorted(cited):
            self.assertIn("`EDIT-%s`" % req, spec, req)

    def test_every_mission_has_its_prompts(self):
        for kind in M.KINDS:
            diagram = DIAGRAMS / (kind + ".graphml")
            self.assertTrue(diagram.exists(), kind)
            self.assertIn("DONE", prompt_of(diagram, kind))
            self.assertIn("REPRODUCED", prompt_of(diagram, "confirm"))


class BriefTest(unittest.TestCase):
    def test_lift_brief(self):
        doc = D.parse_dump((GOOD / "lift-output.txt").read_text()).document
        text = B.brief("lift", doc)
        good = POLYGON / "tests" / "good" / "lift-brief.txt"
        self.assertEqual(text, good.read_text())


class ComposerTest(unittest.TestCase):
    def test_compose(self):
        a = M.compose(M.STRESS, ENV, 5)
        b = M.compose(M.STRESS, ENV, 5)
        self.assertEqual(a.domain, b.domain)          # deterministic by seed
        self.assertEqual(a.diagram.name, "empty.graphml")
        self.assertEqual(a.strategy.name, "stress.graphml")
        r = M.compose(M.REBUILD, ENV, 1, "lift")
        self.assertEqual(r.original.name, "lift.graphml")
        self.assertEqual(r.diagram.name, "empty.graphml")
        h = M.compose(M.HUNT, ENV, 2, theme_name="geometry extremes")
        self.assertEqual(h.kind, M.HUNT)
        self.assertEqual(h.theme["name"], "geometry extremes")

    def test_stories_load(self):
        ss = M.stories()
        self.assertTrue(ss)
        for s in ss:
            for key in ("name", "subject", "hint", "budget", "source"):
                self.assertIn(key, s)
            self.assertEqual(len(s["budget"]), 2)
            self.assertLess(s["budget"][0], s["budget"][1])

    def test_compose_draw(self):
        a = M.compose(M.DRAW, ENV, 3)
        b = M.compose(M.DRAW, ENV, 3)
        self.assertEqual(a.story["name"], b.story["name"])
        self.assertEqual(tuple(a.budget), tuple(a.story["budget"]))
        named = M.compose(M.DRAW, ENV, 1, story_name="toaster-oven")
        self.assertEqual(named.story["name"], "toaster-oven")
        self.assertEqual(named.name, "draw-toaster-oven")


class CompetitionTest(unittest.TestCase):
    def test_ledger_and_table(self):
        from polygon import __main__ as CLI
        with tempfile.TemporaryDirectory() as tmp:
            ledger = CLI._Ledger(Path(tmp) / "productivity.json")
            a = {"backend": "flash:m", "rounds": 6, "accepted": 6, "errors": 0, "calls": 6,
                 "tokens": 200000, "commands": 70, "elapsed": 800.0, "defects": 3,
                 "candidates": 0, "reproduction": "match"}
            b = dict(a, backend="haiku:m", calls=12, tokens=100000, commands=36,
                     elapsed=100.0, defects=1, reproduction="differ 2")
            ledger.add("flash", a)
            ledger.add("haiku", b)
            ledger.add("flash", a)
            ledger.save()
            data = json.loads((Path(tmp) / "productivity.json").read_text())
            self.assertEqual(data["flash"]["sessions"], 2)
            self.assertEqual(data["flash"]["reproductions_matched"], 2)
            self.assertEqual(data["haiku"]["defects"], 1)
            mission = M.Mission(M.REBUILD, 1, "lift", Path("empty.graphml"),
                                theme={"name": "text-heavy behaviours"})
            table = CLI._comparison_table(mission, [a, b])
            self.assertIn("flash:m", table)
            self.assertIn("haiku:m", table)
            self.assertIn("match", table)


if __name__ == "__main__":
    unittest.main()
