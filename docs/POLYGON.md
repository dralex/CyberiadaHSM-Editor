# The Test Polygon

The polygon is the exploratory test system of the editor: a tester agent edits
diagrams through the batch mode, reference-free oracles judge every step, and
the found problems are kept with their reproductions for bugfixing. The
good-file tests of `TESTING.md` prove fixed diagrams against reviewed
references; they cannot invent the long, twisted edit histories where the
editor crashes. The polygon lets an agent build real diagrams and combine the
editing operations in ways nobody planned.

The behaviour it judges is the one written down in `docs/EDITOR-SPEC.md` — the
root specification of the editor's required behaviour. Its requirements become
the polygon's standing laws and the targets of the hunt mission. Two complexity
metrics — the diagram metric of `docs/COMPLEXITY.md` and the drawing-process
metric of `docs/DRAWING_COMPLEXITY.md` — measure how hard a diagram and its
edit history are and steer the stress mission.

The tool lives in `tests/polygon/`, is written in Python 3 and runs the editor
binary exactly as the ctest tiers do: offscreen, `--batch`, hermetic fonts and
config. The deterministic producers (the fuzzer, the drills, the tool tour, the
register) need the standard library only. `fuzz` and `drill` take `--snap`
(optionally `--grid <n>`) to run a whole sweep with snap-to-grid on: the snap is
recorded as `snap on`/`grid n` verbs at the head of the script, so a finding
reproduces on its own rather than depending on an unrecorded editor flag. The LLM tester is an agent of the
cybersphere agents framework (`sources/cybersphere-agents` over
`hsm-dispatcher-py`): its strategy is a state machine diagram, its hands are
the editor's verb families offered as tools, its judgement is the oracles raised
as facts. The model is configurable; the tester is never a fixed vendor.

## Architecture

```
  polygon run --backend b --mission m --seed s
      |
      v
  harness.py:  compose the mission (start document, story / brief / domain,
               budget), install the components, build the agent on
               diagrams/<m>.graphml, run it
      |
      v  the diagram: the strategy (what the tester is doing now)
  +-----------------------------------------------------------------------------+
  |  Mission machine      Focus machine (hunt)         Watchdog machine         |
  |  Editing -> Applying  Nodes -> Edges -> ...        Watching --budget.       |
  |  -> Checking -> Confirm -> ... -> @                EXCEEDED--> @            |
  +------+-------------------------------+----------------+---------------------+
         | methods                       | facts, at a tick                     | agent loop tools
         v                               |                                      | prompt budget log
  +--------------------------------------+-----------------------------------+  (the framework)
  |  the polygon components                                                  |
  |   document   the diagram under edit: the start document plus the         |
  |              accepted script, replayed on every edit                      |
  |   oracle     the reruns, the laws, the render check; the expectations     |
  |   register   the problems and their reproductions                         |
  |   focus      the areas of EDITOR-SPEC: the families, the requirements,    |
  |              the untried cells of the coverage                            |
  |   metric     the complexity C and D of the document                       |
  |   fuzzer     a burst of random micro-operations                           |
  +---------------------------+----------------------------------------------+
                              | wraps
  runner  dump  oracles  laws  render  expectations  register  coverage  fuzzer  climb
                              ^
     the tools of the model:  create  connect  arrange  behave  clipboard  history
                              gesture  (one per verb family)   inspect   expect
                              signal (the framework's: the moves of the diagram)
                              |
                      the engine (Pydantic AI)  <->  the model
```

The components own the mechanics, the diagram owns the control flow, the tools
are the model's hands. An edit is one editor run on the start document with the
accumulated script, so the accumulated script is always the reproduction. The
long work — an edit, a check, a burst, a comparison — is an activity of a
component: a method starts it and returns, the dispatcher keeps ticking, and the
result arrives as a signal that moves the diagram. The model is asked only when
the diagram offers it something.

## The tester

### The workspace: the document

The `document` component holds the diagram under edit: the start document (the
empty document with one state machine `G0`, or a corpus diagram), the accepted
script and the last dump. `apply(family, script)` is the activity behind every
family tool: the script is parsed, a verb outside the family or an empty script
is refused at once, otherwise the editor runs the accumulated script plus the
new lines with `--dump --dump-stack`; at the next tick the component raises
`EDITED(family, commands, created)`, `REJECTED(line, message)` (an error in the
new lines, the document unchanged) or `CRASHED(signature)`. Its `summary` is the
edit in words — the lines accepted, the new ids in creation order, the counts,
the undo depth; after a rejection the current ids and the note to use only
them. `inspect()` renders the document, the scene (absolute rects) and the undo
stack from the last dump without running the editor.

For a reproduction `branch(target)` sets the main script aside and starts an
empty one; `restore()` brings the main line back. `compare(name)` runs the
structural diff against a corpus original (`MATCHES` / `DIFFERS(count, text)`).
The variables the diagram reads: `commands`, `edits` (since the last check),
`elements`, `undo_depth`, `rejections` (consecutive), `tries`, `branched`,
`target`, and the limits of the session `budget`, `max_calls`, `max_seconds`.

### The hands: the verb families

The batch script has sixty-two verbs (`catalog/operations.json`): thirty model
verbs, sixteen raw gestures and sixteen named gesture forms. The tester sees them
grouped by their effect on the diagram, each family aligned with the areas of
`EDITOR-SPEC` whose requirements it can violate, and each family is one tool:

