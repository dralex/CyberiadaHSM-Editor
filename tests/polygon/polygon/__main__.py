# -----------------------------------------------------------------------------
# CyberiadaPro, the Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the command line
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

"""python3 -m polygon <command> ... (see README.md)."""

import argparse
import sys
import tempfile
from pathlib import Path

from . import config as C
from . import dump as D
from . import env as E
from . import oracles
from . import register as R
from . import render
from . import catalog as CAT
from . import coverage as COV
from . import fuzzer as F
from . import session as S
from . import brief as B
from . import composer as M
from . import runner
from . import tour as TOUR
from . import toolcover as TC
from .drills import nesting as DRILLS
from .drills import features as DRILLF
from .drills import compound as DRILLC
import json

HARNESS_HINT = ("the LLM commands need the cybersphere agents harness (cybersphere-agents, "
                "hsm-dispatcher-py, pydantic-ai, the CyberiadaML binding); use the environment "
                "that has them, e.g. ~/sources/_venv")


def _env():
    env = E.Env.discover()
    if not env.available():
        print("editor binary not found: %s" % env.binary, file=sys.stderr)
        sys.exit(2)
    return env


def _diagram(env, name):
    document = Path(name)
    if not document.exists():
        document = env.diagrams / (name + ".graphml")
    if not document.exists():
        print("no diagram %s" % name, file=sys.stderr)
        sys.exit(2)
    return document.resolve()


def _register(env):
    return R.Register(env.polygon / "problems")


def cmd_check_script(args):
    env = _env()
    cfg = C.load(args.config)
    document = _diagram(env, args.diagram)
    script = Path(args.script).read_text()
    facts = Path(args.expectations).read_text() if args.expectations else ""
    with tempfile.TemporaryDirectory(prefix="polygon-") as tmp:
        round_ = oracles.Round(env, cfg, document, tmp)
        result = round_.evaluate(script, expectation_text=facts, render=render.oracle)
        if result.script_error:
            print("script error at line %d: %s" % result.script_error)
        for f in result.findings:
            print("%s %s: %s" % (f.kind, f.signature, f.note))
        if args.dump and result.dump:
            print(D.describe(result.dump.document))
    return 1 if (result.findings or result.script_error) else 0


def cmd_check(args):
    """The regression case of a registered problem: 0 while it reproduces."""
    env = _env()
    cfg = C.load(args.config)
    register = _register(env)
    problem = register.find(args.problem)
    if problem is None:
        print("no problem %s" % args.problem, file=sys.stderr)
        return 2
    holds, result = register.check(env, cfg, problem)
    for f in result.findings:
        print("%s %s: %s" % (f.kind, f.signature, f.note))
    mismatch = register.env_mismatch(env, problem)
    if mismatch:
        print("NOTE: %s" % mismatch, file=sys.stderr)
    if holds:
        print("%s reproduces: %s" % (problem.id, problem.title))
        return 0
    if mismatch:
        print("%s does not reproduce here, but the runtime changed - rebuild/point at the "
              "recorded runtime before concluding it is fixed" % problem.id)
        return 1
    print("%s does not reproduce any more: %s" % (problem.id, problem.title))
    return 1


def cmd_register(args):
    env = _env()
    cfg = C.load(args.config)
    register = _register(env)
    if args.action == "list":
        for p in register.problems:
            print("%s %s %s [%s] %s (%s, hits %d)" % (p.id, p.status, p.kind, p.signature[:60],
                                                     p.title, p.diagram, p.hits))
        return 0
    if args.action == "cases":
        register.write_cases()
        print(register.folder / R.CASES_NAME)
        return 0
    if args.action == "set-status":
        problem = register.find(args.problem or "")
        if problem is None or args.status not in R.STATUSES:
            print("usage: register set-status --problem <id> --status %s" % "|".join(R.STATUSES), file=sys.stderr)
            return 2
        problem.status = args.status
        if args.note:
            problem.note = args.note
        register.save()
        print("%s %s" % (problem.id, problem.status))
        return 0
    if args.action == "add":
        document = _diagram(env, args.diagram)
        script = Path(args.script).read_text() if args.script else ""
        facts = Path(args.expectations).read_text() if args.expectations else ""
        added, result = R.register_script(register, env, cfg, document, script, facts,
                                          title=args.title or "", producer="manual",
                                          root=env.root, do_minimize=not args.no_minimize,
                                          kinds=None)
        if result.script_error:
            print("script error at line %d: %s" % result.script_error)
        for problem, is_new in added:
            print("%s %s: %s" % (problem.id, "registered" if is_new else "known", problem.title))
        return 0
    return 2


