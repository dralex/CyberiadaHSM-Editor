# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the dump parser
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

"""The parser of the batch dump: the document tree, the scene items and the
undo stack; the description of a document in words; the structural diff."""

import re
from dataclasses import dataclass, field

KIND_DOCUMENT = "Document"
KIND_SM = "State Machine"
KIND_SIMPLE = "Simple State"
KIND_COMPOSITE = "Composite State"
KIND_COMMENT = "Comment"
KIND_FORMAL = "Formal Comment"
KIND_INITIAL = "Initial"
KIND_FINAL = "Final"
KIND_CHOICE = "Choice"
KIND_TERMINATE = "Terminate"
KIND_TRANSITION = "Transition"

STATE_KINDS = (KIND_SIMPLE, KIND_COMPOSITE)
VERTEX_KINDS = (KIND_INITIAL, KIND_FINAL, KIND_CHOICE, KIND_TERMINATE)
COMMENT_KINDS = (KIND_COMMENT, KIND_FORMAL)
NODE_KINDS = STATE_KINDS + VERTEX_KINDS + COMMENT_KINDS
META_COMMENT = "CGML_META"

# the short kind names of the expectation language and the catalog
SHORT_KINDS = {
    "simple": KIND_SIMPLE, "composite": KIND_COMPOSITE,
    "initial": KIND_INITIAL, "final": KIND_FINAL, "choice": KIND_CHOICE,
    "terminate": KIND_TERMINATE, "comment": KIND_COMMENT,
    "formal-comment": KIND_FORMAL, "transition": KIND_TRANSITION,
    "sm": KIND_SM,
}

WORDS = {
    KIND_SIMPLE: "simple state", KIND_COMPOSITE: "composite state",
    KIND_INITIAL: "initial pseudostate", KIND_FINAL: "final state",
    KIND_CHOICE: "choice pseudostate", KIND_TERMINATE: "terminate pseudostate",
    KIND_COMMENT: "comment", KIND_FORMAL: "formal comment",
}

ACTION_ENTRY = "entry"
ACTION_EXIT = "exit"
ACTION_TRANSITION = "transition"


class DumpError(Exception):
    pass


class _Reader:
    """A recursive descent reader of the libcyberiadamlpp stream:
    Kind: {key: value, key: 'text', key: (1; 2), key: [ (1; 2) ], a {...}, {...}}"""

    DELIMITERS = ",})]"

    def __init__(self, text):
        self.s = text
        self.i = 0

    def peek(self):
        return self.s[self.i] if self.i < len(self.s) else ""

    def skip_ws(self):
        while self.i < len(self.s) and self.s[self.i] in " \t\r\n":
            self.i += 1

    def value(self):
        self.skip_ws()
        c = self.peek()
        if c == "'":
            return self.string()
        if c == "{":
            return self.obj()
        if c == "[":
            return self.list()
        if c == "(":
            return self.tuple()
        return self.bare()

    def string(self):
        self.i += 1
        start = self.i
        while True:
            j = self.s.find("'", self.i)
            if j < 0:
                raise DumpError("unterminated string at %d" % start)
            # the closing quote is the one followed by a delimiter
            k = j + 1
            while k < len(self.s) and self.s[k] in " \t\r\n":
                k += 1
            if k >= len(self.s) or self.s[k] in self.DELIMITERS:
                self.i = j + 1
                return self.s[start:j]
            self.i = j + 1

    def bare(self):
        start = self.i
        while self.i < len(self.s) and self.s[self.i] not in self.DELIMITERS:
            self.i += 1
        return self.s[start:self.i].strip()

    def tuple(self):
        self.i += 1
        j = self.s.find(")", self.i)
        if j < 0:
            raise DumpError("unterminated tuple at %d" % self.i)
        parts = [p.strip() for p in self.s[self.i:j].split(";")]
        self.i = j + 1
        return tuple(float(p) for p in parts if p)

    def list(self):
        self.i += 1
        items = []
        while True:
            self.skip_ws()
            if self.peek() == "]":
                self.i += 1
                return items
            if self.peek() == "":
                raise DumpError("unterminated list")
            items.append(self.value())
            self.skip_ws()
            if self.peek() == ",":
                self.i += 1

    def obj(self):
        self.i += 1
        entries = []
        while True:
            self.skip_ws()
            c = self.peek()
            if c == "}":
                self.i += 1
                return entries
            if c == "":
                raise DumpError("unterminated object")
            if c == "{":
                entries.append((None, self.obj()))
            else:
                start = self.i
                while self.i < len(self.s) and self.s[self.i] not in ":{,}":
                    self.i += 1
                key = self.s[start:self.i].strip()
                c = self.peek()
                if c == ":":
                    self.i += 1
                    entries.append((key, self.value()))
                elif c == "{":
                    entries.append((key, self.obj()))
                else:
                    entries.append((key, True))
            self.skip_ws()
            if self.peek() == ",":
                self.i += 1


