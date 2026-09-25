# Drawing Process Complexity Metric

**Overview:** a reference-free number `D(S)` measuring the complexity of the drawing *process* that
produced a diagram — the tools used and how, the restructuring, clipboard reuse, correction, and above
all the **drawing of transitions**, recorded in a session — and **inheriting** the finished diagram's
complexity `C`.
Its purpose is to make "a richer, more interesting drawing process" measurable, so the polygon can be
steered toward diagrams that are both complex *and* built in interesting ways.
**Document version:** 0.1 (2026-09-25) — DRAFT; weights provisional pending session calibration (§7).
**Related authorities:** `COMPLEXITY.md` (the inherited diagram metric `C`), `EDITOR-SPEC.md` (the edit
operations and `EDIT-HIST-4`), `POLYGON.md` + `catalog/operations.json` + `polygon/toolcover.py` (the
verb and tool-usage vocabulary).

## 1. Purpose and properties
`D(S)` scores a finished drawing *session* `S` (a `session.script` — the recorded batch/gesture verbs)
together with the diagram it produced. It is:
- **Inherited from `C`** — a complex diagram implies a complex drawing, so `D` is built on top of `C`.
- **Reference-free** — computed from the script and the final diagram alone.
- **Decomposable** — a category breakdown (breadth, how-used, restructuring, reuse, transition-drawing,
  gestures, trajectory), all under a directedness factor `η` that discounts parasite oscillation.
- **The process analogue of `C`** — same philosophy: *variety* and *higher-order, structure-changing*
  operations count more than raw volume.
- **Process-dependent** — `D` scores the *drawing*, not only the diagram. The **same** finished diagram
  (same `C`) can be built by many different sessions, and `D` separates them: it **ranks the alternative
  drawings of one target and prefers the richer, more code-stressing one** (which reparents, reuses and
  refactors) over a linear or a thrashing build. The target fixes *what* to build (the `C` level);
  higher `D` selects *how*.

Non-goal: `D` is not effort, wall-clock time, or script length. Tediously hand-placing many identical
states is **low** drawing complexity; a short session that restructures, reuses substructures and
explores with undo/redo is **high**. The interesting drawings are varied and non-linear, not long.

## 2. What is measured — and what is not
From the session's verbs (`operations.json` marks each verb `form: model` or `form: gesture`):
- **Tools** — the twelve creation tools (`new-state/-comment/-formal-comment/-initial/-final/-choice/
  -terminate/-shallow-history/-deep-history/-submachine-state/-entry-point/-exit-point`) plus `new-sm`,
  and **how** each is used (§3.2).
- **Transition drawing (emphasised)** — drawing edges with the transition tool and shaping them:
  polyline routing, endpoint re-attachment, self-loops, cross-boundary edges, labels (§3.7).
- **Refinement** — `rename`, `set-color`, `label`, `new/update/delete-action`, `new/delete-subject`,
  `update-comment/-id/-meta`, `polyline`.
- **Restructuring** — `reparent`, `move`, `delete`, splitting via `new-sm`.
- **Clipboard reuse** — `copy`, `cut`, `paste`.
- **Correction / exploration** — `undo`, `redo`.
- **Gesture craft** — border resize, body drag, multi-select, and the `press`/`drag`/`release`,
  `double-click`, `edit`/`edit-text`, `select-all` granularity.
Plus the final diagram's `C` (inherited).

**Not measured:** wall-clock time, raw script length, coordinate/geometry values, and text content
(the diagram metric already scores the finished text). `D` cares about the *shape* of the process.

## 3. The metric
`D(S) = C_final + P`, where `P = w_O·O + w_H·H + w_R·R + w_U·U + w_E·E + w_G·G + w_Δ·Δ`, and the
*farmable* part of `P` is scaled by the trajectory **directedness** `η` (§3.5).
```
  D = C_final                              inherited outcome (COMPLEXITY.md)
    + O  operation breadth (variety of verbs/tools)
    + H  how-used (the usage pattern richness of each tool)
    + R  restructuring (reparent / move / delete / split)
    + U  reuse (copy / cut / paste of substructures)
    + E  TRANSITION-DRAWING craft (routing, endpoints, self-loops, crossings, labels)  ◀ emphasised
    + G  gesture craft (resize, multi-select, drag, gesture granularity)
    + Δ  complexity trajectory — productive refactoring dips
    ─── all damped by η = net progress / total motion (parasite oscillations → η→0)
```
Drawing a **transition** is the hardest thing to draw well — and the operation that exercises the
editor's most intricate code (endpoint border-attachment, polyline routing, self-loop geometry,
cross-boundary edges, label placement). So it is pulled out of the generic gesture craft into its own
**emphasised** component `E` (§3.7), weighted above the rest: the drawings that route transitions
richly are the ones that most stress the editor and make the best tests.
`C_final` ties the drawing to what it produced; `P` scores the process beyond it. Provisional weights.