DRILL_CLASSES = {
    "nesting": DRILLS.NestingDrill, "pseudostate": DRILLS.PseudostateDrill,
    "transition-points": DRILLF.TransitionPointDrill, "rebind": DRILLF.RebindDrill,
    "choice": DRILLF.ChoiceDrill, "action": DRILLF.ActionDrill,
    "resize": DRILLF.ResizeDrill, "copy-paste": DRILLF.CopyPasteDrill,
    "transition-stress": DRILLF.TransitionStressDrill,
    "submachine": DRILLF.SubmachineDrill,
    "lifetime": DRILLF.LifetimeDrill,
    "compound": DRILLC.CompoundDrill,
}


def _snap_preamble(args):
    """The script verbs a --snap/--grid option prepends, so a whole sweep runs
    with snap on (the batch default is off) and the finding reproduces on its
    own: snap is recorded in the script, not passed as an unrecorded flag."""
    if not getattr(args, "snap", False):
        return ()
    lines = ["snap on"]
    if getattr(args, "grid", None):
        lines.append("grid %d" % args.grid)
    return tuple(lines)


def cmd_drill(args):
    """A deterministic drill: escalate one feature, check its invariants."""
    env = _env()
    preamble = _snap_preamble(args)
    cfg = C.load(args.config)
    catalog = CAT.Catalog()
    coverage = COV.Coverage(env.polygon / "coverage.json")
    which = list(DRILL_CLASSES) if args.which == "all" else [args.which]
    start = env.polygon / "corpus" / "empty.graphml"
    total = 0
    for name in which:
        producer = DRILL_CLASSES[name](catalog, coverage, args.seed, budget=args.rounds)
        if getattr(args, "boundary", False) and hasattr(producer, "boundary"):
            producer.boundary = True
        folder = S.session_folder(env.polygon, "drill-" + name, args.seed)
        session = S.Session(env, cfg, start, producer, _register(env), coverage, folder,
                            producer_name="drill:" + name, seed=args.seed, minimize=not args.no_minimize,
                            preamble=preamble)
        session.stress = True
        session.invariant = True
        session.run(args.rounds)
        print("%s [%s]: %s" % (folder, name, session.summary()))
        for r in session.rounds:
            for kind, signature, note in r.findings:
                print("  round %d %s %s: %s" % (r.number, kind, signature[:60], note[:80]))
        total += 1
    return 0


def cmd_tour(args):
    """The deterministic tool tour: walk the (tool, pattern) matrix, no LLM."""
    env = _env()
    cfg = C.load(args.config)
    catalog = CAT.Catalog()
    coverage = COV.Coverage(env.polygon / "coverage.json")
    toolcover = TC.ToolCoverage(env.polygon / "toolcover.json")
    start = env.polygon / "corpus" / "empty.graphml"
    producer = TOUR.DeterministicTour(catalog, coverage, toolcover, args.seed)
    folder = S.session_folder(env.polygon, "tour", args.seed)
    session = S.Session(env, cfg, start, producer, _register(env), coverage, folder,
                        producer_name="tour", seed=args.seed, minimize=not args.no_minimize)
    session.stress = True
    session.run(args.rounds)
    toolcover.save()
    print("%s: %s" % (folder, session.summary()))
    print(toolcover.report())
    for r in session.rounds:
        for kind, signature, note in r.findings:
            print("round %d %s %s: %s" % (r.number, kind, signature[:60], note[:90]))
    return 0


