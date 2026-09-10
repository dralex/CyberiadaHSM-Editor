# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the coverage store
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

"""coverage.json: how often every (verb, kind) cell and every consecutive
verb pair ran across the sessions, and which cells fired an oracle; the
weights that bias the next draw toward the untried cells."""

import json
from pathlib import Path

FIRED_BONUS = 2.0


class Coverage:
    def __init__(self, path):
        self.path = Path(path)
        self.cells = {}
        self.pairs = {}
        self.fired = {}
        self.load()

    @staticmethod
    def key(verb, kind):
        return "%s|%s" % (verb, kind)

    def load(self):
        if self.path.exists():
            data = json.loads(self.path.read_text())
            self.cells = data.get("cells", {})
            self.pairs = data.get("pairs", {})
            self.fired = data.get("fired", {})

    def save(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        data = {"cells": dict(sorted(self.cells.items())),
                "pairs": dict(sorted(self.pairs.items())),
                "fired": dict(sorted(self.fired.items()))}
        self.path.write_text(json.dumps(data, indent=2) + "\n")

    def record(self, verb, kind, previous=None, fired=False):
        key = self.key(verb, kind)
        self.cells[key] = self.cells.get(key, 0) + 1
        if previous:
            pair = "%s>%s" % (previous, verb)
            self.pairs[pair] = self.pairs.get(pair, 0) + 1
        if fired:
            self.fired[key] = self.fired.get(key, 0) + 1

    def count(self, verb, kind):
        return self.cells.get(self.key(verb, kind), 0)

    def weight(self, verb, kind):
        """Untried cells weigh most; a fired cell keeps a bonus so it is
        revisited with other neighbours."""
        key = self.key(verb, kind)
        weight = 1.0 / (1.0 + self.cells.get(key, 0))
        if self.fired.get(key):
            weight += FIRED_BONUS / (1.0 + self.cells.get(key, 0))
        return weight

    def verb_weight(self, verb, kinds):
        kinds = kinds or ["-"]
        return max(self.weight(verb, kind) for kind in kinds)

    def untried(self, catalog):
        return [(verb, kind) for verb, kind in catalog.cells() if self.count(verb, kind) == 0]
