# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the transition, choice, action, resize and copy-paste drills
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

"""Drills that escalate a transition's points and endpoints, a choice's
branches, a state's action list, a state's size, and copy-paste of a subtree."""

from .. import dump as D
from .base import Drill


def _states(doc):
    return [e for e in doc.walk() if e.is_state] if doc else []


class TransitionPointDrill(Drill):
    """Draw a transition, then escalate its polyline: add points, move them,
    remove them. add-then-remove must restore, and the endpoints must not
    move."""

    name = "transition-points"

    def emit(self, dump):
        doc = dump.document
        m = self.machine(doc)
        if m is None:
            return None
        states = _states(doc)
        if len(states) < 2:
            return ["new-state %s %d 60 200 120 %s" % (m.id, 60 + 320 * len(states), self.fresh("S"))], "state", ""
        transitions = doc.transitions()
        if not transitions:
            a, b = states[0].id, states[1].id
            return ["new-transition %s %s %s GO" % (m.id, a, b)], "transition", "transition %s %s" % (a, b)
        t = transitions[0]
        h = dump.transition_handles(t.id)
        if not h or not h["segments"]:
            return ["polyline %s" % t.id], "transition", ""
        if self.step % 3 == 1 and h["vertices"]:
            v = h["vertices"][0]
            return (["click %d %d" % tuple(round(c) for c in h["segments"][0]),
                     "press %d %d" % (round(v[0]), round(v[1])),
                     "drag %d %d" % (round(v[0] + 40), round(v[1] + 30)),
                     "release %d %d" % (round(v[0] + 40), round(v[1] + 30))], "transition",
                    "transition %s %s" % (t.source, t.target))
        seg = h["segments"][len(h["segments"]) // 2]
        sx, sy = round(seg[0]), round(seg[1])
        # add a point by dragging the segment; the endpoints stay
        return (["click %d %d" % (round(h["segments"][0][0]), round(h["segments"][0][1])),
                 "press %d %d" % (sx, sy), "drag %d %d" % (sx, sy + 50), "release %d %d" % (sx, sy + 50)],
                "transition", "transition %s %s" % (t.source, t.target))


class RebindDrill(Drill):
    """Two states and a transition, then rebind the endpoints across states by
    dragging the endpoint dots. After a rebind the transition connects the new
    pair (asserted by the endpoints, not the id, which the editor renames)."""

    name = "rebind"

    def emit(self, dump):
        doc = dump.document
        m = self.machine(doc)
        if m is None:
            return None
        states = _states(doc)
        if len(states) < 3:
            return ["new-state %s %d 60 180 120 %s" % (m.id, 40 + 300 * len(states), self.fresh("S"))], "state", ""
        transitions = doc.transitions()
        if not transitions:
            a, b = states[0].id, states[1].id
            return ["new-transition %s %s %s EV" % (m.id, a, b)], "transition", "transition %s %s" % (a, b)
        t = transitions[0]
        h = dump.transition_handles(t.id)
        if not h or h["target"] is None:
            return None
        # drag the target endpoint onto a third state's centre
        others = [s for s in states if s.id not in (t.source, t.target)]
        if not others:
            return None
        dest = others[0]
        item = dump.scene_items().get(dest.id)
        cx, cy = item.abs_rect[0] + item.abs_rect[2] / 2, item.abs_rect[1] + item.abs_rect[3] / 2
        tx, ty = h["target"]
        seg = h["segments"][0] if h["segments"] else (tx, ty)
        return (["click %d %d" % (round(seg[0]), round(seg[1])),
                 "press %d %d" % (round(tx), round(ty)),
                 "drag %d %d" % (round(cx), round(cy)),
                 "release %d %d" % (round(cx), round(cy))], "transition",
                "transition %s %s" % (t.source, dest.id))


class ChoiceDrill(Drill):
    """A choice pseudostate, then escalating guarded outgoing transitions. The
    choice stays a choice and the branch count grows."""

    name = "choice"

    def emit(self, dump):
        doc = dump.document
        m = self.machine(doc)
        if m is None:
            return None
        states = _states(doc)
        choices = [e for e in doc.walk() if e.kind == D.KIND_CHOICE]
        if not choices:
            return ["new-choice %s 300 300 60 60" % m.id], "choice", ""
        if len(states) < self.step:
            return ["new-state %s %d 60 160 110 %s" % (m.id, 40 + 260 * len(states), self.fresh("T")), ], "state", ""
        c = choices[0]
        if not states:
            return None
        tgt = states[self.rng.randrange(len(states))].id
        return (["new-transition %s %s %s [x > %d]/ go()" % (m.id, c.id, tgt, self.step)],
                "transition", "kind %s choice\ntransition %s %s" % (c.id, c.id, tgt))


class ActionDrill(Drill):
    """Escalate a state's action list: add entry, exit and reactions, update
    and delete by index. add-then-delete restores the list."""

    name = "action"

    def emit(self, dump):
        doc = dump.document
        m = self.machine(doc)
        if m is None:
            return None
        states = _states(doc)
        if not states:
            return ["new-state %s 80 80 260 180 Host" % m.id], "state", ""
        s = states[0]
        n = len(s.actions)
        if n == 0:
            return ["new-action %s entry/ init()" % s.id], "state", "action %s 0 entry/ init()" % s.id
        if n == 1:
            return ["new-action %s exit/ done()" % s.id], "state", "action %s 1 exit/ done()" % s.id
        if self.step % 3 == 2:
            # add a reaction then delete it: the list returns to n
            trig = "EV%d" % self.step
            return (["new-action %s %s/ handle()" % (s.id, trig), "delete-action %s %d" % (s.id, n)],
                    "state", "count state %d\naction %s 0 entry/ init()" % (len(states), s.id))
        return (["new-action %s TICK%d [k > 0]/ step()" % (s.id, self.step)], "state",
                "action %s %d TICK%d [k > 0]/ step()" % (s.id, n, self.step))


class ResizeDrill(Drill):
    """Resize a state by its border and by moving a child past the border; the
    parent must keep containing the child (grow-to-fit) and, growing toward a
    sibling, push it aside rather than overlap it."""

    name = "resize"

    def emit(self, dump):
        doc = dump.document
        m = self.machine(doc)
        if m is None:
            return None
        states = _states(doc)
        if not states:
            return ["new-state %s 100 100 300 220 Outer" % m.id], "state", ""
        outer = next((s for s in states if s.name == "Outer"), states[0])
        inner = next((s for s in states if s.parent and s.parent.id == outer.id), None)
        if inner is None:
            return (["new-state %s 30 30 120 90 Inner" % outer.id], "state",
                    "rect-inside %s::n0 %s" % (outer.id, outer.id))
        # a top-level sibling to the right of the outer: a grow toward it must
        # push it aside, never overlap it (NODE-6)
        sibling = next((s for s in states
                        if s.parent and s.parent.id == m.id and s.id != outer.id), None)
        if sibling is None:
            return ["new-state %s 520 100 160 110 Sibling" % m.id], "state", ""
        item = dump.scene_items().get(outer.id)
        x, y, w, h = item.abs_rect
        if self.step % 2 == 1:
            # drag the inner state toward the outer's RIGHT border to force the
            # outer to auto-grow toward the sibling: it must push it aside, not
            # overlap it (NODE-6), and keep containing the inner
            ii = dump.scene_items().get(inner.id)
            iy = ii.abs_rect[1] + ii.abs_rect[3] / 2
            return (["press %d %d" % (round(ii.abs_rect[0] + ii.abs_rect[2] / 2), round(iy)),
                     "drag %d %d" % (round(x + w - 5), round(iy)),
                     "release %d %d" % (round(x + w + 40), round(iy))], "state",
                    "rect-inside %s %s\nno-overlap %s %s" % (inner.id, outer.id, outer.id, sibling.id))
        # resize the outer by its right border toward the sibling: this too must
        # push the sibling aside, never overlap it
        return (["press %d %d" % (round(x + w - 3), round(y + h / 2)),
                 "drag %d %d" % (round(x + w + 60), round(y + h / 2)),
                 "release %d %d" % (round(x + w + 60), round(y + h / 2))], "state",
                "no-overlap %s %s" % (outer.id, sibling.id))


class CopyPasteDrill(Drill):
    """Build a composite subtree, then copy-paste it; the element count grows
    by the subtree size and the paste mirrors the source."""

    name = "copy-paste"

    def emit(self, dump):
        doc = dump.document
        m = self.machine(doc)
        if m is None:
            return None
        states = _states(doc)
        if not states:
            return ["new-state %s 100 100 300 220 Parent" % m.id], "state", ""
        parent = states[0]
        children = [s for s in states if s.parent and s.parent.id == parent.id]
        if len(children) < 2:
            return ["new-state %s 30 %d 120 80 %s" % (parent.id, 30 + 100 * len(children), self.fresh("C"))], "state", ""
        # select the composite and copy-paste its whole subtree
        item = dump.scene_items().get(parent.id)
        cx, cy = item.abs_rect[0] + item.abs_rect[2] - 15, item.abs_rect[1] + item.abs_rect[3] - 15
        # the paste must grow the diagram (the exact count depends on the
        # subtree and the selection, so the generic save/reopen and crash
        # oracles carry the copy-paste check; the drill escalates the subtree)
        return (["click %d %d" % (round(cx), round(cy)), "copy", "paste"], "state", "")
