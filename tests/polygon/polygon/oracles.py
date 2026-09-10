# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the oracles
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

"""The reference-free checks of one round: the no-brain reruns (tier 1), the
expectations (tier 2) and the render check (tier 3)."""

import re
from dataclasses import dataclass, field
from pathlib import Path

from . import dump as D
from . import expectations
from . import runner

NUMBER = re.compile(r"-?\d+(?:\.\d+)?")

KIND_CRASH = "crash"
KIND_ORACLE = "oracle"
KIND_SEMANTIC = "semantic"
KIND_RENDER = "render"
KIND_REVIEW = "review"


def normalize(line):
    """The signature form of a line: every number becomes '#'."""
    return NUMBER.sub("#", line.strip())


@dataclass
class Finding:
    kind: str
    signature: str
    note: str = ""
    files: dict = field(default_factory=dict)


@dataclass
class RoundResult:
    run: runner.RunResult = None
    dump: D.Dump = None
    findings: list = field(default_factory=list)
    script_error: tuple = None   # (line, message) of an error in the new commands

    @property
    def accepted(self):
        return self.script_error is None and not any(
            f.kind == KIND_CRASH for f in self.findings)


def crash_finding(result, stage):
    """The crash finding of a run, or None."""
    if result.timed_out:
        return Finding(KIND_CRASH, "timeout", "the %s run exceeded the timeout" % stage)
    if result.signal is not None:
        return Finding(KIND_CRASH, "signal:%s" % result.signal_name,
                       "the %s run died by %s" % (stage, result.signal_name))
    if result.exit == runner.EXIT_INTERNAL:
        lines = result.messages()
        first = lines[0] if lines else "internal error"
        return Finding(KIND_CRASH, "assert:" + normalize(first),
                       "the %s run: %s" % (stage, first))
    return None


def export_frame(result):
    """(x, y, w, h) of the picture an export run reported, or None."""
    for line in result.messages():
        if line.startswith("export frame "):
            try:
                return tuple(float(v) for v in line.split()[2:6])
            except ValueError:
                return None
    return None


class Round:
    """The runs of one round on a start document and an accumulated script."""

    def __init__(self, env, config, start, workdir):
        self.env = env
        self.config = config
        self.start = Path(start)
        self.workdir = Path(workdir)
        self.workdir.mkdir(parents=True, exist_ok=True)
        self._start_dump = None
        self.export_frame = None   # (x, y, w, h) the last png export reported

    def run(self, script_text, name, **options):
        script = runner.write_script(self.workdir / (name + ".script"), script_text)
        return runner.run(self.env, self.start, script=script, timeout=self.config.timeout,
                          workdir=self.workdir, **options)

    def start_dump(self):
        if self._start_dump is None:
            result = runner.run(self.env, self.start, dump=True, timeout=self.config.timeout,
                                workdir=self.workdir)
            self._start_dump = result
        return self._start_dump

    def evaluate(self, script_text, prefix_lines=0, expectation_text="", render=None, family=None):
        """Run the script with every oracle. prefix_lines: the lines of the
        script the previous round accepted; an error inside them is a replay
        oracle failure, later ones are the round's own script error. family
        limits the tier 1 reruns to one oracle family (the signature head)."""
        out = RoundResult()
        main = self.run(script_text, "main", dump=True, stack=True)
        out.run = main
        crash = crash_finding(main, "main")
        if crash:
            out.findings.append(crash)
            return out
        if main.exit != runner.EXIT_OK:
            error = main.script_error()
            if main.exit == runner.EXIT_SCRIPT and error is not None:
                line, message = error
                if line <= prefix_lines:
                    out.findings.append(Finding(KIND_ORACLE, "replay:" + normalize(message),
                                                "line %d failed on replay: %s" % (line, message)))
                else:
                    out.script_error = error
            else:
                out.findings.append(Finding(KIND_ORACLE, "exit:%d" % main.exit,
                                            "exit code %d: %s" % (main.exit, " ".join(main.messages()))))
            return out
        try:
            out.dump = D.parse_dump(main.stdout)
        except D.DumpError as e:
            out.findings.append(Finding(KIND_ORACLE, "dump:" + normalize(str(e)), str(e)))
            return out
        out.findings += self.tier1(script_text, main, out.dump, family)
        for fact, reason in expectations.evaluate(expectation_text, out.dump):
            out.findings.append(Finding(KIND_SEMANTIC, "expect:" + normalize(fact), reason))
        if render is not None and family in (None, "render"):
            out.findings += render(self, script_text, out.dump)
        return out

    def _compare(self, name, first, second, stage):
        crash = crash_finding(second, stage)
        if crash:
            return [crash]
        if second.exit != runner.EXIT_OK:
            return [Finding(KIND_ORACLE, "%s:exit:%d" % (name, second.exit),
                            "the %s run exited %d: %s" % (stage, second.exit, " ".join(second.messages())))]
        try:
            diff = D.compare(first, second.stdout)
        except D.DumpError as e:
            return [Finding(KIND_ORACLE, "%s:dump:%s" % (name, normalize(str(e))), str(e))]
        if diff is None:
            return []
        return [Finding(KIND_ORACLE, "%s:%s" % (name, diff.tag),
                        "%s: expected %r, got %r" % (stage, diff.expected, diff.got))]

    def tier1(self, script_text, main, dump, family=None):
        findings = []
        if family in (None, "save-reopen"):
            findings += self.save_reopen(script_text, main)
        if family in (None, "undo-all", "redo-all"):
            findings += self.undo_redo(script_text, main, dump)
        if family in (None, "export"):
            findings += self.exports(script_text)
        return findings

    def save_reopen(self, script_text, main):
        findings = []
        saved = self.workdir / "saved.graphml"
        save = self.run(script_text, "save", save=saved)
        crash = crash_finding(save, "save")
        if crash:
            findings.append(crash)
        elif save.exit != runner.EXIT_OK or not saved.exists():
            findings.append(Finding(KIND_ORACLE, "save:exit:%d" % save.exit,
                                    "the save run exited %d: %s" % (save.exit, " ".join(save.messages()))))
        else:
            reopen = runner.run(self.env, saved, dump=True, timeout=self.config.timeout,
                                workdir=self.workdir)
            findings += self._compare("save-reopen", main.stdout, reopen, "reopen")
        return findings

    def undo_redo(self, script_text, main, dump):
        findings = []
        steps = dump.stack.index if dump.stack else 0
        undo_text = script_text.rstrip("\n") + "\n" + "undo\n" * steps
        undo = self.run(undo_text, "undo", dump=True)
        findings += self._compare("undo-all", self.start_dump().stdout, undo, "undo-all")
        redo_text = undo_text + "redo\n" * steps
        redo = self.run(redo_text, "redo", dump=True)
        findings += self._compare("redo-all", main.stdout, redo, "redo-all")
        return findings

    def exports(self, script_text):
        findings = []
        for suffix in ("png", "svg"):
            target = self.workdir / ("export." + suffix)
            result = self.run(script_text, "export-" + suffix, export=target)
            if suffix == "png":
                self.export_frame = export_frame(result)
            crash = crash_finding(result, "export " + suffix)
            if crash:
                findings.append(crash)
            elif result.exit != runner.EXIT_OK or not target.exists():
                findings.append(Finding(KIND_ORACLE, "export:" + suffix,
                                        "the %s export failed: %s" % (suffix, " ".join(result.messages()))))
        return findings
