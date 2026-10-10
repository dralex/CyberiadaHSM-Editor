# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the verb families
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

import unittest

from polygon import catalog as CAT


class FamilyTest(unittest.TestCase):
    def setUp(self):
        self.cat = CAT.Catalog()

    def test_every_verb_has_one_family(self):
        for op in self.cat.operations:
            self.assertIn(op.family, CAT.FAMILIES, op.verb)
        for family in CAT.FAMILIES:
            self.assertTrue(self.cat.by_family(family), family)
        self.assertEqual(sum(len(self.cat.by_family(f)) for f in CAT.FAMILIES),
                         len(self.cat.operations))

    def test_family_of(self):
        self.assertEqual(self.cat.family_of("new-state"), CAT.FAMILY_CREATE)
        self.assertEqual(self.cat.family_of("move-endpoint"), CAT.FAMILY_CONNECT)
        self.assertEqual(self.cat.family_of("undo"), CAT.FAMILY_HISTORY)
        self.assertEqual(self.cat.family_of("press"), CAT.FAMILY_GESTURE)
        self.assertIsNone(self.cat.family_of("nope"))

    def test_accepts_own_verbs_and_gestures(self):
        self.assertTrue(self.cat.accepts(CAT.FAMILY_CREATE, "new-initial"))
        self.assertTrue(self.cat.accepts(CAT.FAMILY_CREATE, "click"))
        self.assertFalse(self.cat.accepts(CAT.FAMILY_CREATE, "new-transition"))
        self.assertFalse(self.cat.accepts(CAT.FAMILY_GESTURE, "undo"))
        self.assertFalse(self.cat.accepts(CAT.FAMILY_ARRANGE, "nope"))

    def test_family_table(self):
        table = self.cat.table(CAT.FORM_MODEL, family=CAT.FAMILY_CONNECT)
        self.assertIn("`new-transition", table)
        self.assertNotIn("`new-state", table)
        self.assertNotIn("`draw-loop", table)   # a gesture form, not a verb


if __name__ == "__main__":
    unittest.main()