def cmd_tour_report(args):
    env = _env()
    print(TC.ToolCoverage(env.polygon / "toolcover.json").report())
    return 0


def cmd_fuzz(args):
    env = _env()
    cfg = C.load(args.config)
    document = _diagram(env, args.diagram)
    catalog = CAT.Catalog()
    coverage = COV.Coverage(env.polygon / "coverage.json")
    producer = F.Fuzzer(catalog, coverage, args.seed, gestures=not args.no_gestures)
    folder = S.session_folder(env.polygon, "fuzz", args.seed)
    session = S.Session(env, cfg, document, producer, _register(env), coverage, folder,
                        producer_name="fuzzer", seed=args.seed, minimize=not args.no_minimize,
                        preamble=_snap_preamble(args))
    session.stress = getattr(args, "stress", False)
    if session.stress:
        producer.gesture_bias = 4.0   # exercise the drags and edge/point edits
    session.run(args.rounds)
    print("%s: %s" % (folder, session.summary()))
    for r in session.rounds:
        for kind, signature, note in r.findings:
            print("round %d %s %s: %s" % (r.number, kind, signature[:60], note[:100]))
    return 0


def _harness():
    """The harness module, or a clear message without the framework."""
    try:
        from . import harness as H
    except ImportError as e:
        print("%s (%s)" % (HARNESS_HINT, e), file=sys.stderr)
        sys.exit(2)
    return H


def _session_stats(result):
    accepted = sum(1 for r in result.rounds if r["accepted"])
    errors = sum(1 for r in result.rounds if r["script_error"])
    rep = result.reproduction
    stats = {"rounds": len(result.rounds), "accepted": accepted, "errors": errors,
             "calls": result.calls, "tokens": result.tokens, "commands": result.commands,
             "elapsed": result.elapsed, "defects": result.defects, "candidates": result.candidates,
             "reproduction": "-" if rep is None else ("match" if rep["matches"] else "differ %d" % len(rep["differences"]))}
    if result.metrics:
        cs = [c["C"] for c, _ in result.metrics if c]
        ds = [d["P"] for _, d in result.metrics if d]
        peak_c = max(cs, default=0.0)
        stats.update({"peak_c": round(peak_c, 1), "peak_d": round(max(ds, default=0.0), 1),
                      "band": next((c["band"] for c, _ in result.metrics if c and c["C"] == peak_c), ""),
                      "density": round(peak_c / result.commands, 2) if result.commands else 0.0})
    return stats


def cmd_run(args):
    env = _env()
    cfg = C.load(args.config)
    H = _harness()
    catalog = CAT.Catalog()
    coverage = COV.Coverage(env.polygon / "coverage.json")
    names = [b.strip() for b in (args.backends.split(",") if args.backends else [args.backend]) if b and b.strip()]
    if not names:
        print("give --backend or --backends", file=sys.stderr)
        return 2
    mission = M.compose(args.mission, env, args.seed, args.diagram, args.theme, args.story)
    ledger = _Ledger(env.polygon / "productivity.json")
    compare = len(names) > 1
    base = S.session_folder(env.polygon, "compare-" + mission.kind if compare else "agent-" + mission.kind, args.seed)
    rows = []
    code = 0
    for name in names:
        backend = cfg.backend(name)
        folder = (base / name) if compare else base
        result = H.run_session(env, cfg, catalog, coverage, backend, mission, folder, _register(env),
                               burst=args.burst or H.PC.DEFAULT_BURST, trace=args.trace)
        stats = _session_stats(result)
        stats["backend"] = "%s:%s" % (backend.name, backend.model)
        rows.append(stats)
        ledger.add(backend.name, stats)
        extra = ""
        if "peak_c" in stats:
            extra = ", peak C=%.1f (%s) D=%.1f density=%.2f" % (
                stats["peak_c"], stats["band"], stats["peak_d"], stats["density"])
        print("%s [%s]: %s, %d calls%s" % (folder, backend.name, result.summary(), stats["calls"], extra))
        for r in result.rounds:
            for kind, signature, note in r["findings"]:
                print("  edit %d %s %s: %s" % (r["n"], kind, signature[:60], note[:100]))
        if result.failed or result.error:
            code = 2
    ledger.save()
    if compare:
        base.mkdir(parents=True, exist_ok=True)
        (base / "comparison.txt").write_text(_comparison_table(mission, rows))
        print("\n" + _comparison_table(mission, rows))
    return code


