# -----------------------------------------------------------------------------
# CyberiadaPro, the Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the tester on the agents harness
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

"""The mission diagrams on the dispatcher with stub components, and one
session end to end on a scripted model and the real editor. Skipped where
the framework is not importable."""

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from polygon import catalog as CAT
from polygon import composer as M
from polygon import config as C
from polygon import coverage as COV
from polygon import register as R
from tests.helpers import CONFIG, ENV, POLYGON, needs_editor

try:
    import hsmd_components
    from cybersphere_agents import Engine
    from cybersphere_agents.controller import Controller
    from polygon import components as PC
    from polygon import harness as H
    HAVE_HARNESS = True
except ImportError:
    HAVE_HARNESS = False

needs_harness = unittest.skipUnless(HAVE_HARNESS, "the cybersphere agents harness is not importable")
DIAGRAMS = POLYGON / "diagrams"
VALIDATOR = POLYGON.parents[2] / "cyberiadaml-compat-tests"


def validate(path):
    """The error findings of cgmlval, or None when the validator is absent."""
    if not (VALIDATOR / "cgmlval").is_dir():
        return None
    out = subprocess.run([sys.executable, "-m", "cgmlval", "validate", "--strict", "--json", str(path)],
                         cwd=VALIDATOR, capture_output=True, text=True, timeout=120, check=False)
    try:
        report = json.loads(out.stdout)[0]
    except (ValueError, IndexError):
        return ["no report: " + out.stderr[-200:]]
    return [f for f in report.get("findings", []) if f.get("severity", "ERROR") == "ERROR"]


if HAVE_HARNESS:
    class FakeEngine(Engine):
        """Records every control."""

        def __init__(self):
            self.calls = []

        def allowed(self):
            return [c[1] for c in self.calls if c[0] == "allow"]

        def messages(self):
            return [c[1] for c in self.calls if c[0] == "add"]

        def allow_tools(self, allowed, denied):
            self.calls.append(("allow", None if allowed is None else list(allowed)))

        def set_prompt(self, name):
            self.calls.append(("prompt", name))

        def add_message(self, text):
            self.calls.append(("add", text))

        def set_model(self, name):
            pass

        def set_effort(self, level):
            pass

        def start_command(self, command, timeout):
            raise RuntimeError("no commands in the polygon")

        def ask_user(self, text):
            return False

        def stop_loop(self):
            self.calls.append(("stop",))

        def run_child(self, role, task):
            pass

        def stop_children(self, role):
            pass

        def send_child(self, role, name, parameters):
            pass

        def report(self, text):
            pass

    class Stub(PC.Polygon):
        """A component whose activities end when the test says so."""

        def __init__(self, ident, dispatcher, **parameters):
            PC.Polygon.__init__(self, ident, dispatcher, **parameters)
            self.calls = []
            self.pending = []          # (signal, parameters) emitted at the next tick

        def active(self):
            return bool(self.pending)

        def tick(self, now):
            pending, self.pending = self.pending, []
            for signal, parameters in pending:
                self.emit(signal, **parameters)

        def later(self, signal, **parameters):
            self.pending.append((signal, parameters))

        def stop(self):
            self.pending = []

    class StubDocument(Stub):
        variables = PC.Document.variables

        def __init__(self, ident, dispatcher, **parameters):
            Stub.__init__(self, ident, dispatcher, **parameters)
            for name in self.variables:
                setattr(self, name, 0)
            self.target, self.summary, self.records = "", "the summary", []
            self.max_calls, self.budget = 5, 40

        def on_edit(self, family, script):
            self.calls.append(("edit", family))
            self.emit("STARTED", family=family)

        def edited(self):
            self.edits += 1
            self.commands += 1
            self.later("EDITED", family="create", commands=1, created=["n0"])

        def branch(self, target):
            self.calls.append(("branch", target))
            self.target, self.branched, self.tries, self.edits = target, True, 0, 0

        def attempt(self):
            self.tries += 1

        def restore(self):
            self.calls.append(("restore",))
            self.target, self.branched = "", False

        def compare(self):
            self.calls.append(("compare",))

    class StubOracle(Stub):
        variables = PC.Oracle.variables

        def __init__(self, ident, dispatcher, **parameters):
            Stub.__init__(self, ident, dispatcher, **parameters)
            self.checks = self.clean_streak = self.defects = self.candidates = 0
            self.profile_name = parameters.get("profile", "")
            self.signatures, self.target, self.last, self.report = [], "", "", "the report"

        def check(self):
            self.calls.append(("check",))
            self.checks += 1
            self.document.edits = 0

    class StubRegister(Stub):
        variables = PC.Register.variables

        def __init__(self, ident, dispatcher, **parameters):
            Stub.__init__(self, ident, dispatcher, **parameters)
            self.defects = self.candidates = self.confirmed = 0

        def confirm(self, signature):
            self.calls.append(("confirm", signature))
            self.confirmed += 1
            return True

    class StubFocus(Stub):
        variables = PC.Focus.variables

        def __init__(self, ident, dispatcher, **parameters):
            Stub.__init__(self, ident, dispatcher, **parameters)
            self.area_name, self.families, self.untried, self.covered = "", [], [], 0.0
            self.requirements, self.untried_text = "", ""

        def area(self, name):
            self.area_name = name
            self.families = list(dict(((a, f) for a, _, f in PC.AREAS))[name])
            self.requirements = "requirements of " + name
            self.emit("CHANGED", area=name)

        def exhausted(self):
            self.emit("EXHAUSTED")

    class StubMetric(Stub):
        variables = PC.Metric.variables

        def __init__(self, ident, dispatcher, **parameters):
            Stub.__init__(self, ident, dispatcher, **parameters)
            self.c, self.band, self.d, self.target, self.nudges = 0.0, "", 0.0, 65.0, "nudge"

        def measure(self):
            self.calls.append(("measure",))

    class StubFuzzer(Stub):
        variables = PC.Fuzzer.variables

        def __init__(self, ident, dispatcher, **parameters):
            Stub.__init__(self, ident, dispatcher, **parameters)
            self.bursts = 0

        def burst(self, count=None):
            self.calls.append(("burst", count))

    STUBS = {"Document": StubDocument, "Oracle": StubOracle, "Register": StubRegister,
             "Focus": StubFocus, "Metric": StubMetric, "Fuzzer": StubFuzzer}


