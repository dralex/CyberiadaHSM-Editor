# -----------------------------------------------------------------------------
# CyberiadaPro, the Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the generated texts of the tester
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

"""The texts generated for the model: the descriptions of the family tools
(from the catalog), the vocabulary of the expect tool, the task a session
begins with. The static strategies live in the PROMPT comments of the
mission diagrams."""

from . import catalog as CAT

LINES = """Every command is one line; `#` starts a comment; a trailing text field (a name,
a body, an action text) takes the rest of the line and `\\n` embeds a newline in
it. Coordinates are scene units; ids are the ones the editor reports."""

GESTURES = """Gestures are delivered to the scene as a mouse would, at scene coordinates; a
press..release pair is one undo step. A creation tool (`tool <name>`) draws a
rect with press/drag/release or places on a click, then reverts to the select
tool; the transition tool presses a source and drags to a target. The tools:"""

EXPECT = """Facts to verify on the current document, one per line, from this vocabulary:
  state <name> parent <id>        the state exists directly under <id>
  kind <id> <kind>                simple|composite|initial|final|choice|terminate|comment|formal-comment|transition
  count <kind> <n>                <kind> as above or `state` for both state kinds
  transition <src-id> <tgt-id>
  action <id> <i> <text>          action <i> (0-based) of the element in the action notation
  exists <id> / absent <id>
  rect-inside <id> <id>           the first scene rect lies inside the second
  no-overlap <id> <id>
  undo-depth <n>                  the undo stack index
  shown <id> <role> <text>        the canvas shows the text for the element
                                  (role title|action|label|body; \\n a newline)
  hidden <id> <role>              the element shows no such text
The answer says which facts do not hold now; the facts are checked again at the
next check and a failed one is kept as a candidate finding."""

INSPECT = """The current document in words, every element with its id, the scene (the
absolute rect of every item) and the undo stack. No change is made."""

HEADS = {
    CAT.FAMILY_CREATE: "Create elements: states, state machines, pseudostates, comments, points.",
    CAT.FAMILY_CONNECT: "Connect elements: transitions, their polylines and labels, comment subjects.",
    CAT.FAMILY_ARRANGE: "Arrange elements: move or resize, reparent, delete.",
    CAT.FAMILY_BEHAVE: "Write the texts: names, actions, comment bodies, ids, metainformation.",
    CAT.FAMILY_CLIPBOARD: "The clipboard: copy, cut and paste the selected element (select it with a click).",
    CAT.FAMILY_HISTORY: "The history: undo and redo one step at a time.",
    CAT.FAMILY_GESTURE: "Raw mouse and keyboard gestures on the canvas, and the inline text editors.",
}


def gesture_line(catalog):
    """The raw gestures every tool accepts, in one line."""
    usages = [op.usage for op in catalog.by_family(CAT.FAMILY_GESTURE)]
    return "Raw gestures are accepted in every tool: " + " | ".join(usages) + "."


def protocol(family, catalog):
    """The description of a family tool: its verbs and the line syntax."""
    parts = [HEADS[family], "Apply a script of these commands to the diagram.", "", LINES, ""]
    if family == CAT.FAMILY_GESTURE:
        parts += [GESTURES, "  " + ", ".join(t.name for t in CAT.tools()
                                                  if t.family != CAT.FAMILY_VIEW), "",
                  catalog.table(CAT.FORM_GESTURE)]
    else:
        parts += [catalog.table(CAT.FORM_MODEL, family=family), "", gesture_line(catalog)]
    parts += ["", "The result of the edit comes as the next message: the new ids, or the line "
                  "that was rejected and the ids the document has."]
    return "\n".join(parts)


def task(mission, brief=""):
    """The seeded part of a mission, the first message of the session."""
    lo, hi = mission.budget if mission.budget else (0, 0)
    if mission.kind == "rebuild":
        head = ["Task: rebuild the diagram \"%s\"." % mission.name, "", brief.rstrip()]
    elif mission.kind == "draw":
        head = ["Task: draw %s." % mission.story["subject"], "",
                "Follow this shape:", mission.story["hint"].strip()]
    elif mission.kind == "stress":
        head = ["Task: design and keep improving %s as a hierarchical state machine." % mission.domain]
    else:
        head = ["Task: hunt for defects of the editor, area by area."]
    if mission.theme:
        head += ["", "Theme: %s - %s." % (mission.theme["name"], mission.theme["hint"])]
    head += ["", "The start document is the empty document with one state machine, id G0.",
             "Budget: %d to %d commands over the session." % (lo, hi)]
    head += ["", "The model of the diagrams:", CAT.model_card().rstrip()]
    return "\n".join(head)
