# -----------------------------------------------------------------------------
# CyberiadaPro, the Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the platform components of the tester
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

"""The components the mission diagrams drive (docs/POLYGON.md, the tester):
the document under edit, the oracle, the register, the focus, the metric and
the fuzzer. Long work is an activity: a method starts a Job and returns, the
tick reports its end by a signal. Needs the cybersphere agents framework."""

import json
import re
import threading
import time
from dataclasses import dataclass
from datetime import date
from pathlib import Path

import hsmd_components
from cybersphere_agents.activities import Job
from cybersphere_agents.components import PlatformComponent

from . import catalog as CAT
from . import climb
from . import dump as D
from . import expectations as X
from . import fuzzer as F
from . import oracles
from . import register as R
from . import render
from . import runner

PRIORITY = 5
MAX_IDS_SHOWN = 40            # ids listed after a rejection before "use inspect"
MAX_UNTRIED_SHOWN = 8
MAX_NUDGES = 4
DEFAULT_BURST = 5
PROFILE_FULL = "full"
PROFILE_STRESS = "stress"
SPEC_NAME = "EDITOR-SPEC.md"
REQUIREMENT = re.compile(r"^- `(EDIT-[A-Z]+-\d+)` (MUST|SHOULD|MAY) \[(.)\]: (.*)$")
SECTION = re.compile(r"^### (4\.\d+) ")

# the areas of the hunt, in order: the spec sections and the families
AREAS = (
    ("node", ("4.3",), (CAT.FAMILY_CREATE, CAT.FAMILY_ARRANGE, CAT.FAMILY_CLIPBOARD)),
    ("edge", ("4.4",), (CAT.FAMILY_CREATE, CAT.FAMILY_CONNECT)),
    ("text", ("4.5", "4.9"), (CAT.FAMILY_CREATE, CAT.FAMILY_BEHAVE)),
    ("tool", ("4.6",), (CAT.FAMILY_GESTURE,)),
    ("history", ("4.7", "4.8"), CAT.FAMILIES),
)

EDITED, REJECTED, CRASHED = "edited", "rejected", "crashed"


@dataclass
class Context:
    """What a session gives its components; installed before the diagram loads."""
    env: object
    config: object
    mission: object
    folder: Path
    register: R.Register
    coverage: object
    catalog: CAT.Catalog
    backend: object = None
    seed: int = 0
    burst: int = DEFAULT_BURST
    producer: str = "agent"


CONTEXT = None


def install(context):
    """Register the types under their names with the context the next
    session uses; the diagram declares them with `type/ Document` etc."""
    global CONTEXT                     # pylint: disable=global-statement
    CONTEXT = context
    for name, cls in TYPES.items():
        hsmd_components.register(name, cls)


class Polygon(PlatformComponent):
    """The base: the context, one job at a time, a job error as a signal."""

    priority = PRIORITY

    def __init__(self, ident, dispatcher, **parameters):
        PlatformComponent.__init__(self, ident, dispatcher, **parameters)
        self.context = CONTEXT
        self._job = None
        self._done = None

    @property
    def document(self):
        return self.dispatcher.platform.get("document")

    def active(self):
        return self._job is not None

    def start(self, done, function, *arguments):
        """Run the function in a thread; done(result) at the tick that sees it end."""
        self._done = done
        self._job = Job(function, *arguments)

    def stop(self):
        if self._job is not None:
            self._job.kill()
            self._job = None

    def tick(self, now):
        if self._job is None or not self._job.done():
            return
        job, self._job = self._job, None
        if job.error is not None:
            self.failed("%s: %s" % (type(job.error).__name__, job.error))
        else:
            self._done(job.result)

    def failed(self, message):
        self.emit("FAILED", message=message)


# --- the document ------------------------------------------------------------

def clean_lines(script):
    return [l.strip() for l in script.splitlines() if l.strip() and not l.strip().startswith("#")]