def context(tmp, kind="hunt"):
    mission = M.Mission(kind, 1, kind, ENV.polygon / "corpus" / "empty.graphml", budget=(10, 40))
    return PC.Context(ENV, CONFIG, mission, Path(tmp) / "session", R.Register(Path(tmp) / "problems"),
                      COV.Coverage(Path(tmp) / "coverage.json"), CAT.Catalog(), seed=1)


def load(kind, tmp, stubs=True):
    PC.install(context(tmp, kind))
    if stubs:
        for name, cls in STUBS.items():
            hsmd_components.register(name, cls)
    engine = FakeEngine()
    controller = Controller(str(DIAGRAMS / (kind + ".graphml")), engine, output=open(os.devnull, "w"))
    controller.start()
    return controller, engine


@needs_harness
class DiagramTest(unittest.TestCase):
    def test_valid_and_loadable(self):
        with tempfile.TemporaryDirectory() as tmp:
            for kind in M.KINDS:
                errors = validate(DIAGRAMS / (kind + ".graphml"))
                if errors is not None:
                    self.assertEqual(errors, [], kind)
                controller, engine = load(kind, tmp)
                self.assertIsNone(controller.failed, kind)
                self.assertEqual(sorted(controller.requests()), ["CHECK", "DONE"], kind)
                self.assertEqual(controller.states("W"), ["w_watching"], kind)
                self.assertEqual(("prompt", kind), [c for c in engine.calls if c[0] == "prompt"][0])

    def test_hunt_areas_and_edit_loop(self):
        with tempfile.TemporaryDirectory() as tmp:
            controller, engine = load("hunt", tmp)
            document, oracle = controller.component("document"), controller.component("oracle")
            focus = controller.component("focus")
            self.assertEqual(engine.allowed()[0], ["inspect", "expect", "create", "arrange", "clipboard"])
            self.assertEqual(focus.area_name, "node")
            self.assertIn("requirements of node", engine.messages()[0])
            # an edit: Applying hides the tools, EDITED brings the summary back
            controller.notify("document", "EDIT", family="create", script="new-state G0 A")
            self.assertEqual(controller.states("G"), ["h_applying"])
            self.assertEqual(engine.allowed()[-1], [])
            document.edited()
            controller.tick()
            self.assertEqual(controller.states("G"), ["h_editing"])
            self.assertEqual(engine.messages()[-1], "the summary")
            self.assertEqual(engine.allowed()[-1], ["inspect", "expect", "create", "arrange", "clipboard"])
            # the sixth edit forces a check
            for _ in range(5):
                controller.notify("document", "EDIT", family="create", script="x")
                document.edited()
                controller.tick()
            self.assertEqual(controller.states("G"), ["h_checking"])
            self.assertEqual(oracle.calls[-1], ("check",))
            oracle.later("CLEAN")
            controller.tick()
            self.assertEqual(controller.states("G"), ["h_editing"])
            # three clean checks move the focus to the edges
            for _ in range(2):
                controller.request("CHECK")      # rejected: no edits
                document.edits = 1
                controller.request("CHECK")
                oracle.clean_streak += 1
                oracle.later("CLEAN")
                controller.tick()
            oracle.clean_streak = 3
            document.edits = 1
            controller.request("CHECK")
            oracle.later("CLEAN")
            controller.tick()
            self.assertEqual(controller.states("F"), ["f_edge"])
            self.assertEqual(engine.allowed()[-1], ["inspect", "expect", "create", "connect"])
            # a new defect enters Confirm; the reproduction confirms it
            document.edits = 1
            controller.request("CHECK")
            oracle.target, oracle.last = "law:NODE-1:x", "law: x outside"
            oracle.later("FOUND", new=1, known=0)
            controller.tick()
            self.assertEqual(controller.states("G"), ["h_confirm"])
            self.assertEqual(document.calls[-1], ("branch", "law:NODE-1:x"))
            self.assertIsNone(engine.allowed()[-1])            # allow_all
            self.assertIn("law: x outside", engine.messages()[-1])
            self.assertEqual(sorted(controller.requests()), ["REPRODUCED"])
            controller.notify("document", "EDIT", family="create", script="new-state G0 A")
            document.edited()
            controller.tick()
            answer = controller.request("REPRODUCED")
            self.assertEqual(answer.status, "accepted")
            self.assertEqual(controller.states("G"), ["h_confirming"])
            self.assertEqual(document.tries, 1)
            oracle.signatures = ["law:NODE-1:x"]
            oracle.later("FOUND", new=0, known=1)
            controller.tick()
            self.assertEqual(controller.states("G"), ["h_editing"])
            self.assertEqual(controller.component("register").calls[-1], ("confirm", "law:NODE-1:x"))
            self.assertEqual(document.calls[-1], ("restore",))
            # DONE with nothing to check ends the mission
            document.edits = 0
            controller.request("DONE")
            self.assertTrue(controller.finished("G"))
            self.assertEqual(engine.calls[-1], ("stop",))

    def test_confirm_gives_up_after_three_tries(self):
        with tempfile.TemporaryDirectory() as tmp:
            controller, engine = load("draw", tmp)
            document, oracle = controller.component("document"), controller.component("oracle")
            document.edits = 1
            controller.request("CHECK")
            oracle.target = "crash:x"
            oracle.later("FOUND", new=1, known=0)
            controller.tick()
            self.assertEqual(controller.states("G"), ["d_confirm"])
            for i in range(3):
                controller.notify("document", "EDIT", family="create", script="x")
                document.edited()
                controller.tick()
                controller.request("REPRODUCED")
                oracle.later("CLEAN")
                controller.tick()
                self.assertEqual(document.tries, i + 1)
            self.assertEqual(controller.states("G"), ["d_editing"])
            self.assertEqual(document.calls[-1], ("restore",))
            self.assertNotIn(("confirm", "crash:x"), controller.component("register").calls)

    def test_stress_bursts_and_measures(self):
        with tempfile.TemporaryDirectory() as tmp:
            controller, engine = load("stress", tmp)
            document, oracle = controller.component("document"), controller.component("oracle")
            fuzzer, metric = controller.component("fuzzer"), controller.component("metric")
            document.edits = 1
            controller.request("CHECK")
            oracle.later("CLEAN")
            controller.tick()
            self.assertEqual(controller.states("G"), ["s_stress"])
            self.assertEqual(fuzzer.calls, [("burst", None)])
            fuzzer.later("DONE", commands=5)
            controller.tick()
            self.assertEqual(metric.calls, [("measure",)])
            metric.c = 10.0
            metric.later("MEASURED", c=10.0, band="simple", d=3.0)
            controller.tick()
            self.assertEqual(controller.states("G"), ["s_editing"])
            self.assertEqual(engine.messages()[-1], "nudge")
            metric.c = 70.0
            document.edits = 1
            controller.request("CHECK")
            oracle.later("CLEAN")
            fuzzer.later("DONE", commands=0)
            controller.tick()
            controller.tick()
            metric.later("MEASURED", c=70.0, band="complex", d=3.0)
            controller.tick()
            self.assertTrue(controller.finished("G"))

    def test_rebuild_compares_and_watchdog_stops(self):
        with tempfile.TemporaryDirectory() as tmp:
            controller, engine = load("rebuild", tmp)
            document, oracle = controller.component("document"), controller.component("oracle")
            document.edits = 2
            controller.request("DONE")
            self.assertEqual(controller.states("G"), ["r_final"])
            oracle.later("CLEAN")
            controller.tick()
            self.assertEqual(document.calls[-1], ("compare",))
            document.commands = 3
            document.later("DIFFERS", count=1, text="  - a state is missing")
            controller.tick()
            self.assertEqual(controller.states("G"), ["r_editing"])
            self.assertIn("a state is missing", engine.messages()[-1])
            controller.notify("budget", "usage", requests=6)
            self.assertTrue(controller.finished("W"))
            self.assertEqual(engine.calls[-1], ("stop",))
            self.assertEqual(engine.allowed()[-1], [])


