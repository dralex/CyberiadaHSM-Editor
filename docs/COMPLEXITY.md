# HSM Diagram Complexity Metric

**Overview:** a reference-free number `C(D)` measuring the structural and behavioural richness of a
CyberiadaML diagram — its element kinds and counts, the **non-linear** effect of nesting, the
behaviour carried by states and transitions (triggers, guards, action code), element naming, multiple
machines and submachine references, and the hard combinations of these (branching, boundary-crossing).
Its purpose is to make "a more complex, more interesting diagram" measurable.
**Document version:** 0.2 (2026-09-25) — DRAFT; weights provisional pending corpus calibration (§7).
**Related authorities:** `EDITOR-SPEC.md` (element vocabulary), PNST 1044-2025 §6/§8 (element
definitions).

## 1. Purpose and properties
`C(D)` scores a *finished* diagram and is:
- **Reference-free** — from the diagram alone, no golden file (like the polygon oracles).
- **Decomposable** — reported as a category breakdown (kinds, nesting, actions, naming, transitions,
  combinations, references, variety), so any score is explainable.
- **Non-linear** — nesting **amplifies** the complexity of the elements it contains: the same
  sub-structure placed one level deeper is worth more (§4.2). Behavioural richness and branching are
  likewise super-linear.
- **Not strictly monotone per edit** — while authoring, a *restructuring* step (e.g. wrapping states
  in a new composite, or splitting a machine) may transiently lower `C`; that is expected. What matters
  is that the **finished** diagram scores higher the richer it is.

Non-goal: `C` is not a quality or UML-correctness score. A large but dull diagram (100 sibling simple
states) must score below a small rich one (a choice branching into nested composites with guarded
internal transitions).

## 2. What is measured — and what is not
**Structural:** element kinds and counts; nesting (amplified); number of state machines; submachine
references (internal and external).
**Behavioural:** state actions (entry / exit) and **internal transitions**, and transition
triggers / guards / behaviours — scored by guard presence, **number of action parts**, and behaviour
**text length**; an internal transition weighs **more** than an entry/exit behaviour.
**Naming:** whether an element carries a name, and the name's length.
**Combinations:** choice/branch fan-out (super-linear), boundary-crossing transitions, and the
complexity a submachine inherits from the machine it references.

**Not measured:** geometry (position, size, polyline shape), colour, and comment *body* prose.
**Concurrency is out of scope:** `fork`/`join` are reserved in the standard (PNST 1044 Table 3) and
absent from the model, and orthogonal regions exist only to host them — so a diagram cannot express
true concurrency and there is nothing to score there.

## 3. Element vocabulary (16 kinds)
| Kind | Geometry | Nests? | Note |
|---|---|---|---|
| simple state | rect | no | the unit of behaviour |
| composite state | rect | yes | opens a nesting level (amplifier) |
| submachine state | rect | entry/exit pts | reference to another machine |
| state machine | rect | yes | a top-level scope; >1 = multi-machine |
| initial | point | no | required (≥1 per machine) |
| final / terminate | point | no | terminal / hard stop |
| choice | rect | no | guarded branch (fan-out) |
| shallow / deep history | point | no | remembered configuration |
| entry / exit point | point | no | named boundary of a (sub)machine |
| comment / formal comment | rect | no | annotation / machine-readable meta |
| transition | edge | no | event `[guard] / behaviour`, local or external |

## 4. The metric
`C(D) = Σ_machines C_machine + M + V`, where per machine
`C_machine(SM) = struct(SM) + Σ_transitions trans(t) + combo(SM)`.
`M` is the document-level machine/reference coupling (§4.7); `V` the variety bonus (§4.8).

### 4.1 Element intrinsic score
Every element has an intrinsic score `int(e) = kind(e) + name(e) + act(e)`:
- `kind(e)` — the kind weight (§4.3);
- `name(e)` — the naming score (§4.4);
- `act(e)` — the state action/behaviour score (§4.5); 0 for non-states.

### 4.2 Structural complexity with nesting amplification (the non-linearity)
`struct` is computed bottom-up; a container amplifies its children by **β = 1.3**:
```
  struct(leaf)      = int(leaf)
  struct(container) = int(container) + β · Σ_children struct(child)
```
So an element at nesting depth `d` contributes `int(e)·β^d` — **super-linear in depth**: the same
sub-tree one level deeper is worth 1.3× more, two levels 1.69×, three 2.2×. Nesting therefore
multiplies, not merely adds. (β applies to composite states, submachine states and the state machine —
the collection kinds.)

### 4.3 Kind weights (provisional)
| kind | w | kind | w | kind | w |
|---|---|---|---|---|---|
| simple state | 1.0 | choice | **4.0** | entry/exit point | 2.0 |
| composite state | 2.0 | shallow history | 3.0 | comment | 0.5 |
| submachine state | 4.0 | deep history | 4.0 | formal comment | 0.25 |
| initial | 0.5 | terminate | 1.5 | state machine | 2.0 |
| final | 1.0 | | | | |

### 4.4 Naming — `name(e)`
`name(e) = 0` if unnamed, else `0.5 + 0.1·min(words(name), 6)`. A named, descriptive element is
richer than an unnamed placeholder; the length term is capped so verbosity cannot dominate. (Applies
to named-able elements — states, submachine, choice; comments carry a *body*, scored as text under
§4.5 only where it is behaviour, not prose.)

