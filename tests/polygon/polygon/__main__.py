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


def cmd_check_script(args):
    env = E.Env.discover()
    if not env.available():
        print("editor binary not found: %s" % env.binary, file=sys.stderr)
        return 2
    cfg = C.load(args.config)
    document = Path(args.diagram)
    if not document.exists():
        document = env.diagrams / (args.diagram + ".graphml")
    script = Path(args.script).read_text()
    facts = Path(args.expectations).read_text() if args.expectations else ""
    with tempfile.TemporaryDirectory(prefix="polygon-") as tmp:
        round_ = oracles.Round(env, cfg, document, tmp)
        result = round_.evaluate(script, expectation_text=facts)
        if result.script_error:
            print("script error at line %d: %s" % result.script_error)
        for f in result.findings:
            print("%s %s: %s" % (f.kind, f.signature, f.note))
        if args.dump and result.dump:
            print(D.describe(result.dump.document))
    return 1 if (result.findings or result.script_error) else 0


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
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