def _get(entries, key, default=None):
    for k, v in entries:
        if k == key:
            return v
    return default


@dataclass
class Action:
    type: str
    trigger: str = ""
    guard: str = ""
    behavior: str = ""

    def notation(self, escape=False):
        """The CyberiadaML text of the action; escape=True writes an embedded
        newline as the script does (\\n)."""
        if self.type == ACTION_TRANSITION:
            head = self.trigger
            if self.guard:
                head += " [%s]" % self.guard
        else:
            head = self.type
        text = ("%s/ %s" % (head, self.behavior)).rstrip()
        return text.replace("\n", "\\n") if escape else text

    @classmethod
    def from_entries(cls, entries, transition=False):
        if _get(entries, "entry") is True:
            type_ = ACTION_ENTRY
        elif _get(entries, "exit") is True:
            type_ = ACTION_EXIT
        else:
            type_ = ACTION_TRANSITION
        return cls(type_, _get(entries, "trigger", ""), _get(entries, "guard", ""),
                   _get(entries, "behavior", ""))


@dataclass
class Element:
    kind: str
    id: str
    name: str = ""
    body: str = ""
    actions: list = field(default_factory=list)
    geometry: tuple = None
    region: tuple = None
    subjects: list = field(default_factory=list)
    children: list = field(default_factory=list)
    parent: object = field(default=None, repr=False)
    # transition fields
    ttype: str = ""
    source: str = ""
    target: str = ""
    action: Action = None
    polyline: list = field(default_factory=list)

    def walk(self):
        yield self
        for child in self.children:
            yield from child.walk()

    @property
    def is_state(self):
        return self.kind in STATE_KINDS

    @property
    def is_vertex(self):
        return self.kind in VERTEX_KINDS

    @property
    def is_meta(self):
        return self.kind == KIND_FORMAL and self.name == META_COMMENT

    def path(self):
        chain = []
        node = self
        while node is not None:
            chain.append(node)
            node = node.parent
        return list(reversed(chain))


def _build(kind, entries, parent=None):
    e = Element(kind=kind, id=_get(entries, "id", ""), parent=parent)
    e.name = _get(entries, "name", "")
    e.body = _get(entries, "body", "")
    for key, value in entries:
        if key == "actions":
            e.actions = [Action.from_entries(v) for k, v in value if k == "a"]
        elif key == "geometry" and isinstance(value, tuple):
            e.geometry = value
        elif key == "region" and isinstance(value, tuple):
            e.region = value
        elif key == "subjects":
            e.subjects = [(_get(v, "type", ""), _get(v, "to", ""), _get(v, "fragment", ""))
                          for k, v in value if k is None]
        elif key == "elements":
            e.children = [_build(k, v, e) for k, v in value if k is not None]
        elif key == "type" and isinstance(value, str):
            e.ttype = value
        elif key == "source":
            e.source = value
        elif key == "target":
            e.target = value
        elif key == "action" and isinstance(value, list):
            e.action = Action.from_entries(value, transition=True)
        elif key == "polyline" and isinstance(value, list):
            e.polyline = value
    return e