| Family | Model verbs | Gesture forms | Spec areas |
|---|---|---|---|
| `create` | new-state, new-sm, new-submachine-state, new-initial, new-final, new-choice, new-terminate, new-shallow-history, new-deep-history, new-entry-point, new-exit-point, new-comment, new-formal-comment | the creation tools: `tool <name>` and a click or a rect | STRUCT, SEM, NODE |
| `connect` | new-transition, polyline, label, new-subject, delete-subject, subject-points, subject-polyline, retarget-subject | draw-loop, add-point, move-point, remove-point, move-endpoint, draw-link | EDGE, SEM, STRUCT-12 |
| `arrange` | move, reparent, delete | drag-state, resize-state, resize-comment, click-delete | NODE |
| `behave` | new-action, update-action, delete-action, rename, update-comment, update-id, update-meta | edit-title, edit-action, edit-label, edit-body, double-click-action, comment-selection | TEXT, META, TOOL-17 |
| `clipboard` | — | copy-paste, cut-paste (copy, cut, paste) | TOOL, NODE |
| `history` | undo, redo | — | HIST |
| `gesture` | — | press, drag, release, click, double-click, tool, delete-selected, edit, edit-text, type, key, select-all, commit, comment-selection | TOOL |

A family tool takes a script of its own verbs; its description, generated from
the catalog, lists only those verbs with the line syntax and the id rules, so the
model carries the protocol of the state it is in, not the whole catalog. The
diagram allows families per state with `tools.allow(...)`: in the edge area the
model sees `create` and `connect` and nothing else, and the framework hides the
rest before every request. Two more tools: `inspect()` returns the current
document, scene and undo stack; `expect(facts)` evaluates facts of the
expectation language on the last dump and returns the result, a failed fact
becoming a `CANDIDATE` at the next step. `signal(event, reason)` is the
framework's: the moves of the diagram the model may request (`CHECK`, `DONE`,
`REPRODUCED`).

A tool returns at once: "applying N commands, the result comes as the next
message". The diagram enters a state without tools while the edit runs, the
engine waits, and when `EDITED` arrives the diagram adds `document.summary` as a
message and re-allows the tools; the next request carries the summary.

### The judgement: the oracle

The `oracle` component runs the reference-free checks (below) on the accumulated
script as an activity, `check()`, when the diagram asks: on the model's
`CHECK`, every few edits, on leaving an area, before the end. At the tick that
sees it done it raises one `DEFECT(kind, req, signature, note, new)` per crash,
oracle, render or law finding (`req` is the `EDIT-` requirement of a law), one
`CANDIDATE(fact, reason)` per failed expectation, and then `CLEAN` or
`FOUND(new, known)`. Every defect is recorded in the register with the script
that produced it; `new` counts the signatures the register had not seen.
`profile('full' | 'stress')` turns the render check on or off; `expect(facts)`
keeps the facts for the model's tool; `stop()` kills a running check. The
variables: `checks`, `clean_streak`, `defects`, `candidates`, `signatures` (of
the last check), `target` and `last` (the first new defect and its note).

### Defects are a state: Confirm

A defect found on a long history is a fact with a long script. The hunt, draw
and stress diagrams react to `FOUND` with a new signature by entering
`Confirm`: the document is branched, the model is told what fired and asked to
rebuild it from the start document in as few commands as it can with every
family at hand, and signals `REPRODUCED`; a check of the branch that raises the
same signature confirms the problem and the register takes the short script as
its reproduction. Three misses leave the problem with the long script, and the
main line is restored either way. The model's reproduction replaces mechanical
minimisation for the tester's findings; the fuzzer, the drills and the tour keep
the prefix-replay minimisation of the register.

### The focus, the metric and the fuzzer

`focus` knows the areas of `EDITOR-SPEC` §4: `area(name)` sets the families of
the area (the table above), its requirement lines read from the specification
(`requirements`, for the prompt), and the untried cells of the coverage store
within those families (`untried`); `next()` moves to the next area and raises
`CHANGED(area)` or `EXHAUSTED`. `metric` measures the complexity `C` of the
document and `D` of the script (`measure()`, an activity; `MEASURED(c, band,
d)`; `nudges`, the thinnest dimensions as instruction; `target`, the complex
band). `fuzzer` fires `burst(k)` random micro-operations through the same
document as an activity, `DONE(commands)`; a crash in a burst is the document's
`CRASHED`.

### The session

A session is one run of the agent on one mission diagram with one backend. Its
folder `sessions/<date>-agent-<mission>-<seed>/` holds `session.json` (the
mission, the edits with their family, lines, acceptance and findings — the
`rounds` of the replay command), `script` (the accepted lines), `edit-<n>.dump`,
`messages.json` (the conversation and the snapshot of the dispatcher, the engine
session, resumable with `--load`), `usage.json` (requests and tokens) and
`trace.txt` (the moves of the machine). `replay <folder>` re-runs the stored
lines through the oracles without a model.

## Missions

A mission is a diagram in `tests/polygon/diagrams/`, drawn with the editor and
validated like any CyberiadaML document. Every document declares the framework
components (`agent`, `loop`, `tools`, `prompt`, `log`, `budget`) and the polygon
ones it uses, holds its instructions as `PROMPT <name>` formal comments, and
carries a `Watchdog` machine beside the mission machine: `budget.limit` on
entry, and on `budget.EXCEEDED` it stops the running activities, hides the tools
and ends the loop.

