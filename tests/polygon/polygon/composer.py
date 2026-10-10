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

"""A mission from a seed: the start document, the story, the corpus
diagram, the domain, the theme and the budget. The strategy is the
diagram of the mission (diagrams/<kind>.graphml)."""

import json
import random
from dataclasses import dataclass, field
from pathlib import Path

from . import catalog as CAT

HUNT = "hunt"
DRAW = "draw"
STRESS = "stress"
REBUILD = "rebuild"
KINDS = (HUNT, DRAW, STRESS, REBUILD)

DOMAINS = ("a traffic light", "a vending machine", "a lift controller",
           "a microwave oven", "a robot patrol mission", "a game character AI",
           "a turnstile", "a thermostat", "a media player")
BUDGETS = {HUNT: (40, 120), STRESS: (30, 70), REBUILD: (20, 80)}
DIAGRAMS_DIR = Path(__file__).resolve().parents[1] / "diagrams"


@dataclass
class Mission:
    kind: str
    seed: int
    name: str            # the corpus diagram name, the story, or the kind
    diagram: Path        # the start document the session edits
    original: Path = None   # the corpus diagram to rebuild
    theme: dict = None   # a hint seasoning the task (catalog/themes.json)
    budget: tuple = (0, 0)
    domain: str = ""     # the stress mission's design subject
    story: dict = None   # the draw mission's diagram to draw (catalog/stories.json)
    operations: list = field(default_factory=list)   # kept for the session record
    untried: list = field(default_factory=list)

    @property
    def strategy(self):
        """The diagram that drives the mission."""
        return DIAGRAMS_DIR / (self.kind + ".graphml")


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


def stories():
    return json.loads((CAT.CATALOG_DIR / "stories.json").read_text())["stories"]


def compose(kind, env, seed, name=None, theme_name=None, story_name=None):
    rng = random.Random(seed)
    diagrams, starts = corpus(env)
    empty = starts["empty"]
    theme = next((t for t in themes() if t["name"] == theme_name), None) if theme_name else None
    if kind == REBUILD:
        if name is None:
            name, path = rng.choice(diagrams)
        else:
            path = dict(diagrams).get(name) or (env.diagrams / (name + ".graphml"))
        return Mission(kind, seed, name, empty, original=path, budget=BUDGETS[kind], theme=theme)
    if kind == DRAW:
        all_stories = stories()
        story = next((s for s in all_stories if s["name"] == story_name), None) if story_name \
            else rng.choice(all_stories)
        return Mission(kind, seed, "draw-" + story["name"], empty, story=story,
                       budget=tuple(story["budget"]), theme=theme)
    if kind == STRESS:
        return Mission(kind, seed, "stress", empty, domain=rng.choice(DOMAINS),
                       budget=BUDGETS[kind], theme=theme)
    return Mission(HUNT, seed, "hunt", empty, budget=BUDGETS[HUNT], theme=theme)
