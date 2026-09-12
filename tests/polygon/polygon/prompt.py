# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the prompts
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

"""The texts the agent reads: the preamble, the mission bodies, the
feedback of a round; and the parser of its answer."""

import re

from . import catalog as CAT
from . import dump as D

SECTION = re.compile(r"^\s*(?:=+|#+)\s*(plan|script|expectations)\s*(?:=+)?\s*$", re.IGNORECASE)
FENCE = re.compile(r"^\s*```")

INTRO = """You edit hierarchical state machine diagrams through a batch script of
the Cyberiada HSM editor. The editor runs your script on a document and
returns the exit code, the diagnostics and a dump of the resulting scene.
Every command is one line; `#` starts a comment; a trailing text field
(name, body, action text) takes the rest of the line and `\\n` embeds a
newline in it. Coordinates are scene units. Gestures are delivered to the
scene as a mouse would: `press`, `drag`, `release` at scene coordinates.
"""

FORMAT = """Answer with three sections and nothing else, in this order:

== plan
a numbered list of what you intend and, for each risky step, what you
expect the editor to do
== script
the commands, one per line
== expectations
facts to verify on the result, one per line, from this vocabulary:
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
                                  (role title|action|label|body; \n a newline)
  hidden <id> <role>              the element shows no such text

Example answer:

== plan
1. two states under the state machine G0, an entry action on the first
2. a transition between them, then check the structure
== script
new-state G0 0 0 200 100 Idle
new-state G0 300 0 200 100 Busy
new-action n0 entry/ lamp_off()
new-transition G0 n0 n1 START
== expectations
state Idle parent G0
state Busy parent G0
action n0 0 entry/ lamp_off()
transition n0 n1
count state 2
"""


def preamble(catalog):
    return "\n".join([INTRO, catalog_text(catalog), "The commands:\n",
                      catalog.table(CAT.FORM_MODEL, CAT.FORM_GESTURE), "", FORMAT])


def catalog_text(catalog):
    return CAT.model_card()


def mission_reproduce(brief):
    return "Mission: reproduce a diagram.\n" + brief


def mission_combine(name, description, scene, stack, operations, theme, budget, untried):
    lines = ["Mission: stress the editor by combining operations in an order a user",
             "would not plan.", "",
             "Starting document (%s):" % name, description.rstrip(), "",
             "Scene (kind id: absolute rect x y w h):", scene.rstrip(), "",
             "Undo stack: index %d of %d." % (stack.index, stack.count) if stack else "",
             "Operations to combine in this session: %s." % ", ".join(operations),
             "Theme: %s - %s." % (theme["name"], theme["hint"]),
             "Budget: %d to %d commands over the session, a few per round." % tuple(budget)]
    if untried:
        lines.append("Untried so far by the polygon: %s." % "; ".join(untried))
    lines += ["", "Combine the operations so that each step changes the situation the next",
              "step relies on. Reuse the ids the dump shows. In the plan, say for each",
              "risky step what you expect the editor to do. Every command must be one the",
              "editor accepts; a rejected command is reported back and the round is",
              "repeated."]
    return "\n".join(lines)


def scene_text(dump):
    lines = []
    for item in dump.scene_items().values():
        x, y, w, h = item.abs_rect
        lines.append("  %s %s: %g %g %g %g" % (item.kind, item.id, x, y, w, h))
    return "\n".join(lines)


def feedback(number, accepted, script_error, findings, expectation_failures, dump):
    lines = ["Round %d: %s." % (number, "accepted" if accepted else "not accepted")]
    if script_error:
        lines.append("The script was rejected at %s. The document is unchanged; send a" % script_error)
        lines.append("corrected script for this round.")
    for kind, signature, note in findings:
        if kind != "semantic":
            lines.append("The editor showed a problem (%s): %s" % (kind, note))
    for fact, reason in expectation_failures:
        lines.append("Expectation failed: %s (%s)" % (fact, reason))
    if dump is not None and dump.document is not None:
        lines += ["", "The document now:", D.describe(dump.document).rstrip(), "",
                  "Scene (kind id: absolute rect x y w h):", scene_text(dump)]
        if dump.stack:
            lines.append("Undo stack: index %d of %d." % (dump.stack.index, dump.stack.count))
    lines += ["", "Continue with the next round: answer with the three sections."]
    return "\n".join(lines)


def parse_answer(text):
    """(plan, script, expectations) of an answer, or None when it has no
    script section. Code fences inside the sections are ignored."""
    parts = {"plan": [], "script": [], "expectations": []}
    current = None
    for line in text.splitlines():
        match = SECTION.match(line)
        if match:
            current = match.group(1).lower()
            continue
        if FENCE.match(line):
            continue
        if current:
            parts[current].append(line)
    if not parts["script"]:
        return None
    clean = lambda ls: "\n".join(ls).strip() + "\n"
    return clean(parts["plan"]), clean(parts["script"]), clean(parts["expectations"])