| Mission | Start | Strategy | Profile |
|---|---|---|---|
| `hunt` | empty | the main one: a `Focus` machine walks the areas of `EDITOR-SPEC` (nodes, edges, texts, tools, history); in each the model has the families of the area and its requirements as the target — make the editor violate one of them; the area changes on a clean streak or when its cells are covered | full |
| `draw` | empty | draw one named diagram (a drawing story) cleanly, under the drawing rules | stress |
| `stress` | empty | build a real machine of a domain; after every clean check a fuzzer burst and a measurement, the thinnest dimensions of `C`/`D` fed back as nudges until the complex band | stress |
| `rebuild` | empty | rebuild a corpus diagram from its brief, never from the file; `compare` after the check, the differences fed back | full |

A `--theme` seasons the hunt and draw task with a hint from
`catalog/themes.json` (geometry extremes, undo/redo interleaving, text-heavy
behaviours, ...). The seeded part of a mission — the story, the brief, the
domain, the theme, the budget, the start document — is the task the run begins
with; the static strategy is the `PROMPT` of the state.

The hunt diagram, the pattern of the others:

```
 machine Hunt
 * -> Editing: entry/ tools.allow(*focus.families); prompt.set('hunt')
      loop.FINISHED/ prompt.add('A text answer changes nothing: use a tool, or signal CHECK or DONE.')
      focus.CHANGED/ prompt.add(focus.requirements + focus.untried)
 Editing  --document.STARTED--> Applying: entry/ tools.allow()
 Applying --document.EDITED [document.edits < 6]/ prompt.add(document.summary)--> Editing
 Applying --document.EDITED [else]/ prompt.add(document.summary)--> Checking
 Applying --document.REJECTED/ prompt.add(document.summary)--> Editing
 Applying --document.CRASHED/ prompt.add(document.summary)--> Checking
 Editing  --agent.CHECK [document.edits > 0]--> Checking
 Checking: entry/ tools.allow(); oracle.check()
 Checking --oracle.CLEAN--> Editing
 Checking --oracle.FOUND [event.new == 0]--> Editing
 Checking --oracle.FOUND [else]/ document.branch(oracle.target); prompt.add(oracle.last)--> Confirm
 Confirm: entry/ tools.allow_all(); prompt.set('confirm')
 Confirm  --document.STARTED--> Reproducing: entry/ tools.allow()
 Reproducing --document.EDITED | REJECTED | CRASHED / prompt.add(document.summary)--> Confirm
 Confirm  --agent.REPRODUCED [document.edits > 0]--> Confirming: entry/ tools.allow(); document.attempt(); oracle.check()
 Confirming --oracle.FOUND [document.target in oracle.signatures]/ register.confirm(document.target); document.restore()--> Editing
 Confirming --oracle.FOUND | CLEAN [document.tries < 3]/ prompt.add('Not reproduced; try again from the start.')--> Confirm
 Confirming --oracle.FOUND | CLEAN [else]/ document.restore()--> Editing
 Editing  --agent.DONE [document.edits == 0]--> @
 Editing  --focus.EXHAUSTED--> Checking (the last check, then @)

 machine Focus
 * -> Nodes: entry/ focus.area('node')
 Nodes --oracle.CLEAN [oracle.clean_streak >= 3 or not focus.untried]/ focus.next()--> Edges
 Edges -> Texts -> Tools -> History the same way (entry/ focus.area(...)); the last raises EXHAUSTED

 machine Watchdog
 * -> Watching: entry/ budget.limit(requests=document.max_calls, seconds=document.max_seconds)
 Watching --budget.EXCEEDED/ document.stop(); oracle.stop(); tools.allow(); loop.stop()--> @
```

The `agent.` triggers sit on the states that offer tools: while an activity runs
the diagram offers nothing and the model is not asked. `draw` has no `Focus`
machine and allows `create`, `connect`, `behave` and `arrange`; `stress` adds a
`Stress` state after every clean check (`fuzzer.burst(5)`, then
`metric.measure()`, then the nudges as a message, until `metric.c` reaches the
target); `rebuild` ends with `Comparing` (`document.compare(name)`; a
difference is fed back while the budget lasts).

### What the model reads

| Text | Where it lives |
|---|---|
| the line syntax, the id rules, the verbs of one family | the description of the family tool, generated from the catalog |
| the expectation vocabulary | the description of `expect` |
| the strategy of a mission, the drawing rules, the confirm instructions | `PROMPT <name>` comments of the diagram, `prompt.set` on entry |
| the story, the brief, the domain, the theme, the budget | the task the run begins with |
| the requirements and the untried cells of an area | `prompt.add(focus.requirements + focus.untried)` on `focus.CHANGED` |
| the outcome of an edit, the ids | `prompt.add(document.summary)` on `EDITED` / `REJECTED` / `CRASHED` |
| the defect to reproduce | `prompt.add(oracle.last)` entering `Confirm` |
| the complexity nudges | `prompt.add(metric.nudges)` on `MEASURED` |
| the current document, scene and undo stack | the `inspect` tool, on demand |

### Drawing stories

