# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
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
from . import agent as A
from . import brief as B
from . import composer as M
from . import prompt as P
from . import runner
from .adapters import base as adapters
import json


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
    if holds:
        print("%s reproduces: %s" % (problem.id, problem.title))
        return 0
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


def cmd_fuzz(args):
    env = _env()
    cfg = C.load(args.config)
    document = _diagram(env, args.diagram)
    catalog = CAT.Catalog()
    coverage = COV.Coverage(env.polygon / "coverage.json")
    producer = F.Fuzzer(catalog, coverage, args.seed, gestures=not args.no_gestures)
    folder = S.session_folder(env.polygon, "fuzz", args.seed)
    session = S.Session(env, cfg, document, producer, _register(env), coverage, folder,
                        producer_name="fuzzer", seed=args.seed, minimize=not args.no_minimize)
    session.run(args.rounds)
    print("%s: %s" % (folder, session.summary()))
    for r in session.rounds:
        for kind, signature, note in r.findings:
            print("round %d %s %s: %s" % (r.number, kind, signature[:60], note[:100]))
    return 0


def _mission(env, cfg, catalog, coverage, args):
    return M.compose(args.mission, env, catalog, coverage, args.seed, args.diagram)


def _first_message(env, cfg, mission):
    result = runner.run(env, mission.original if mission.kind == M.REPRODUCE else mission.diagram,
                        dump=True, stack=True, timeout=cfg.timeout)
    if result.exit != runner.EXIT_OK:
        print("cannot open %s: %s" % (mission.name, " ".join(result.messages())), file=sys.stderr)
        sys.exit(2)
    dump = D.parse_dump(result.stdout)
    if mission.kind == M.REPRODUCE:
        return P.mission_reproduce(B.brief(mission.name, dump.document))
    return P.mission_combine(mission.name, D.describe(dump.document), P.scene_text(dump), dump.stack,
                             mission.operations, mission.theme, mission.budget, mission.untried)


def cmd_run(args):
    env = _env()
    cfg = C.load(args.config)
    backend = cfg.backend(args.backend)
    key = C.api_key(backend)
    if not key:
        print("no key for backend %s (%s)" % (backend.name, backend.key_env or backend.key_file), file=sys.stderr)
        return 2
    catalog = CAT.Catalog()
    coverage = COV.Coverage(env.polygon / "coverage.json")
    mission = _mission(env, cfg, catalog, coverage, args)
    folder = S.session_folder(env.polygon, "agent-" + mission.kind, args.seed)
    folder.mkdir(parents=True, exist_ok=True)
    log = open(folder / "conversation.txt", "a")
    recorder = lambda kind, text: (log.write("---- %s\n%s\n" % (kind, text)), log.flush())
    adapter = adapters.make(backend, key)
    producer = A.Agent(adapter, catalog, mission, _first_message(env, cfg, mission), recorder)
    session = S.Session(env, cfg, mission.diagram, producer, _register(env), coverage, folder,
                        producer_name="agent:%s:%s" % (backend.name, backend.model), seed=args.seed,
                        minimize=not args.no_minimize)
    session.mission = mission
    session.run(args.rounds or cfg.rounds)
    (folder / "usage.json").write_text(json.dumps(adapter.usage, indent=2) + "\n")
    log.close()
    print("%s: %s, %d calls" % (folder, session.summary(), adapter.calls))
    for r in session.rounds:
        for kind, signature, note in r.findings:
            print("round %d %s %s: %s" % (r.number, kind, signature[:60], note[:100]))
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
    p.add_argument("--no-minimize", action="store_true")
    p.set_defaults(func=cmd_fuzz)
    p = sub.add_parser("run", help="an agent session")
    p.add_argument("--backend", required=True)
    p.add_argument("--mission", choices=[M.REPRODUCE, M.COMBINE], default=M.COMBINE)
    p.add_argument("--diagram")
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--rounds", type=int)
    p.add_argument("--no-minimize", action="store_true")
    p.set_defaults(func=cmd_run)
    p = sub.add_parser("replay", help="replay a recorded session without the backend")
    p.add_argument("folder")
    p.set_defaults(func=cmd_replay)
    p = sub.add_parser("brief", help="print the reproduction brief of a diagram")
    p.add_argument("diagram")
    p.set_defaults(func=cmd_brief)
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
