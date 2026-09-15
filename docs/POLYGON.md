# The Test Polygon

The polygon is an exploratory test system driven by a child LLM agent. The
good-file tests of `TESTING.md` prove fixed diagrams against reviewed
references; they cannot invent the long, twisted edit histories where the
editor crashes. The polygon lets an agent draw real diagrams and combine the
editing operations in low-predicted ways through the batch mode, judges every
run with reference-free checks, and keeps the found problems with their
reproductions for bugfixing.

The behaviour it judges is the one written down in `docs/EDITOR-SPEC.md` — the
root specification of the editor's required behaviour. Its requirements become
the polygon's standing laws, the way the good-file tests take their references
from reviewed dumps.

The tool lives in `tests/polygon/`, is written in Python 3 with the standard
library and one HTTP client, and runs the editor binary exactly as the ctest
tiers do: offscreen, `--batch`, `--no-text`, hermetic fonts and config. The
LLM backend is configurable; the agent is never a fixed vendor.

## Architecture

```
  catalog/operations      coverage.json            catalog/briefs + corpus
  (verbs, kinds,          (coverage: verb pairs,   (domain briefs, the
   preconditions)          verb x kind, fired)      diagrams to reproduce)
        \                      |                      /
         v                     v                     v
       +-------------------- mission composer --------------------+
       |  seed -> mission: start document, operations, theme,     |
       |          budget, untried cells, or a diagram to rebuild  |
       +---------------------------+-------------------------------+
                                   | prompt
                  +----------------+----------------+
                  v                                 v
           agent adapter                          fuzzer
     (chat-completions | messages,        (random valid sequence
      vision flag, retries, budget)          from the catalog, seeded)
                  |  == plan / == script / == expectations
                  v
       +------------------------------ runner --------------------------------+
       |  CyberiadaEditor --batch <doc> --script <s> --dump --dump-stack |
       |  subprocess, timeout, signal and exit code capture                 |
       |  reruns: --save + reopen | undo-all / redo-all | --export          |
       +-------------------------------+------------------------------------+
                                       | exit code, stderr, dump, files
                                       v
       +--------------------------- oracles ----------------------------+
       | tier 1  no-brain: crash, save/reopen, undo-all, export         |
       | tier 2  expectations vs the parsed dump                        |
       | tier 3  render: dump vs image ink; vision review (optional)    |
       +---------------+-----------------------------+------------------+
                       | feedback to the next round  | problems
                       v                             v
                 agent (round n+1)        problems/register.json
                                          problems/<id>/  script, doc, png
                                                     |
                                                     v
                                          ctest tier polygon-<id>
                                          (known failure, green on a fix)
```

The composer decides *what* is combined, the agent decides *how*. The
oracles need no reference: every check compares the run with itself. The
document of a round is the start document with the accumulated script
replayed from the beginning, so the accumulated script is the reproduction.

## Missions

A session is one mission and a number of rounds on one document. Two mission
kinds share the runner, the oracles and the register and differ in the prompt.

| Mission | Start | Task of the agent | Extra oracle |
|---|---|---|---|
| A reproduction | empty document, one state machine | rebuild a corpus diagram from its description, never from the file | structural diff of the result dump against the original dump: kinds, names, nesting, transitions, actions; geometry free |
| B combination | a corpus diagram (dump given) | combine a drawn subset of operations under a theme so that each step changes what the next one relies on | none beyond the tiers |
| C exploration | empty document | design and keep improving a real diagram of a given domain; between rounds a fuzzer burst injects random micro-operations on the current diagram | none; the stress profile (crash, save/reopen, undo-all, export) runs each round |

Mission A exercises the creation paths and grows the corpus: every accepted
reproduction is stored as a new starting document. Mission B exercises the
editing paths in sequences nobody planned. Each theme carries a `start` rule
(`corpus`, `small` or `empty`) so a story can begin from an empty document,
and an optional `forms` restriction. The stories: nesting under change,
geometry extremes, undo/redo interleaving, deletion of parents, identity
changes, comments and subjects, transitions everywhere, gestures on a
crowded diagram, build from scratch by gestures (empty start, gesture and
text verbs only), refactoring session, text-heavy behaviours (real code in
C, Python or a pseudolanguage, multi-line, edits), student session with
mistakes (small start, the refusals and corrections), and sequential growth
from a single state to a clean complex system.

