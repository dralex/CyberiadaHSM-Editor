# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the standing universal laws
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

"""The [U] universal laws of docs/EDITOR-SPEC.md: whole-scene rectangle and graph
predicates that must hold after any operation. Each law reads one parsed dump and
yields the requirement it enforces and the offending detail; oracles.Round runs
them every round in every mode. Reference-free — a property, not a prediction."""

from . import dump as D
from .expectations import RECT_TOLERANCE

TOL = RECT_TOLERANCE


class Violation:
    """A failed law: the EDITOR-SPEC requirement and a human detail with ids."""

    def __init__(self, req, detail):
        self.req = req          # e.g. "NODE-1"
        self.detail = detail    # e.g. "n2 is not inside its parent n0::n0"


def _degenerate(rect):
    return rect is None or rect[2] <= 0 or rect[3] <= 0


def _inside(a, b, tol=TOL):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return (ax >= bx - tol and ay >= by - tol and
            ax + aw <= bx + bw + tol and ay + ah <= by + bh + tol)


def _overlap(a, b, tol=TOL):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return (ax < bx + bw - tol and bx < ax + aw - tol and
            ay < by + bh - tol and by < ay + ah - tol)


def _machine_of(e):
    node = e
    while node is not None and node.kind != D.KIND_SM:
        node = node.parent
    return node


# --- structural laws (the == document tree) -------------------------------

def unique_ids(dump):                                    # EDIT-STRUCT-1
    doc = dump.document
    if doc is None:
        return
    seen = set()
    for e in doc.walk():
        if e.kind == D.KIND_DOCUMENT:
            continue
        if e.id in seen:
            yield Violation("STRUCT-1", "duplicate id %s" % e.id)
        seen.add(e.id)


def no_cycle(dump):                                      # EDIT-STRUCT-3
    doc = dump.document
    if doc is None:
        return
    for e in doc.walk():
        ids = [n.id for n in e.path()]
        if len(ids) != len(set(ids)):
            yield Violation("STRUCT-3", "the ancestor chain of %s repeats an id" % e.id)


def endpoints_same_machine(dump):                        # EDIT-STRUCT-4
    doc = dump.document
    if doc is None:
        return
    for t in doc.transitions():
        s, g = doc.find(t.source), doc.find(t.target)
        if s is None or g is None:
            continue
        ms, mg = _machine_of(s), _machine_of(g)
        if ms is not None and mg is not None and ms.id != mg.id:
            yield Violation("STRUCT-4", "transition %s joins two machines (%s, %s)"
                            % (t.id, ms.id, mg.id))


def composite_by_children(dump):                         # EDIT-STRUCT-6
    doc = dump.document
    if doc is None:
        return
    for e in doc.walk():
        if e.kind == D.KIND_COMPOSITE and not e.children:
            yield Violation("STRUCT-6", "composite state %s has no children" % e.id)
        if e.kind == D.KIND_SIMPLE and e.children:
            yield Violation("STRUCT-6", "simple state %s has children" % e.id)


def no_dangling(dump):                                   # EDIT-STRUCT-5 / 9
    doc = dump.document
    if doc is None:
        return
    ids = set(doc.by_id())
    for t in doc.transitions():
        for end in (t.source, t.target):
            if end and end not in ids:
                yield Violation("STRUCT-5", "transition %s references the missing %s"
                                % (t.id, end))
    for e in doc.walk():
        for _type, to, _frag in e.subjects:
            if to and to not in ids:
                yield Violation("STRUCT-9", "comment %s subject references the missing %s"
                                % (e.id, to))


def one_initial_per_parent(dump):                        # EDIT-SEM-1
    doc = dump.document
    if doc is None:
        return
    from collections import Counter
    counts = Counter()
    for e in doc.walk():
        if e.kind == D.KIND_INITIAL:
            counts[e.parent.id if e.parent else "?"] += 1
    for parent, n in counts.items():
        if n > 1:
            yield Violation("SEM-1", "%d initial pseudostates under %s" % (n, parent))


# a history pseudostate is a valid target, and the source of an optional default
# transition (PNST 984, not required) - so it is legal on either end; a submachine
# state is a state, so it is already a valid source and target through STATE_KINDS
_HISTORY_KINDS = (D.KIND_SHALLOW_HISTORY, D.KIND_DEEP_HISTORY)
_SOURCE_KINDS = D.STATE_KINDS + (D.KIND_INITIAL, D.KIND_CHOICE) + _HISTORY_KINDS
_TARGET_KINDS = D.STATE_KINDS + (D.KIND_FINAL, D.KIND_CHOICE, D.KIND_TERMINATE) + _HISTORY_KINDS