@needs_harness
@needs_editor
class SessionTest(unittest.TestCase):
    def test_hunt_session_on_a_scripted_model(self):
        from pydantic_ai.messages import ModelResponse, TextPart, ToolCallPart
        from pydantic_ai.models.function import FunctionModel

        def call(name, **arguments):
            return ModelResponse(parts=[ToolCallPart(name, arguments)])

        steps = [call("create", script="new-state G0 0 0 200 100 Idle\nnew-state G0 300 0 200 100 Busy"),
                 call("create", script="new-initial G0\nnew-state nope 0 0 100 100 X"),
                 call("arrange", script="move n1 400 0 200 100"),
                 call("inspect"),
                 call("expect", facts="state Idle parent G0\ncount state 2"),
                 call("signal", event="CHECK", reason="two states"),
                 call("signal", event="DONE", reason="enough"),
                 ModelResponse(parts=[TextPart("done")])]
        offered = []
        requests = []

        def script(messages, info):
            offered.append(sorted(t.name for t in info.function_tools))
            requests.append(messages)
            return steps.pop(0)

        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            backend = C.Backend("fake", "test:fake", max_calls=20)
            mission = M.compose(M.HUNT, ENV, 1)
            result = H.run_session(ENV, CONFIG, CAT.Catalog(), COV.Coverage(tmp / "coverage.json"),
                                   backend, mission, tmp / "session", R.Register(tmp / "problems"),
                                   model=FunctionModel(script))
            self.assertEqual(result.failed, "")
            self.assertTrue(result.finished, result.summary())
            self.assertFalse(result.limit)
            self.assertEqual(len(result.rounds), 3)
            self.assertEqual([r["accepted"] for r in result.rounds], [True, False, True])
            self.assertEqual(result.commands, 3)
            self.assertEqual(result.error, "")
            self.assertEqual(result.calls, 8)
            self.assertEqual(offered[0], ["arrange", "clipboard", "create", "expect", "inspect", "signal"])
            # the summary of an edit is the message of the next request
            text = str(requests[1][-1].parts[-1].content)
            self.assertIn("Edit 1 (create): 2 commands accepted", text)
            self.assertIn("n0 (simple 'Idle')", text)
            text = str(requests[2][-1].parts[-1].content)
            self.assertIn("rejected at line 2", text)
            self.assertIn("Ids now", text)
            folder = tmp / "session"
            for name in ("session.json", "script", "messages.json", "usage.json", "trace.txt", "edit-1.dump"):
                self.assertTrue((folder / name).exists(), name)
            data = json.loads((folder / "session.json").read_text())
            self.assertEqual(data["mission"]["kind"], "hunt")
            self.assertEqual(len(data["rounds"]), 3)
            self.assertEqual((folder / "script").read_text().count("\n"), 3)

    def test_exported_key(self):
        backend = C.Backend("k", "deepseek:deepseek-chat", key_env="POLYGON_TEST_KEY")
        os.environ["POLYGON_TEST_KEY"] = "secret"
        try:
            with H.exported_key(backend) as keyed:
                self.assertTrue(keyed)
                self.assertEqual(os.environ["POLYGON_TEST_KEY"], "secret")
        finally:
            os.environ.pop("POLYGON_TEST_KEY", None)
        backend = C.Backend("k", "deepseek:deepseek-chat", key_file="/nonexistent/key")
        os.environ.pop("DEEPSEEK_API_KEY", None)
        with H.exported_key(backend) as keyed:
            self.assertFalse(keyed)
            self.assertNotIn("DEEPSEEK_API_KEY", os.environ)


if __name__ == "__main__":
    unittest.main()