def _comparison_table(mission, rows):
    head = "mission: %s %s, seed %d, theme %s\n" % (
        mission.kind, mission.name, mission.seed, mission.theme["name"] if mission.theme else "-")
    cols = "%-28s %6s %4s %4s %5s %8s %5s %8s %6s %10s\n" % (
        "backend", "rounds", "acc", "err", "calls", "tokens", "cmds", "elapsed", "defect", "reproduction")
    lines = [head, cols]
    for r in rows:
        lines.append("%-28s %6d %4d %4d %5d %8d %5d %7.0fs %6d %10s\n" % (
            r["backend"], r["rounds"], r["accepted"], r["errors"], r["calls"], r["tokens"],
            r["commands"], r["elapsed"], r["defects"], r["reproduction"]))
    return "".join(lines)


class _Ledger:
    """productivity.json: per backend, the totals across the campaigns."""
    def __init__(self, path):
        self.path = Path(path)
        self.data = json.loads(self.path.read_text()) if self.path.exists() else {}

    def add(self, backend, stats):
        d = self.data.setdefault(backend, {"sessions": 0, "elapsed": 0.0, "calls": 0,
                                           "tokens": 0, "commands": 0, "defects": 0,
                                           "reproductions_matched": 0})
        d["sessions"] += 1
        d["elapsed"] += stats["elapsed"]
        d["calls"] += stats["calls"]
        d["tokens"] += stats["tokens"]
        d["commands"] += stats["commands"]
        d["defects"] += stats["defects"]
        d["reproductions_matched"] += 1 if stats["reproduction"] == "match" else 0

    def save(self):
        self.path.write_text(json.dumps(self.data, indent=2) + "\n")


def cmd_report(args):
    env = _env()
    path = env.polygon / "productivity.json"
    if not path.exists():
        print("no productivity ledger yet", file=sys.stderr)
        return 1
    data = json.loads(path.read_text())
    print("%-14s %8s %9s %10s %9s %8s %8s" % (
        "backend", "sessions", "elapsed", "tokens", "commands", "defects", "repro"))
    for name, d in sorted(data.items()):
        print("%-14s %8d %8.0fs %10d %9d %8d %8d" % (
            name, d["sessions"], d["elapsed"], d["tokens"], d["commands"],
            d["defects"], d["reproductions_matched"]))
    return 0


def cmd_replay(args):
    env = _env()
    cfg = C.load(args.config)
    data = json.loads((Path(args.folder) / "session.json").read_text())
    coverage = COV.Coverage(env.polygon / "coverage.json")
    folder = S.session_folder(env.polygon, "replay", data.get("seed", 0))
    session = S.Session(env, cfg, data["start"], S.Replay(data["rounds"]), _register(env), coverage,
                        folder, producer_name="replay", seed=data.get("seed", 0), minimize=False)
    session.run(len(data["rounds"]))
    print("%s: %s" % (folder, session.summary()))
    for r in session.rounds:
        for kind, signature, note in r.findings:
            print("round %d %s %s: %s" % (r.number, kind, signature[:60], note[:100]))
    return 0


