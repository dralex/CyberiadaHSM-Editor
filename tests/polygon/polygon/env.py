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

import os
import re
from dataclasses import dataclass, field
from pathlib import Path

BINARY_ENV = "POLYGON_INSPECTOR"
BINARY_NAME = "CyberiadaEditor"


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

    @classmethod
    def discover(cls, root=None):
        root = Path(root) if root else repo_root()
        binary = Path(os.environ.get(BINARY_ENV) or root / "build" / BINARY_NAME)
        environ = dict(os.environ)
        if "QT_QPA_PLATFORM" not in environ:
            # outside ctest: take the baked environment of the build tree
            environ.update(ctest_environment(root / "build"))
        return cls(root=root, binary=binary, environ=environ)