### The drill class

A drill takes one editor feature and escalates its complexity through a deterministic
operation sequence, checking feature-specific **reversibility and conservation invariants**
the generic oracles miss. Drills target the dependent-history logic where the worst bugs
live. Each drill is a producer on a small base (`polygon/drills/`), holding a state model
and a step counter; each round is one escalation step returning a script and its invariant
facts. The invariants are guaranteed properties, so a violation registers as a defect
(`invariant` kind), unlike an agent's expectations which stay candidates.

The drills: **nesting** (create, reparent deeper, move out then in; depth grows, out-then-in
restores, a child stays inside its parent); **pseudostate** (initial and final in and out of
states, and the unique-initial gesture that reproduces the two-initial hang); **transition-points**
(add, move, remove polyline points; the endpoints stay); **rebind** (drag a transition's
endpoint across states; the transition connects the new pair, asserted by endpoints not the
renamed id); **choice** (a choice and escalating guarded branches); **action** (escalate a
state's action list; add-then-delete restores it); **resize** (border resize and grow-to-fit
when a child is dragged past the border); **copy-paste** (build a subtree and duplicate it).

Deterministic drills are the primary, guaranteed-coverage form, run with `drill --which
<name|all>` and no LLM. An agent drill is an optional realism variant. A reversibility step
emits an operation and its inverse in one round and asserts the pre-operation structure is
restored, catching corruption that save-reopen and undo-all miss.

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

The **tour mission** is the systematic build-from-scratch strategy: the agent works through
the tools one at a time, using each in several ways (a single element, several, inside a
container, at an extreme position, then undo, in combination with what it built) before
moving to the next. It is pure agent, no fuzzer burst, and runs the full oracles including
the render-content check, so a tool that draws nothing is a finding. It complements the
free-design `explore` mission.

The **fuzzer** is a lean tool-driven core: it picks a tool or a select-tool manipulation
weighted by coverage, and emits the gesture — a creation tool draws or places, the
transition tool presses a source and drags to a target, the select tool drags, resizes,
edits text, drives the transition point dots, or deletes. Creation goes through the real
tools, not the model verbs, so the fuzzer exercises the same paths a user does. The
gesture-less model-only verbs (metainformation, id changes, comment subjects, whole-polyline
replacement) are left to the agent and the good-file cases.

### Exploration and the fuzzer burst

The exploration mission is the build-then-stress strategy and the default balance
between the two producers. The agent draws a real diagram of a domain (a lift, a
vending machine, a robot mission) from the empty document and keeps improving it,
changing its mind, moving and re-nesting states, rerouting and re-pointing edges,
rewriting actions. After every accepted agent round a **fuzzer burst** fires K random
micro-operations on the current diagram: the drags, resizes, endpoint moves and polyline
point edits a designer would not think to try. The agent supplies coherent, dependent
state where the worst crashes live; the fuzzer supplies volume and the awkward
micro-gestures. The balance is layered, not a dial: the fuzzer is the cheap, seeded,
reproducible breadth over single operations and short sequences, the agent is the
human-like design history. The ratio is tuned by measurement (the productivity ledger).

The edges are stressed directly. From the document `sp`/`tp`/`polyline` and the scene
node centres the polygon computes the scene coordinates of every transition handle, then
drives them by gesture: `add-point` (drag a segment), `move-point` (drag a point dot),
`remove-point` (click a point dot, `key delete`), and `move-endpoint` (drag an endpoint,
Ctrl to snap, onto a node to reattach). A pure `fuzz --stress` sweep biases toward the
gesture forms so the drags and the edge and point edits are exercised while the model
verbs keep the diagram populated.

The **stress oracle profile** is reference-free: crash and hang, save then reopen,
undo-all then redo-all, and export success (PNG and SVG exit and write a file). The heavy
render-content ink check is off for random operations, and no result is predicted for the
agent. A universal law is not a prediction but a property of the result, so the standing
`[U]` laws (when added) belong to this profile as well.

### Prompts

The preamble is generated from the operation catalog and is the same for
both missions:

```
You edit hierarchical state machine diagrams through a batch script.
The editor runs your script on a document and returns the exit code,
the diagnostics and a dump of the resulting scene.

Commands, one per line (ids are deterministic: top-level states n0, n1...,
children parent::n0..., transitions src-tgt):
  <the verb table of TESTING.md, generated from catalog/operations>

Answer with three sections and nothing else:
  == plan          a numbered list of what you intend and what you expect
  == script        the commands
  == expectations  facts to verify on the result, one per line
```

Mission A body:

```
Mission: reproduce a diagram.
Start from an empty document with one state machine G0.
The diagram is a pure hierarchy, no transitions, no vertices:
  - two top-level composite states, "Parent 0" and "Parent 1";
  - "Parent 0" holds a simple state "State 0-0" and a composite state
    "Subparent 0-1", which holds "State 0-1-0" and "State 0-1-1";
  - "Parent 1" holds "State 1-0" and "State 1-1".
Choose a geometry in which every child lies inside its parent with a
margin and siblings do not overlap. Create parents before children.
Your expectations must cover every state and its parent.
```

Mission B body:

```
Mission: stress the editor by combining operations in an order a user
would not plan.
Starting document: <dump of tests/diagrams/hierarchy.graphml>
Operations to combine: reparent, move (resize form), delete, undo, redo.
Theme: nesting under change. Budget: 12 to 20 commands.
Untried so far: resize a parent below the size of its child, then reparent
the child out and undo; delete a parent that was just resized and redo.
Combine the operations so that each step changes the situation the next
step relies on. In the plan, say for each risky step what you expect the
editor to do. Keep the document valid: every command must be accepted.
```

An answer to mission B:

```
== plan
1. shrink n0 to 60x40 so that its child n0::n1 no longer fits
2. reparent n0::n1 to n1 while the parent is smaller than the child
3. undo twice: the child is back in n0, n0 has its old size
4. redo once, then delete n0 with n0::n0 still inside
5. undo the delete: n0 and n0::n0 are back with the old geometry
== script
move n0 10 10 60 40
reparent n0::n1 n1
undo
undo
redo
delete n0
undo
== expectations
state Parent 0 parent G0
state Subparent 0-1 parent n0
rect-inside n0::n1 n0
count state 8
undo-depth 4
```

The description of a corpus diagram for mission A is written once into
`catalog/briefs/` (a human description, or generated from the dump and
reviewed); the file itself never reaches the agent.

## Round protocol

```
  round n:  prompt (mission + history of the session + last feedback)
               |
               v
            agent answer  ->  parse: plan, script, expectations
               |                    (a malformed answer is retried once,
               v                     then the round is dropped)
            runner: document(n-1) + script  ->  document(n), dump(n)
               |
               v
            oracles  ->  feedback: exit code, stderr, dump, expectation diff
               |
               v
  round n+1 continues on document(n); a crashed run keeps document(n-1)
```

A session is fully determined by the mission seed, the backend and the
model, so a session can be replayed from its stored prompts and answers
without the backend (`sessions/<date>-<seed>/`). The budget of a session:
the number of rounds and the number of model calls, both configured.

## Oracles

| Tier | Check | Problem kind |
|---|---|---|
| 1 | the process dies by a signal (134, 139), exits 3, or exceeds the timeout | `crash` |
| 1 | an accepted script exits 4 or 2 on a document the previous round accepted | `oracle` |
| 1 | save the result, reopen it, dump again: the two dumps differ | `oracle` |
| 1 | append one `undo` per step: the dump differs from the input dump; redo them all: the dump differs from the edited dump | `oracle` |
| 1 | `--export` to png and svg fails | `oracle` |
| 2 | an expectation of the agent does not hold on the dump | `semantic` |
| 3 | an element of the dump leaves no ink on the image, a child is painted outside its parent, siblings overlap where they must not | `render` |
| 3 | the vision review says the picture does not show the plan | `review` |

The tier 1 checks are the no-brain checks: mechanical reruns of the same
script with other batch options. The comparisons skip the fields the
serialization format does not preserve, listed in `catalog/format.json`
with the reference to the standard for each (the transition type: PNST
1044-2025 6.3.2 says the format cannot distinguish local and external
transitions); `[oracles] ignore` in the configuration adds tags locally.
A problem that turns out to be such a property gets the register status
`format` and no regression case.