def cmd_brief(args):
    env = _env()
    cfg = C.load(args.config)
    document = _diagram(env, args.diagram)
    result = runner.run(env, document, dump=True, timeout=cfg.timeout)
    if result.exit != runner.EXIT_OK:
        print(" ".join(result.messages()), file=sys.stderr)
        return 1
    print(B.brief(Path(args.diagram).stem, D.parse_dump(result.stdout).document), end="")
    return 0


def cmd_reconstruct(args):
    """Reconstruct each diagram's geometry and judge the result: structural
    equivalence (only the geometry may change), render soundness (registered),
    and the closeness to the original arrangement (docs/RECONSTRUCT_CLOSENESS.md)."""
    env = _env()
    cfg = C.load(args.config)
    register = _register(env)
    if args.diagram:
        doc = env.polygon / "corpus" / (args.diagram + ".graphml")
        docs = [doc if doc.exists() else _diagram(env, args.diagram)]
    else:
        docs = [p for p in sorted((env.polygon / "corpus").glob("*.graphml"))
                if p.stem not in ("empty", "small")]
    outroot = env.polygon / "reconstruct"
    try:
        from tools import reconstruct_closeness as RC
    except Exception:
        RC = None
    rows = []
    for doc in docs:
        name = doc.stem
        dest = outroot / name
        dest.mkdir(parents=True, exist_ok=True)
        recon = dest / "recon.graphml"
        with tempfile.TemporaryDirectory(prefix="polygon-") as tmp:
            tmp = Path(tmp)
            scriptf = runner.write_script(tmp / "r.script", "reconstruct\n")
            # renders (original and reconstructed, png and svg) for human review
            runner.run(env, doc, export=dest / "orig.png", timeout=cfg.timeout, workdir=tmp)
            runner.run(env, doc, export=dest / "orig.svg", timeout=cfg.timeout, workdir=tmp)
            runner.run(env, doc, script=scriptf, save=recon, export=dest / "recon.png",
                       timeout=cfg.timeout, workdir=tmp)
            runner.run(env, doc, script=scriptf, export=dest / "recon.svg", timeout=cfg.timeout, workdir=tmp)
            # the structure must be untouched: only the geometry may change
            d0 = D.parse_dump(runner.run(env, doc, dump=True, timeout=cfg.timeout, workdir=tmp).stdout)
            d1 = D.parse_dump(runner.run(env, doc, script=scriptf, dump=True, timeout=cfg.timeout, workdir=tmp).stdout)
            struct_ok = bool(d0.document and d1.document) and not D.structural_diff(d0.document, d1.document)
            # render soundness on the reconstructed layout, registered as problems
            _, result = R.register_script(register, env, cfg, doc, "reconstruct\n",
                                          title="reconstruct " + name, producer="reconstruct", root=env.root)
            defects = [f for f in result.findings if f.kind in R.DEFECT_KINDS]
        K = None
        if RC is not None and recon.exists():
            try:
                K = RC.closeness(str(doc), str(recon))["K"]
            except Exception:
                K = None
        rows.append(dict(name=name, struct_ok=struct_ok, defects=len(defects), closeness=K))
        print("%-22s struct=%-4s defects=%d closeness=%s" %
              (name, "ok" if struct_ok else "DIFF", len(defects),
               "%.3f" % K if K is not None else "-"))
    ks = [r["closeness"] for r in rows if r["closeness"] is not None]
    print("\n%d diagrams · structural %d/%d · soundness defects %d · closeness rate %s" %
          (len(rows), sum(1 for r in rows if r["struct_ok"]), len(rows),
           sum(r["defects"] for r in rows),
           "%.3f" % (sum(ks) / len(ks)) if ks else "n/a"))
    (outroot / "summary.json").write_text(json.dumps(rows, indent=2) + "\n")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(prog="polygon")
    parser.add_argument("--config", help="the polygon.toml to use")
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("check-script", help="run the oracles on a script")
    p.add_argument("--diagram", required=True, help="a corpus name or a graphml path")
    p.add_argument("--script", required=True)
    p.add_argument("--expectations")
    p.add_argument("--dump", action="store_true", help="describe the result")
    p.set_defaults(func=cmd_check_script)
    p = sub.add_parser("check", help="reproduce a registered problem (the regression case)")
    p.add_argument("--problem", required=True)
    p.set_defaults(func=cmd_check)
    p = sub.add_parser("register", help="the problem register")
    p.add_argument("action", choices=["list", "add", "cases", "set-status"])
    p.add_argument("--problem")
    p.add_argument("--status")
    p.add_argument("--note")
    p.add_argument("--diagram")
    p.add_argument("--script")
    p.add_argument("--expectations")
    p.add_argument("--title")
    p.add_argument("--no-minimize", action="store_true")
    p.set_defaults(func=cmd_register)
    p = sub.add_parser("fuzz", help="a fuzzer session")
    p.add_argument("--diagram", required=True)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--rounds", type=int, default=20)
    p.add_argument("--no-gestures", action="store_true")
    p.add_argument("--stress", action="store_true", help="crash/save-reopen/undo-all/export only, no ink check")
    p.add_argument("--no-minimize", action="store_true")
    p.add_argument("--snap", action="store_true", help="run every gesture with snap-to-grid on")
    p.add_argument("--grid", type=int, default=None, help="the grid spacing for --snap")
    p.set_defaults(func=cmd_fuzz)
    p = sub.add_parser("tour", help="the deterministic tool tour (no backend)")
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--rounds", type=int, default=60)
    p.add_argument("--no-minimize", action="store_true")
    p.set_defaults(func=cmd_tour)
    p = sub.add_parser("tour-report", help="the tool-coverage matrix")
    p.set_defaults(func=cmd_tour_report)
    p = sub.add_parser("drill", help="a deterministic drill (escalate one feature, check invariants)")
    p.add_argument("--which", default="all", choices=["all"] + list(DRILL_CLASSES))
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--rounds", type=int, default=20)
    p.add_argument("--boundary", action="store_true", help="drive features to extremes (compound drill)")
    p.add_argument("--no-minimize", action="store_true")
    p.add_argument("--snap", action="store_true", help="run every gesture with snap-to-grid on")
    p.add_argument("--grid", type=int, default=None, help="the grid spacing for --snap")
    p.set_defaults(func=cmd_drill)
    p = sub.add_parser("run", help="a tester session (one backend, or a comma list to compare)")
    p.add_argument("--backend")
    p.add_argument("--backends", help="a comma list: run the same mission on each and compare")
    p.add_argument("--mission", choices=list(M.KINDS), default=M.HUNT)
    p.add_argument("--diagram", help="the corpus diagram to rebuild (rebuild mission)")
    p.add_argument("--theme", help="a theme seasoning the task (catalog/themes.json)")
    p.add_argument("--story", help="the diagram to draw (draw mission; catalog/stories.json)")
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--burst", type=int, help="fuzzer micro-ops after each clean check of a stress session (default 5)")
    p.add_argument("--trace", action="store_true", help="print the moves of the machine")
    p.set_defaults(func=cmd_run)
    p = sub.add_parser("replay", help="replay a recorded session without the backend")
    p.add_argument("folder")
    p.set_defaults(func=cmd_replay)
    p = sub.add_parser("brief", help="print the reproduction brief of a diagram")
    p.add_argument("diagram")
    p.set_defaults(func=cmd_brief)
    p = sub.add_parser("reconstruct", help="reconstruct each diagram and judge structure, soundness, closeness")
    p.add_argument("--diagram", help="a single diagram (default: the whole corpus)")
    p.set_defaults(func=cmd_reconstruct)
    p = sub.add_parser("report", help="the per-backend productivity ledger")
    p.set_defaults(func=cmd_report)
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