def label(element):
    """An element for the model: id, kind, name or endpoints."""
    kind = F.short_kind(element)
    if element.kind == D.KIND_TRANSITION:
        return "%s (%s %s -> %s)" % (element.id, kind, element.source, element.target)
    return "%s (%s%s)" % (element.id, kind, (" '%s'" % element.name) if element.name else "")


def scene_text(dump):
    lines = []
    for item in dump.scene_items().values():
        x, y, w, h = item.abs_rect
        lines.append("  %s %s: %g %g %g %g" % (item.kind, item.id, x, y, w, h))
    return "\n".join(lines)


def elements_of(dump):
    if dump is None or dump.document is None:
        return {}
    return {i: e for i, e in dump.document.by_id().items() if e.kind != D.KIND_DOCUMENT}


class Document(Polygon):
    """The diagram under edit: the start document plus the accepted script,
    replayed on every edit (the script is the reproduction)."""

    variables = ("commands", "edits", "elements", "undo_depth", "rejections", "tries",
                 "branched", "target", "budget", "max_calls", "max_seconds")

    def __init__(self, ident, dispatcher, **parameters):
        Polygon.__init__(self, ident, dispatcher, **parameters)
        ctx = self.context
        self.env, self.config, self.catalog = ctx.env, ctx.config, ctx.catalog
        self.start_path = Path(ctx.mission.diagram)
        self.folder = Path(ctx.folder)
        self.work = self.folder / "work"
        self.script = []               # the accepted lines
        self.dump = None               # the last parsed dump
        self.records = []              # the edits, the rounds of session.json
        self.summary = ""              # the last edit in words
        self.previous_verb = None
        self.reproduction = None
        self.lock = threading.Lock()   # one editor run on the script at a time
        self.started = time.time()
        self._pending = None           # (family, lines) of the running edit
        self._main = None              # (script, dump, edits) set aside by branch()
        self.last_crash = None         # (signature, lines) of the last crashing edit
        self.commands = 0
        self.edits = 0                 # since the last check
        self.elements = 0
        self.undo_depth = 0
        self.rejections = 0            # consecutive
        self.tries = 0                 # reproductions attempted in a branch
        self.branched = False
        self.target = ""               # the signature a branch reproduces
        budget = getattr(ctx.mission, "budget", None)
        self.budget = budget[1] if budget else 0
        self.max_calls = ctx.backend.max_calls if ctx.backend else ctx.config.calls
        self.max_seconds = getattr(ctx.config, "seconds", 0)

    # --- the edits -------------------------------------------------------------

    def refusal(self, family, script):
        """Why a script is refused at once, or None: an activity running, no
        command, a verb outside the family."""
        if self.active():
            return "the editor is busy with the previous script; wait for its result"
        lines = clean_lines(script)
        if not lines:
            return "the script has no command"
        for i, line in enumerate(lines):
            verb = line.split()[0]
            if not self.catalog.accepts(family, verb):
                known = self.catalog.family_of(verb)
                why = ("'%s' belongs to the %s tool" % (verb, known) if known
                       else "'%s' is not a command" % verb)
                return "line %d: %s; the script is not applied" % (i + 1, why)
        return None

    def on_edit(self, family, script):
        """The notification of a family tool: start the edit; EDITED,
        REJECTED or CRASHED at a later tick."""
        lines = clean_lines(script)
        self.current_dump()
        self._pending = (family, lines)
        self.start(self._edited, self.run_lines, lines, "edit-%d" % (len(self.records) + 1))
        self.emit("STARTED", family=family)

    def run_lines(self, lines, name, base=None):
        """One editor run of the base script (the accepted one by default)
        plus the lines, in a thread: (EDITED, dump, run) | (REJECTED, line,
        message) | (CRASHED, finding)."""
        with self.lock:
            base = list(self.script if base is None else base)
            round_ = oracles.Round(self.env, self.config, self.start_path, self.work)
            run = round_.run("\n".join(base + list(lines)) + "\n", name,
                             dump=True, stack=True, dump_text=round_.text)
        crash = oracles.crash_finding(run, "edit")
        if crash is not None:
            return (CRASHED, crash)
        if run.exit != runner.EXIT_OK:
            error = run.script_error()
            if run.exit == runner.EXIT_SCRIPT and error is not None:
                line, message = error
                if line > len(base):
                    return (REJECTED, line - len(base), message)
                return (CRASHED, oracles.Finding(oracles.KIND_ORACLE, "replay:" + oracles.normalize(message),
                                                 "line %d failed on replay: %s" % (line, message)))
            return (CRASHED, oracles.Finding(oracles.KIND_ORACLE, "exit:%d" % run.exit,
                                             "exit code %d: %s" % (run.exit, " ".join(run.messages()))))
        try:
            return (EDITED, D.parse_dump(run.stdout), run)
        except D.DumpError as e:
            return (CRASHED, oracles.Finding(oracles.KIND_ORACLE, "dump:" + oracles.normalize(str(e)), str(e)))

    def _edited(self, result):
        family, lines = self._pending
        self._pending = None
        self.accept(family, lines, result)

    def failed(self, message):
        family, lines = self._pending or ("", [])
        self._pending = None
        self.accept(family, lines, (CRASHED, oracles.Finding(oracles.KIND_CRASH, "harness:" + message[:60], message)))

    def accept(self, family, lines, result, burst=False):
        """Record the outcome of a run on the dispatcher's side and raise it."""
        n = len(self.records) + 1
        record = {"n": n, "verb": family, "kind": "burst" if burst else "agent",
                 "lines": list(lines), "accepted": False, "script_error": None,
                 "findings": [], "expectations": "", "plan": "", "branch": self.branched}
        self.records.append(record)
        if result[0] == REJECTED:
            _, line, message = result
            record["script_error"] = "line %d: %s" % (line, message)
            self.rejections += 1
            self.summary = self._rejected_summary(n, family, line, message)
            self.save()
            self.emit("REJECTED", line=line, message=message)
            return
        if result[0] == CRASHED:
            finding = result[1]
            record["findings"].append((finding.kind, finding.signature, finding.note))
            self.rejections += 1
            self.last_crash = (finding.signature, list(lines))
            register = self.dispatcher.platform.get("register")
            is_new = register.found(finding, self.text() + "\n".join(lines) + "\n")[1] if register else False
            self.summary = ("Edit %d (%s): the editor failed - %s. The document is unchanged; "
                            "the failure is recorded (%s)." % (n, family, finding.note,
                                                              "new" if is_new else "known"))
            self.save()
            self.emit("CRASHED", signature=finding.signature, note=finding.note, new=bool(is_new))
            return
        _, dump, run = result
        before = set(elements_of(self.dump))
        self._record_coverage(lines)
        self.script += list(lines)
        self.dump = dump
        record["accepted"] = True
        self.commands = len(self.script)
        self.edits += 1
        self.rejections = 0
        self.elements = len(elements_of(dump))
        self.undo_depth = dump.stack.index if dump.stack else 0
        (self.folder / ("edit-%d.dump" % n)).write_text(run.stdout)
        created = [e for i, e in elements_of(dump).items() if i not in before]
        self.summary = self._edited_summary(n, family, len(lines), created)
        self.save()
        self.emit("EDITED", family=family, commands=len(lines), created=[e.id for e in created])

    def _record_coverage(self, lines):
        coverage, previous = self.context.coverage, self.previous_verb
        ids = elements_of(self.dump)
        for line in lines:
            parts = line.split()
            verb, op = parts[0], self.catalog.find(parts[0])
            kind = "-"
            if op is not None and op.targets:
                kind = next((F.short_kind(ids[a]) for a in parts[1:] if a in ids), op.targets[0])
            coverage.record(verb, kind, previous)
            previous = verb
        self.previous_verb = previous

    def _edited_summary(self, n, family, count, created):
        lines = ["Edit %d (%s): %d command%s accepted." % (n, family, count, "" if count == 1 else "s")]
        if created:
            lines.append("New elements: " + ", ".join(label(e) for e in created) + ".")
        lines.append("Document: %d elements, %d commands so far; undo stack index %d of %d."
                     % (self.elements, self.commands, self.undo_depth,
                        self.dump.stack.count if self.dump.stack else 0))
        return "\n".join(lines)

    def _rejected_summary(self, n, family, line, message):
        lines = ["Edit %d (%s) rejected at line %d: %s." % (n, family, line, message),
                 "The document is unchanged. Use only the ids it has; a created element gets the",
                 "next free id in creation order."]
        ids = list(elements_of(self.dump).values())
        if ids:
            shown = ", ".join(label(e) for e in ids[:MAX_IDS_SHOWN])
            lines.append("Ids now: %s%s" % (shown, "; more: use inspect." if len(ids) > MAX_IDS_SHOWN else "."))
        return "\n".join(lines)

    def inspect(self):
        """The document, the scene and the undo stack in words, from the last dump."""
        dump = self.current_dump()
        if dump is None or dump.document is None:
            return "the document does not open"
        lines = [D.describe(dump.document).rstrip(), "",
                 "Ids: " + ", ".join(label(e) for e in elements_of(dump).values()), "",
                 "Scene (kind id: absolute rect x y w h):", scene_text(dump)]
        if dump.stack:
            lines.append("Undo stack: index %d of %d." % (dump.stack.index, dump.stack.count))
        return "\n".join(lines)

    def current_dump(self):
        """The last dump; the start document's, taken now, before any edit."""
        if self.dump is None:
            result = self.run_lines([], "start")
            if result[0] == EDITED:
                self.dump = result[1]
                self.elements = len(elements_of(self.dump))
        return self.dump

    def text(self):
        return "\n".join(self.script) + "\n"

    # --- the reproduction branch and the comparison ---------------------

    def branch(self, target):
        """Set the main line aside: an empty script from the start document,
        for a reproduction of the target signature."""
        self._main = (self.script, self.dump, self.edits)
        self.script, self.dump, self.edits = [], None, 0
        self.commands, self.tries, self.rejections = 0, 0, 0
        self.target, self.branched = target, True

    def attempt(self):
        self.tries += 1

    def restore(self):
        if self._main is not None:
            self.script, self.dump, self.edits = self._main
            self._main = None
        self.commands = len(self.script)
        self.target, self.branched, self.tries = "", False, 0
        self.elements = len(elements_of(self.dump))

    def compare(self):
        """The structural diff against the mission's original: MATCHES or DIFFERS."""
        self.start(self._compared, self._compare, self.context.mission.name)

    def _compare(self, name):
        original = self.context.mission.original
        with self.lock:
            run = runner.run(self.env, original, dump=True, timeout=self.config.timeout, workdir=self.work)
            if run.exit != runner.EXIT_OK:
                raise RuntimeError("the original %s does not open: %s" % (name, " ".join(run.messages())))
            diff = D.structural_diff(D.parse_dump(run.stdout).document, self.dump.document)
            saved = ""
            if not diff and self.script:
                target = self.env.polygon / "corpus" / ("%s-repro-%d.graphml" % (name, self.context.seed))
                script = runner.write_script(self.work / "final.script", self.text())
                runner.run(self.env, self.start_path, script=script, save=target,
                           timeout=self.config.timeout, workdir=self.work)
                saved = target.name
        return diff, saved

    def _compared(self, result):
        diff, saved = result
        self.reproduction = {"matches": not diff, "differences": diff}
        if saved:
            self.reproduction["saved"] = saved
        self.save()
        if diff:
            self.emit("DIFFERS", count=len(diff), text="\n".join("  - %s" % d for d in diff))
        else:
            self.emit("MATCHES")

    # --- the session record ------------------------------------------------

    def note_findings(self, findings):
        """The findings of a check go on the last edit of the record."""
        if self.records:
            self.records[-1]["findings"] += [(f.kind, f.signature, f.note) for f in findings]
            self.save()

    def save(self):
        ctx = self.context
        mission = ctx.mission
        data = {"start": str(self.start_path), "producer": ctx.producer, "seed": ctx.seed,
                "date": date.today().isoformat(), "error": "",
                "mission": {"kind": mission.kind, "name": mission.name,
                            "operations": list(getattr(mission, "operations", [])),
                            "theme": mission.theme["name"] if getattr(mission, "theme", None) else "",
                            "untried": list(getattr(mission, "untried", []))},
                "reproduction": self.reproduction,
                "elapsed": round(time.time() - self.started, 1),
                "rounds": self.records}
        self.folder.mkdir(parents=True, exist_ok=True)
        (self.folder / "session.json").write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")
        (self.folder / "script").write_text(self.text())
        ctx.coverage.save()


