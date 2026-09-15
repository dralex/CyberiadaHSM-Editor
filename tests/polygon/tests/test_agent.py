# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the prompt, brief, composer and agent tests
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
import threading
import unittest
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

from polygon import agent as A
from polygon import brief as B
from polygon import catalog as CAT
from polygon import composer as M
from polygon import config as C
from polygon import coverage as COV
from polygon import dump as D
from polygon import prompt as P
from polygon import register as R
from polygon import session as S
from polygon.adapters import base as adapters
from tests.helpers import CONFIG, DIAGRAMS, ENV, GOOD, POLYGON, needs_editor

ANSWER = """== plan
1. two states and a transition
== script
```
new-state G0 0 0 200 100 Idle
new-state G0 300 0 200 100 Busy
new-transition G0 n0 n1 START
```
== expectations
state Idle parent G0
transition n0 n1
"""


class PromptTest(unittest.TestCase):
    def test_parse_answer(self):
        plan, script, facts = P.parse_answer(ANSWER)
        self.assertEqual(plan, "1. two states and a transition\n")
        self.assertEqual(script.splitlines()[0], "new-state G0 0 0 200 100 Idle")
        self.assertEqual(len(script.splitlines()), 3)
        self.assertEqual(facts, "state Idle parent G0\ntransition n0 n1\n")
        self.assertIsNone(P.parse_answer("no sections here"))
        self.assertIsNotNone(P.parse_answer("## Plan\nx\n## Script\nundo\n## Expectations\n"))

    def test_preamble(self):
        text = P.preamble(CAT.Catalog())
        self.assertIn("| `new-state <parent> [x y w h] <name>` |", text)
        self.assertIn("| `press x y [ctrl|shift|alt ...]` |", text)
        self.assertIn("entry/ behaviour", text)
        self.assertIn("== expectations", text)

    def test_mission_draw(self):
        story = M.stories()[0]
        text = P.mission_draw(story, (12, 20))
        self.assertIn(story["subject"], text)
        self.assertIn(story["hint"].strip().splitlines()[0], text)
        self.assertIn(P.DRAWING_RULES, text)
        self.assertIn("12 to 20 commands", text)

    def test_drawing_rules_cite_real_ids(self):
        # every EDIT-<AREA>-<n> the drawing rules cite must exist in the spec
        ids = set(re.findall(r"(?:STRUCT|SEM|NODE|EDGE)-\d+", P.DRAWING_RULES))
        self.assertTrue(ids)
        spec = (POLYGON.parent.parent / "docs" / "EDITOR-SPEC.md").read_text()
        for rid in ids:
            self.assertIn("EDIT-" + rid, spec, "%s not in EDITOR-SPEC" % rid)


class BriefTest(unittest.TestCase):
    def test_lift_brief(self):
        doc = D.parse_dump((GOOD / "lift-output.txt").read_text()).document
        text = B.brief("lift", doc)
        good = POLYGON / "tests" / "good" / "lift-brief.txt"
        self.assertEqual(text, good.read_text())


class ComposerTest(unittest.TestCase):
    def test_compose(self):
        cat = CAT.Catalog()
        with tempfile.TemporaryDirectory() as tmp:
            cov = COV.Coverage(Path(tmp) / "c.json")
            a = M.compose(M.COMBINE, ENV, cat, cov, 5)
            b = M.compose(M.COMBINE, ENV, cat, cov, 5)
            self.assertEqual((a.name, a.operations, a.theme, a.untried), (b.name, b.operations, b.theme, b.untried))
            self.assertGreaterEqual(len([o for o in a.operations if o not in ("undo", "redo")]), M.MIN_OPERATIONS)
            self.assertTrue(a.untried)
            r = M.compose(M.REPRODUCE, ENV, cat, cov, 1, "lift")
            self.assertEqual(r.original.name, "lift.graphml")
            self.assertEqual(r.diagram.name, "empty.graphml")

    def test_stories_load(self):
        ss = M.stories()
        self.assertTrue(ss)
        for s in ss:
            for key in ("name", "subject", "hint", "budget", "source"):
                self.assertIn(key, s)
            self.assertEqual(len(s["budget"]), 2)
            self.assertLess(s["budget"][0], s["budget"][1])

    def test_compose_draw(self):
        cat = CAT.Catalog()
        with tempfile.TemporaryDirectory() as tmp:
            cov = COV.Coverage(Path(tmp) / "c.json")
            a = M.compose(M.DRAW, ENV, cat, cov, 3)
            b = M.compose(M.DRAW, ENV, cat, cov, 3)
            self.assertEqual(a.story["name"], b.story["name"])   # deterministic by seed
            self.assertEqual(a.diagram.name, "empty.graphml")
            self.assertEqual(tuple(a.budget), tuple(a.story["budget"]))
            named = M.compose(M.DRAW, ENV, cat, cov, 1, story_name="toaster-oven")
            self.assertEqual(named.story["name"], "toaster-oven")
            self.assertEqual(named.name, "draw-toaster-oven")


