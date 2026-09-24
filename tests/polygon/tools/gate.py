#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# The Cyberiada HSM Editor test polygon: the corpus adoption gate
#
# Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see https://www.gnu.org/licenses/
#
# -----------------------------------------------------------------------------

"""The adoption gate of the corpus diagrams.

    python3 tools/gate.py check <diagram.graphml>...
    python3 tools/gate.py adopt <source.graphml> <corpus name>

A diagram passes when the editor loads it, saves it to the native format, the
saved document reopens to the same dump (save/reopen stable), the standing
laws are silent, and the saved document reloads under `--strict` — the strict
mode of libcyberiadaml, which checks the requirements of PNST 1044 the format
cannot express structurally. `adopt` runs the gate on a foreign document and
stores the saved native form under corpus/<name>.graphml.
"""

import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

from polygon import dump as D            # noqa: E402
from polygon import env as E             # noqa: E402
from polygon import laws as L            # noqa: E402
from polygon import runner as R          # noqa: E402

CORPUS = os.path.join(os.path.dirname(HERE), "corpus")
TIMEOUT = 60.0


def gate(env, path, workdir):
    """The list of failures of one diagram (empty when it passes) and the
    path of the saved native document."""
    failures = []
    saved = os.path.join(workdir, "saved.graphml")
    path = os.path.abspath(path)
    main = R.run(env, path, dump=True, save=saved, timeout=TIMEOUT, workdir=workdir)
    if main.exit != R.EXIT_OK or not os.path.exists(saved):
        return ["load/save: exit %s %s" % (main.exit, " ".join(main.messages()))], None
    try:
        dump = D.parse_dump(main.stdout)
    except D.DumpError as e:
        return ["dump: %s" % e], saved
    for v in L.check(dump):
        failures.append("law %s: %s" % (v.req, v.detail))
    reopen = R.run(env, saved, dump=True, timeout=TIMEOUT, workdir=workdir)
    if reopen.exit != R.EXIT_OK:
        failures.append("reopen: exit %s" % reopen.exit)
    else:
        diff = D.compare(main.stdout, reopen.stdout, set())
        if diff:
            failures.append("save/reopen: %s" % diff[0])
    strict = R.run(env, saved, dump=True, timeout=TIMEOUT, workdir=workdir, strict=True)
    if strict.exit != R.EXIT_OK:
        failures.append("strict: %s" % (" ".join(strict.messages()) or "exit %s" % strict.exit))
    return failures, saved


def main(argv):
    if len(argv) < 3 or argv[1] not in ("check", "adopt"):
        sys.stderr.write(__doc__)
        return 2
    env = E.Env.discover()
    if not env.available():
        sys.stderr.write("the editor binary is not built\n")
        return 2
    status = 0
    with tempfile.TemporaryDirectory() as workdir:
        if argv[1] == "check":
            for path in argv[2:]:
                failures, _ = gate(env, path, workdir)
                print("%-40s %s" % (os.path.basename(path), "ok" if not failures else "FAIL"))
                for f in failures:
                    print("    " + f)
                status = status or (1 if failures else 0)
        else:
            source, name = argv[2], argv[3]
            failures, saved = gate(env, source, workdir)
            for f in failures:
                print("    " + f)
            if failures or saved is None:
                print("%s: not adopted" % name)
                return 1
            target = os.path.join(CORPUS, name + ".graphml")
            shutil.copy(saved, target)
            print("%s: adopted as %s" % (source, target))
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv))
