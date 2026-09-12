# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the mission composer
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

"""A mission from a seed: the diagram, and for a combination the operation
subset, the theme, the budget and the untried cells of the coverage."""

import json
import random
from dataclasses import dataclass, field
from pathlib import Path

from . import catalog as CAT

REPRODUCE = "reproduce"
COMBINE = "combine"
MIN_OPERATIONS = 3
MAX_OPERATIONS = 6
MAX_UNTRIED = 4


@dataclass
class Mission:
    kind: str
    seed: int
    name: str            # the corpus diagram name
    diagram: Path        # the document the session edits (empty for a reproduction)
    original: Path = None   # the diagram to reproduce
    operations: list = field(default_factory=list)
    theme: dict = None
    budget: tuple = (0, 0)
    untried: list = field(default_factory=list)


def corpus(env):
    """[(name, path)] of the corpus documents, and the start documents by rule."""
    folder = env.polygon / "corpus"
    manifest = json.loads((folder / "manifest.json").read_text())
    starts = {"empty": folder / manifest["start"],
              "small": folder / manifest.get("small", "small.graphml")}
    names = list(manifest["diagrams"])
    for path in sorted(folder.glob("*.graphml")):
        if path.stem not in names and path.name != manifest["start"]:
            names.append(path.stem)
    out = []
    for name in names:
        path = env.diagrams / (name + ".graphml")
        if not path.exists():
            path = folder / (name + ".graphml")
        if path.exists():
            out.append((name, path))
    return out, starts


def themes():
    return json.loads((CAT.CATALOG_DIR / "themes.json").read_text())["themes"]


def compose(kind, env, catalog, coverage, seed, name=None, theme_name=None):
    rng = random.Random(seed)
    diagrams, starts = corpus(env)
    if name is None:
        name, path = rng.choice(diagrams)
    else:
        path = dict(diagrams).get(name) or (env.diagrams / (name + ".graphml"))
    if kind == REPRODUCE:
        return Mission(kind, seed, name, starts["empty"], original=path)
    theme = next((t for t in themes() if t["name"] == theme_name), None) if theme_name else rng.choice(themes())
    start = theme.get("start", "corpus")
    if start in starts and starts[start].exists():
        # the story begins from an empty or a small document, not the corpus
        name, path = start, starts[start]
    forms = theme.get("forms", [CAT.FORM_MODEL, CAT.FORM_GESTURE_FORM])
    verbs = catalog.verbs(*forms)
    verbs = [v for v in verbs if v not in ("undo", "redo")]
    weights = [coverage.verb_weight(v, catalog.find(v).targets) for v in verbs]
    chosen = []
    while len(chosen) < rng.randint(MIN_OPERATIONS, MAX_OPERATIONS) and verbs:
        v = rng.choices(verbs, weights=weights)[0]
        i = verbs.index(v)
        verbs.pop(i)
        weights.pop(i)
        chosen.append(v)
    chosen += ["undo", "redo"] if rng.random() < 0.5 else []
    untried = [(v, k) for v, k in coverage.untried(catalog) if v in chosen]
    rng.shuffle(untried)
    untried = ["%s on a %s" % (v, k) for v, k in untried[:MAX_UNTRIED]]
    return Mission(COMBINE, seed, name, path, operations=chosen, theme=theme,
                   budget=tuple(theme["budget"]), untried=untried)
