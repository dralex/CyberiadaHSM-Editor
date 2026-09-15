# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the problem register
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

"""The found problems: problems/register.json with one reproduction folder
per problem, the minimisation of a reproduction and the ctest cases."""

import json
import shutil
import subprocess
import tempfile
from dataclasses import dataclass, field, asdict
from datetime import date
from pathlib import Path

from . import dump as D
from . import oracles
from . import render

REGISTER_NAME = "register.json"
CASES_NAME = "cases.cmake"
STATUS_OPEN = "open"
STATUS_FIXED = "fixed"
STATUS_CLOSED = "closed"
STATUS_FORMAT = "format"     # a property of the serialization format, not a defect
STATUSES = (STATUS_OPEN, STATUS_FIXED, STATUS_CLOSED, STATUS_FORMAT)


@dataclass
class Problem:
    id: str
    kind: str
    signature: str
    title: str
    note: str = ""
    diagram: str = ""
    found: str = ""
    revision: str = ""
    env: str = ""                # the runtime fingerprint that found it
    status: str = STATUS_OPEN
    hits: int = 1
    producer: str = ""

    @property
    def folder(self):
        return self.id


def revision(root):
    try:
        return subprocess.run(["git", "-C", str(root), "rev-parse", "--short", "HEAD"],
                              capture_output=True, text=True, timeout=10).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def evaluate_script(env, config, diagram, script_text, expectation_text="", workdir=None,
                    family=None, invariant=False):
    """The findings of a script with every oracle (or one family), in a
    temporary workdir. invariant raises expectation failures as invariants."""
    with tempfile.TemporaryDirectory(prefix="polygon-") as tmp:
        round_ = oracles.Round(env, config, diagram, workdir or tmp)
        return round_.evaluate(script_text, 0, expectation_text, render.oracle, family, invariant)


def family_of(signature):
    """The oracle family a signature belongs to: the reruns it needs."""
    head = signature.split(":")[0]
    if head in ("save-reopen", "undo-all", "redo-all", "export", "render"):
        return head
    return "main"


class Register:
    def __init__(self, folder):
        self.folder = Path(folder)
        self.problems = []
        self.load()

    @property
    def path(self):
        return self.folder / REGISTER_NAME

    def load(self):
        self.problems = []
        if self.path.exists():
            data = json.loads(self.path.read_text())
            self.problems = [Problem(**p) for p in data.get("problems", [])]

    def save(self):
        self.folder.mkdir(parents=True, exist_ok=True)
        data = {"problems": [asdict(p) for p in self.problems]}
        self.path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")
        self.write_cases()

    def find(self, id_):
        for p in self.problems:
            if p.id == id_:
                return p
        return None

    def by_signature(self, signature):
        for p in self.problems:
            if p.signature == signature:
                return p
        return None

    def next_id(self):
        numbers = [int(p.id.split("-")[1]) for p in self.problems if p.id.startswith("P-")]
        return "P-%d" % (max(numbers) + 1 if numbers else 1)

    def open_problems(self):
        return [p for p in self.problems if p.status == STATUS_OPEN]

    def add(self, finding, diagram, script_text, expectation_text="", title="",
            producer="", root=None, files=None, plan="", dump_text="", stderr_text="",
            env_fp=""):
        """Register a finding with its reproduction; a known signature counts
        a hit and returns the existing problem."""
        known = self.by_signature(finding.signature)
        if known is not None:
            known.hits += 1
            self.save()
            return known, False
        problem = Problem(id=self.next_id(), kind=finding.kind, signature=finding.signature,
                          title=title or finding.note[:80], note=finding.note,
                          diagram=Path(diagram).name, found=date.today().isoformat(),
                          revision=revision(root) if root else "", env=env_fp, producer=producer)
        folder = self.folder / problem.folder
        folder.mkdir(parents=True, exist_ok=True)
        shutil.copy(diagram, folder / "start.graphml")
        (folder / "script").write_text(script_text)
        if expectation_text:
            (folder / "expectations").write_text(expectation_text)
        if plan:
            (folder / "plan").write_text(plan)
        if dump_text:
            (folder / "dump").write_text(dump_text)
        if stderr_text:
            (folder / "stderr").write_text(stderr_text)
        for name, path in (files or finding.files or {}).items():
            if path and Path(path).exists():
                shutil.copy(path, folder / (name + Path(path).suffix))
        self.problems.append(problem)
        self.save()
        return problem, True

    def write_cases(self):
        lines = ["# generated by the polygon register: one ctest case per open problem,",
                 "# green while the problem reproduces (see docs/POLYGON.md)"]
        for p in self.open_problems():
            lines.append("add_polygon_case(%s)" % p.id)
        (self.folder / CASES_NAME).write_text("\n".join(lines) + "\n")

    def reproduction(self, problem):
        """(start document, script text, expectation text) of a problem."""
        folder = self.folder / problem.folder
        expectations = folder / "expectations"
        return (folder / "start.graphml", (folder / "script").read_text(),
                expectations.read_text() if expectations.exists() else "")

    def check(self, env, config, problem):
        """True when the recorded signature reproduces. Evaluated in the
        signature's own oracle mode, so an invariant problem is checked as an
        invariant (else its signature could never match)."""
        start, script, facts = self.reproduction(problem)
        result = evaluate_script(env, config, start, script, facts,
                                 family=family_of(problem.signature),
                                 invariant=problem.signature.startswith("invariant:"))
        return any(f.signature == problem.signature for f in result.findings), result

    def env_mismatch(self, env, problem):
        """A human note when the current runtime differs from the one that
        recorded the problem, so a non-reproduction is read as a runtime change
        rather than a fix; None when they match or none was recorded."""
        if not problem.env:
            return None
        now = env.fingerprint()
        if now == problem.env:
            return None
        was = dict(p.split(":", 1) for p in problem.env.split() if ":" in p)
        cur = dict(p.split(":", 1) for p in now.split() if ":" in p)
        changed = [k for k in sorted(set(was) | set(cur)) if was.get(k) != cur.get(k)]
        return "runtime differs from when %s was recorded (%s)" % (
            problem.id, ", ".join("%s %s->%s" % (k, was.get(k, "-"), cur.get(k, "-")) for k in changed))