**Subgraph-argument principle.** Operations that act on a *subgraph* — restructuring (`R`) and
clipboard reuse (`U`) — are weighted by the diagram complexity `C(g)` of the subgraph `g` they operate
on (its state at the moment of the operation, via script replay), **not** by flat per-operation
constants. Reparenting a deep composite, or reusing a rich substructure, is a significant real task;
moving one simple state, or copy-pasting a single node, is not. This is combined with **diminishing
returns on repetition** (the same argument repeated decays geometrically) so that *parasite* spam —
moving a state back and forth, or pasting the same trivial thing many times — cannot inflate `D`.

### 3.1 O — operation breadth
`O = 1.0·(distinct model verbs used) + 1.0·(distinct manipulation actions used) + 0.05·min(total ops, 200)`.
Rewards using *many kinds* of operations; the small volume term keeps a longer varied session above a
short one, but cannot dominate (capped). "Manipulation actions" are the `toolcover` set
(`draw-transition`, `copy-paste`, `drag-state`, `resize-state`, `move-endpoint`, `add/move/remove-point`,
`edit-*`, `click-delete`).

### 3.2 H — how the tools are used
Reuse the polygon's usage patterns (`toolcover.py`: `single < several < in-container < combined`, plus
`extreme`, `then-undo`). For each tool, credit the richest pattern observed:
`single 0.5, several 1.0, in-container 2.0, combined 3.0`, and `+1.0` each if also used at an
**extreme** position or immediately **undone/redone**. `H = Σ_tools richest-pattern-credit`. Using a
tool inside a container, or combined with a transition/comment/paste, is worth more than placing it
alone.

### 3.3 R — restructuring
Changing structure after it exists. These operations exercise the editor's **most complex,
defect-prone code** — coordinate re-basing, parent auto-grow, region reassignment — so they are
**positively supported for their own sake**, on top of a scaling by the argument subgraph's
complexity `C(g)`:
- **`reparent`** of `g`: `1.0 + 0.8·C(g)` — the highest-weighted operation. Moving an element across a
  container boundary re-bases its coordinates, regrows the old and new parents and reassigns its
  region; even reparenting one state hits that code, and reparenting a deep composite stresses it hard.
  (These are exactly the paths that surfaced the grow/re-base defects — so the drawings that reparent
  make the best tests.)
- **`new-sm`/wrap** that adopts existing content: `1.0 + 0.5·C(g)` (adopting a border-less machine also
  re-bases every child).
- **`move`** of `g` across a boundary: `0.5·C(g)`; a plain in-place move: small.
- **`delete`** of `g`: `0.3·C(g)` (a fraction of what was removed).

Reparent and wrap carry a **base** so they are rewarded even on a simple argument (the code path is the
point); the `C(g)` term adds more for richer subgraphs. A genuine reparent *changes* `C_final` (a
deeper element is amplified by COMPLEXITY.md §4.2), so it is real progress; a reparent-and-back nets
nothing and is damped as oscillation (§3.5) — the base cannot be farmed.

### 3.4 U — clipboard reuse (and the parasite question)
`copy`/`cut` of a subgraph `g`: `0.2·C(g)` (capturing a complex structure is itself work). `paste` of
`g`: `ρ·C(g)`, `ρ = 0.4`. The weight is a **fraction** of the structure's complexity, and only that,
because the structure is already counted once in `C_final` — the second copy was *"already covered"*
by building the first, so pasting must not double-count it. What the paste *does* add is the cost of
handling reuse, which scales with `C(g)`: a **parasite** copy-paste of a trivial node (`C(g)≈0`) is
worth almost nothing, while reusing a rich substructure — a genuine real-life task — is significant.
Repeated pastes of the same source decay geometrically (`ρ·C(g)·γ^{k−1}`, `γ = 0.5`), and any
paste-then-undo thrash is caught globally as complexity oscillation (§3.5) — so paste spam cannot
inflate `D`. And hand-building the same structure earns comparable credit through
`O`/`H` (it exercises the creation tools instead) — so building by hand and reusing by clipboard land
close, neither dominating, exactly as they should.