```
  script  --dump                 -> dump A
  script  --save out.graphml     -> reopen out.graphml --dump  -> must equal A
  script + undo x N  --dump      -> must equal the input dump
  script + undo x N + redo x N   -> must equal A
  script  --export out.png/.svg  -> exit 0, files exist
```

The polygon runs in text mode by default (`--text`): the canvas texts are
shown, so the agent edits titles, actions, transition labels and comment
bodies as a user does, and the render check verifies that every shown text
leaves ink on the canvas (`shown`/`hidden` facts state what the canvas
shows). The borders and the containment are still checked on a text-free
render, which stays reproducible; the L1/L2 editor tiers keep `--no-text`.

A `semantic` or `review` finding is a candidate: the agent may have expected
the wrong thing. Both stay in the session record with the plan attached for
a human to read; `register add` promotes one with its script and
expectations. A `crash`, `oracle` or `render` finding is a defect until
proven otherwise and goes to the register at once.

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
no dangling reference, one initial per level, endpoint kinds, containment, the
hidden metainformation node) register as defects and are silent on the good
corpus; the overlap law (`NODE-6`) is implemented but gated report-only until the
overlap-semantics decisions (the paste offset, the reconstruct layout) are
settled.

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

The vocabulary is deliberately small; what it cannot express is left to the
vision review. The undo-all rerun reuses the evaluator with the facts of the
untouched input.

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

The vision review is a separate, optional step: when the backend declares
`vision = true`, the agent receives its plan and the png and answers whether
the picture shows the plan. Its verdicts are `review` candidates.

## Operation catalog, composer, coverage, fuzzer

`catalog/operations.json` lists every operation of the batch script: the verb,
the argument shape, the element kinds it applies to and its preconditions
(`reparent` needs a target that may contain the element, `delete-action`
needs an existing action, ...). It is the single source for the prompt verb
table, the fuzzer grammar and the coverage cells. It covers the model verbs
of `TESTING.md` and the gesture verbs of the same script:

| command | effect |
|---|---|
| `press x y [ctrl\|shift\|alt ...]` | mouse press at scene coordinates |
| `drag x y` | mouse move with the pressed button |
| `release x y` | mouse release |
| `click x y [mods]` | press and release at one point |
| `double-click x y` | double click |
| `tool select\|transition` | select the scene tool |
| `delete-selected` | delete the selected element |
| `edit <id> <role> [<i>]` | open the inline editor of a canvas text |
| `edit-text <id> <role> [<i>] <text>` | open, replace and commit a canvas text in one line |
| `type <text>` / `key <name>` / `select-all` / `commit` | drive the open inline editor by keys |

A gesture is one undo step opened on press and closed on release, as in the
GUI, and the dump after a gesture shows the scene the gesture built, so the
expectation facts apply unchanged. The catalog lists the gesture forms of the
operations (`drag`, `resize`, `draw transition`, `delete selected`) beside
the verbs, and mission B draws them alike.

The composer draws a mission from a seed: the start document, three to six
operations, a theme, a budget, and the untried cells it reads from the
coverage store. `coverage.json` counts, across sessions, every executed
`(verb, kind)` cell and every consecutive verb pair, and marks the cells
that fired an oracle. The draw is biased toward empty cells, so the coverage grows the way
pairwise testing does, and the fired cells are revisited with other
neighbours.

The fuzzer is the second producer: from the same catalog it generates a
random valid sequence for the drawn operations, tracking the ids and kinds
the previous commands created so that every command is accepted. It brings
volume and exact reproducibility; the agent brings intent. Both feed the same
runner, oracles and register, and a fuzzer session is a mission with the
producer set to `fuzzer`.

## Competition

`run --backends deepseek,haiku` composes one mission and runs it on each
backend in turn; the pair's folders sit under `sessions/<date>-compare-<seed>/`
with a `comparison.txt` (wall time, calls, tokens, accepted rounds, commands,
script errors, defects, reproduction). `productivity.json` accumulates the
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
  "note": "hierarchy.graphml, mission B seed 4711, round 3, command 5",
  "found": "2026-09-12", "revision": "a41928f",
  "status": "open" }