@dataclass
class Document:
    root: Element
    file: str = ""
    format: str = ""
    meta: list = field(default_factory=list)

    def walk(self):
        return self.root.walk()

    def by_id(self):
        return {e.id: e for e in self.walk() if e.kind != KIND_DOCUMENT}

    def find(self, id_):
        return self.by_id().get(id_)

    def machines(self):
        return [e for e in self.root.children if e.kind == KIND_SM]

    def elements(self, *kinds):
        return [e for e in self.walk() if e.kind in kinds]

    def transitions(self):
        return self.elements(KIND_TRANSITION)

    def states(self):
        return self.elements(*STATE_KINDS)


def parse_document(text):
    """The `== document` section: one LocalDocument stream."""
    reader = _Reader(text.strip())
    key = reader.bare() if False else None
    # the stream starts with 'LocalDocument: {'
    head, _, rest = reader.s.partition(": ")
    if head.strip() != "LocalDocument":
        raise DumpError("not a LocalDocument dump")
    reader = _Reader(rest)
    entries = reader.value()
    if not isinstance(entries, list):
        raise DumpError("malformed LocalDocument")
    doc_entries = _get(entries, KIND_DOCUMENT)
    if doc_entries is None:
        raise DumpError("no Document in the dump")
    root = _build(KIND_DOCUMENT, doc_entries)
    meta = _get(doc_entries, "meta", [])
    return Document(root=root, file=_get(entries, "file", ""),
                    format=_get(entries, "format", ""),
                    meta=meta if isinstance(meta, list) else [])


SCENE_LINE = re.compile(
    r"^( *)(.+?): \{id: '(.*?)', pos: \(([^;]+); ([^)]+)\), "
    r"rect: \(([^;]+); ([^;]+); ([^;]+); ([^)]+)\)\}$")


@dataclass
class SceneItem:
    kind: str
    id: str
    pos: tuple
    rect: tuple
    depth: int
    parent: object = field(default=None, repr=False)
    children: list = field(default_factory=list)

    @property
    def origin(self):
        """The scene position of the item's coordinate origin."""
        x, y = self.pos
        if self.parent is not None:
            px, py = self.parent.origin
            x, y = x + px, y + py
        return (x, y)

    @property
    def abs_rect(self):
        ox, oy = self.origin
        x, y, w, h = self.rect
        return (ox + x, oy + y, w, h)

    def walk(self):
        yield self
        for child in self.children:
            yield from child.walk()


def parse_scene(text):
    """The `== scene` section: the item tree with absolute geometry."""
    roots = []
    stack = []
    for line in text.splitlines():
        if not line.strip():
            continue
        match = SCENE_LINE.match(line)
        if not match:
            raise DumpError("malformed scene line: %s" % line)
        indent, kind, id_, px, py, x, y, w, h = match.groups()
        depth = len(indent) // 2
        item = SceneItem(kind=kind, id=id_, pos=(float(px), float(py)),
                         rect=(float(x), float(y), float(w), float(h)), depth=depth)
        while stack and stack[-1].depth >= depth:
            stack.pop()
        if stack:
            item.parent = stack[-1]
            stack[-1].children.append(item)
        else:
            roots.append(item)
        stack.append(item)
    return roots


@dataclass
class Stack:
    count: int = 0
    index: int = 0
    clean: bool = True


# the roles the == text section uses, mapped to the fact role words
TEXT_ROLE = {"title": "title", "action": "action", "transition": "label",
             "comment": "body", "formal comment": "body"}

TEXT_LINE = re.compile(
    r"^ *(.+?): \{id: '(.*?)', role: (.+?), font: '(.*?)' (\d+)( bold)?, "
    r"pos: \(([^;]+); ([^)]+)\), size: \(([^;]+); ([^)]+)\), text: '(.*)'\}$")