### 3.5 η — directedness (parasites as complexity oscillations)
Parasite actions — the ones an agent would spam to game the score — are exactly the ones that make the
complexity **oscillate**: an `add` then `delete`, a `move` and back, a `paste` then `undo`, or an
`undo`/`redo` cycle climbs `C` and immediately throws it away. So parasites are caught not by ad-hoc
per-verb rules but by the shape of the `C` trajectory (§3.8):
- **total motion** `TV = Σ_i |ΔC_i|` — every up and down;
- **net progress** `NP = C_final`;
- **oscillation (waste)** `W = TV − NP ≥ 0` — motion that produced nothing lasting;
- **directedness** `η = NP / TV ∈ (0,1]` — `η = 1` for a purely constructive build, `η → 0` for a
  session that thrashes without progress.

The *farmable* credit — the volume term of `O`, the base of `U`, and any per-op counts — is scaled by
`η`, so paste-spam, add/delete churn and undo/redo loops (high `W`, `η → 0`) cannot inflate `D`. This
subsumes what a separate "undo/redo" term would do: correction is neither rewarded nor forbidden — a
revert that returns to an earlier state is oscillation (damped), a revert that redirects to a better
result shows up as net progress and a productive dip (§3.8). A **productive dip** ends *higher* than it
started, so its motion is mostly `NP` and it barely dents `η`; only motion returning to an
already-visited level is waste. `η` is the single principled parasite defense; the subgraph-`C`
weighting of `R`/`U` (§3.3–3.4) is complementary (it makes *trivial* arguments cheap in the first
place).

### 3.6 G — gesture craft (non-transition)
`G = 1.0·(distinct fine manipulations: resize-state, drag-state, multi-select) + 2.0·(gesture verbs /
(gesture verbs + model verbs))`. Rewards low-level drawing craft — resizing borders, dragging bodies,
box-selecting — over pure semantic creation. (Transition-shaping gestures live in `E`, §3.7.)

### 3.7 E — transition-drawing craft (emphasised)
Drawing and shaping a **transition** is the process's most demanding craft and the operation that most
stresses the editor's drawing code. Each hand-drawn transition and every way it is shaped is credited;
weights lead the other components:
- **`+1.5` per transition drawn by hand** (the `transition` tool / `draw-transition` / a `new-transition`
  with a drag) — picking a source and dragging to a target, with endpoint border-attachment, is far
  more involved than clicking a state into place.
- **`+1.0` per self-loop** drawn (`source == target`) — the border-to-border loop routing.
- **`+1.0` per boundary-crossing** transition drawn (endpoints under different containers) — routing
  across the hierarchy.
- **`+0.5` per endpoint re-attachment** (`move-endpoint`) — re-anchoring an end to a border (the
  forward-ray attachment code).
- **`+0.5` per polyline vertex edit** (`add-point` / `move-point` / `remove-point`), with diminishing
  returns per edge — hand-routing a clean orthogonal path.
- **`+0.5` per transition to/from a pseudostate or choice** — varied, branching endpoints.
- **`+0.3` per label placed or dragged** (`label`).

So a drawing that routes transitions richly — polylines, moved endpoints, self-loops, cross-boundary
edges, placed labels — scores well above one that drops auto-attached straight edges, and it exercises
exactly the endpoint / polyline / self-loop code this project has been hardening. `E` is subject to the
same `η` damping (a draw-then-delete-edge thrash is oscillation) and its per-edge shaping decays so a
single edge cannot be farmed by nudging its points forever.