def reproduces(env, config, diagram, script_text, expectation_text, signature):
    result = evaluate_script(env, config, diagram, script_text, expectation_text,
                             family=family_of(signature),
                             invariant=signature.startswith("invariant:"))
    return any(f.signature == signature for f in result.findings)


def minimize(env, config, diagram, script_text, expectation_text, signature):
    """The shortest prefix that reproduces the signature (found by bisection,
    then checked), then every command dropped in turn while the signature
    holds. Comments are dropped first; only the oracle family of the
    signature is rerun."""
    lines = [l for l in script_text.splitlines() if l.strip() and not l.strip().startswith("#")]
    if not lines:
        return script_text
    rep = lambda ls: reproduces(env, config, diagram, "\n".join(ls) + "\n", expectation_text, signature)
    if not rep(lines):
        return script_text
    low, high = 0, len(lines)     # rep(lines[:high]) holds
    if rep([]):
        high = 0
    while high - low > 1:
        mid = (low + high) // 2
        if rep(lines[:mid]):
            high = mid
        else:
            low = mid
    kept = lines[:high]
    i = len(kept) - 1
    while i >= 0:
        candidate = kept[:i] + kept[i + 1:]
        if reproduces(env, config, diagram, "\n".join(candidate) + "\n", expectation_text, signature):
            kept = candidate
        i -= 1
    return "\n".join(kept) + "\n"


DEFECT_KINDS = (oracles.KIND_CRASH, oracles.KIND_ORACLE, oracles.KIND_RENDER,
                oracles.KIND_INVARIANT, oracles.KIND_LAW)


def register_script(register, env, config, diagram, script_text, expectation_text="",
                    title="", producer="", root=None, do_minimize=True, plan="", kinds=DEFECT_KINDS,
                    invariant=False):
    """Run a script with every oracle and register the findings of the given
    kinds (None: every kind); returns [(problem, is_new)]."""
    result = evaluate_script(env, config, diagram, script_text, expectation_text, invariant=invariant)
    env_fp = env.fingerprint()
    out = []
    for finding in result.findings:
        # the expectation and review findings are candidates of the session,
        # not defects; a hand-run `register add` with kinds=None takes them too
        if kinds is not None and finding.kind not in kinds:
            continue
        own_title = title if finding is result.findings[0] or not title else finding.note[:80]
        if register.by_signature(finding.signature) is not None:
            out.append(register.add(finding, diagram, script_text))
            continue
        text = script_text
        if do_minimize:
            text = minimize(env, config, diagram, script_text, expectation_text, finding.signature)
        # the reproduction files come from one more run of the final script
        with tempfile.TemporaryDirectory(prefix="polygon-") as tmp:
            again = oracles.Round(env, config, diagram, tmp).evaluate(text, 0, expectation_text, render.oracle, invariant=invariant)
            match = next((f for f in again.findings if f.signature == finding.signature), finding)
            files = {"render": match.files["png"]} if match.files.get("png") else {}
            out.append(register.add(match, diagram, text, expectation_text, own_title, producer, root,
                                    files, plan, again.run.stdout, "\n".join(again.run.messages()),
                                    env_fp=env_fp))
    return out, result
