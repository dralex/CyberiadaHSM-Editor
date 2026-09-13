# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the session
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

"""The round loop of a session: a producer (the fuzzer or the agent) gives
the next commands, the runner replays the accumulated script from the start
document, the oracles judge, the register and the coverage store learn, the
producer gets the feedback. Everything is recorded under sessions/."""

import json
import shutil
import time
from dataclasses import dataclass, field
from datetime import date
from pathlib import Path

from . import dump as D
from . import oracles
from . import register as R
from . import render
from . import runner
from .adapters.base import AdapterError

MAX_REJECTIONS = 5


@dataclass
class Round:
    number: int
    lines: list
    verb: str = ""
    kind: str = ""
    accepted: bool = False
    script_error: str = ""
    findings: list = field(default_factory=list)   # [(kind, signature, note)]
    expectations: str = ""
    plan: str = ""
    result: object = field(default=None, repr=False)


class Session:
    def __init__(self, env, config, start, producer, register, coverage, folder,
                 producer_name="", seed=0, minimize=True, gestures=True):
        self.env = env
        self.config = config
        self.start = Path(start)
        self.producer = producer
        self.register = register
        self.coverage = coverage
        self.folder = Path(folder)
        self.producer_name = producer_name
        self.seed = seed
        self.minimize = minimize
        self.script = []           # the accepted lines
        self.rounds = []
        self.dump = None
        self.previous_verb = None
        self.mission = None
        self.reproduction = None
        self.error = ""
        self.stress = False
        self.burst = None
        self.invariant = False   # a drill: expectation failures are invariants (registered)
        self.folder.mkdir(parents=True, exist_ok=True)
        self.work = self.folder / "work"

    def text(self, extra=()):
        return "\n".join(self.script + list(extra)) + "\n"

    def evaluate(self, lines, expectations=""):
        round_ = oracles.Round(self.env, self.config, self.start, self.work)
        oracle = None if self.stress else render.oracle
        return round_.evaluate(self.text(lines), len(self.script), expectations, oracle,
                               invariant=self.invariant)

    def initial_dump(self):
        result = self.evaluate([])
        if result.dump is None:
            raise RuntimeError("the start document does not open: %s" % " ".join(result.run.messages()))
        self.dump = result.dump
        return result

    def run(self, rounds):
        """rounds rounds; returns the number of findings."""
        self.started = time.time()
        self.initial_dump()
        total = 0
        for n in range(1, rounds + 1):
            try:
                record = self.round(n)
            except AdapterError as e:
                self.error = str(e)
                break
            if record is None:
                break
            self.rounds.append(record)
            total += len(record.findings)
            if record.accepted and self.burst:
                total += self.run_burst(n)
            self.save()
        self.finish()
        self.elapsed = time.time() - getattr(self, "started", time.time())
        self.save()
        return total

    def finish(self):
        """A reproduction mission: the structural diff against the original;
        a match saves the result into the corpus."""
        if self.mission is None or self.mission.original is None or self.dump is None:
            return
        self.work.mkdir(parents=True, exist_ok=True)
        original = runner.run(self.env, self.mission.original, dump=True,
                              timeout=self.config.timeout, workdir=self.work)
        if original.exit != runner.EXIT_OK:
            return
        diff = D.structural_diff(D.parse_dump(original.stdout).document, self.dump.document)
        self.reproduction = {"matches": not diff, "differences": diff}
        if not diff and self.script:
            target = self.env.polygon / "corpus" / ("%s-repro-%d.graphml" % (self.mission.name, self.seed))
            script = runner.write_script(self.work / "final.script", self.text())
            runner.run(self.env, self.start, script=script, save=target,
                       timeout=self.config.timeout, workdir=self.work)
            self.reproduction["saved"] = target.name

    def round(self, n):
        rejected = []
        for _ in range(MAX_REJECTIONS):
            step = self.producer.next(self.dump, exclude=rejected)
            if step is None:
                return None
            lines, verb, kind = step[0], step[1], step[2]
            expectations = step[3] if len(step) > 3 else ""
            plan = step[4] if len(step) > 4 else ""
            record = self.apply(n, lines, verb, kind, expectations, plan)
            if record.script_error:
                rejected.append(verb)
                if hasattr(self.producer, "rejected"):
                    self.producer.rejected(record, record.script_error, self.dump)
                continue
            if hasattr(self.producer, "feedback"):
                self.producer.feedback(record, record.result)
            return record
        return record

    def apply(self, n, lines, verb, kind, expectations="", plan=""):
        """Evaluate one step: register the defects, update the coverage and the
        accepted script and dump, return the record (carrying .result)."""
        record = Round(n, list(lines), verb, kind, expectations=expectations, plan=plan)
        result = self.evaluate(lines, expectations)
        record.result = result
        if result.script_error is not None:
            line, message = result.script_error
            record.script_error = "line %d: %s" % (line, message)
            return record
        fired = False
        for f in result.findings:
            record.findings.append((f.kind, f.signature, f.note))
            if f.kind in (oracles.KIND_SEMANTIC, oracles.KIND_REVIEW):
                continue
            fired = True
            self.register_finding(f, lines, expectations, plan, verb)
        self.coverage.record(verb, kind, self.previous_verb, fired)
        self.previous_verb = verb
        record.accepted = result.accepted
        if result.accepted:
            self.script += list(lines)
            self.dump = result.dump
            (self.folder / ("round-%d.dump" % n)).write_text(result.run.stdout)
        return record

    def run_burst(self, n):
        """A burst of fuzzer micro-ops on the current diagram after an accepted
        agent round; returns the number of findings. A crash or a rejected op
        stops the burst."""
        fuzzer, count = self.burst
        found = 0
        for k in range(count):
            step = fuzzer.next(self.dump)
            if step is None:
                break
            lines, verb, kind = step[0], step[1], step[2]
            record = self.apply(n, lines, "burst:" + verb, kind)
            self.rounds.append(record)
            found += len(record.findings)
            if record.script_error or any(f[0] == oracles.KIND_CRASH for f in record.findings):
                break
        return found

    def register_finding(self, finding, lines, expectations, plan, verb):
        script = self.text(lines)
        known = self.register.by_signature(finding.signature)
        if known is not None:
            self.register.add(finding, self.start, script)
            return known
        added, _ = R.register_script(self.register, self.env, self.config, self.start, script,
                                     expectations, title="",
                                     producer=self.producer_name, root=self.env.root,
                                     do_minimize=self.minimize, plan=plan)
        for problem, is_new in added:
            if problem.signature == finding.signature:
                return problem
        return None

    def save(self):
        data = {"start": str(self.start), "producer": self.producer_name, "seed": self.seed,
                "date": date.today().isoformat(), "error": self.error,
                "mission": {"kind": self.mission.kind, "name": self.mission.name,
                            "operations": self.mission.operations,
                            "theme": self.mission.theme["name"] if self.mission.theme else "",
                            "untried": self.mission.untried} if self.mission else None,
                "reproduction": self.reproduction,
                "elapsed": round(getattr(self, "elapsed", 0.0), 1),
                "rounds": [{"n": r.number, "verb": r.verb, "kind": r.kind, "lines": r.lines,
                            "accepted": r.accepted, "script_error": r.script_error,
                            "findings": r.findings, "expectations": r.expectations,
                            "plan": r.plan} for r in self.rounds]}
        (self.folder / "session.json").write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")
        (self.folder / "script").write_text(self.text())
        self.coverage.save()
        if self.work.exists():
            shutil.rmtree(self.work, ignore_errors=True)

    def summary(self):
        accepted = sum(1 for r in self.rounds if r.accepted)
        findings = sum(len(r.findings) for r in self.rounds)
        text = "%d rounds, %d accepted, %d findings, %d commands" % (
            len(self.rounds), accepted, findings, len(self.script))
        if self.reproduction is not None:
            text += ", reproduction %s" % ("matches" if self.reproduction["matches"] else
                                           "differs (%d)" % len(self.reproduction["differences"]))
        if self.error:
            text += ", stopped: " + self.error
        return text


class Replay:
    """A producer replaying the rounds of a recorded session."""

    def __init__(self, rounds):
        self.rounds = list(rounds)

    def next(self, dump, exclude=()):
        while self.rounds:
            r = self.rounds.pop(0)
            if r["accepted"] or not r["script_error"]:
                return r["lines"], r["verb"], r["kind"], r.get("expectations", ""), r.get("plan", "")
        return None


def session_folder(root, producer, seed):
    base = Path(root) / "sessions"
    name = "%s-%s-%s" % (date.today().isoformat(), producer, seed)
    folder = base / name
    n = 1
    while folder.exists():
        n += 1
        folder = base / ("%s-%d" % (name, n))
    return folder