# --- the oracle ----------------------------------------------------------

class Oracle(Polygon):
    """The reference-free checks of the accumulated script, as an activity;
    the findings become facts and go to the register."""

    variables = ("checks", "clean_streak", "defects", "candidates", "profile_name",
                 "signatures", "target")

    def __init__(self, ident, dispatcher, **parameters):
        Polygon.__init__(self, ident, dispatcher, **parameters)
        self.profile_name = parameters.get("profile", PROFILE_FULL)
        self.checks = 0
        self.clean_streak = 0
        self.defects = 0               # new this session
        self.candidates = 0
        self.signatures = []           # of the last check
        self.target = ""               # the first new defect of the last check
        self.last = ""                 # its note, for the prompt
        self.report = ""               # the last check in words, for the prompt
        self.facts = ""                # the expectations the next check evaluates

    @property
    def register(self):
        return self.dispatcher.platform.get("register")

    def profile(self, name):
        self.profile_name = name

    def check(self):
        document = self.document
        self.start(self._checked, self._check, document.text(), len(document.script), self.facts)

    def _check(self, text, prefix, facts):
        document = self.document
        with document.lock:
            round_ = oracles.Round(document.env, document.config, document.start_path, document.work)
            oracle = render.oracle if self.profile_name == PROFILE_FULL else None
            return round_.evaluate(text, prefix, facts, oracle)

    def expect(self, facts):
        """Evaluate the facts on the last dump now; keep them for the check."""
        self.facts = facts
        dump = self.document.current_dump()
        if dump is None:
            return "the document does not open"
        failures = X.evaluate(facts, dump)
        count = len(X.parse(facts))
        if not failures:
            return "all %d facts hold" % count
        return "\n".join(["%d of %d facts do not hold:" % (len(failures), count)]
                         + ["  %s (%s)" % f for f in failures])

    def _checked(self, result):
        document = self.document
        document.edits = 0
        self.checks += 1
        self.signatures = []
        self.target, self.last = "", ""
        new = known = 0
        for finding in result.findings:
            if finding.kind not in R.DEFECT_KINDS:
                self.candidates += 1
                self.emit("CANDIDATE", fact=finding.signature, reason=finding.note)
                continue
            _, is_new = self.register.found(finding, document.text())
            self.signatures.append(finding.signature)
            new += is_new
            known += not is_new
            if is_new and not self.target:
                self.target, self.last = finding.signature, "%s: %s" % (finding.kind, finding.note)
            req = finding.signature.split(":")[1] if finding.kind == oracles.KIND_LAW else ""
            self.emit("DEFECT", kind=finding.kind, req=req, signature=finding.signature,
                      note=finding.note, new=bool(is_new))
        document.note_findings(result.findings)
        self.defects += new
        self.report = self._report(result.findings, new, known)
        if self.signatures:
            self.clean_streak = 0
            self.emit("FOUND", new=new, known=known)
        else:
            self.clean_streak += 1
            self.emit("CLEAN")

    def _report(self, findings, new, known):
        defects = [f for f in findings if f.kind in R.DEFECT_KINDS]
        failed = [f for f in findings if f.kind not in R.DEFECT_KINDS]
        if not defects:
            lines = ["Check %d: clean - no crash, the document survives save/reopen and undo/redo%s."
                     % (self.checks, ", every law holds" if self.profile_name == PROFILE_FULL else "")]
        else:
            lines = ["Check %d: %d defect%s fired (%d new, %d already known), recorded:"
                     % (self.checks, len(defects), "" if len(defects) == 1 else "s", new, known)]
            lines += ["  - %s: %s" % (f.kind, f.note) for f in defects]
        if failed:
            lines.append("Your expectations that do not hold (candidates, not defects):")
            lines += ["  - %s" % f.note for f in failed]
        return "\n".join(lines)


