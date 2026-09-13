# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the fuzzer
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

"""The random producer: one valid command, or one gesture group, per round,
drawn from the catalog against the last dump with the coverage weights."""

import random

from . import catalog as CAT
from . import dump as D

BODY_INSET = 20          # the free body area of a state, from its bottom right
BORDER_INSET = 3         # the resize zone, inside the border
MIN_GESTURE_SIZE = 60    # a state smaller than this is not dragged or resized
RECT_SIZES = (80, 120, 160, 200, 300)
DELTAS = (-80, -40, -15, 15, 40, 80)
TRIGGERS = ("EV", "TICK", "DONE", "CALL", "STOP")
BEHAVIOURS = ("f()", "g(x)", "reset()", "count += 1")
GUARDS = ("", "", "x > 0", "ready")
# the texts an edit writes: names, code-like behaviours, multi-line, edges
NAMES = ("Idle", "Running", "Waiting state", "S2", "Готово")
BODIES = ("f()", "red++", "motor_up();\\nlamp_on()", "x = x + 1; y = 0", "note")
LABELS = ("EV/ f()", "TICK [x > 0]/ g()", "DONE/", "/ init()")
META = (("transitionOrder", "exitFirst"), ("transitionOrder", "actionFirst"),
        ("eventPropagation", "propagate"), ("author", "fuzzer"), ("target", "none"))

SHORT = {v: k for k, v in D.SHORT_KINDS.items()}
SHORT[D.KIND_SM] = "sm"


def short_kind(element):
    return SHORT.get(element.kind, element.kind.lower())