@dataclass
class TextItem:
    kind: str
    id: str
    role: str          # the raw dump role
    family: str
    points: int
    bold: bool
    pos: tuple         # local to the element origin, as the scene item
    size: tuple
    text: str          # the escaped one-line form (\n for a newline)

    @property
    def fact_role(self):
        return TEXT_ROLE.get(self.role, self.role)

    def plain(self):
        return self.text.replace("\\n", "\n").replace("\\\\", "\\")


def parse_text(text):
    items = []
    for line in text.splitlines():
        if not line.strip():
            continue
        m = TEXT_LINE.match(line)
        if not m:
            raise DumpError("malformed text line: %s" % line)
        kind, id_, role, family, points, bold, px, py, w, h, body = m.groups()
        items.append(TextItem(kind=kind.strip(), id=id_, role=role, family=family,
                              points=int(points), bold=bool(bold),
                              pos=(float(px), float(py)), size=(float(w), float(h)), text=body))
    return items


def parse_stack(text):
    stack = Stack()
    for line in text.splitlines():
        key, _, value = line.partition(":")
        value = value.strip()
        if key == "count":
            stack.count = int(value)
        elif key == "index":
            stack.index = int(value)
        elif key == "clean":
            stack.clean = value == "yes"
    return stack


def sections(text):
    """The dump sections by name: {'document': ..., 'scene': ..., 'stack': ...}."""
    result = {}
    name = None
    lines = []
    for line in text.splitlines():
        if line.startswith("== "):
            if name is not None:
                result[name] = "\n".join(lines)
            name = line[3:].strip()
            lines = []
        elif name is not None:
            lines.append(line)
    if name is not None:
        result[name] = "\n".join(lines)
    return result


def fields(document):
    """One comparable record per element, in document order: [(field, value)]
    with the identity first; the meta first of all, without the geometry
    declaration the save writes. The file identity plays no part."""
    out = [("meta", "meta", [(k, v) for k, v in document.meta if k != "geometry"])]
    for e in document.walk():
        if e.kind == KIND_DOCUMENT:
            continue
        record = [("kind", e.kind), ("id", e.id), ("name", e.name)]
        if e.kind in COMMENT_KINDS and not e.is_meta:
            record.append(("body", e.body))
        record.append(("actions", [a.notation() for a in e.actions]))
        record.append(("geometry", e.geometry))
        record.append(("region", e.region))
        record.append(("subjects", list(e.subjects)))
        if e.kind == KIND_TRANSITION:
            record.append(("type", e.ttype))
            record.append(("source", e.source))
            record.append(("target", e.target))
            record.append(("action", e.action.notation() if e.action else None))
            record.append(("polyline", list(e.polyline)))
        out.append((e.kind, e.id, record))
    return out


def record_text(record):
    kind, id_, pairs = record
    return "%s %s " % (kind, id_) + " ".join("%s=%r" % (k, v) for k, v in pairs
                                             if v not in (None, [], "") or k in ("kind", "id"))


@dataclass
class Difference:
    where: str      # the element kind (or 'meta', 'scene', 'count')
    field: str      # the differing field
    expected: str
    got: str

    @property
    def tag(self):
        """The signature part: kind and field, no instance data."""
        return "%s:%s" % (self.where.lower().replace(" ", "-"), self.field)


def compare(a_text, b_text, ignore=()):
    """The first difference of two dumps by the document structure and
    geometry, then by the scene section; None when they agree. ignore holds
    the tags (kind:field) of the fields the format does not preserve."""
    da, db = parse_dump(a_text), parse_dump(b_text)
    if da.document is None or db.document is None:
        return Difference("document", "missing", "a document", "no document")
    fa, fb = fields(da.document), fields(db.document)
    for ra, rb in zip(fa, fb):
        if ra == rb:
            continue
        if ra[0] == "meta":
            return Difference("meta", "meta", str(ra[2]), str(rb[2]))
        if ra[0] != rb[0] or ra[1] != rb[1]:
            return Difference(ra[0], "identity", record_text(ra), record_text(rb))
        skipped = False
        for (ka, va), (kb, vb) in zip(ra[2], rb[2]):
            if va != vb:
                d = Difference(ra[0], ka, record_text(ra), record_text(rb))
                if d.tag in ignore:
                    skipped = True
                    continue
                return d
        if skipped:
            continue
        return Difference(ra[0], "record", record_text(ra), record_text(rb))
    if len(fa) != len(fb):
        extra = fa[len(fb)] if len(fa) > len(fb) else fb[len(fa)]
        return Difference("count", "elements", "%d elements" % len(fa),
                          "%d elements (%s)" % (len(fb), record_text(extra)))
    sa = sections(a_text).get("scene", "").splitlines()
    sb = sections(b_text).get("scene", "").splitlines()
    for x, y in zip(sa, sb):
        if x != y:
            kind = x.strip().split(":")[0] if x.strip() else "item"
            return Difference("scene " + kind, "item", x.strip(), y.strip())
    if len(sa) != len(sb):
        return Difference("scene", "count", "%d items" % len(sa), "%d items" % len(sb))
    return None