The **draw mission** (`run --mission draw`) asks the tester to draw one named
diagram from an empty document. The diagrams are **drawing stories** in
`catalog/stories.json` — each a `name`, a one-line `subject`, a `hint` giving the
states, nesting, pseudostates and key transitions to draw, a `budget`, and a
`source`. They are taken from the example statecharts of *Practical UML
Statecharts in C/C++* (`docs/PSiCC2.pdf`), filtered to the elements the editor
supports (simple/composite states, `initial`/`final`/`choice`/`terminate` and
shallow/deep `history` pseudostates, event/guard/action transitions,
entry/exit/internal behaviours, comments — no orthogonal regions or fork/join).
One story (`job-runner-submachine`, after PNST 1044 appendix В) draws a submachine
state referencing an external machine with its entry/exit connection points, so
those are covered by the drawing missions as well as the fuzzer and the
submachine drill. Pick one with `--story <name>`, or let the seed choose.

Unlike the stress mission, a drawing session fires no fuzzer burst: it is a
clean build of a known machine. Its prompt **presents the editor's rules**
(the `PROMPT draw` comment of `diagrams/draw.graphml`, kept in step with
`EDITOR-SPEC` §4): containment and
grow-to-fit, siblings apart, one initial per region, legal endpoint kinds,
guarded choices, unique names, endpoints on the border, an arrow on the line,
colour drawn per kind. The tester draws to satisfy them and the reference-free
tiers (crash, save/reopen, undo-all, export) plus the standing `[U]` laws check
that the drawn diagram does; a check runs every six edits and before the end. A clean drawing can be kept as a corpus document,
as a reproduction is.

### The drill class

A drill takes one editor feature and escalates its complexity through a deterministic
operation sequence, checking feature-specific **reversibility and conservation invariants**
the generic oracles miss. Drills target the dependent-history logic where the worst bugs
live. Each drill is a producer on a small base (`polygon/drills/`), holding a state model
and a step counter; each round is one escalation step returning a script and its invariant
facts. The invariants are guaranteed properties, so a violation registers as a defect
(`invariant` kind), unlike the tester's expectations which stay candidates.

The drills: **nesting** (create, reparent deeper, move out then in; depth grows, out-then-in
restores, a child stays inside its parent); **pseudostate** (initial and final in and out of
states, and the unique-initial gesture that reproduces the two-initial hang); **transition-points**
(add, move, remove polyline points; the endpoints stay); **rebind** (drag a transition's
endpoint across states; the transition connects the new pair, asserted by endpoints not the
renamed id); **choice** (a choice and escalating guarded branches); **action** (escalate a
state's action list; add-then-delete restores it); **resize** (border resize and grow-to-fit
when a child is dragged past the border); **copy-paste** (build a subtree and duplicate it);
**transition-stress** (a tool-drawn transition with a stored endpoint, then the restructuring
that tends to strand it — add actions to an endpoint state, unlink it from its parent, resize and
grow it through the model; the endpoint stays on the border by `EDIT-EDGE-1`); **submachine** (a
submachine state, its entry/exit connectors and standalone entry/exit points, then a transition
whose direction follows the connector/standalone role); **lifetime** (add and remove scene items
of every kind in many situations — a composite with children, a comment, a transition, a comment
subject link, then create pseudostates/choices, delete, undo, redo, cut+paste and drop a subject —
repainting the view after each step so the real backing-store paint runs while the scene is in each
transient state; the structural laws, the undo/redo identity and strict-reload oracles and the
per-round export paint carry the checks).

Deterministic drills are the primary, guaranteed-coverage form, run with `drill --which
<name|all>` and no LLM. An agent drill is an optional realism variant. A reversibility step
emits an operation and its inverse in one round and asserts the pre-operation structure is
restored, catching corruption that save-reopen and undo-all miss.

