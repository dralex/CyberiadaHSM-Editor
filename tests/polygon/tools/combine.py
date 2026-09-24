#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the orchestrator-combination diagram generator
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

"""Compose corpus state machines into an orchestrator diagram whose states are
submachine states, each referencing one machine and wired through entry/exit
connection points. Two reference styles: `embedded` inlines the reused machines
as sibling state machines (referenced by internal id); `external` references
them by file name. Child geometry is relative to the state machine, so a machine
is placed by offsetting its own graph rect only."""

import os
import sys
import xml.etree.ElementTree as ET

NS = "http://graphml.graphdrawing.org/xmlns"
ET.register_namespace("", NS)

HERE = os.path.dirname(os.path.abspath(__file__))
CORPUS = os.path.normpath(os.path.join(HERE, "..", "corpus"))

META_BODY = ("standardVersion/ 1.0\n\ngeometry/ full\n\n"
             "transitionOrder/ actionFirst\n\neventPropagation/ block")


def q(tag):
    return "{%s}%s" % (NS, tag)


def corpus_path(name):
    return os.path.join(CORPUS, name + ".graphml")


def _prefix_ids(graph, prefix):
    """Prefix every id in the machine subtree (graph/node ids and edge
    id/source/target). The full id string is prefixed, so the `::` nesting and
    the trailing `:` of region graphs are preserved."""
    graph.set("id", prefix + graph.get("id"))

    def walk(el):
        for child in el:
            if child.tag in (q("graph"), q("node")) and child.get("id") is not None:
                child.set("id", prefix + child.get("id"))
            elif child.tag == q("edge"):
                for attr in ("id", "source", "target"):
                    if child.get(attr) is not None:
                        child.set(attr, prefix + child.get(attr))
            walk(child)

    walk(graph)


def _drop_meta(graph):
    for node in list(graph.findall(q("node"))):
        if node.get("id") == "nMeta":
            graph.remove(node)


def _embed(graph, top):
    """Place the machine so its content's top edge sits at global y=`top`, and
    return its content height. The machine's own SM rect is dropped so every
    machine is content-anchored the same way (a state machine without geometry
    has its top-level children in global coordinates); the children are then
    shifted uniformly in y. X is left as-is (the machines form a vertical
    column). Descendant geometry is relative to its parent, so it follows."""
    for d in list(graph.findall(q("data"))):
        if d.get("key") == "dGeometry":
            graph.remove(d)
    geoms = []
    for node in graph.findall(q("node")):
        for data in node.findall(q("data")):
            if data.get("key") == "dGeometry":
                geoms.append(data.find(q("rect")) if data.find(q("rect")) is not None
                             else data.find(q("point")))
    geoms = [g for g in geoms if g is not None]
    ys = []
    for g in geoms:
        y = float(g.get("y"))
        ys.append(y)
        if g.tag == q("rect"):
            ys.append(y + float(g.get("height")))
    if not ys:
        return 400.0
    shift = top - min(ys)
    for g in geoms:
        g.set("y", "%f" % (float(g.get("y")) + shift))
    return max(ys) - min(ys)


def _add_points(graph, prefix, top, height):
    """The embedded machine gets a top-level entry point `in` and exit point
    `out`: the orchestrator's connectors bind to them by name (PNST 1044 8.1)."""
    for suffix, kind, x in (("in", "entryPoint", -60.0), ("out", "exitPoint", -60.0)):
        node = ET.SubElement(graph, q("node"), {"id": prefix + suffix})
        _data(node, "dVertex", kind)
        _data(node, "dName", suffix)
        y = top if suffix == "in" else top + height
        _point(node, x, y)


def _distinct_name(graph, mname, others):
    """A document holds no two machines with one name (PNST 1044 6.1.2)."""
    name = None
    for d in graph.findall(q("data")):
        if d.get("key") == "dName":
            name = d
    taken = {dd.text for g in others for dd in g.findall(q("data")) if dd.get("key") == "dName"}
    if name is not None and name.text in taken:
        name.text = "%s (%s)" % (name.text, mname)


# --- orchestrator construction -------------------------------------------

def _data(parent, key, text=None):
    d = ET.SubElement(parent, q("data"), {"key": key})
    if text is not None:
        d.text = text
    return d


def _rect(parent, x, y, w, h):
    d = ET.SubElement(parent, q("data"), {"key": "dGeometry"})
    ET.SubElement(d, q("rect"), {"x": str(x), "y": str(y),
                                 "width": str(w), "height": str(h)})


def _point(parent, x, y):
    d = ET.SubElement(parent, q("data"), {"key": "dGeometry"})
    ET.SubElement(d, q("point"), {"x": str(x), "y": str(y)})