@dataclass
class Dump:
    document: Document = None
    scene: list = field(default_factory=list)
    stack: Stack = None
    texts: list = field(default_factory=list)
    text: str = ""

    def scene_items(self):
        return {item.id: item for root in self.scene for item in root.walk()}

    def text_of(self, id_, role):
        """The visible text of an element by fact role (title|action|label|body),
        or None. For actions the first one; add an index to text_items."""
        for t in self.texts:
            if t.id == id_ and t.fact_role == role:
                return t
        return None

    def abs_box(self, text_item):
        """The absolute rect of a text item: its element's scene origin + pos."""
        item = self.scene_items().get(text_item.id)
        if item is None:
            return None
        ox, oy = item.origin
        return (ox + text_item.pos[0], oy + text_item.pos[1], text_item.size[0], text_item.size[1])


def parse_dump(text):
    parts = sections(text)
    dump = Dump(text=text)
    if "document" in parts:
        dump.document = parse_document(parts["document"])
    if "scene" in parts:
        dump.scene = parse_scene(parts["scene"])
    if "stack" in parts:
        dump.stack = parse_stack(parts["stack"])
    if "text" in parts:
        dump.texts = parse_text(parts["text"])
    return dump


# --- the description in words ------------------------------------------------

def _label(element, machine):
    """How the brief names an endpoint: a state by its name, a vertex by its
    kind with an ordinal among the vertices of that kind in the machine."""
    if element is None:
        return "?"
    if element.is_state or element.kind == KIND_SM:
        return '"%s"' % element.name
    if element.kind in COMMENT_KINDS:
        return "the comment"
    same = [e for e in machine.walk() if e.kind == element.kind]
    word = WORDS.get(element.kind, element.kind.lower())
    if len(same) <= 1:
        return "the " + word
    return "%s %d" % (word, same.index(element) + 1)


def _describe_element(e, machine, depth, lines):
    indent = "  " * depth + "- "
    if e.is_meta:
        return
    if e.kind in COMMENT_KINDS:
        head = 'a %s "%s"' % (WORDS[e.kind], e.body.replace("\n", " / "))
        if e.subjects:
            targets = [_label(machine_find(machine, to), machine) for _, to, _ in e.subjects]
            head += ", pointing at " + ", ".join(targets)
        lines.append(indent + head)
        return
    if e.is_vertex:
        lines.append(indent + "an " + WORDS[e.kind] if WORDS[e.kind][0] in "aeiou"
                     else indent + "a " + WORDS[e.kind])
        return
    head = '%s "%s"' % (WORDS.get(e.kind, e.kind.lower()), e.name)
    if e.actions:
        head += "; " + "; ".join(a.notation(escape=True) for a in e.actions)
    if e.children:
        head += "; it holds:"
    lines.append(indent + head)
    for child in e.children:
        if child.kind != KIND_TRANSITION:
            _describe_element(child, machine, depth + 2, lines)


def machine_find(machine, id_):
    for e in machine.walk():
        if e.id == id_:
            return e
    return None


def _plural(n, word, plural=None):
    return "%d %s" % (n, word if n == 1 else (plural or word + "s"))


