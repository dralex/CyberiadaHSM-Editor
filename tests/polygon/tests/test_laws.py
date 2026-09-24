# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the standing universal law tests
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

"""The standing laws must be silent on the good corpus (no false positives) and
fire on an injected violation of each requirement."""

import glob
import os
import unittest

from polygon import dump as D
from polygon import laws as L
from tests.helpers import GOOD


def node(kind, id_, name="", parent=None, children=()):
    e = D.Element(kind=kind, id=id_, name=name, parent=parent)
    e.children = list(children)
    for c in e.children:
        c.parent = e
    return e


def wrap(sm_children):
    """A Dump whose document is one state machine G0 holding sm_children."""
    sm = node(D.KIND_SM, "G0", "SM", children=sm_children)
    root = node(D.KIND_DOCUMENT, "", children=[sm])
    d = D.Dump()
    d.document = D.Document(root=root)
    d.scene = []
    return d


def reqs(dump, gated=False):
    return sorted(v.req for v in L.check(dump, gated=gated))


class CorpusTest(unittest.TestCase):
    """Running every hard law over every good dump yields no violation; the
    gated laws flag exactly the known, recorded conflicts (Phase 2)."""

    # the gated laws have all been promoted to hard; the good corpus is clean
    EXPECTED_GATED = set()

    def _corpus(self):
        for path in sorted(glob.glob(str(GOOD / "*-output.txt"))):
            try:
                yield os.path.basename(path), D.parse_dump(open(path).read())
            except D.DumpError:
                continue

    def test_hard_laws_are_silent_on_the_good_corpus(self):
        offenders = [(name, v.req, v.detail)
                     for name, d in self._corpus() for v in L.check(d, gated=False)]
        self.assertEqual(offenders, [], "hard laws must not fire on good files")

    def test_gated_laws_flag_only_the_known_conflicts(self):
        got = {(name, v.req) for name, d in self._corpus()
               for v in L.check(d, gated=True)}
        self.assertEqual(got, self.EXPECTED_GATED)


class LawTest(unittest.TestCase):
    def test_clean_document_passes(self):
        d = wrap([node(D.KIND_SIMPLE, "n0", "A"), node(D.KIND_SIMPLE, "n1", "B")])
        self.assertEqual(L.check(d, gated=True), [])

    def test_unique_ids(self):
        d = wrap([node(D.KIND_SIMPLE, "n0", "A"), node(D.KIND_SIMPLE, "n0", "B")])
        self.assertIn("STRUCT-1", reqs(d))

    def test_composite_without_children(self):
        d = wrap([node(D.KIND_COMPOSITE, "n0", "A")])
        self.assertIn("STRUCT-6", reqs(d))

    def test_simple_with_children(self):
        d = wrap([node(D.KIND_SIMPLE, "n0", "A", children=[node(D.KIND_SIMPLE, "n0::n0", "B")])])
        self.assertIn("STRUCT-6", reqs(d))

    def test_dangling_transition(self):
        t = D.Element(kind=D.KIND_TRANSITION, id="t0", source="n0", target="ghost")
        d = wrap([node(D.KIND_SIMPLE, "n0", "A"), t])
        self.assertIn("STRUCT-5", reqs(d))

    def test_two_initials_one_level(self):
        d = wrap([node(D.KIND_INITIAL, "i0"), node(D.KIND_INITIAL, "i1")])
        self.assertIn("SEM-1", reqs(d))

    def test_bad_endpoint_kind(self):
        t = D.Element(kind=D.KIND_TRANSITION, id="t0", source="c0", target="n0")
        d = wrap([node(D.KIND_COMMENT, "c0"), node(D.KIND_SIMPLE, "n0", "A"), t])
        self.assertIn("SEM-2", reqs(d))     # a comment cannot be a source

    def test_submachine_holds_a_plain_state(self):
        sub = node(D.KIND_SUBMACHINE_STATE, "n0",
                   children=[node(D.KIND_SIMPLE, "n0::n0", "X")])
        d = wrap([sub])
        self.assertIn("SEM-4", reqs(d))     # only entry/exit points are allowed

    def test_submachine_entry_exit_pass(self):
        sub = node(D.KIND_SUBMACHINE_STATE, "n0",
                   children=[node(D.KIND_ENTRY_POINT, "n0::n0"),
                             node(D.KIND_EXIT_POINT, "n0::n1")])
        self.assertNotIn("SEM-4", reqs(wrap([sub])))

    def test_connector_entry_used_as_source(self):
        # an entry connector is a target only; using it as a source breaks SEM-5
        sub = node(D.KIND_SUBMACHINE_STATE, "n0",
                   children=[node(D.KIND_ENTRY_POINT, "n0::n0")])
        t = D.Element(kind=D.KIND_TRANSITION, id="t0", source="n0::n0", target="s0")
        d = wrap([sub, node(D.KIND_SIMPLE, "s0", "A"), t])
        self.assertIn("SEM-5", reqs(d))

    def test_standalone_entry_used_as_source_passes(self):
        # a standalone entry point (in the machine) is a source
        entry = node(D.KIND_ENTRY_POINT, "e0")
        t = D.Element(kind=D.KIND_TRANSITION, id="t0", source="e0", target="s0")
        d = wrap([entry, node(D.KIND_SIMPLE, "s0", "A"), t])
        self.assertNotIn("SEM-5", reqs(d))
        self.assertNotIn("SEM-2", reqs(d))

    def test_cross_machine_transition(self):
        sm1 = node(D.KIND_SM, "G1", children=[node(D.KIND_SIMPLE, "m0", "X")])
        d = wrap([node(D.KIND_SIMPLE, "n0", "A")])
        d.document.root.children.append(sm1)
        sm1.parent = d.document.root
        t = D.Element(kind=D.KIND_TRANSITION, id="t0", source="n0", target="m0")
        d.document.root.children[0].children.append(t)
        self.assertIn("STRUCT-4", reqs(d))

    def test_meta_drawn_on_scene(self):
        meta = D.Element(kind=D.KIND_FORMAL, id="nMeta", name=D.META_COMMENT)
        d = wrap([meta])
        d.scene = [D.SceneItem(kind=D.KIND_FORMAL, id="nMeta", pos=(0, 0),
                               rect=(0, 0, 10, 10), depth=0)]
        self.assertIn("META-1", reqs(d))

    def test_containment_fires_on_a_child_outside_its_parent(self):
        d = D.parse_dump((GOOD / "geometry-output.txt").read_text())
        self.assertNotIn("NODE-1", reqs(d))          # clean to start
        items = d.scene_items()
        child = next(i for i in items.values()
                     if i.parent is not None and i.parent.kind in D.STATE_KINDS
                     and i.kind in D.STATE_KINDS)
        child.pos = (child.pos[0] + 5000, child.pos[1] + 5000)   # shove it out
        self.assertIn("NODE-1", reqs(d))


