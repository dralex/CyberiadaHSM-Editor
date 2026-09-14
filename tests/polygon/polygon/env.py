# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the editor environment
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

"""Where the editor binary is and which environment it needs: the same one
ctest bakes into build/tests/CTestTestfile.cmake (offscreen platform, the
private fontconfig, the Qt library paths)."""

import hashlib
import os
import re
import subprocess
from dataclasses import dataclass, field
from pathlib import Path

BINARY_ENV = "POLYGON_INSPECTOR"
BINARY_NAME = "CyberiadaEditor"

# the shared libraries whose version decides the editor's behaviour; a finding
# is only comparable across runs that link the same ones
TRACKED_LIBS = ("libcyberiadamlpp", "libcyberiadaml", "libhtgeom", "libhtreegeom")


def _digest(path):
    try:
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()[:8]
    except OSError:
        return "?"


def repo_root():
    return Path(__file__).resolve().parents[3]


def ctest_environment(build_dir):
    """The KEY=VALUE pairs of the first ENVIRONMENT property of the test file,
    as regen-good.sh reads them."""
    testfile = Path(build_dir) / "tests" / "CTestTestfile.cmake"
    if not testfile.exists():
        return {}
    match = re.search(r'ENVIRONMENT "([^"]*)"', testfile.read_text())
    if not match:
        return {}
    pairs = {}
    for item in match.group(1).split(";"):
        if "=" in item:
            key, value = item.split("=", 1)
            pairs[key] = value
    return pairs


@dataclass
class Env:
    root: Path
    binary: Path
    environ: dict = field(default_factory=dict)

    @property
    def tests(self):
        return self.root / "tests"

    @property
    def diagrams(self):
        return self.tests / "diagrams"

    @property
    def polygon(self):
        return self.tests / "polygon"

    def available(self):
        return self.binary.exists()

    def resolved_libs(self):
        """The tracked shared libraries the binary actually links, resolved
        under this env's library paths (not the ambient ones)."""
        libs = {}
        try:
            out = subprocess.run(["ldd", str(self.binary)], capture_output=True,
                                 text=True, env=self.environ, timeout=15).stdout
        except (OSError, subprocess.SubprocessError):
            return libs
        for line in out.splitlines():
            m = re.match(r"\s*(lib\S+?\.so\S*)\s*=>\s*(\S+)", line)
            if m and any(m.group(1).startswith(p) for p in TRACKED_LIBS):
                libs[m.group(1)] = m.group(2)
        return libs

    def fingerprint(self):
        """A compact identity of the runtime that ran a script: the editor
        binary and each tracked library it links, by a short content hash.
        Recorded with a finding so a later check can tell whether it reproduces
        under the same runtime or a different one."""
        libs = self.resolved_libs()
        parts = ["bin:" + _digest(self.binary)]
        for name in sorted(libs):
            parts.append("%s:%s" % (name.split(".so")[0], _digest(libs[name])))
        return " ".join(parts)

    @classmethod
    def discover(cls, root=None):
        root = Path(root) if root else repo_root()
        binary = Path(os.environ.get(BINARY_ENV) or root / "build" / BINARY_NAME)
        environ = dict(os.environ)
        # always pin the build tree's library and plugin paths so a manual run
        # loads the same libraries ctest does; a stale system libcyberiadaml
        # otherwise shadows the freshly built one and findings stop reproducing
        baked = ctest_environment(root / "build")
        for key, value in baked.items():
            if key == "LD_LIBRARY_PATH":
                existing = environ.get(key)
                environ[key] = value if not existing else value + os.pathsep + existing
            else:
                environ.setdefault(key, value)
        return cls(root=root, binary=binary, environ=environ)
