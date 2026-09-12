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

    # --- the model verbs ---------------------------------------------------

    def gen_new_node(self, verb, doc):
        parent = self.pick(self.containers(doc))
        if parent is None:
            return None
        if verb == "new-state":
            args = (self.rect_args() + " " if self.rng.random() < 0.7 else "") + self.fresh("S")
        elif verb in ("new-comment", "new-formal-comment"):
            args = "note " + self.fresh("c")
        elif verb == "new-choice":
            args = self.rect_args() if self.rng.random() < 0.7 else ""
        else:
            args = self.point_args() if self.rng.random() < 0.7 else ""
        return ["%s %s %s" % (verb, parent.id, args)], short_kind(parent)

    def gen_new_sm(self, doc):
        return ["new-sm %s %s" % (self.rect_args(), self.fresh("SM"))], "document"

    def gen_label(self, doc):
        t = self.pick(doc.transitions())
        if t is None:
            return None
        if self.rng.random() < 0.3:
            return ["label %s" % t.id], "transition"   # reset to auto
        return ["label %s %s" % (t.id, self.point_args())], "transition"

    def gen_new_transition(self, doc):
        sources = [e for e in doc.walk() if e.is_state or e.kind in (D.KIND_INITIAL, D.KIND_CHOICE)]
        src = self.pick(sources)
        if src is None:
            return None
        machine = self.machine_of(src)
        targets = [e for e in machine.walk() if e.is_state or e.kind in (D.KIND_FINAL, D.KIND_CHOICE, D.KIND_TERMINATE)]
        tgt = self.pick(targets)
        if tgt is None:
            return None
        trigger = self.pick(TRIGGERS) if self.rng.random() < 0.7 else ""
        return ["new-transition %s %s %s %s" % (machine.id, src.id, tgt.id, trigger)], short_kind(src)

    def gen_rename(self, doc):
        e = self.pick(doc.states() + doc.machines())
        if e is None:
            return None
        return ["rename %s %s" % (e.id, self.fresh("Name"))], short_kind(e)

    def gen_move(self, doc):
        e = self.pick([e for e in self.nodes(doc) if e.geometry is not None])
        if e is None:
            return None
        g = e.geometry
        dx, dy = self.pick(DELTAS), self.pick(DELTAS)
        if len(g) == 4:
            w = max(40, g[2] + self.pick(DELTAS))
            h = max(40, g[3] + self.pick(DELTAS))
            return ["move %s %d %d %d %d" % (e.id, g[0] + dx, g[1] + dy, w, h)], short_kind(e)
        return ["move %s %d %d" % (e.id, g[0] + dx, g[1] + dy)], short_kind(e)

    def gen_reparent(self, doc):
        e = self.pick(self.nodes(doc))
        if e is None:
            return None
        inside = set(x.id for x in e.walk())
        parents = [c for c in self.containers(doc) if c.id not in inside and c is not e.parent]
        parent = self.pick(parents)
        if parent is None:
            return None
        return ["reparent %s %s" % (e.id, parent.id)], short_kind(e)

    def gen_delete(self, doc):
        e = self.pick(self.nodes(doc) + doc.transitions())
        if e is None:
            return None
        return ["delete %s" % e.id], short_kind(e)

    def gen_new_action(self, doc):
        states = doc.states()
        transitions = [t for t in doc.transitions() if t.action is None]
        e = self.pick(states + transitions)
        if e is None:
            return None
        kind = "transition" if e.kind == D.KIND_TRANSITION else self.pick(("entry", "exit", "transition"))
        return ["new-action %s %s" % (e.id, self.action_text(kind))], short_kind(e)

    def gen_update_action(self, verb, doc):
        candidates = [e for e in doc.states() if e.actions] + [t for t in doc.transitions() if t.action]
        e = self.pick(candidates)
        if e is None:
            return None
        if e.kind == D.KIND_TRANSITION:
            i, kind = 0, "transition"
        else:
            i = self.rng.randrange(len(e.actions))
            kind = e.actions[i].type
        if verb == "delete-action":
            return ["delete-action %s %d" % (e.id, i)], short_kind(e)
        return ["update-action %s %d %s" % (e.id, i, self.action_text(kind))], short_kind(e)

    def gen_update_comment(self, doc):
        e = self.pick([e for e in doc.elements(*D.COMMENT_KINDS) if not e.is_meta])
        if e is None:
            return None
        return ["update-comment %s %s" % (e.id, self.fresh("body "))], short_kind(e)

    def gen_update_meta(self, doc):
        key, value = self.pick(META)
        return ["update-meta %s %s" % (key, value)], "document"

    def gen_update_id(self, doc):
        e = self.pick(self.nodes(doc) + doc.transitions())
        if e is None:
            return None
        return ["update-id %s %s" % (e.id, self.fresh("id"))], short_kind(e)

    def gen_polyline(self, doc):
        t = self.pick(doc.transitions())
        if t is None:
            return None
        n = self.rng.randint(0, 3)
        points = " ".join(self.point_args() for _ in range(n))
        return [("polyline %s %s" % (t.id, points)).rstrip()], "transition"

    def gen_new_subject(self, doc):
        c = self.pick([e for e in doc.elements(*D.COMMENT_KINDS) if not e.is_meta])
        target = self.pick([e for e in self.nodes(doc) + doc.transitions() if e is not c])
        if c is None or target is None:
            return None
        form = self.pick(("", "name", "data"))
        extra = "" if not form else " %s %s" % (form, (target.name or target.body or "x")[:4] or "x")
        return ["new-subject %s %s%s" % (c.id, target.id, extra)], short_kind(c)

    def gen_delete_subject(self, doc):
        c = self.pick([e for e in doc.elements(*D.COMMENT_KINDS) if e.subjects and not e.is_meta])
        if c is None:
            return None
        return ["delete-subject %s %d" % (c.id, self.rng.randrange(len(c.subjects)))], short_kind(c)

    def gen_undo_redo(self, verb, dump):
        stack = dump.stack
        if stack is None:
            return None
        if verb == "undo" and stack.index == 0:
            return None
        if verb == "redo" and stack.index >= stack.count:
            return None
        return [verb], "document"

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

    # --- the draw ------------------------------------------------------------

    def generate(self, verb, dump):
        doc = dump.document
        if verb in ("new-state", "new-initial", "new-final", "new-comment",
                    "new-formal-comment", "new-choice", "new-terminate"):
            return self.gen_new_node(verb, doc)
        if verb == "new-sm":
            return self.gen_new_sm(doc)
        if verb == "label":
            return self.gen_label(doc)
        if verb == "new-transition":
            return self.gen_new_transition(doc)
        if verb == "rename":
            return self.gen_rename(doc)
        if verb == "move":
            return self.gen_move(doc)
        if verb == "reparent":
            return self.gen_reparent(doc)
        if verb == "delete":
            return self.gen_delete(doc)
        if verb == "new-action":
            return self.gen_new_action(doc)
        if verb in ("update-action", "delete-action"):
            return self.gen_update_action(verb, doc)
        if verb == "update-comment":
            return self.gen_update_comment(doc)
        if verb == "update-meta":
            return self.gen_update_meta(doc)
        if verb == "update-id":
            return self.gen_update_id(doc)
        if verb == "polyline":
            return self.gen_polyline(doc)
        if verb == "new-subject":
            return self.gen_new_subject(doc)
        if verb == "delete-subject":
            return self.gen_delete_subject(doc)
        if verb in ("undo", "redo"):
            return self.gen_undo_redo(verb, dump)
        if verb in ("edit-title", "edit-action", "edit-label", "edit-body"):
            return self.gen_text(verb, dump)
        if verb in ("add-point", "move-point", "remove-point", "move-endpoint"):
            return self.gen_edge(verb, dump)
        return self.gen_gesture(verb, dump)

    def next(self, dump, exclude=()):
        """(lines, verb, kind) for the next round, or None when nothing
        applies; exclude holds the verbs refused in this round already."""
        forms = [CAT.FORM_MODEL, CAT.FORM_GESTURE_FORM] if self.gestures else [CAT.FORM_MODEL]
        ops = [op for op in self.catalog.by_form(*forms) if op.verb not in exclude]
        for _ in range(30):
            if not ops:
                return None
            weights = [self.coverage.verb_weight(op.verb, op.targets) for op in ops]
            op = self.rng.choices(ops, weights=weights)[0]
            result = self.generate(op.verb, dump)
            if result is not None:
                lines, kind = result
                return lines, op.verb, kind
            ops.remove(op)
        return None