class StubHandler(BaseHTTPRequestHandler):
    requests = []

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = json.loads(self.rfile.read(length))
        StubHandler.requests.append((self.path, {k.lower(): v for k, v in self.headers.items()}, body))
        if self.path.endswith("/chat/completions"):
            reply = {"choices": [{"message": {"role": "assistant", "content": ANSWER}}], "usage": {"total_tokens": 1}}
        else:
            reply = {"content": [{"type": "text", "text": ANSWER}], "usage": {"input_tokens": 1}}
        data = json.dumps(reply).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, *args):
        pass


class AdapterTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = HTTPServer(("127.0.0.1", 0), StubHandler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.url = "http://127.0.0.1:%d/v1" % cls.server.server_port

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()

    def test_chat_completions(self):
        backend = C.Backend("t", "chat-completions", "m", self.url, vision=True, max_calls=2)
        adapter = adapters.make(backend, "secret")
        text = adapter.complete("sys", [{"role": "user", "content": "hi"}], images=[b"\x89PNG"])
        self.assertEqual(text, ANSWER)
        path, headers, body = StubHandler.requests[-1]
        self.assertEqual(path, "/v1/chat/completions")
        self.assertEqual(headers["authorization"], "Bearer secret")
        self.assertEqual(body["messages"][0], {"role": "system", "content": "sys"})
        self.assertEqual(body["messages"][1]["content"][1]["type"], "image_url")
        adapter.complete("sys", [{"role": "user", "content": "hi"}])
        with self.assertRaises(adapters.BudgetExceeded):
            adapter.complete("sys", [{"role": "user", "content": "hi"}])

    def test_messages(self):
        backend = C.Backend("t", "messages", "m", self.url, vision=False)
        adapter = adapters.make(backend, "secret")
        text = adapter.complete("sys", [{"role": "user", "content": "hi"}], images=[b"png"])
        self.assertEqual(text, ANSWER)
        path, headers, body = StubHandler.requests[-1]
        self.assertEqual(path, "/v1/messages")
        self.assertEqual(headers["x-api-key"], "secret")
        self.assertEqual(body["system"], "sys")
        self.assertEqual(body["messages"], [{"role": "user", "content": "hi"}])
        self.assertIn("max_tokens", body)


class FakeAdapter(adapters.Adapter):
    kind = "fake"

    def __init__(self, answers):
        super().__init__(C.Backend("fake", "fake", "m", "x", max_calls=20), "")
        self.answers = list(answers)

    def request(self, system, messages, images):
        return self.answers.pop(0) if self.answers else "== plan\nnothing\n== script\nundo\n== expectations\n"


@needs_editor
class AgentSessionTest(unittest.TestCase):
    def test_reproduction_session(self):
        answers = [ANSWER,
                   "== plan\nfix\n== script\nrename nope X\n== expectations\n",   # rejected
                   "== plan\nfix\n== script\nnew-action n0 entry/ lamp_off()\n== expectations\naction n0 0 entry/ lamp_off()\n"]
        with tempfile.TemporaryDirectory() as tmp:
            cat = CAT.Catalog()
            cov = COV.Coverage(Path(tmp) / "coverage.json")
            reg = R.Register(Path(tmp) / "problems")
            mission = M.compose(M.REPRODUCE, ENV, cat, cov, 1, "lift")
            log = []
            producer = A.Agent(FakeAdapter(answers), cat, mission, "brief", lambda k, t: log.append(k))
            session = S.Session(ENV, CONFIG, mission.diagram, producer, reg, cov, Path(tmp) / "s",
                                producer_name="agent", seed=1, minimize=False)
            session.mission = mission
            session.run(2)
            self.assertEqual(len(session.rounds), 2)
            self.assertTrue(session.rounds[0].accepted)
            self.assertTrue(session.rounds[1].accepted)
            self.assertEqual(len(session.script), 4)
            self.assertEqual(log.count("answer"), 3)
            self.assertIsNotNone(session.reproduction)
            self.assertFalse(session.reproduction["matches"])
            data = json.loads((Path(tmp) / "s" / "session.json").read_text())
            self.assertEqual(data["mission"]["kind"], "reproduce")
            # the feedback carried the structure and the scene
            last = producer.messages[-1]["content"]
            self.assertIn("Round 2: accepted", last)
            self.assertIn('simple state "Idle"', last)


if __name__ == "__main__":
    unittest.main()


class CompetitionTest(unittest.TestCase):
    def test_ledger_and_table(self):
        from polygon import __main__ as M2
        import tempfile
        from pathlib import Path
        with tempfile.TemporaryDirectory() as tmp:
            ledger = M2._Ledger(Path(tmp) / "productivity.json")
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

            class M: pass
            m = M(); m.kind = "reproduce"; m.name = "lift"; m.seed = 1
            m.theme = {"name": "text-heavy behaviours"}
            table = M2._comparison_table(m, [a, b])
            self.assertIn("flash:m", table)
            self.assertIn("haiku:m", table)
            self.assertIn("match", table)


@needs_editor
class BurstTest(unittest.TestCase):
    def test_burst_after_accepted_round(self):
        from polygon import fuzzer as F, catalog as CAT, coverage as COV
        import tempfile
        from pathlib import Path
        answers = ["== plan\nbuild\n== script\nnew-state G0 20 20 150 80 A\nnew-state G0 220 20 150 80 B\n== expectations\n"]
        with tempfile.TemporaryDirectory() as tmp:
            cat = CAT.Catalog()
            cov = COV.Coverage(Path(tmp) / "coverage.json")
            reg = R.Register(Path(tmp) / "problems")
            mission = M.compose(M.EXPLORE, ENV, cat, cov, 2)
            producer = A.Agent(FakeAdapter(answers), cat, mission, "brief")
            session = S.Session(ENV, CONFIG, mission.diagram, producer, reg, cov, Path(tmp) / "s",
                                producer_name="agent", seed=2, minimize=False)
            session.mission = mission
            session.stress = True
            burst = F.Fuzzer(cat, cov, 2); burst.gestures_only = True
            session.burst = (burst, 3)
            session.run(1)
            # the agent round plus up to three burst sub-rounds recorded
            self.assertGreaterEqual(len(session.rounds), 1)
            self.assertTrue(any(r.verb.startswith("burst:") for r in session.rounds))


class ExploreComposeTest(unittest.TestCase):
    def test_explore_mission(self):
        from polygon import composer as M2, catalog as CAT, coverage as COV
        import tempfile
        from pathlib import Path
        with tempfile.TemporaryDirectory() as tmp:
            cov = COV.Coverage(Path(tmp) / "c.json")
            m = M2.compose(M2.EXPLORE, ENV, CAT.Catalog(), cov, 5)
            self.assertEqual(m.kind, "explore")
            self.assertTrue(m.domain)
            self.assertEqual(m.diagram.name, "empty.graphml")


class TourComposeTest(unittest.TestCase):
    def test_tour_mission(self):
        from polygon import composer as M2, catalog as CAT, coverage as COV, prompt as P2
        from polygon.__main__ import _tour_tools
        import tempfile
        from pathlib import Path
        with tempfile.TemporaryDirectory() as tmp:
            cov = COV.Coverage(Path(tmp) / "c.json")
            m = M2.compose(M2.TOUR, ENV, CAT.Catalog(), cov, 7)
            self.assertEqual(m.kind, "tour")
            self.assertEqual(m.diagram.name, "empty.graphml")
            body = P2.mission_tour(_tour_tools(), m.budget)
            self.assertIn("one tool at", body)
            self.assertIn("new-state", body)
            self.assertIn("new-comment", body)
            self.assertNotIn("pan", body)   # view tools are excluded