The **`repaint`** batch verb forces the active view's backing-store paint (`QGraphicsView::
paintEvent` → `drawItems` → every item's `boundingRect()`/`paint()`), the path the per-round
`--export` `scene->render()` bypasses; the **lifetime** drill and the **scene item lifetime** theme
emit it after every add/remove so a paint runs on the transient scene. This broadens the add/remove
and paint surface; it does not reproduce the narrow freed-item *race* (a batch run paints the item
while it is still alive, never mid-destruction) — that exact invariant is guarded deterministically
by the editor unit test `l4-scene::test_retire_vertex_dots`.

#### The shadow model

The feature-specific invariants above check one property of one operation. A drill can go
deeper by carrying a **shadow model** (`polygon/drills/model.py`): an abstract mirror of the
whole document — states, pseudostates, their nesting and actions, and transitions by their
endpoints. The model is name- and structure-keyed and never predicts ids; it **reconciles**
with the dump each round, matching an element by kind, name and its chain of ancestor names
(a state that gains children flips Simple↔Composite, matched under one token). After every
operation the model turns its expected structure into fact lines in the ordinary expectation
vocabulary (`count`, `state … parent`, `transition`, `action`, `rect-inside`), and the round
checks the editor's dump against **all** of them. Any divergence is a full-document
equivalence failure, registered as an `invariant` defect.

The model checks **structure exactly** and **geometry where the editor is deterministic**:
containment and grow-only as bounds (`rect-inside`), and an explicit rect as an exact fact
(`rect <id> <x> <y> <w> <h>`, within a small tolerance) where a placement is fully specified.
It deliberately does not shadow the layout engine pixel-for-pixel — that would re-implement
the code under test — so a layout-dependent position is asserted only as a bound.

A drill opts in by setting `tracks_model` and keeping the model in lock-step; the base merges
the model's equivalence facts with the drill's own invariants. **nesting** tracks the model,
so its deep chain is checked for full containment and conservation at every level.

#### The compound drill and cross-feature invariants

The **compound** drill (`drill --which compound`) interleaves the features over the whole
accumulated structure, which is where dependent-history bugs actually live. Each round it
**grounds** the model from the real dump, then predicts exactly **one** operation's delta —
nest a state, add a cross-level transition, add an action, reparent a state, delete a
subtree, or copy-paste — and the equivalence check verifies the editor produced that delta
and nothing else. Re-grounding each round keeps the model from drifting over a long history.

The cross-feature invariants fall out of the full-document check for free: a **reparent** must
keep the state's incident transitions (the model re-asserts every transition by its
endpoints), a **delete** must carry its whole subtree and its incident transitions, and an
**action** must survive a reparent or resize of its state. Copy-paste assigns fresh ids and
names the model cannot predict, so a paste round relies on the generic oracles and the next
round's re-grounding to catch a corrupt result.

The **boundary** sweep (`drill --which compound --boundary`) drives the features to extremes —
deep nesting, long action lists, many transitions, and tiny, huge and negative-origin rects —
with the same equivalence check throughout; here a crash or a broken containment bound is the
expected catch.

### The tool tour and the tool-driven fuzzer

The editor's tools were rebuilt into twelve: the select tool, pan and zoom, the transition
tool, and eight element-creation tools (a state machine, a state, the four pseudostates,
a comment and a formal comment). The rect tools (state machine, state) draw a rubber-band
rect or place a default size on a click; the six placement tools create on a click; the
container is the element under the press point, and a creation tool reverts to select after
one use. `catalog/tools.json` is the polygon's model of them.

The **deterministic tour** (`tour`) walks the (tool, pattern) matrix of
`toolcover.json` in prerequisite order without a model, under the stress
profile. The tester's tour is the `tools` area of the hunt mission: the model
has the `gesture` family only, the requirements of `EDITOR-SPEC` §4.6 as its
target, and the full oracles including the render-content check, so a tool that
draws nothing is a finding.

The **fuzzer** is a lean tool-driven core: it picks a tool or a select-tool manipulation
weighted by coverage, and emits the gesture — a creation tool draws or places, the
transition tool presses a source and drags to a target, the select tool drags, resizes,
edits text, drives the transition point dots, or deletes. Creation goes through the real
tools, not the model verbs, so the fuzzer exercises the same paths a user does. The
gesture-less model-only verbs (metainformation, id changes, comment subjects, whole-polyline
replacement) are left to the tester and the good-file cases.

### Stress, the fuzzer burst and the climb

The stress mission is the build-then-stress strategy and the balance between the
two producers. The tester builds a real machine of a domain (a lift, a vending
machine, a robot mission) from the empty document and keeps improving it. After
every clean check a **fuzzer burst** fires K random micro-operations on the
current diagram: the drags, resizes, endpoint moves and polyline point edits a
designer would not think to try. The tester supplies coherent, dependent state
where the worst crashes live; the fuzzer supplies volume and the awkward
micro-gestures. The ratio is tuned by measurement (the productivity ledger).

The edges are stressed directly. From the document `sp`/`tp`/`polyline` and the
scene node centres the polygon computes the scene coordinates of every
transition handle, then drives them by gesture: `add-point` (drag a segment),
`move-point` (drag a point dot), `remove-point` (click a point dot, `key
delete`), and `move-endpoint` (drag an endpoint, Ctrl to snap, onto a node to
reattach). A pure `fuzz --stress` sweep biases toward the gesture forms so the
drags and the edge and point edits are exercised while the model verbs keep the
diagram populated.

The **stress oracle profile** is reference-free: crash and hang, save then
reopen, undo-all then redo-all, and export success (PNG and SVG exit and write
a file). The heavy render-content ink check is off for random operations. A
universal law is a property of the result, not a prediction, so the standing
`[U]` laws belong to this profile as well.

After the burst the `metric` measures the **diagram complexity** `C` of the
document (`docs/COMPLEXITY.md`, `tools/complexity.py` — the weighted element
count with nesting amplified, the branching, the connectivity, the submachine
coupling and the kind variety) and the **drawing complexity** `D` of the script
(`docs/DRAWING_COMPLEXITY.md`, `tools/drawing_complexity.py` — the breadth and
manner of the operations, the restructuring and reuse, the transition-drawing
craft). The thinnest dimensions go back to the model as nudges: a flat diagram
is told to nest a composite, one with no feedback edge to add a cycle, one with
no branch to add a guarded choice, a single machine to factor a part into a
submachine, a diagram that never restructured to reparent a subtree, one that
never reused to copy-paste, and one with plain edges to route a polyline, a
self-loop or a cross-boundary transition. The mission ends when `C` reaches the
complex band or the budget runs out. `C` is non-linear — a nested loop or a
submachine is worth far more than another flat sibling — so the efficient spend
is dense structure, and the session records the peak `C`, the peak `D` and the
density (`C` per command) beside its commands and defects. A deep, restructured,
transition-heavy machine reaches editor code the fixed missions never touch.

## Oracles

| Tier | Check | Problem kind |
|---|---|---|
| 1 | the process dies by a signal (134, 139), exits 3, or exceeds the timeout | `crash` |
| 1 | an accepted script exits 4 or 2 on a document the previous round accepted | `oracle` |
| 1 | save the result, reopen it, dump again: the two dumps differ | `oracle` |
| 1 | append one `undo` per step: the dump differs from the input dump; redo them all: the dump differs from the edited dump | `oracle` |
| 1 | `--export` to png and svg fails | `oracle` |
| 2 | an expectation of the tester does not hold on the dump | `semantic` |
| 3 | an element of the dump leaves no ink on the image, a child is painted outside its parent, siblings overlap where they must not | `render` |

The tier 1 checks are the no-brain checks: mechanical reruns of the same
script with other batch options. The comparisons skip the fields the
serialization format does not preserve, listed in `catalog/format.json`
with the reference to the standard for each (the transition type: PNST
1044-2025 6.3.2 says the format cannot distinguish local and external
transitions); `[oracles] ignore` in the configuration adds tags locally.
A problem that turns out to be such a property gets the register status
`format` and no regression case. The save/reopen rerun ends with a **strict
reload**: the saved file is loaded once more with `--strict`, which turns on
the strict mode of libcyberiadaml, so the requirements of PNST 1044 that only
the library checks (the behaviour text grammar of 6.8, the submachine references
and point names of 8.1 and 8.3) become `oracle` findings with a `strict:`
signature; `[oracles] strict = false` switches it off.

```
  script  --dump                 -> dump A
  script  --save out.graphml     -> reopen out.graphml --dump  -> must equal A
  script + undo x N  --dump      -> must equal the input dump
  script + undo x N + redo x N   -> must equal A
  script  --export out.png/.svg  -> exit 0, files exist
