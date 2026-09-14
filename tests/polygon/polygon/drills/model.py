# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the drill shadow model
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

"""An abstract structural mirror of the document a drill builds. Every
operation the drill emits is applied to the model too; the model translates
its expected structure into fact lines checked against the editor's dump
(structure exactly, geometry exactly where deterministic, as bounds where
layout-dependent). A divergence is a registrable invariant violation."""

from .. import dump as D

PASTE_OFFSET = 20.0


class MElement:
    def __init__(self, kind, name, parent=None, rect=None, exact=False):
        self.kind = kind            # dump kind constant
        self.name = name            # unique within the model (states/comments)
        self.parent = parent        # MElement or None (top under the machine)
        self.children = []
        self.actions = []           # action notation strings
        self.rect = rect            # (x, y, w, h) absolute, or None
        self.exact = exact          # the rect is a deterministic prediction
        self.dump_id = None         # the dump id when grounded by import_dump


class MTransition:
    def __init__(self, source, target):
        self.source = source        # MElement
        self.target = target        # MElement


class Model:
    """The expected structure. Elements are keyed by name; transitions by their
    endpoint elements. Ids are never predicted - resolve() reads them from the
    dump by name and nesting."""

    def __init__(self):
        self.machine_name = None
        self.elements = []          # MElement, top-level and nested
        self.transitions = []

    # --- mutation (called in lock-step with the emitted script) -----------

    def set_machine(self, name):
        self.machine_name = name

    def add(self, kind, name, parent=None, rect=None, exact=False):
        e = MElement(kind, name, parent, rect, exact)
        if parent is not None:
            parent.children.append(e)
        self.elements.append(e)
        return e

    def reparent(self, e, parent):
        if e.parent is not None:
            e.parent.children.remove(e)
        e.parent = parent
        if parent is not None:
            parent.children.append(e)
        # a reparent preserves the absolute position; the layout re-bases it,
        # so the rect becomes a bound, not an exact prediction
        e.exact = False

    def add_transition(self, source, target):
        t = MTransition(source, target)
        self.transitions.append(t)
        return t

    def rebind(self, t, source=None, target=None):
        if source is not None:
            t.source = source
        if target is not None:
            t.target = target

    def remove(self, e):
        # drop the element, its descendants and their incident transitions
        drop = set()
        stack = [e]
        while stack:
            x = stack.pop()
            drop.add(x)
            stack.extend(x.children)
        self.elements = [x for x in self.elements if x not in drop]
        if e.parent is not None and e in e.parent.children:
            e.parent.children.remove(e)
        self.transitions = [t for t in self.transitions
                            if t.source not in drop and t.target not in drop]

    def import_dump(self, dump):
        """Re-ground the model to the current dump: rebuild every state, vertex
        and transition by structure. The compound drill grounds each round from
        reality, then predicts one operation's delta on top."""
        self.elements = []
        self.transitions = []
        doc = dump.document
        if doc is None:
            return
        ms = doc.machines()
        self.machine_name = ms[0].name if ms else None
        by_id = {}
        for e in doc.walk():
            if e.kind not in D.STATE_KINDS and e.kind not in D.VERTEX_KINDS:
                continue
            parent = by_id.get(e.parent.id) if e.parent is not None else None
            m = self.add(e.kind, e.name, parent)
            m.actions = [a.notation(escape=True) for a in e.actions]
            m.dump_id = e.id
            by_id[e.id] = m
        for tr in doc.transitions():
            s, t = by_id.get(tr.source), by_id.get(tr.target)
            if s is not None and t is not None:
                self.add_transition(s, t)

    # --- reconciliation with the dump -------------------------------------

    def states(self):
        return [e for e in self.elements if e.kind in D.STATE_KINDS]

    @staticmethod
    def _norm(kind):
        # a state flips Simple<->Composite as it gains or loses children; match
        # both under one token so tracking survives the flip
        return "state" if kind in D.STATE_KINDS else kind

    def resolve(self, dump, e):
        """The current dump id of a model element, matched by kind, name and
        the chain of ancestor names; None when the editor does not have it."""
        want_path = []
        node = e
        while node is not None:
            want_path.append((self._norm(node.kind), node.name))
            node = node.parent
        want_path.reverse()
        if dump.document is None:
            return None
        for cand in dump.document.walk():
            if self._norm(cand.kind) != self._norm(e.kind) or cand.name != e.name:
                continue
            path = []
            n = cand
            while n is not None and n.kind != D.KIND_DOCUMENT and n.kind != D.KIND_SM:
                path.append((self._norm(n.kind), n.name))
                n = n.parent
            path.reverse()
            if path == want_path:
                return cand.id
        return None

    # --- the equivalence facts --------------------------------------------

    def check_facts(self, dump):
        """Fact lines (the existing vocabulary) describing the expected
        structure, resolved against the dump. Empty when the model has nothing
        to assert. oracles.evaluate turns any failing fact into an invariant
        finding."""
        facts = []
        # counts by kind conserve what the model built
        for kind, word in ((D.KIND_INITIAL, "initial"), (D.KIND_FINAL, "final"),
                           (D.KIND_CHOICE, "choice"), (D.KIND_TERMINATE, "terminate")):
            n = len([e for e in self.elements if e.kind == kind])
            facts.append("count %s %d" % (word, n))
        facts.append("count state %d" % len(self.states()))
        facts.append("count transition %d" % len(self.transitions))
        # each state's parent nesting and its actions
        for e in self.states():
            parent_id = self.resolve(dump, e.parent) if e.parent is not None else \
                (dump.document.machines()[0].id if dump.document and dump.document.machines() else None)
            if parent_id is not None:
                facts.append("state %s parent %s" % (e.name, parent_id))
            eid = self.resolve(dump, e)
            if eid is not None:
                for i, a in enumerate(e.actions):
                    facts.append("action %s %d %s" % (eid, i, a))
                # containment bound: the child stays inside its parent
                if e.parent is not None:
                    pid = self.resolve(dump, e.parent)
                    if pid is not None:
                        facts.append("rect-inside %s %s" % (eid, pid))
                # exact geometry where the model predicts it
                if e.rect is not None and e.exact:
                    facts.append("rect %s %g %g %g %g" % (eid, e.rect[0], e.rect[1], e.rect[2], e.rect[3]))
        # transitions by endpoint id
        for t in self.transitions:
            sid, tid = self.resolve(dump, t.source), self.resolve(dump, t.target)
            if sid is not None and tid is not None:
                facts.append("transition %s %s" % (sid, tid))
        return facts
