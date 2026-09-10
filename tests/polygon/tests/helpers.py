# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the shared test helpers
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

"""Paths and skips shared by the unit tests."""

import unittest
from pathlib import Path

from polygon import config as C
from polygon import env as E

HERE = Path(__file__).resolve().parent
POLYGON = HERE.parent
TESTS = POLYGON.parent
GOOD = TESTS / "good"
DIAGRAMS = TESTS / "diagrams"
SCRIPTS = TESTS / "scripts"

ENV = E.Env.discover()
CONFIG = C.load(POLYGON / "polygon.example.toml")

needs_editor = unittest.skipUnless(ENV.available(), "editor binary not found: %s" % ENV.binary)