```

The polygon runs in text mode by default (`--text`): the canvas texts are
shown, so the tester edits titles, actions, transition labels and comment
bodies as a user does, and the render check verifies that every shown text
leaves ink on the canvas (`shown`/`hidden` facts state what the canvas
shows). The borders and the containment are still checked on a text-free
render, which stays reproducible; the L1/L2 editor tiers keep `--no-text`.

A `semantic` finding is a candidate: the tester may have expected the wrong
thing. It stays in the session record for a human to read; `register add`
promotes one with its script and expectations. A `crash`, `oracle`, `render`
or `law` finding is a defect until proven otherwise and goes to the register
at once, with the script that produced it; the tester then reproduces it in
`Confirm` (above).

The check kinds of `EDITOR-SPEC` (§2) map onto these tiers: **[C] crash-free** is
the tier-1 crash and hang check; **[I] identity** is the tier-1 save/reopen and
undo-all/redo-all reruns; **[A] action-effect** is a tier-2 expectation (a
candidate) or, guaranteed, the drill and shadow-model equivalence; **[U]
universal law** is the standing law oracle below.

The **standing law oracle** (`polygon/laws.py`) holds the `[U]` geometric and
structural laws of `EDITOR-SPEC` over the whole dump after every operation, in
every mode — it is inserted in `Round.evaluate` right after tier 1, so it is
always on and never behind the render gate. Each law reads one parsed dump and
yields the requirement it enforces and the offending detail; a violation is a
`law` finding whose signature keeps the requirement id (`law:NODE-1:…`), so it
traces to the specification and, like a drill invariant, reproduces minimally and
skips minimisation. Reference-free like tier 1 — a property, not a prediction. The
hard laws (unique ids, one-parent, endpoints in one machine, composite⇔children,
no dangling reference, the subject targets of `STRUCT-12` with a fragment inside one
field of its target, a drawn fragment link ending at the middle of its first underline
segment that lies in a text box of the target (`EDGE-21`, the boxes from `== text`), a
drawn comment link starting on its comment border and, on a choice, ending anywhere on its
rhombus (`EDGE-17`; the vertices bind only the transitions) (`EDGE-17`, from the `Subject Link` scene lines, which the dump
emits in the same frame as the element rects so a nested comment's link is not falsely off-border), a
stored transition endpoint lying on its state's border (`EDGE-1`, from the
`transition_handles` source/target points — an auto endpoint is exempt), one
initial per level, endpoint kinds, containment, sibling no-overlap, the hidden
metainformation node) register as defects and are silent on the good corpus. The
fuzzer draws comment links too (`draw-link`: the transition tool from a comment to a state,
a choice or another comment) and
comments a selection (`comment-selection`: a state title or a comment body opened,
selected whole and linked to a new comment beside its element).

The `[U]` geometric laws are editing-mode invariants (`EDITOR-SPEC` §4.10): the editor's
recovery maintains them, but inspection mode suspends it and shows the stored geometry as-is.
The polygon runs the editor in editing mode (never `--inspect`), so the laws apply; an
inspection-mode dump is exempt.

## Expectation language

One fact per line, evaluated on the parsed dump (`== document` for the
structure, `== scene` for the geometry). Names address states, ids address
anything.

| Fact | Holds when |
|---|---|
| `state <name> parent <id>` | a state with that name exists directly under `<id>` |
| `kind <id> <kind>` | the element has that kind (`simple`, `composite`, `initial`, `final`, `choice`, `terminate`, `comment`, `formal-comment`) |
| `count <kind> <n>` | the document holds `<n>` elements of the kind (`state` counts both state kinds) |
| `transition <src> <tgt>` | a transition `src-tgt` exists |
| `action <id> <i> <text>` | action `<i>` of the element has that text |
| `rect-inside <id> <id>` | the scene rect of the first lies within the second |
| `no-overlap <id> <id>` | the scene rects do not intersect |
| `exists <id>` / `absent <id>` | the element exists / does not |
| `undo-depth <n>` | the `== stack` section of `--dump-stack` reports `index: <n>` |
| `shown <id> <role> <text>` | the canvas shows that text for the element (role `title`, `action`, `label`, `body`) |
| `hidden <id> <role>` | the element shows no such text on the canvas |

The vocabulary is deliberately small. The undo-all rerun reuses the evaluator
with the facts of the untouched input; the shadow model of the drills emits
its equivalence facts in the same language.

## Render check

The export is one pixel per scene unit on a white background, the grid and
the service objects off (`TESTING.md`, rendering). The dump gives the rect of
every item. No recognition is needed: the check samples the image where the
dump says an element is.

```
  dump rect (x; y; w; h)  ->  expected path in pixels per kind:
        state, comment: the rounded rect border
        choice: the four diagonals of the rect
        initial, final, terminate: the centre of the point
        transition: the polyline segments
        |
        v
  sample the pixel under the path and its neighbours within RENDER_PROBE_PX
  ink ratio = non-white samples / path length
        |
        v
  ratio < RENDER_INK_MIN  ->  `render` problem "element not painted"