# --- the register ---------------------------------------------------------

class Register(Polygon):
    """The problems and their reproductions (register.py), as a component."""

    variables = ("defects", "candidates", "confirmed")

    def __init__(self, ident, dispatcher, **parameters):
        Polygon.__init__(self, ident, dispatcher, **parameters)
        self.store = self.context.register
        self.defects = 0               # new this session
        self.candidates = 0
        self.confirmed = 0

    def known(self, signature):
        return self.store.by_signature(signature) is not None

    def found(self, finding, script):
        """Record a defect with the script that produced it; a known open
        signature counts a hit, a fixed one fires again and is reopened as
        new. Returns (problem, is_new)."""
        document = self.document
        problem, is_new = self.store.add(
            finding, document.start_path, script, producer=self.context.producer,
            root=document.env.root, env_fp=document.env.fingerprint())
        if not is_new and problem.status == R.STATUS_FIXED:
            problem.status = R.STATUS_OPEN
            problem.note = (problem.note + " | fired again on %s" % date.today().isoformat()).strip(" |")
            (self.store.folder / problem.folder / "script").write_text(script)
            self.store.save()
            is_new = True
        if is_new:
            self.defects += 1
        return problem, is_new

    def confirm(self, signature):
        """The reproduction the model rebuilt replaces the long script; a
        crash at edit time keeps the lines that crashed."""
        problem = self.store.by_signature(signature)
        if problem is None:
            return False
        document = self.document
        text = document.text()
        if document.last_crash and document.last_crash[0] == signature:
            text += "\n".join(document.last_crash[1]) + "\n"
        (self.store.folder / problem.folder / "script").write_text(text)
        problem.note = (problem.note + " (reproduced by the tester)").strip()
        self.store.save()
        self.confirmed += 1
        return True


