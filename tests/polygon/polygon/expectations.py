# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the expectation language
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

"""The facts an agent states about the result, evaluated on the parsed dump."""

from . import dump as D


class Fact:
    def __init__(self, text):
        self.text = text.strip()
        self.tokens = self.text.split()

    @property
    def verb(self):
        return self.tokens[0] if self.tokens else ""


def parse(text):
    """One fact per non-empty, non-comment line."""
    facts = []
    for line in text.splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            facts.append(Fact(line))
    return facts


def _rect_inside(a, b):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return ax >= bx and ay >= by and ax + aw <= bx + bw and ay + ah <= by + bh


def _rects_overlap(a, b):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah


def _kind(word):
    if word == "state":
        return D.STATE_KINDS
    kind = D.SHORT_KINDS.get(word)
    return (kind,) if kind else None


def check(fact, dump):
    """None when the fact holds, else the reason."""
    t = fact.tokens
    doc = dump.document
    items = dump.scene_items()
    verb = fact.verb
    if verb == "state":
        # state <name...> parent <id>
        if len(t) < 4 or t[-2] != "parent":
            return "usage: state <name> parent <id>"
        name, parent = " ".join(t[1:-2]), t[-1]
        found = [e for e in doc.states() if e.name == name]
        if not found:
            return "no state named %r" % name
        if not any(e.parent is not None and e.parent.id == parent for e in found):
            return "state %r is under %s, not %s" % (
                name, ", ".join(e.parent.id if e.parent else "?" for e in found), parent)
        return None
    if verb == "kind":
        if len(t) != 3:
            return "usage: kind <id> <kind>"
        e = doc.find(t[1])
        kinds = _kind(t[2])
        if e is None:
            return "no element %s" % t[1]
        if kinds is None:
            return "unknown kind %r" % t[2]
        return None if e.kind in kinds else "%s is a %s" % (t[1], e.kind.lower())
    if verb == "count":
        if len(t) != 3:
            return "usage: count <kind> <n>"
        kinds = _kind(t[1])
        if kinds is None:
            return "unknown kind %r" % t[1]
        n = len(doc.elements(*kinds))
        return None if str(n) == t[2] else "%d %s elements, not %s" % (n, t[1], t[2])
    if verb == "transition":
        if len(t) != 3:
            return "usage: transition <src> <tgt>"
        if any(tr.source == t[1] and tr.target == t[2] for tr in doc.transitions()):
            return None
        return "no transition %s -> %s" % (t[1], t[2])
    if verb == "action":
        if len(t) < 4:
            return "usage: action <id> <i> <text>"
        e = doc.find(t[1])
        if e is None:
            return "no element %s" % t[1]
        try:
            i = int(t[2])
        except ValueError:
            return "the action index must be a number"
        actions = e.actions if e.kind != D.KIND_TRANSITION else ([e.action] if e.action else [])
        if i < 0 or i >= len(actions):
            return "%s has %d actions" % (t[1], len(actions))
        expected = " ".join(t[3:])
        got = actions[i].notation()
        return None if " ".join(got.split()) == " ".join(expected.split()) else "action %d is %r" % (i, got)
    if verb == "absent":
        if len(t) != 2:
            return "usage: absent <id>"
        return None if doc.find(t[1]) is None else "%s exists" % t[1]
    if verb == "exists":
        if len(t) != 2:
            return "usage: exists <id>"
        return None if doc.find(t[1]) is not None else "no element %s" % t[1]
    if verb in ("rect-inside", "no-overlap"):
        if len(t) != 3:
            return "usage: %s <id> <id>" % verb
        a, b = items.get(t[1]), items.get(t[2])
        if a is None or b is None:
            return "no scene item %s" % (t[1] if a is None else t[2])
        if verb == "rect-inside":
            return None if _rect_inside(a.abs_rect, b.abs_rect) else "%s is not inside %s" % (t[1], t[2])
        return None if not _rects_overlap(a.abs_rect, b.abs_rect) else "%s overlaps %s" % (t[1], t[2])
    if verb == "undo-depth":
        if len(t) != 2:
            return "usage: undo-depth <n>"
        if dump.stack is None:
            return "no stack section in the dump"
        return None if str(dump.stack.index) == t[1] else "the undo index is %d" % dump.stack.index
    return "unknown fact %r" % verb


def evaluate(text, dump):
    """[(fact text, reason)] of the facts that do not hold."""
    failures = []
    if dump.document is None:
        return [(f.text, "no document in the dump") for f in parse(text)]
    for fact in parse(text):
        reason = check(fact, dump)
        if reason is not None:
            failures.append((fact.text, reason))
    return failures
