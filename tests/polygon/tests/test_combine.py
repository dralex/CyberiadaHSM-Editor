# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the orchestrator-combination generator tests
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

"""The combination generator: an embedded document holds the orchestrator plus
one sibling machine per reference (referenced by resolvable id), an external one
holds only the orchestrator (referencing the machines by file name); ids are
unique and every submachine state carries an entry and an exit connector."""

import unittest
import xml.etree.ElementTree as ET

from tools import combine

NS = "{http://graphml.graphdrawing.org/xmlns}"


def _texts(root, key):
    return [d.text for n in root.iter(NS + "node")
            for d in n.findall(NS + "data") if d.get("key") == key]


class CombineTest(unittest.TestCase):
    def test_embedded_machines_and_resolved_refs(self):
        root = ET.fromstring(combine.combine(
            "orchestrate-home", ["microwave", "simple-hoover"], "embedded"))
        graphs = root.findall(NS + "graph")
        ids = [g.get("id") for g in graphs]
        self.assertEqual(ids[0], "M0")            # the orchestrator comes first
        self.assertEqual(len(graphs), 3)          # orchestrator + two machines
        refs = _texts(graphs[0], "dSubmachineState")
        self.assertEqual(len(refs), 2)
        for ref in refs:                          # each reference resolves to a sibling
            self.assertIn(ref, ids)

    def test_connectors_present(self):
        root = ET.fromstring(combine.combine(
            "x", ["semaphore", "semaphore-hierarchy"], "embedded"))
        vtx = _texts(root.findall(NS + "graph")[0], "dVertex")
        self.assertEqual(vtx.count("entryPoint"), 2)
        self.assertEqual(vtx.count("exitPoint"), 2)

    def test_no_id_collision(self):
        # the graphml key declarations reuse ids by design (dName/dGeometry per
        # `for`); only the element ids (graph/node/edge) must be document-unique
        root = ET.fromstring(combine.combine(
            "x", ["semaphore", "semaphore-hierarchy"], "embedded"))
        kinds = (NS + "graph", NS + "node", NS + "edge")
        ids = [e.get("id") for e in root.iter() if e.tag in kinds and e.get("id")]
        self.assertEqual(len(ids), len(set(ids)))

    def test_external_refs_are_file_names(self):
        root = ET.fromstring(combine.combine(
            "x", ["microwave", "simple-hoover"], "external"))
        self.assertEqual(len(root.findall(NS + "graph")), 1)   # orchestrator only
        self.assertEqual(sorted(_texts(root, "dSubmachineState")),
                         ["microwave.graphml", "simple-hoover.graphml"])

    def test_all_scenarios_generate(self):
        for name, (machines, style) in combine.SCENARIOS.items():
            ET.fromstring(combine.combine(name, machines, style))   # parses = well-formed


if __name__ == "__main__":
    unittest.main()