def describe(document):
    """The structure of the document in words, without ids and geometry: the
    brief of a reproduction mission and the structure part of the feedback."""
    lines = []
    for machine in document.machines():
        states = [e for e in machine.walk() if e.is_state]
        transitions = [e for e in machine.walk() if e.kind == KIND_TRANSITION]
        counts = [_plural(len(states), "state")]
        for kind in VERTEX_KINDS:
            n = len([e for e in machine.walk() if e.kind == kind])
            if n:
                counts.append(_plural(n, WORDS[kind]))
        comments = [e for e in machine.walk() if e.kind in COMMENT_KINDS and not e.is_meta]
        if comments:
            counts.append(_plural(len(comments), "comment"))
        counts.append(_plural(len(transitions), "transition"))
        lines.append('State machine "%s": %s.' % (machine.name, ", ".join(counts)))
        lines.append("Elements, in order:")
        for e in machine.children:
            if e.kind != KIND_TRANSITION:
                _describe_element(e, machine, 1, lines)
        if transitions:
            lines.append("Transitions:")
            for t in transitions:
                src = _label(machine_find(machine, t.source), machine)
                tgt = _label(machine_find(machine, t.target), machine)
                text = "  - %s -> %s" % (src, tgt)
                if t.action is not None:
                    text += ": " + t.action.notation(escape=True)
                lines.append(text)
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


# --- the structural diff -----------------------------------------------------

def _shape(e, machine):
    """The structural identity of an element: kind, name, actions, subjects."""
    actions = tuple(a.notation() for a in e.actions)
    if e.kind in COMMENT_KINDS:
        return (e.kind, e.body, tuple((t, _label(machine_find(machine, to), machine), f)
                                       for t, to, f in e.subjects))
    if e.is_vertex:
        return (e.kind, "", ())
    return (e.kind, e.name, actions)


def _diff_children(a, b, machine_a, machine_b, where, out):
    ka = [c for c in a.children if c.kind != KIND_TRANSITION and not c.is_meta]
    kb = [c for c in b.children if c.kind != KIND_TRANSITION and not c.is_meta]
    sa = [_shape(c, machine_a) for c in ka]
    sb = [_shape(c, machine_b) for c in kb]
    for shape in sa:
        if shape not in sb:
            out.append("%s: expected %s %r" % (where, shape[0].lower(), shape[1]))
    for shape in sb:
        if shape not in sa:
            out.append("%s: unexpected %s %r" % (where, shape[0].lower(), shape[1]))
    for ca in ka:
        for cb in kb:
            if _shape(ca, machine_a) == _shape(cb, machine_b):
                _diff_children(ca, cb, machine_a, machine_b, where + "/" + (ca.name or ca.kind), out)
                break


def _transition_shape(t, machine):
    src = _label(machine_find(machine, t.source), machine)
    tgt = _label(machine_find(machine, t.target), machine)
    return (src, tgt, t.action.notation() if t.action else "")


def structural_diff(expected, actual):
    """The differences of two documents by structure: the state machines, the
    element tree by kind, name and actions, the transitions by endpoints and
    action. Ids and geometry play no part. Empty when they agree."""
    out = []
    ma, mb = expected.machines(), actual.machines()
    if len(ma) != len(mb):
        out.append("expected %d state machines, got %d" % (len(ma), len(mb)))
    for a, b in zip(ma, mb):
        if a.name != b.name:
            out.append("state machine: expected name %r, got %r" % (a.name, b.name))
        _diff_children(a, b, a, b, a.name or "sm", out)
        ta = [_transition_shape(t, a) for t in a.walk() if t.kind == KIND_TRANSITION]
        tb = [_transition_shape(t, b) for t in b.walk() if t.kind == KIND_TRANSITION]
        for shape in ta:
            if shape not in tb:
                out.append("transition expected: %s -> %s %s" % shape)
            else:
                tb.remove(shape)
        for shape in tb:
            out.append("transition unexpected: %s -> %s %s" % shape)
    return out
