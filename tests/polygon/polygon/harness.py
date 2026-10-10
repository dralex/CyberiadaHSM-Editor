# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the tester on the cybersphere agents harness
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

"""One session of the tester: the family tools, the agent built on the
mission diagram, the key of the backend exported for the run, the result.
The only module importing the framework beside components.py."""

import asyncio
import contextlib
import json
import os
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

import hsmd
from cybersphere_agents.cli import Trace
from cybersphere_agents.pydantic_engine import create_agent
from pydantic_ai import Tool
from pydantic_ai.exceptions import UnexpectedModelBehavior, UsageLimitExceeded
from pydantic_ai.usage import UsageLimits

from . import brief as B
from . import catalog as CAT
from . import components as PC
from . import config as C
from . import dump as D
from . import prompt as P
from . import runner

EXTRA_REQUESTS = 2               # over the backend's calls: the hard stop of a run
MESSAGES_NAME = "messages.json"
USAGE_NAME = "usage.json"
TRACE_NAME = "trace.txt"


@dataclass
class Result:
    """What a session left: the record and the numbers of the ledger."""
    folder: Path
    rounds: list = field(default_factory=list)
    commands: int = 0
    elapsed: float = 0.0
    calls: int = 0
    tokens: int = 0
    reproduction: dict = None
    defects: int = 0                 # new signatures registered
    candidates: int = 0
    metrics: list = field(default_factory=list)   # (c, d) of the stress measurements
    finished: bool = False           # the mission machine reached its final state
    failed: str = ""                 # the error that stopped the diagram
    error: str = ""                  # the error that stopped the run (the model misbehaved)
    limit: bool = False              # the request limit ended the run

    def summary(self):
        accepted = sum(1 for r in self.rounds if r["accepted"])
        findings = sum(len(r["findings"]) for r in self.rounds)
        text = "%d edits, %d accepted, %d findings, %d commands" % (
            len(self.rounds), accepted, findings, self.commands)
        if self.reproduction is not None:
            text += ", reproduction %s" % ("matches" if self.reproduction["matches"] else
                                           "differs (%d)" % len(self.reproduction["differences"]))
        if self.failed:
            text += ", the diagram failed: " + self.failed
        if self.error:
            text += ", the run failed: " + self.error
        if self.limit:
            text += ", the request limit was reached"
        return text


@contextlib.contextmanager
def exported_key(backend):
    """The key of the backend in the provider's variable for the run only."""
    variable = backend.key_variable
    key = C.api_key(backend)
    previous = os.environ.get(variable) if variable else None
    if variable and key:
        os.environ[variable] = key
    try:
        yield bool(key)
    finally:
        if variable and key:
            if previous is None:
                os.environ.pop(variable, None)
            else:
                os.environ[variable] = previous


class Harness:
    """The tools of the model over the components of one session."""

    def __init__(self, catalog, controller=None):
        self.catalog = catalog
        self.controller = controller

    def component(self, ident):
        return self.controller.component(ident)

    def tools(self):
        out = [Tool(self._family_tool(f), name=f, description=P.protocol(f, self.catalog))
               for f in CAT.FAMILIES]
        out.append(Tool(self.inspect, name="inspect", description=P.INSPECT))
        out.append(Tool(self.expect, name="expect", description=P.EXPECT))
        return out

    def _family_tool(self, family):
        async def tool(script: str) -> str:
            return await self.edit(family, script)
        tool.__name__ = family
        return tool

    async def edit(self, family, script):
        """Start the edit; the result comes as the next message."""
        document = self.component("document")
        refusal = document.refusal(family, script)
        if refusal:
            return refusal
        try:
            self.controller.notify("document", "EDIT", family=family, script=script)
        except hsmd.DispatcherError as e:
            return "the diagram has failed: %s" % e
        return ("Applying %d commands (edit %d); the result comes as the next message."
                % (len(PC.clean_lines(script)), len(document.records) + (1 if document.active() else 0)))

    async def inspect(self) -> str:
        return await asyncio.to_thread(self.component("document").inspect)

    async def expect(self, facts: str) -> str:
        return await asyncio.to_thread(self.component("oracle").expect, facts)


def task_text(env, config, mission):
    """The first message: the seeded part of the mission."""
    brief = ""
    if mission.kind == "rebuild":
        run = runner.run(env, mission.original, dump=True, timeout=config.timeout)
        if run.exit != runner.EXIT_OK:
            raise SystemExit("cannot open %s: %s" % (mission.name, " ".join(run.messages())))
        brief = B.brief(mission.name, D.parse_dump(run.stdout).document)
    return P.task(mission, brief)


def run_session(env, config, catalog, coverage, backend, mission, folder, register,
                burst=PC.DEFAULT_BURST, trace=False, model=None):
    """One session: install the components, build the agent on the mission
    diagram, run it, write the record. `model` replaces the backend's model
    (a scripted one in the tests). Returns the Result."""
    folder = Path(folder)
    folder.mkdir(parents=True, exist_ok=True)
    producer = "agent:%s:%s" % (backend.name, backend.model)
    PC.install(PC.Context(env, config, mission, folder, register, coverage, catalog,
                          backend=backend, seed=mission.seed, burst=burst, producer=producer))
    harness = Harness(catalog)
    started = time.time()
    with open(folder / TRACE_NAME, "w", encoding="utf-8") as log, exported_key(backend) as keyed:
        if not keyed:
            print("no key for backend %s; the provider may refuse" % backend.name, file=sys.stderr)
        session = create_agent(str(mission.strategy), model or backend.model, workspace=str(folder),
                               coder=False, tools=harness.tools(), output=log,
                               trace=Trace() if trace else Trace(log))
        harness.controller = session.controller
        result = Result(folder)
        run = None
        try:
            run = session.run_sync(task_text(env, config, mission),
                                   usage_limits=UsageLimits(request_limit=backend.max_calls + EXTRA_REQUESTS),
                                   model_settings={"temperature": backend.temperature})
        except UsageLimitExceeded:
            result.limit = True
        except UnexpectedModelBehavior as e:
            result.error = str(e)
        session.save(str(folder / MESSAGES_NAME))
    document = session.controller.component("document")
    oracle = session.controller.component("oracle")
    metric = session.controller.component("metric") if "metric" in session.controller.dispatcher.platform else None
    result.rounds = document.records
    result.commands = document.commands
    result.elapsed = round(time.time() - started, 1)
    result.reproduction = document.reproduction
    result.defects = oracle.defects + (register_defects(session) - oracle.defects)
    result.candidates = oracle.candidates
    result.metrics = metric.history if metric else []
    result.finished = session.controller.finished("G")
    result.failed = str(session.failed) if session.failed else ""
    usage = {}
    if run is not None:
        u = run.usage() if callable(getattr(run, "usage", None)) else getattr(run, "usage", None)
        if u is not None:
            usage = {"requests": getattr(u, "requests", 0), "input_tokens": getattr(u, "input_tokens", 0) or 0,
                     "output_tokens": getattr(u, "output_tokens", 0) or 0,
                     "total_tokens": getattr(u, "total_tokens", 0) or 0}
    (folder / USAGE_NAME).write_text(json.dumps(usage, indent=2) + "\n")
    result.calls = usage.get("requests", 0)
    result.tokens = usage.get("total_tokens", 0)
    document.save()
    return result


def register_defects(session):
    """The new defects of the session as the register component counts
    them (the crashes at edit time included)."""
    register = session.controller.dispatcher.platform.get("register")
    return register.defects if register else 0
