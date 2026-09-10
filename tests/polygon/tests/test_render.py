# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the render check tests
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

import tempfile
import unittest
from pathlib import Path

from polygon import dump as D
from polygon import oracles
from polygon import render
from polygon import runner
from tests.helpers import CONFIG, DIAGRAMS, ENV, GOOD, SCRIPTS, needs_editor


class PngTest(unittest.TestCase):
    def test_every_good_render_decodes(self):
        files = sorted(GOOD.glob("*-render.png"))
        self.assertTrue(files)
        for path in files:
            with self.subTest(path.name):
                image = render.decode_png(path)
                self.assertGreater(image.width, 0)
                self.assertGreater(image.height, 0)
                self.assertEqual(len(image.rows), image.height)
                # the margin around the diagram is white
                self.assertFalse(image.ink(1, 1))
                self.assertFalse(image.ink(image.width - 2, image.height - 2))

    def test_geometry_render_matches_its_dump(self):
        dump = D.parse_dump((GOOD / "geometry-output.txt").read_text())
        image = render.decode_png(GOOD / "geometry-render.png")
        result = render.check(dump, image, CONFIG.probe_px, CONFIG.ink_min)
        self.assertEqual(result.frame_error, "")
        self.assertEqual(result.unpainted, [])
        self.assertEqual(result.outside, [])
        # an element the scene does not paint is found
        ghost = D.SceneItem(kind=D.KIND_SIMPLE, id="ghost", pos=(0, 0), rect=(0, 0, 100, 60), depth=1)
        ghost.parent = dump.scene[0]
        dump.scene[0].children.append(ghost)
        result = render.check(dump, image, CONFIG.probe_px, CONFIG.ink_min)
        self.assertEqual([item.id for item, _ in result.unpainted], ["ghost"])

    def test_paths(self):
        self.assertTrue(all(len(p) == 2 for p in render.border_path((0, 0, 100, 50))))
        self.assertEqual(len(render.diamond_path((0, 0, 40, 40))), 4 * 21)


@needs_editor
class RenderOracleTest(unittest.TestCase):
    def test_edited_diagram_is_painted(self):
        with tempfile.TemporaryDirectory() as tmp:
            round_ = oracles.Round(ENV, CONFIG, DIAGRAMS / "hierarchy.graphml", tmp)
            result = round_.evaluate((SCRIPTS / "add-elements.script").read_text(), 0, "", render.oracle)
            self.assertEqual([(f.kind, f.signature) for f in result.findings], [])
            self.assertTrue((Path(tmp) / "export.png").exists())


if __name__ == "__main__":
    unittest.main()