```

The containment and overlap facts are rectangle arithmetic on the dump alone.
The thresholds are named constants of the configuration, not literals. The
check runs under `--no-text`, so it samples shapes only and stays
machine-independent. The png of every fired check is kept with the
reproduction so a human sees what fired.

## Operation catalog, composer, coverage, fuzzer

`catalog/operations.json` lists every operation of the batch script: the verb,
the argument shape, the element kinds it applies to, its form (`model`,
`gesture`, `gesture-form`) and its **family** (the table above). It is the
single source for the tool descriptions of the tester, the fuzzer grammar and
the coverage cells. The gesture verbs of `TESTING.md` are in it beside the
model verbs:

| command | effect |
|---|---|
| `press x y [ctrl\|shift\|alt ...]` | mouse press at scene coordinates |
| `drag x y` | mouse move with the pressed button |
| `release x y` | mouse release |
| `click x y [mods]` | press and release at one point |
| `double-click x y` | double click |
| `tool <name>` | select the scene tool |
| `delete-selected` | delete the selected element |
| `edit <id> <role> [<i>]` | open the inline editor of a canvas text |
| `edit-text <id> <role> [<i>] <text>` | open, replace and commit a canvas text in one line |
| `type <text>` / `key <name>` / `select-all` / `commit` | drive the open inline editor by keys |

A gesture is one undo step opened on press and closed on release, as in the
GUI, and the dump after a gesture shows the scene the gesture built, so the
expectation facts apply unchanged. The catalog lists the gesture forms of the
operations (`drag-state`, `resize-state`, `draw-loop`, `click-delete`, ...)
beside the verbs.

The composer draws a mission from a seed: the start document, the story or the
corpus diagram, the domain, the theme and the budget. `coverage.json` counts,
across sessions, every executed `(verb, kind)` cell and every consecutive verb
pair, and marks the cells that fired an oracle; a family's untried cells are
what `focus.untried` tells the tester. The fuzzer's draw is biased toward empty
cells, so the coverage grows the way pairwise testing does, and the fired cells
are revisited with other neighbours.

The fuzzer is the second producer: from the same catalog it generates a random
valid sequence, tracking the ids and kinds the previous commands created so
that every command is accepted. It brings volume and exact reproducibility; the
tester brings intent. Both feed the same runner, oracles and register: a fuzzer
session is a mission with the producer set to `fuzzer`, and the stress mission
calls it through the `fuzzer` component.

## Competition

`run --backends deepseek,haiku` composes one mission and runs it on each
backend in turn, one session after the other; the pair's folders sit under `sessions/<date>-compare-<seed>/`
with a `comparison.txt` (wall time, requests, tokens, accepted edits, commands,
rejections, defects, reproduction). `productivity.json` accumulates the
totals per backend across campaigns, printed by `report`. The keys never
enter any of it; only the backend and the model names are recorded.

## Problem register

`problems/register.json` follows the `defects.json` convention of the
compliance suite:

```
{ "id": "P-12",
  "kind": "crash",
  "title": "reparent into a resized parent after undo aborts",
  "signature": "signal:SIGSEGV|CyberiadaSMEditorScene::syncItem",
  "note": "hunt seed 4711, edit 3, command 5",
  "found": "2026-09-12", "revision": "a41928f",
  "status": "open" }
```

The signature is the normalized failure line: the signal or the assertion
`file:line` for a crash, the oracle name and the ids for the others. A run
whose signature is already registered is counted, not registered again.

`problems/<id>/` holds the reproduction: `start.graphml`, `script`, the
`dump` and `stderr` of the run, the `png` for a render problem. A finding of
the fuzzer, a drill or the tour is minimized before storing: prefix replay
finds the first failing command, then every earlier command is dropped in turn
while the signature stays (a law or an invariant keeps its full script). A
finding of the tester is stored with its long script at once and replaced by
the short script the model rebuilds in `Confirm`. Sessions keep their full
history in `sessions/`.

With the opt-in ctest tier (`-DPOLYGON_TESTS=ON`) every open problem is a case
`polygon-<id>` running
`python3 -m polygon check --problem <id>`, which replays the reproduction
with every oracle and exits 0 while the recorded signature reproduces. It
stays green while the bug is there and fails the day a change fixes it,
which is the signal to review the reproduction, turn it into an ordinary
good-file case where that makes sense and set the status to `fixed`. The
register writes `problems/cases.cmake`, one line per open problem, which
`tests/CMakeLists.txt` includes next to the existing tiers when present.
The register, the reproductions and the coverage store are local results
of the machine running the polygon; they are not committed. A problem
worth keeping is reported to the developers with its reproduction.

## LLM backends

The tester runs on the cybersphere agents framework, so the model is a
Pydantic AI model id, `provider:model-name`, and the key is the provider's
environment variable. `polygon.toml` (the example is committed, the real one is
not) names the backends:

```
[run]
timeout = 30          # seconds per editor run
calls = 40            # model requests per session
seconds = 0           # wall-clock limit of a session, 0 for none