# --- the focus -------------------------------------------------------------

def read_requirements(spec_path, sections):
    """The requirement lines of the given spec sections, rendered for a prompt."""
    lines, current, inside = [], None, False
    for line in Path(spec_path).read_text().splitlines():
        head = SECTION.match(line)
        if head:
            current, inside = head.group(1), False
            continue
        if current not in sections:
            continue
        if line.startswith("- "):
            m = REQUIREMENT.match(line)
            inside = m is not None
            if inside:
                lines.append("  - %s [%s]: %s" % (m.group(1), m.group(3), m.group(4)))
        elif inside and line.startswith("  "):
            lines[-1] += " " + line.strip()
        else:
            inside = False
    return "\n".join(lines)


class Focus(Polygon):
    """The areas of EDITOR-SPEC: the families, the requirements and the
    untried cells of the coverage."""

    variables = ("area_name", "families", "untried", "covered")

    def __init__(self, ident, dispatcher, **parameters):
        Polygon.__init__(self, ident, dispatcher, **parameters)
        self.spec = Path(self.context.env.root) / "docs" / SPEC_NAME
        self.area_name = ""
        self.families = list(CAT.FAMILIES)
        self.requirements = ""
        self.untried = []
        self.covered = 0.0

    def area(self, name):
        """Enter an area: its families, its requirements, its untried cells; CHANGED."""
        entry = next((a for a in AREAS if a[0] == name), None)
        if entry is None:
            raise ValueError("unknown area '%s'" % name)
        _, sections, families = entry
        self.area_name = name
        self.families = list(families)
        text = read_requirements(self.spec, sections) if self.spec.exists() else ""
        self.requirements = ("The %s area: the tools %s. Make the editor violate one of these "
                             "requirements:\n%s\n" % (name, ", ".join(self.families), text))
        self.refresh()
        self.emit("CHANGED", area=name)

    def exhausted(self):
        self.emit("EXHAUSTED")

    def refresh(self):
        catalog, coverage = self.context.catalog, self.context.coverage
        cells = [(v, k) for v, k in catalog.cells() if catalog.family_of(v) in self.families]
        untried = [(v, k) for v, k in cells if coverage.count(v, k) == 0]
        self.untried = ["%s on a %s" % (v, k) for v, k in untried[:MAX_UNTRIED_SHOWN]]
        self.covered = round(1.0 - len(untried) / len(cells), 2) if cells else 1.0

    @property
    def untried_text(self):
        if not self.untried:
            return ""
        return "Untried so far by the polygon: %s.\n" % "; ".join(self.untried)