def _connection_role(e):
    """The single legal endpoint role of an entry/exit point (EDIT-SEM-5). A
    connector - a child of a submachine state - mirrors its container: an entry
    is a target, an exit is a source. A standalone point in a state machine is
    reversed. (PNST 984)"""
    connector = e.parent is not None and e.parent.kind == D.KIND_SUBMACHINE_STATE
    is_entry = e.kind == D.KIND_ENTRY_POINT
    return "source" if (is_entry != connector) else "target"


def endpoint_kinds(dump):                                # EDIT-SEM-2 / SEM-5
    doc = dump.document
    if doc is None:
        return
    for t in doc.transitions():
        s, g = doc.find(t.source), doc.find(t.target)
        if s is not None:
            if s.kind in D.CONNECTION_POINT_KINDS:
                if _connection_role(s) != "source":
                    yield Violation("SEM-5", "transition %s starts on %s, a target-only %s"
                                    % (t.id, s.id, D.WORDS.get(s.kind, s.kind.lower())))
            elif s.kind not in _SOURCE_KINDS:
                yield Violation("SEM-2", "transition %s starts on a %s" % (t.id, s.kind.lower()))
        if g is not None:
            if g.kind in D.CONNECTION_POINT_KINDS:
                if _connection_role(g) != "target":
                    yield Violation("SEM-5", "transition %s ends on %s, a source-only %s"
                                    % (t.id, g.id, D.WORDS.get(g.kind, g.kind.lower())))
            elif g.kind not in _TARGET_KINDS:
                yield Violation("SEM-2", "transition %s ends on a %s" % (t.id, g.kind.lower()))


def submachine_children(dump):                           # EDIT-SEM-4
    # a submachine state references another machine; its only own children are
    # the entry/exit connection points
    doc = dump.document
    if doc is None:
        return
    for e in doc.walk():
        if e.kind != D.KIND_SUBMACHINE_STATE:
            continue
        for c in e.children:
            if c.kind not in D.CONNECTION_POINT_KINDS:
                yield Violation("SEM-4", "submachine state %s holds a %s"
                                % (e.id, D.WORDS.get(c.kind, c.kind.lower())))


def meta_hidden(dump):                                   # EDIT-META-1
    doc = dump.document
    if doc is None:
        return
    items = dump.scene_items()
    for e in doc.walk():
        if e.is_meta and e.id in items:
            yield Violation("META-1", "the metainformation node %s is drawn on the scene" % e.id)


# --- geometry laws (the == scene tree) ------------------------------------

def containment(dump):                                   # EDIT-NODE-1
    # a drawable child of a STATE stays inside it; the state-machine frame
    # (NODE-4) is a display-time union of content and is not a containment box
    # in the scene dump, so it is out of scope here; a transition's path may
    # legitimately bow outside a node, so transitions are excluded
    for item in dump.scene_items().values():
        parent = item.parent
        if parent is None or parent.kind not in D.STATE_KINDS:
            continue
        # a submachine connector sits on its container's border (NODE-14/EDGE-2),
        # so it is not a containment box violation; a transition path may bow out
        if (item.kind == D.KIND_TRANSITION or item.kind in D.CONNECTION_POINT_KINDS
                or _degenerate(parent.rect) or _degenerate(item.rect)):
            continue
        if not _inside(item.abs_rect, parent.abs_rect):
            yield Violation("NODE-1", "%s is drawn outside its parent %s" % (item.id, parent.id))


def no_overlap(dump):                                    # EDIT-NODE-6
    by_parent = {}
    for item in dump.scene_items().values():
        if item.kind in D.STATE_KINDS and not _degenerate(item.rect):
            by_parent.setdefault(item.parent.id if item.parent else None, []).append(item)
    for siblings in by_parent.values():
        for i, a in enumerate(siblings):
            for b in siblings[i + 1:]:
                if _overlap(a.abs_rect, b.abs_rect):
                    yield Violation("NODE-6", "siblings %s and %s overlap" % (a.id, b.id))


# --- the registry ---------------------------------------------------------

# hard laws run always-on and register as defects; gated laws are implemented
# and tested but not yet registered (pending an EDITOR-SPEC decision)
HARD = [unique_ids, no_cycle, endpoints_same_machine, composite_by_children,
        no_dangling, one_initial_per_parent, endpoint_kinds, submachine_children,
        meta_hidden, containment, no_overlap]
GATED = []


def check(dump, gated=False):
    """The violations of the standing laws on one dump. A law must never crash
    the oracle, so each is guarded."""
    out = []
    for law in HARD + (GATED if gated else []):
        try:
            out.extend(law(dump))
        except Exception as e:               # a broken law degrades, never aborts
            out.append(Violation("LAW", "%s raised %s" % (law.__name__, e)))
    return out