class Fuzzer:
    def __init__(self, catalog, coverage, seed, gestures=True):
        self.catalog = catalog
        self.coverage = coverage
        self.rng = random.Random(seed)
        self.gestures = gestures
        self.counter = 0
        self.previous = None

    # --- helpers -----------------------------------------------------------

    def fresh(self, prefix):
        self.counter += 1
        return "%s%d" % (prefix, self.counter)

    def pick(self, items):
        return self.rng.choice(items) if items else None

    @staticmethod
    def containers(doc):
        return [e for e in doc.walk() if e.kind == D.KIND_SM or e.is_state]

    @staticmethod
    def nodes(doc):
        return [e for e in doc.walk() if e.kind in D.NODE_KINDS and not e.is_meta]

    @staticmethod
    def machine_of(element):
        for e in element.path():
            if e.kind == D.KIND_SM:
                return e
        return None

    @staticmethod
    def sized_states(dump):
        out = []
        for item in dump.scene_items().values():
            if item.kind in D.STATE_KINDS and item.rect[2] >= MIN_GESTURE_SIZE and item.rect[3] >= MIN_GESTURE_SIZE:
                out.append(item)
        return out

    def rect_args(self):
        w = self.pick(RECT_SIZES)
        h = self.pick(RECT_SIZES[:3])
        return "%d %d %d %d" % (self.rng.randint(-200, 600), self.rng.randint(-200, 400), w, h)

    def point_args(self):
        return "%d %d" % (self.rng.randint(-200, 600), self.rng.randint(-200, 400))

    def action_text(self, kind):
        if kind == "entry":
            return "entry/ " + self.pick(BEHAVIOURS)
        if kind == "exit":
            return "exit/ " + self.pick(BEHAVIOURS)
        guard = self.pick(GUARDS)
        head = self.pick(TRIGGERS) + (" [%s]" % guard if guard else "")
        return "%s/ %s" % (head, self.pick(BEHAVIOURS))

    # --- container geometry from the scene dump -------------------------

    def container_items(self, dump):
        """Scene items that can hold a new element: state machines and states."""
        return [i for i in dump.scene_items().values()
                if i.kind in (D.KIND_SM,) + D.STATE_KINDS and i.rect[2] > 40 and i.rect[3] > 40]

    def inside_point(self, item):
        """A point comfortably inside a scene item's rect."""
        x, y, w, h = item.abs_rect
        return (round(x + w * self.rng.uniform(0.3, 0.7)), round(y + h * self.rng.uniform(0.3, 0.7)))

    def free_point(self, dump):
        """A point away from the existing elements, for a new top-level machine."""
        items = list(dump.scene_items().values())
        for _ in range(20):
            p = (self.rng.randint(-100, 900), self.rng.randint(-100, 700))
            if all(not self._inside(p, i.abs_rect) for i in items):
                return p
        return (self.rng.randint(600, 1000), self.rng.randint(400, 700))

    @staticmethod
    def _inside(p, rect):
        x, y, w, h = rect
        return x <= p[0] <= x + w and y <= p[1] <= y + h

    # --- the tool emitters (creation by the reworked tools) ----------------

    def emit_creation(self, tool, dump):
        """`tool <name>` then a click or a rect draw at a valid point; the tool
        reverts to select on release. Returns (lines, kind)."""
        if tool.family == CAT.FAMILY_RECT and tool.name == "new-sm":
            x, y = self.free_point(dump)
            w, h = self.pick(RECT_SIZES), self.pick(RECT_SIZES[:3])
            return (["tool new-sm", "press %d %d" % (x, y),
                     "drag %d %d" % (x + w, y + h), "release %d %d" % (x + w, y + h)], "sm")
        container = self.pick(self.container_items(dump))
        if container is None:
            return None
        x, y = self.inside_point(container)
        if tool.family == CAT.FAMILY_RECT:            # new-state
            w, h = self.pick((80, 120, 160)), self.pick((60, 80, 100))
            if self.rng.random() < 0.3:
                return (["tool new-state", "click %d %d" % (x, y)], tool.element)
            return (["tool new-state", "press %d %d" % (x, y),
                     "drag %d %d" % (x + w, y + h), "release %d %d" % (x + w, y + h)], tool.element)
        return (["tool %s" % tool.name, "click %d %d" % (x, y)], tool.element)   # place tool

    def emit_clipboard(self, dump, cut=False):
        """Select a state by a click on its body, copy or cut it, then paste."""
        states = self.sized_states(dump)
        if not states:
            return None
        item = self.pick(states)
        x, y, w, h = item.abs_rect
        bx, by = round(x + w - BODY_INSET), round(y + h - BODY_INSET)
        verb = "cut" if cut else "copy"
        return (["click %d %d" % (bx, by), verb, "paste"], "state")

    def emit_transition(self, dump):
        """`tool transition` then press a source state and drag to a target."""
        states = self.sized_states(dump)
        if not states:
            return None
        src = self.pick(states)
        sx, sy, sw, sh = src.abs_rect
        press = (round(sx + sw - BODY_INSET), round(sy + sh - BODY_INSET))
        tgt = self.pick(states)
        tx, ty, tw, th = tgt.abs_rect
        to = (round(tx + tw / 2.0), round(ty + th / 2.0))
        return (["tool transition", "press %d %d" % press,
                 "drag %d %d" % to, "release %d %d" % to], "transition")

    # --- the edge forms ----------------------------------------------------

    def transitions_with_handles(self, dump):
        out = []
        for t in dump.document.transitions() if dump.document else []:
            h = dump.transition_handles(t.id)
            if h and h["segments"]:
                out.append((t, h))
        return out

    def gen_edge(self, verb, dump):
        options = self.transitions_with_handles(dump)
        if verb in ("move-point", "remove-point"):
            options = [(t, h) for t, h in options if h["vertices"]]
        pick = self.pick(options)
        if pick is None:
            return None
        t, h = pick
        sel = self.pick(h["segments"])          # a point on the line, to select it
        select = "click %d %d" % (round(sel[0]), round(sel[1]))
        if verb == "add-point":
            seg = self.pick(h["segments"])
            dx, dy = self.pick(DELTAS), self.pick(DELTAS)
            return [select, "press %d %d" % (round(seg[0]), round(seg[1])),
                    "drag %d %d" % (round(seg[0] + dx), round(seg[1] + dy)),
                    "release %d %d" % (round(seg[0] + dx), round(seg[1] + dy))], "transition"
        if verb == "move-point":
            v = self.pick(h["vertices"])
            dx, dy = self.pick(DELTAS), self.pick(DELTAS)
            return [select, "press %d %d" % (round(v[0]), round(v[1])),
                    "drag %d %d" % (round(v[0] + dx), round(v[1] + dy)),
                    "release %d %d" % (round(v[0] + dx), round(v[1] + dy))], "transition"
        if verb == "remove-point":
            v = self.pick(h["vertices"])
            return [select, "click %d %d" % (round(v[0]), round(v[1])), "key delete"], "transition"
        if verb == "move-endpoint":
            end = self.pick([e for e in (h["source"], h["target"]) if e])
            if end is None:
                return None
            mods = " ctrl" if self.rng.random() < 0.4 else ""
            if self.rng.random() < 0.5:
                # reattach: drag toward a random node centre
                nodes = [i for i in dump.scene_items().values() if i.kind in D.STATE_KINDS]
                node = self.pick(nodes)
                if node is None:
                    return None
                x, y, w, hh = node.abs_rect
                to = (x + w / 2.0, y + hh / 2.0)
            else:
                to = (end[0] + self.pick(DELTAS), end[1] + self.pick(DELTAS))
            return [select, "press %d %d%s" % (round(end[0]), round(end[1]), mods),
                    "drag %d %d" % (round(to[0]), round(to[1])),
                    "release %d %d" % (round(to[0]), round(to[1]))], "transition"
        return None

    # --- the text forms ----------------------------------------------------

    def gen_text(self, verb, dump):
        want = {"edit-title": "title", "edit-action": "action",
                "edit-label": "label", "edit-body": "body"}[verb]
        options = [t for t in dump.texts if t.fact_role == want]
        t = self.pick(options)
        if t is None:
            return None
        if want == "title":
            value = self.pick(NAMES)
        elif want == "label":
            value = self.pick(LABELS)
        else:
            value = self.pick(BODIES)
        role = "action 0" if want == "action" else want
        return ["edit-text %s %s %s" % (t.id, role, value)], "transition" if want == "label" else "state"

    # --- the gesture forms -------------------------------------------------

    def gen_gesture(self, verb, dump):
        item = self.pick(self.sized_states(dump))
        if item is None:
            return None
        x, y, w, h = item.abs_rect
        bx, by = x + w - BODY_INSET, y + h - BODY_INSET
        kind = SHORT[item.kind]
        if verb == "drag-state":
            dx, dy = self.pick(DELTAS), self.pick(DELTAS)
            return ["press %d %d" % (bx, by), "drag %d %d" % (bx + dx, by + dy),
                    "release %d %d" % (bx + dx, by + dy)], kind
        if verb == "resize-state":
            rx, ry = x + w - BORDER_INSET, y + h / 2
            dx = self.pick(DELTAS)
            return ["press %d %d" % (rx, ry), "drag %d %d" % (rx + dx, ry),
                    "release %d %d" % (rx + dx, ry)], kind
        if verb == "draw-loop":
            return ["tool transition", "press %d %d" % (bx, by),
                    "drag %d %d" % (bx - 30, by - 20), "release %d %d" % (bx - 30, by - 20),
                    "tool select"], kind
        if verb == "click-delete":
            return ["click %d %d" % (bx, by), "delete-selected"], kind
        if verb == "double-click-action":
            return ["double-click %d %d" % (bx, by)], kind
        return None

    def gen_undo_redo(self, verb, dump):
        stack = dump.stack
        if stack is None:
            return None
        if verb == "undo" and stack.index == 0:
            return None
        if verb == "redo" and stack.index >= stack.count:
            return None
        return [verb], "document"

    # --- the lean tool-driven draw -----------------------------------------

    MANIPULATIONS = ("drag-state", "resize-state", "click-delete", "double-click-action",
                     "edit-title", "edit-action", "edit-label", "edit-body",
                     "add-point", "move-point", "remove-point", "move-endpoint",
                     "copy-paste", "cut-paste", "undo", "redo")

    def actions(self):
        """The names the fuzzer chooses among: the creation tools, the
        transition draw, and the select-tool manipulations. gestures_only
        (the explore burst) restricts to the manipulations."""
        if getattr(self, "gestures_only", False):
            return list(self.MANIPULATIONS)
        names = [t.name for t in CAT.creation_tools()] + ["draw-transition"]
        return names + list(self.MANIPULATIONS)

    def generate(self, name, dump):
        if name == "draw-transition":
            return self.emit_transition(dump)
        tool = next((t for t in CAT.creation_tools() if t.name == name), None)
        if tool is not None:
            return self.emit_creation(tool, dump)
        if name in ("edit-title", "edit-action", "edit-label", "edit-body"):
            return self.gen_text(name, dump)
        if name in ("add-point", "move-point", "remove-point", "move-endpoint"):
            return self.gen_edge(name, dump)
        if name in ("undo", "redo"):
            return self.gen_undo_redo(name, dump)
        if name == "copy-paste":
            return self.emit_clipboard(dump, cut=False)
        if name == "cut-paste":
            return self.emit_clipboard(dump, cut=True)
        return self.gen_gesture(name, dump)

    def kind_of(self, name):
        tool = next((t for t in CAT.creation_tools() if t.name == name), None)
        return tool.element if tool else ("transition" if "point" in name or name == "draw-transition"
                                          or name == "edit-label" else "state")

    def next(self, dump, exclude=()):
        """(lines, verb, kind) for the next round: a tool or a manipulation,
        weighted by the coverage store; None when nothing applies."""
        names = [n for n in self.actions() if n not in exclude]
        for _ in range(40):
            if not names:
                return None
            weights = [self.coverage.weight(n, self.kind_of(n)) for n in names]
            name = self.rng.choices(names, weights=weights)[0]
            result = self.generate(name, dump)
            if result is not None:
                lines, kind = result
                return lines, name, kind
            names.remove(name)
        return None