# --- the metric --------------------------------------------------------------

class Metric(Polygon):
    """The complexity C of the document and D of the script (climb.py)."""

    variables = ("c", "band", "d", "target")

    def __init__(self, ident, dispatcher, **parameters):
        Polygon.__init__(self, ident, dispatcher, **parameters)
        self.c = 0.0
        self.band = ""
        self.d = 0.0
        self.nudges = ""
        self.target = climb.TARGET_C
        self.history = []              # (c, d) per measurement, for the session stats

    def measure(self):
        document = self.document
        self.start(self._measured, climb.measure, document.env, document.config,
                   document.start_path, document.text(), document.work)

    def _measured(self, result):
        c, d = result
        self.c = round(c["C"], 1) if c else 0.0
        self.band = c["band"] if c else ""
        self.d = round(d["P"], 1) if d else 0.0
        self.history.append((c, d))
        head = "Complexity: C=%.1f (%s), D=%.1f; target C >= %g." % (
            self.c, self.band or "-", self.d, self.target)
        nudges = climb.nudges(c, d)[:MAX_NUDGES]
        self.nudges = head if not nudges else "\n".join(
            [head, "To climb, add where you are thinnest:"] + ["  - " + n for n in nudges])
        self.emit("MEASURED", c=self.c, band=self.band, d=self.d)