### 3.8 Δ — the complexity trajectory (progress toward the maximum, and when to stop)
Track the diagram complexity after every operation: `C_0 = 0 → C_1 → … → C_n = C_final` (from the
per-step replay dumps, §6). It is measured two ways.
- **Progress toward the maximum.** `C_final` is the peak the process reached, and the session is
  ultimately judged on how high it climbed. Individual operations may *lower* `C` locally — a `delete`,
  an `undo`, or a **restructuring** that simplifies before rebuilding better — and that is expected and
  even valued: a *productive dip* (a local `ΔC < 0` that later recovers **above** its pre-dip level) is
  a **refactoring** move, the signature of sophisticated drawing rather than linear accretion. Credit
  each recovered dip `Δ += 0.5·(dip depth)`; a dip never recovered simply lowered the outcome and earns
  nothing. So the trajectory rewards reaching a high `C` *through* non-monotonic, restructuring paths,
  not only monotonic add-add-add.
- **When to stop.** `C` is unbounded — one can always add another element — so there is no intrinsic
  terminal complexity. The stopping signal is the **marginal gain** per operation, `ΔC_i` (and `ΔD_i`):
  while each step still adds *complexity-dense* structure (a nesting level, a new combination, a rich
  reuse) the marginal gain stays high; once the agent only adds flat siblings or churns, it collapses
  and the diagram is **saturated** for its scope. A drawing should stop at the **knee** of its
  cumulative-`C` curve — where added elements stop being interesting and become noise. The metric
  exposes this marginal-gain curve; the actual stop is a budget or target the polygon strategy sets
  against it (§8) — the key open lever for the strategy document (§9).

### 3.9 Bands (provisional, calibrated in §7)
| D − C | process band | shape |
|---|---|---|
| 0–3 | minimal | a straight linear build, one tool kind |
| 3–12 | plain | several tools, a little refinement |
| 12–30 | rich | varied tools, some restructuring or reuse, patterns exercised |
| >30 | intricate | heavy restructuring + reuse + exploration + gesture craft |

(The band reads the *process* surplus `P = D − C`, so a small diagram drawn intricately is still
"intricate".)

## 4. Relation to the diagram metric (inheritance)
`D = C_final + P` makes the diagram complexity a direct term of the drawing complexity — the two
metrics compose. Two readings are reported: the absolute `D` (the polygon-climb signal, rewarding both
a complex outcome and a rich process) and the **process intensity** `P/C` (how much extra process per
unit of outcome — a linear build has low intensity, a restructured/reused build high). A multiplicative
form `D = C·(1 + P/scale)` is an alternative (§9).

## 5. Illustrative examples (schematic)
The first three build the **same** diagram (identical `C`); `D` orders the drawings.
- A **minimal linear build** (`new-state` ×n, `new-transition` ×m) → `D ≈ C` plus a small `O`; process
  band **minimal**.
- The **same diagram via reuse** — build one substructure, `copy`/`paste` it, then `reparent` a state
  and fix a mistake with `undo`/`redo` → same `C`, but `U` and `R` raise `P`; the brief undo/redo is a
  small oscillation, lightly damped by `η`; band **rich**.
- A **parasite session** — the same diagram, then 50× `paste`/`undo` and states shuffled back and forth
  → identical `C`, huge `W`, `η → 0`; the farmed credit collapses and `D ≈ C`: no reward for thrash.
- A **tour/drill session** systematically exercising every tool in several patterns (single, several,
  in-container, combined, extreme, then-undo) → large `O`, `H`; band **intricate** even at modest `C`.
- A **transition-rich drawing** — the same states, but every edge hand-drawn and shaped: polylines
  routed, endpoints dragged onto borders, a self-loop, cross-boundary edges, labels placed → large
  `E`; band **intricate**, and it stresses exactly the edge-drawing code.

## 6. Computation
Parse the `session.script` (one verb per line; classify by `catalog/operations.json` `form` and the
`toolcover` action map), compute `C` on the final diagram with `tests/polygon/tools/complexity.py`, and
aggregate the process components. `O`/`H`/`G`/`E` come from the verb stream (`E` also reads the final
diagram to classify each drawn edge — self-loop, boundary-crossing, pseudostate endpoint); the
trajectory (`Δ`, and `TV`/`NP`/`η`) and `R`/`U`'s argument complexity **`C(g)`** need the per-step
replay dumps —
the complexity of each reparented / moved / deleted / copied / pasted subgraph at the moment of the
operation — obtained by **replaying** the script through the editor batch and dumping between
operations (the runner already does step dumps), then running `complexity.py` on the affected subtree
(a pasted subtree persists in the final diagram; a deleted one is read from the pre-delete dump). Where
a polygon run already emitted `coverage.json` (verb×kind, verb pairs) and `toolcover.json`
(tool|pattern), `O`/`H` can be read from them directly. Proposed tool:
`tests/polygon/tools/drawing_complexity.py`, taking a `session.script` (+ its per-step dumps).