[backend.deepseek]
model = "deepseek:deepseek-chat"
key_file = "~/DEEPSEEK_API_KEY"    # or key_env = "DEEPSEEK_API_KEY"
max_calls = 40
temperature = 0.7

[backend.local]
model = "openai:qwen2.5-coder:32b"  # any server speaking the chat completions protocol
key_env = "OPENAI_API_KEY"          # with OPENAI_BASE_URL in the environment
```

The key is read from the file or the variable and exported into the provider's
variable for the run only; it never enters the configuration, the sessions or
the register. A backend is selected per session on the command line; the backend
name and the model id are recorded with the session and the problems. The LLM
commands need the harness installed (`cybersphere-agents`, `hsm-dispatcher-py`,
`pydantic-ai`, the CyberiadaML binding); without it they stop with a message,
and the deterministic commands run on the standard library.

## Layout

```
tests/polygon/
  README.md                 how to run
  polygon.example.toml      backends and thresholds; the real polygon.toml is not committed
  run-polygon.sh            wrapper: python3 -m polygon <command>
  polygon/                  the package
    __main__.py             run | fuzz | tour | drill | replay | brief | check-script | check | register | report
    config.py  env.py       the toml, the keys; the binary and the batch environment
    runner.py               one editor run: exit code, signal, timeout, streams
    dump.py                 dump parser, describe(), compare(), structural_diff()
    expectations.py         the fact language
    oracles.py  laws.py     tier 1 reruns and tier 2; the standing laws
    render.py               the png ink check
    catalog.py  coverage.py the operation catalog and the families; the coverage store
    fuzzer.py  tour.py      one valid command or gesture group per round; the deterministic tour
    drills/                 the drill class, the shadow model, the compound drill
    composer.py  brief.py   missions from a seed; the generated briefs
    prompt.py               the generated texts: the tool descriptions, the summaries
    components.py           document, oracle, register, focus, metric, fuzzer
    harness.py              the tools, the agent session, the key export
    session.py              the round loop of the deterministic producers, the replay
    register.py             problems, signatures, minimisation, cases.cmake
    climb.py                the complexity measures of the metric
  diagrams/                 the mission diagrams: hunt, draw, stress, rebuild
  catalog/
    operations.json         the operation catalog, with the families
    tools.json              the tool model of the editor
    format.json             the fields the format does not preserve
    model.md                the model card of the tool descriptions
    themes.json  stories.json  the themes; the drawing stories
    briefs/                 hand-written briefs overriding the generated ones
  corpus/                   manifest.json (the corpus names), empty.graphml (the
                            start document), the accepted reproductions
  problems/                 local results, not committed
    register.json  cases.cmake
    P-<n>/                  start.graphml, script, expectations, dump, stderr, render.png
  coverage.json  toolcover.json  productivity.json   local, not committed
  sessions/<date>-agent-<mission>-<seed>/   session.json, script, edit-<n>.dump,
                            messages.json, usage.json, trace.txt;
                            <date>-compare-<mission>-<seed>/ holds a backend set
                            and comparison.txt (not committed)
  tests/                    the unit tests (ctest: polygon-unit)
```

`tests/CMakeLists.txt` can run the unit tests of the package as `polygon-unit`
and include `problems/cases.cmake` for the `polygon-<id>` cases, both under the
`L0_ENVIRONMENT` of the other tiers with the editor binary in
`POLYGON_INSPECTOR`. These are **not** part of the default test run: the polygon
needs Python 3.11+ (`tomllib`) and a configured backend, so it stays a separate
framework with its own runner. Add its ctest tier with `-DPOLYGON_TESTS=ON`; the
tests of the harness skip when the framework is not importable, so point
`Python3_EXECUTABLE` at the environment that has it to run them.

## Running

```
cd tests/polygon
cp polygon.example.toml polygon.toml       # fill in the backends
./run-polygon.sh run --backend deepseek --mission hunt --seed 1
./run-polygon.sh run --backend deepseek --mission draw --story keyboard
./run-polygon.sh run --backends deepseek,haiku --mission stress --seed 9
./run-polygon.sh run --backend deepseek --mission rebuild --diagram lift --trace
./run-polygon.sh fuzz --diagram geometry --seed 1 --rounds 30
./run-polygon.sh drill --which all --seed 5
./run-polygon.sh replay sessions/<folder>
./run-polygon.sh check --problem P-1              # reproduce one registered problem
```

The ctest tier is off by default; configure the build with `-DPOLYGON_TESTS=ON`
to also run the problems from `ctest`:

```
cd ../../build && cmake -DPOLYGON_TESTS=ON . && ctest -R '^polygon-'
```