if __name__ == "__main__":
    unittest.main()


class StandardTextLawTest(unittest.TestCase):
    """The laws added for the final PNST 1044 text (spec 0.7)."""

    def test_points_named(self):
        d = wrap([node(D.KIND_ENTRY_POINT, "p0"), node(D.KIND_EXIT_POINT, "p1", "out")])
        self.assertEqual(reqs(d).count("SEM-7"), 1)

    def test_submachine_reference(self):
        sub = node(D.KIND_SUBMACHINE_STATE, "s0", "Sub",
                   children=[node(D.KIND_ENTRY_POINT, "s0::en", "in")])
        sub.submachine = "G0"
        self.assertIn("SEM-8", reqs(wrap([sub])))
        sub.submachine = "file:other.graphml"
        self.assertNotIn("SEM-8", reqs(wrap([sub])))

    def test_submachine_point_names(self):
        worker = node(D.KIND_SM, "M2", "Worker",
                      children=[node(D.KIND_ENTRY_POINT, "m2en", "start")])
        sub = node(D.KIND_SUBMACHINE_STATE, "s0", "Sub",
                   children=[node(D.KIND_ENTRY_POINT, "s0::en", "in")])
        sub.submachine = "M2"
        d = wrap([sub])
        d.document.root.children.append(worker)
        worker.parent = d.document.root
        self.assertIn("SEM-8", reqs(d))
        sub.children[0].name = "start"
        self.assertNotIn("SEM-8", reqs(d))

    def test_single_behaviour_blocks(self):
        s = node(D.KIND_SIMPLE, "n0", "A")
        s.actions = [D.Action(D.ACTION_ENTRY, behavior="a()"), D.Action(D.ACTION_ENTRY, behavior="b()")]
        self.assertIn("STRUCT-10", reqs(wrap([s])))

    def test_reserved_event_names(self):
        s = node(D.KIND_SIMPLE, "n0", "A")
        s.actions = [D.Action(D.ACTION_TRANSITION, trigger="else", behavior="x()")]
        self.assertIn("TEXT-5", reqs(wrap([s])))
        s.actions = [D.Action(D.ACTION_TRANSITION, trigger="ANY", behavior="x()")]
        self.assertNotIn("TEXT-5", reqs(wrap([s])))

    def test_event_handling(self):
        s = node(D.KIND_SIMPLE, "n0", "A")
        s.actions = [D.Action(D.ACTION_TRANSITION, trigger="TICK", propagation="defer", behavior="x()")]
        self.assertIn("TEXT-6", reqs(wrap([s])))
        s.actions = [D.Action(D.ACTION_TRANSITION, trigger="TICK", propagation="defer")]
        self.assertNotIn("TEXT-6", reqs(wrap([s])))
        t = node(D.KIND_TRANSITION, "t0")
        t.action = D.Action(D.ACTION_TRANSITION, trigger="", propagation="block")
        self.assertIn("TEXT-6", reqs(wrap([t])))

    def test_machine_names(self):
        d = wrap([node(D.KIND_SIMPLE, "n0", "A")])
        second = node(D.KIND_SM, "G1", "SM")
        d.document.root.children.append(second)
        second.parent = d.document.root
        self.assertIn("META-5", reqs(d))