### 4.5 Action & behaviour complexity — for states and transitions
Two reusable sub-scores measure the *content* of an action, so text length is significant here:
- `guard(g)  = 0` if absent, else `0.5 + 0.02·min(chars(g), 50)` (cap +1.0).
- `beh(b)    = 0` if absent, else `0.3·min(parts(b), 10) + 0.01·min(chars(b), 200)` — `parts(b)` is the
  number of behaviour statements (newline / `;`-separated). More parts and longer code score higher.

State action score `act(state)` sums over the state's actions:
| action | contribution |
|---|---|
| entry | `0.4 + beh(behaviour)` |
| exit | `0.4 + beh(behaviour)` |
| **internal transition** | `1.0 + trigger?0.3 + guard(g) + beh(b)` |

The internal-transition **base (1.0) exceeds** the entry/exit base (0.4): an internal transition
(event + guard + behaviour staying in the state) is genuinely harder than a plain entry/exit action.

### 4.6 Transition complexity — `trans(t)`
`trans(t) = 1.0` (base) plus:
| signal | + | signal | + |
|---|---|---|---|
| has trigger (event) | 0.4 | self-loop (`src==tgt`) | 0.5 |
| guard `guard(g)` | see §4.5 | local (in-region) type | 1.0 |
| behaviour `beh(b)` | see §4.5 | endpoint is a pseudostate (per end, max 2) | 1.0 |

### 4.7 Combinations — `combo(SM)` and `M`
The disproportionately hard parts:
- **Branch fan-out** — a `choice` with `k ≥ 2` outgoing branches adds `1.0·k·(k−1)/2`; an ordinary
  state with `k ≥ 3` outgoing transitions adds `0.3·k·(k−1)/2`. Choice branching is deliberately the
  steepest term — a wide choice dominates the score.
- **Boundary-crossing transition** — endpoints at different nesting depths or under different
  composite parents add **+1.5** each (crossing hierarchy boundaries is hard to reason about).
- **References — `M`** (document level): `3.0·(machines−1)` for multi-machine, `2.0` per *internal*
  submachine reference plus `τ·C_machine(referenced)` with **τ = 0.3** (a submachine inherits a
  discounted share of the machine it points to), and `4.0` per *external* `file://` reference (opaque
  coupling).

### 4.8 Variety — `V`
`V = 1.0 · (distinct element kinds present, excluding transition and the meta comment)`. Rewards a
diagram that exercises *many* kinds over one that repeats a single kind — the "interesting" signal.

### 4.9 Breakdown and bands
`C` is reported with its category breakdown (structure incl. nesting amplification, naming, state
actions, transitions, combinations, references, variety). Provisional bands (recalibrated in §7):

| C | band | shape |
|---|---|---|
| 0–4 | trivial | empty / one state |
| 4–15 | simple | a flat few-state machine |
| 15–40 | moderate | nesting, a choice, some guarded actions |
| 40–90 | complex | history / submachine / multi-machine, rich behaviour |
| >90 | extreme | deep multi-machine orchestration |

## 5. Worked examples (illustrative, uncalibrated)
- `empty.graphml` (meta only) → `C ≈ 0` — trivial.
- `semaphore` (3–4 named states, a cycle of triggered transitions, one initial) → modest structure +
  transitions + variety → **simple**.
- `vacuum-robot` (nested composites, a choice, entry actions) → nesting amplification + choice fan-out
  + action text → **moderate**.
- a Berloga program (guarded internal transitions, choice branches) → behaviour-heavy → **moderate–
  complex**, driven by §4.5/§4.7 more than raw count.
- `orchestrate-grand` (four machines, submachine refs, depth) → `M` + amplified `struct` → **extreme**.

## 6. Computation
Implemented (later) as `tests/polygon/polygon/complexity.py` over the `dump.py` tree
(`Document.machines()`, `Element.walk()/children/actions`, `machine.transitions()`), or directly on
`libcyberiadamlpp[-py]` (`find_elements_by_type[s]`, `get_substates`, `get_transitions`,
`get_state_machines`, `has_initial`), with the bottom-up recursion (§4.2), fan-out and boundary tests
computed while walking. Emits the breakdown and `C`. A CLI tabulates `C` over a diagram set.

## 7. Calibration plan
Compute the breakdown over the corpus diagrams (`tests/polygon/corpus/`, `hsm-arduino-course/
diagrams`, `ad-statistics`, `hsm_robot_ros_generator`) and tune weights (kind table, β, the naming /
guard / behaviour coefficients, fan-out weight, τ, band thresholds) so that: the ordering is intuitive
(`empty < semaphore < vacuum-robot < a Berloga program < orchestrate-grand`); no trivial diagram lands
in "complex" and no `orchestrate-*` below "moderate"; and every category is exercised by some corpus
diagram. Record the calibrated tables as version 0.3.

## 8. Open questions for review
1. Weight values and β / the naming and behaviour coefficients / fan-out weight / τ — all provisional
   (§7).
2. Nesting amplification β: geometric per level (β^d) as drafted, vs a steeper (e.g. β growing with
   fan-out) or gentler curve.
3. Referenced-machine inheritance (τ·C) vs treating every submachine as opaque.
4. Naming length: word-count-capped as drafted, or drop the length term and keep only present/absent.
