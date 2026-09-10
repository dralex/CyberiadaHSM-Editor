# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the briefs
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

"""The brief of a reproduction mission: the diagram in words, generated from
its dump, or the hand-written catalog/briefs/<name>.md when present."""

from pathlib import Path

from . import catalog as CAT
from . import dump as D

INSTRUCTIONS = """
Start from an empty document with one state machine, id G0. Create the
elements in the order given, parents before children and states before the
transitions between them. Choose the geometry yourself: every child inside
its parent with a margin, siblings apart, rects about 200 by 100 for a
simple state and larger for a composite one. Keep the names and the action
texts exactly. Your expectations must cover every state with its parent and
every transition. You may build the diagram over several rounds.
"""


def generated(document, name):
    machines = document.machines()
    head = 'Reproduce the diagram "%s".\n' % (machines[0].name if machines else name)
    return head + D.describe(document) + INSTRUCTIONS


def brief(name, document):
    override = CAT.CATALOG_DIR / "briefs" / (name + ".md")
    if override.exists():
        return override.read_text()
    return generated(document, name)