```

The signature is the normalized failure line: the signal or the assertion
`file:line` for a crash, the oracle name and the ids for the others. A run
whose signature is already registered is counted, not registered again.

`problems/<id>/` holds the reproduction: `start.graphml`, `script`, the
`dump` and `stderr` of the run, the `png` for a render problem, the `plan`
for a semantic or review one. The script is minimized before storing:
prefix replay finds the first failing command, then every earlier command is
dropped in turn while the signature stays. Sessions keep their full history
in `sessions/`.

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

`polygon.toml` (the example is committed, the real one is not):

```
[backend.local]
kind = "chat-completions"            # or "messages"
base_url = "http://localhost:11434/v1"
model = "qwen2.5-coder:32b"
key_env = "POLYGON_LOCAL_KEY"        # the key never enters the file
vision = false
temperature = 0.7
max_calls = 40

[backend.cloud]
kind = "messages"
base_url = "..."
model = "..."
key_file = "~/cloud.key"             # or key_env; the first line of the file
vision = true
```

The adapter interface is two calls: `complete(messages) -> text` and
`supports_images()`. The `chat-completions` kind covers every server speaking the
chat completions protocol, local ones included; `messages` covers the
messages protocol. Both are raw HTTP through the standard library, no vendor
SDK. A backend is selected per session on the command line; the model name
and the backend name are recorded with the session and the problems, never
the key.

## Layout

```
tests/polygon/
  README.md                 how to run
  polygon.example.toml      backends and thresholds; the real polygon.toml is not committed
  run-polygon.sh            wrapper: python3 -m polygon <command>
  polygon/                  the package
    __main__.py             run | fuzz | replay | brief | check-script | check | register
    config.py  env.py       the toml, the keys; the binary and the batch environment
    runner.py               one editor run: exit code, signal, timeout, streams
    dump.py                 dump parser, describe(), compare(), structural_diff()
    expectations.py         the fact language
    oracles.py  render.py   tier 1 reruns and tier 2; the png ink check
    catalog.py  coverage.py the operation catalog; the coverage store and its bias
    fuzzer.py               one valid command or gesture group per round
    composer.py  brief.py   missions from a seed; the generated briefs
    prompt.py  agent.py     the preamble, the mission bodies, the feedback; the agent producer
    session.py              the round loop, the recording, the replay
    register.py             problems, signatures, minimisation, cases.cmake
    adapters/               base.py, chat_completions.py, messages.py
  catalog/
    operations.json         the operation catalog
    format.json             the fields the format does not preserve
    model.md                the model card of the preamble
    themes.json             the combination themes
    briefs/                 hand-written briefs overriding the generated ones
  corpus/                   manifest.json (the corpus names), empty.graphml (the
                            start of a reproduction), the accepted reproductions
  problems/                 local results, not committed
    register.json  cases.cmake
    P-<n>/                  start.graphml, script, expectations, dump, stderr, render.png, plan
  coverage.json  productivity.json  local, not committed
  sessions/<date>-<producer>-<seed>/   session.json (with elapsed), script,
                            round-<n>.dump, conversation.txt, usage.json;
                            <date>-compare-<seed>/ holds a backend pair and
                            comparison.txt (not committed)
  tests/                    the unit tests (ctest: polygon-unit)
```

`tests/CMakeLists.txt` can run the unit tests of the package as `polygon-unit`
and include `problems/cases.cmake` for the `polygon-<id>` cases, both under the
`L0_ENVIRONMENT` of the other tiers with the editor binary in
`POLYGON_INSPECTOR`. These are **not** part of the default test run: the polygon
needs Python 3.11+ (`tomllib`) and a configured backend, so it stays a separate
framework with its own runner. Add its ctest tier with `-DPOLYGON_TESTS=ON`.

## Running

```
cd tests/polygon
cp polygon.example.toml polygon.toml       # fill in the backends
POLYGON_LOCAL_KEY=... ./run-polygon.sh --backend local --mission reproduce --seed 1
./run-polygon.sh --backend local --mission combine --seed 4711 --rounds 8
./run-polygon.sh --producer fuzzer --seed 1 --rounds 200
./run-polygon.sh --replay sessions/2026-09-12-4711
./run-polygon.sh check --problem P-1              # reproduce one registered problem
```

The ctest tier is off by default; configure the build with `-DPOLYGON_TESTS=ON`
to also run the problems from `ctest`:

```
cd ../../build && cmake -DPOLYGON_TESTS=ON . && ctest -R '^polygon-'
```