# --- the fuzzer --------------------------------------------------------------

class Fuzzer(Polygon):
    """A burst of random micro-operations through the same document."""

    variables = ("bursts",)

    def __init__(self, ident, dispatcher, **parameters):
        Polygon.__init__(self, ident, dispatcher, **parameters)
        ctx = self.context
        self.fuzzer = F.Fuzzer(ctx.catalog, ctx.coverage, ctx.seed, gestures=True)
        self.fuzzer.gestures_only = True
        self.bursts = 0

    def burst(self, count=None):
        document = self.document
        document.current_dump()
        self.start(self._burst_done, self._burst, count or self.context.burst,
                   list(document.script), document.dump)

    def _burst(self, count, script, dump):
        """k operations on a copy of the script, in a thread; a rejection or
        a crash ends the burst. The tick records the outcomes in order."""
        outcomes = []
        for k in range(count):
            step = self.fuzzer.next(dump)
            if step is None:
                break
            lines, verb = step[0], step[1]
            result = self.document.run_lines(lines, "burst-%d-%d" % (self.bursts + 1, k + 1), base=script)
            outcomes.append((verb, lines, result))
            if result[0] != EDITED:
                break
            script, dump = script + list(lines), result[1]
        return outcomes

    def _burst_done(self, outcomes):
        document = self.document
        commands = 0
        for verb, lines, outcome in outcomes:
            document.accept("burst:" + verb, lines, outcome, burst=True)
            if outcome[0] == EDITED:
                commands += len(lines)
        self.bursts += 1
        self.emit("DONE", commands=commands)


TYPES = {"Document": Document, "Oracle": Oracle, "Register": Register, "Focus": Focus,
         "Metric": Metric, "Fuzzer": Fuzzer}
