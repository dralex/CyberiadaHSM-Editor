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


TOUR_PATTERNS = ("place a single one", "place several", "place one inside a state or a "
                 "state machine", "at an extreme position (tiny, huge or negative "
                 "coordinates)", "then undo and redo it", "combine it with what you already "
                 "built (a transition between two states, a comment on a state)")


def mission_tour(tools, budget):
    lines = ["Mission: exercise every editing tool of the editor, systematically, one tool at",
             "a time.", "",
             "The empty document has one state machine, id G0. Work through the tools below in",
             "order. For each tool, use it in several different ways before moving to the next:",
             ""]
    for name, how in tools:
        lines.append("  - %s: %s" % (name, how))
    lines += ["",
              "For each tool try, where it makes sense: %s." % "; ".join(TOUR_PATTERNS),
              "",
              "Select a tool with `tool <name>`; a creation tool draws a rect",
              "(`press`/`drag`/`release`) or places on a `click`, then reverts to the select",
              "tool. Build a coherent, tidy diagram as you go: children inside their parents,",
              "siblings apart. Budget: %d to %d commands over the session, a few per round." % tuple(budget),
              "Read the dump each round, use the ids it shows, and give expectations for what",
              "you added this round. Announce in the plan which tool you are exercising."]
    return "\n".join(lines)


def mission_explore(domain, budget):
    return "\n".join([
        "Mission: design and keep improving %s as a hierarchical state machine." % domain,
        "",
        "The empty document already has one state machine, id G0. Draw the states, the",
        "transitions and the actions of a real, working %s under it (add more state" % domain,
        "machines with `new-sm <x y w h> <name>` only if the design needs them). Then",
        "keep improving it: change your mind, move and re-nest states,",
        "reroute and re-point transitions, rewrite actions, add and remove points on the",
        "edges, tidy the layout. Make it clean and non-messy: children inside their",
        "parents with a margin, siblings apart, no overlaps.",
        "",
        "Budget: %d to %d commands over the session, a few per round. Between your rounds" % tuple(budget),
        "the polygon injects random micro-operations, so the ids and geometry may shift;",
        "read the dump each round and use the ids it shows. Give expectations only for",
        "what you deliberately changed this round.",
    ])


# the drawing rules the editor enforces, in the agent's terms - kept in step with
# docs/EDITOR-SPEC.md section 4 (the id in brackets is the requirement it renders)
DRAWING_RULES = """The editor keeps a diagram well-formed; draw so these hold, and the
editor will maintain them for you:
  - a child sits inside its parent with a margin; a parent grows to fit its
    children and never shrinks below them [NODE-1, NODE-2]
  - sibling elements do not overlap; keep them apart [NODE-6, NODE-7]
  - one initial pseudostate per state (or per the top machine) [SEM-1]; a
    transition's endpoints are legal kinds - nothing leaves a final, nothing
    enters an initial [SEM-2]; a choice's outgoing transitions are guarded with
    one [else] branch [SEM-3]
  - a state's name is non-empty and unique among its siblings [STRUCT-2]; every
    element has one parent, no cycles [STRUCT-3]; a composite state has children,
    a leaf is simple or a pseudostate [STRUCT-6]
  - a submachine state references another machine and holds only entry/exit
    connection points [SEM-4]; a connector entry is a transition target and a
    connector exit a source, a standalone entry/exit in the machine reversed
    [SEM-5]
  - a transition ends on the border of a rectangular state, on the drawn circle
    of an initial/final, or on a rhombus vertex of a choice [EDGE-1..4]; the line
    ends in an arrow [EDGE-5]; a self-loop stays a loop [EDGE-8]
  - a set colour is drawn on the node or on the transition line and arrow
    [NODE-10, EDGE-14]
  - every entry/exit point has a name [SEM-7]; a submachine state references
    another machine of the document (or an external file), never its own, and
    its connector points are named after that machine's points [SEM-8]
  - a state has at most one `entry/` and one `exit/` block [STRUCT-10]; an
    event is not named entry, exit, do, propagate, block, defer or else
    (ANY and UNKNOWN are allowed) [TEXT-5]; `defer` is the whole behaviour of
    an internal reaction (`EVENT / defer`, nothing after it) and never labels
    a transition; `propagate`/`block` go with an event name [TEXT-6]; the
    machines of a document have distinct names [META-5]
"""


def mission_draw(story, budget):
    return "\n".join([
        "Mission: draw %s as a clean hierarchical state machine." % story["subject"],
        "",
        "The empty document already has one state machine, id G0. Draw the whole",
        "diagram under it - the states, the nesting, the pseudostates, the",
        "transitions and the actions - following this shape:",
        "",
        story["hint"].strip(),
        "",
        DRAWING_RULES,
        "Budget: %d to %d commands over the session, a few per round. Read the dump" % tuple(budget),
        "each round and use the ids it shows; give expectations for what you added",
        "this round. Keep the layout tidy: children inside their parents with a",
        "margin, siblings apart, no overlaps.",
    ])


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
