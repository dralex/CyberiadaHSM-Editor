# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the operation catalog
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

"""The operations of the batch script (catalog/operations.json): the verb
table of the prompt, the cells of the coverage store, the forms the fuzzer
generates."""

import json
from dataclasses import dataclass
from pathlib import Path

CATALOG_DIR = Path(__file__).resolve().parents[1] / "catalog"

FORM_MODEL = "model"
FORM_GESTURE = "gesture"
FORM_GESTURE_FORM = "gesture-form"


@dataclass
class Operation:
    verb: str
    args: str
    effect: str
    targets: list
    form: str

    @property
    def usage(self):
        return (self.verb + " " + self.args).strip()


class Catalog:
    def __init__(self, path=None):
        path = Path(path) if path else CATALOG_DIR / "operations.json"
        data = json.loads(path.read_text())
        self.operations = [Operation(**op) for op in data["operations"]]

    def find(self, verb):
        for op in self.operations:
            if op.verb == verb:
                return op
        return None

    def by_form(self, *forms):
        return [op for op in self.operations if op.form in forms]

    def verbs(self, *forms):
        return [op.verb for op in self.by_form(*forms)]

    def cells(self):
        """Every (verb, target kind) pair the coverage store counts."""
        out = []
        for op in self.by_form(FORM_MODEL, FORM_GESTURE_FORM):
            for kind in op.targets or ["-"]:
                out.append((op.verb, kind))
        return out

    def table(self, *forms):
        """The markdown verb table of the prompt."""
        lines = ["| command | effect |", "|---|---|"]
        for op in self.by_form(*forms):
            lines.append("| `%s` | %s |" % (op.usage, op.effect))
        return "\n".join(lines)


def format_profile(path=None):
    """The fields the serialization format does not preserve (catalog/format.json):
    {tag: reason}; the round-trip oracles skip them."""
    path = Path(path) if path else CATALOG_DIR / "format.json"
    if not path.exists():
        return {}
    data = json.loads(path.read_text())
    return {entry["tag"]: entry.get("reason", "") for entry in data.get("not_preserved", [])}


# the tool families of the reworked editor (catalog/tools.json)
FAMILY_RECT = "rect"          # new-sm, new-state: draw a rect or click for a default
FAMILY_PLACE = "place"        # the six pseudostate/comment tools: click to place
FAMILY_TRANSITION = "transition"
FAMILY_MANIPULATE = "manipulate"   # the select tool: drag, resize, edit, delete
FAMILY_VIEW = "view"          # pan, zoom: not used for editing


class Tool:
    def __init__(self, name, family, element, default):
        self.name = name
        self.family = family
        self.element = element
        self.default = default   # [w, h] for a rect tool, else None


def tools(path=None):
    import json
    path = Path(path) if path else CATALOG_DIR / "tools.json"
    data = json.loads(path.read_text())
    return [Tool(t["name"], t["family"], t["element"], t["default"]) for t in data["tools"]]


def creation_tools(path=None):
    """The eight element-creation tools (rect and place families)."""
    return [t for t in tools(path) if t.family in (FAMILY_RECT, FAMILY_PLACE)]


def model_card(path=None):
    path = Path(path) if path else CATALOG_DIR / "model.md"
    return path.read_text() if path.exists() else ""