def _orchestrator(name, refs):
    """The orchestrator machine: an initial, one submachine state per reference
    (each with an entry+exit connector), a final, and the pipeline transitions
    initial -> sub0 entry, sub_i exit -> sub_{i+1} entry, last exit -> final."""
    g = ET.Element(q("graph"), {"id": "M0", "edgedefault": "directed"})
    _data(g, "dStateMachine")
    _data(g, "dName", name)
    _rect(g, 0, 0, 260 + 320 * len(refs), 360)

    meta = ET.SubElement(g, q("node"), {"id": "nMeta"})
    _data(meta, "dNote", "formal")
    _data(meta, "dName", "CGML_META")
    _data(meta, "dData", META_BODY)

    init = ET.SubElement(g, q("node"), {"id": "oinit"})
    _data(init, "dVertex", "initial")
    _point(init, 40, 180)

    prev_exit = "oinit"
    for i, (ref, label) in enumerate(refs):
        sid = "os%d" % i
        sub = ET.SubElement(g, q("node"), {"id": sid})
        _data(sub, "dSubmachineState", ref)
        _data(sub, "dName", "Run " + label)
        _rect(sub, 120 + 320 * i, 100, 220, 160)
        region = ET.SubElement(sub, q("graph"), {"id": sid + ":",
                                                 "edgedefault": "directed"})
        en = ET.SubElement(region, q("node"), {"id": sid + "::en"})
        _data(en, "dVertex", "entryPoint")
        _data(en, "dName", "in")
        _point(en, 0, 80)
        ex = ET.SubElement(region, q("node"), {"id": sid + "::ex"})
        _data(ex, "dVertex", "exitPoint")
        _data(ex, "dName", "out")
        _point(ex, 220, 80)
        edge = ET.SubElement(g, q("edge"),
                             {"id": "e%d" % i, "source": prev_exit,
                              "target": sid + "::en"})
        if prev_exit != "oinit":
            _data(edge, "dData", "DONE/")
        prev_exit = sid + "::ex"

    fin = ET.SubElement(g, q("node"), {"id": "ofin"})
    _data(fin, "dVertex", "final")
    _point(fin, 120 + 320 * len(refs), 180)
    last = ET.SubElement(g, q("edge"),
                         {"id": "e%d" % len(refs), "source": prev_exit,
                          "target": "ofin"})
    _data(last, "dData", "DONE/")
    return g


def combine(name, machine_names, style):
    """Return the combination graphml as a string. `style` is 'embedded' or
    'external'."""
    template = ET.parse(corpus_path(machine_names[0])).getroot()
    for g in list(template.findall(q("graph"))):
        template.remove(g)

    refs = []
    machines = []
    offy = 500.0
    for i, mname in enumerate(machine_names):
        if style == "external":
            refs.append((mname + ".graphml", mname))
            continue
        g = ET.parse(corpus_path(mname)).getroot().find(q("graph"))
        _drop_meta(g)
        _prefix_ids(g, "R%d_" % i)
        height = _embed(g, offy)
        _add_points(g, "R%d_" % i, offy, height)
        _distinct_name(g, mname, [m for m in machines])
        offy += height + 400.0
        refs.append((g.get("id"), mname))
        machines.append(g)

    template.append(_orchestrator(name, refs))
    for g in machines:
        template.append(g)

    ET.indent(template, space="  ")
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + \
        ET.tostring(template, encoding="unicode")


SCENARIOS = {
    "orchestrate-robot": (["vacuum-robot", "maze-solver", "turtle-square"], "embedded"),
    "orchestrate-traffic": (["semaphore", "semaphore-hierarchy"], "embedded"),
    "orchestrate-home": (["microwave", "simple-hoover"], "embedded"),
    "orchestrate-grand": (["vacuum-robot", "maze-solver", "turtle-square", "dog"], "embedded"),
    "orchestrate-robot-ext": (["vacuum-robot", "maze-solver", "turtle-square"], "external"),
    "orchestrate-home-ext": (["microwave", "simple-hoover"], "external"),
}


def main(argv):
    if len(argv) >= 2 and argv[1] in SCENARIOS:
        machines, style = SCENARIOS[argv[1]]
        sys.stdout.write(combine(argv[1], machines, style))
        return 0
    if len(argv) >= 4:
        name, style = argv[1], argv[2]
        sys.stdout.write(combine(name, argv[3:], style))
        return 0
    sys.stderr.write("usage: combine.py <scenario> | <name> <embedded|external> "
                     "<machine>...\nscenarios: %s\n" % ", ".join(sorted(SCENARIOS)))
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
