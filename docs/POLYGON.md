# The Test Polygon

The polygon is an exploratory test system driven by a child LLM agent. The
good-file tests of `TESTING.md` prove fixed diagrams against reviewed
references; they cannot invent the long, twisted edit histories where the
editor crashes. The polygon lets an agent draw real diagrams and combine the
editing operations in low-predicted ways through the batch mode, judges every
run with reference-free checks, and keeps the found problems with their
reproductions for bugfixing.

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
       |  CyberiadaInspector --batch <doc> --script <s> --dump --dump-stack |
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
oracles need no reference: every check compares the run with itself.

## Missions

A session is one mission and a number of rounds on one document. Two mission
kinds share the runner, the oracles and the register and differ in the prompt.

| Mission | Start | Task of the agent | Extra oracle |
|---|---|---|---|
| A reproduction | empty document, one state machine | rebuild a corpus diagram from its description, never from the file | structural diff of the result dump against the original dump: kinds, names, nesting, transitions, actions; geometry free |
| B combination | a corpus diagram (dump given) | combine a drawn subset of operations under a theme so that each step changes what the next one relies on | none beyond the tiers |

Mission A exercises the creation paths and grows the corpus: every accepted
reproduction is stored as a new starting document. Mission B exercises the
editing paths in sequences nobody planned. Themes: nesting under change,
geometry extremes, undo/redo interleaving, deletion of parents with attached
transitions, identity changes, comments and subjects.

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
script with other batch options.

```
  script  --dump                 -> dump A
  script  --save out.graphml     -> reopen out.graphml --dump  -> must equal A
  script + undo x N  --dump      -> must equal the input dump
  script + undo x N + redo x N   -> must equal A
  script  --export out.png/.svg  -> exit 0, files exist
```

A `semantic` or `review` finding is a candidate: the agent may have expected
the wrong thing. Both are registered with the plan attached and are closed
by a human. A `crash`, `oracle` or `render` finding is a defect until proven
otherwise.

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
| `undo-depth <n>` | the `== stack` section of `--dump-stack` reports `index: <n>` |

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

Every open problem is a ctest case `polygon-<id>` that runs the reproduction
and expects the recorded failure. It stays green while the bug is there and
fails the day a change fixes it, which is the signal to review the
reproduction, turn it into an ordinary good-file case where that makes sense
and set the status to `fixed`. The cases are registered by one CMake
function reading the register, next to the existing tiers.

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
model = "..."
key_env = "POLYGON_CLOUD_KEY"
vision = true
```

The adapter interface is two calls: `complete(messages) -> text` and
`supports_images()`. The `chat-completions` kind covers every server speaking the
chat completions protocol, local ones included; `messages` covers the
messages protocol. A backend is selected per session on the command line;
the model name and the backend name are recorded with the session and the
problems, never the key.

## Layout

```
tests/polygon/
  README.md                 how to run, points here
  polygon.example.toml      backend and threshold configuration
  run-polygon.sh            wrapper: env of the ctest tiers + python -m polygon
  polygon/                  the package
    composer.py             missions, seeds, coverage bias
    adapters/               chat_completions.py, messages.py, base.py
    fuzzer.py               random valid sequences
    runner.py               editor subprocess, timeout, reruns
    dump.py                 dump parser (document + scene)
    oracles.py              tier 1, expectations, render
    render.py               ink sampling on the png
    register.py             problems, signatures, minimization
    session.py              the round loop, replay
  catalog/
    operations.json         the operation catalog
    briefs/                 descriptions of the corpus diagrams (mission A)
    themes.json             the combination themes
  corpus/                   start documents: links to tests/diagrams plus
                            the accepted reproductions
  problems/
    register.json
    P-<n>/                  start.graphml, script, dump, stderr, png, plan
  coverage.json
  sessions/<date>-<seed>/   prompts, answers, feedback, per round
```

`tests/CMakeLists.txt` gains `add_polygon_problem_tests()` reading the
register and reusing `cmake/RunBatchTest.cmake` with `EXPECTED` set to the
recorded exit code, under the same `L0_ENVIRONMENT`.

## Running

```
cd tests/polygon
cp polygon.example.toml polygon.toml       # fill in the backends
POLYGON_LOCAL_KEY=... ./run-polygon.sh --backend local --mission reproduce --seed 1
./run-polygon.sh --backend local --mission combine --seed 4711 --rounds 8
./run-polygon.sh --producer fuzzer --seed 1 --rounds 200
./run-polygon.sh --replay sessions/2026-09-12-4711
cd ../../build && ctest -R '^polygon-'     # the open problems
```