## 7. Calibration
Computed with `tests/polygon/tools/drawing_complexity.py` over the sample scripts
(`tests/scripts/*.script`) and the 161 recorded sessions (`tests/polygon/sessions/*/script`).

**First pass — the process surplus `P` (verb-stream terms `O`/`H`/`E`/`R`/`U`/`G` + an undo/redo
oscillation proxy for `η`).** The labelled sample scripts confirm each component fires on its own
operation type (`reparent.script` → `R`, `copy-paste.script` → `U`, `add-transition` / `transition-
notation` → `E = 3.0 / 6.0`, `gestures-undo` → `η = 0.20`). Over the 161 sessions `P` spans 0–97
(median ≈ 11) with bands **minimal 10 / plain 80 / rich 45 / intricate 34** — the focused missions sit
in *plain*, while the drills, tours and explorations reach *rich*/*intricate* (top: a restructuring
drill at `P = 97`, `R = 17`, `E = 51`). The emphasis holds: transition-heavy drawings lead on `E`, and
undo/redo-heavy ones are damped by `η`. The provisional weights are kept — they order the corpus
sensibly.

**Still pending for a full `D` (version 0.2):** the inherited `C_final`; the `C(g)`-weighting of
`R`/`U` and the exact `Δ`/`η` trajectory (both need the per-step replay dumps, §6); and precise
attribution of raw `press`/`drag`/`release` edge-shaping to `E` (the verb-stream pass folds it into
`G`). These raise `E`/`R`/`U` for the richest sessions but do not change the ordering already seen.

## 8. Forward use (non-normative)
The polygon strategy (third document) will reward raising **both** `C` and `D`, using the `P`
breakdown to push the agent toward thin process dimensions (e.g. "you never restructured — try building
this by reparenting", "reuse a substructure with copy-paste"), extending `productivity.json` /
`coverage.json` with drawing complexity as a measured dimension. Because `D` ranks the alternative
drawings of one target (§1), the strategy can also draw a target several ways and **keep the
highest-`D`** attempt — the one that stresses the most editor code — as the retained test.

## 9. Open questions for review
1. Additive `D = C + P` (drafted) vs multiplicative `D = C·(1 + P/scale)`.
2. **Directedness `η`.** Which components it should scale (drafted: the farmable ones — volume-`O`,
   `U` base, per-op counts — but not the variety of `O`/`H`); whether it damps linearly (`×η`) or more
   sharply (`×η²`); and whether pure oscillation should be merely damped (drafted) or actively
   penalised (`D` dips below a clean build).
3. The reuse fraction `ρ` and decay `γ` (drafted 0.4 / 0.5): they set how close copy-paste of a
   structure lands to hand-building it, and how fast paste-spam is damped. Confirm the intent that the
   two paths are comparable (neither wins) and that only *complex* reuse scores.
4. The restructuring weights — `reparent` `1.0 + 0.8·C(g)`, `wrap` `1.0 + 0.5·C(g)`, `move` `0.5·C(g)`,
   `delete` `0.3·C(g)`: is the reparent boost (and its base-vs-`C(g)` split) right?
5. Volume vs variety weighting in `O` (drafted: variety-led, volume capped).
6. **Transition-drawing emphasis (`E`).** The per-edge weights (`1.5` draw / `1.0` self-loop / `1.0`
   crossing / `0.5` endpoint / `0.5` polyline point / `0.3` label) and how far `E` should lead the
   other components — is drawing transitions the single most-weighted craft, as drafted?
7. Weight values for O/H/R/U/E/G/Δ — all provisional (§7).
8. **The trajectory and termination.** Reward productive refactoring dips (drafted) — and, since `C`
   is unbounded, where should the *stop* decision live: does the drawing metric merely expose the
   marginal-gain curve (drafted) and leave the "knee"/budget to the strategy document, or should `D`
   itself carry a saturation term that penalises low-marginal-gain tails so an over-long spam session
   scores *below* a tighter one?
