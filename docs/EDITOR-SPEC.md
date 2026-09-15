# The Cyberiada HSM Editor — Behavioural Specification

**Overview:** The document contains the requirements for the possible/required
editor behaviour. This document is used as the root specification for the test systems
used within the project.

**Document version:** 0.2 (2026-09-15)

**Related authorities:**

- PNST 984-2024 — the HSM (ПРИМС) diagram semantics (states, regions,
  pseudostates, transitions); the source for the structural rules.
- PNST 1044-2025 — the CyberiadaML-GraphML serialization; checked by the
  separate `cyberiadaml-compat-tests` suite. This document does **not**
  re-check the format; it checks only that the editor round-trips through it.
- UML2 Statecharts (`docs/UML2-statecharts.pdf`) — the semantic background.

## 1. Purpose

The Cyberiada-GraphML format has a standard and a conformance suite; the editor's
*behaviour* should have also. The editor's testing system requires the ground truth to
check the operations results which user sees on the screen and evaluates according to her
semantical estimations.

This document externalises that specification: the requirements the editor must
satisfy, each written so a test can check it against the batch dump. It is the
single source for both test suites:

- the good-file `ctest` layers (`docs/TESTING.md`) — fixed cases per requirement;
- the exploratory polygon (`docs/POLYGON.md`) — the requirements become the
  standing laws its oracles check after every operation.

## 2. How a requirement is checked

Every requirement carries a **level** — MUST (required), SHOULD (recommended), MAY
(allowed) — and a **check kind** that says how it connects to the offscreen behaviour in
the dump:

- **[C] crash-free** — the operation completes and leaves a loadable document;
  checked by running it and inspecting the exit and the reload.
- **[U] universal law** — a property quantified over the *whole* model or scene
  that must hold after *any* operation (an editing-mode invariant — see Modes).
  Checked by graph/rectangle arithmetic on the dump, with no prediction of the
  specific operation — this is the machine form of a person's "that looks wrong".
  A [U] law names the exact offending pair, so it reproduces minimally.
- **[A] action-effect** — the specific result an operation must produce (a
  placement, an offset, a rename). Checked by predicting it in the shadow model
  and comparing.
- **[I] identity** — undo/redo or save/reopen returns the prior state exactly.

Requirement identifiers use `EDIT-<AREA>-<n>`.

A requirement records its **source** (an authority clause, or *design* for editor-specific
behaviour) and its additional **status** - *open* (still in discussion, temporary
exception, etc.)

### Editing and inspection modes

Two modes decide whether the geometric requirements are enforced:

- **Editing mode** (the default): operations mutate the document and the editor's *recovery*
  keeps it spec-compliant — a container grows to fit its content (`EDIT-NODE-2`), a grown or
  reparented element pushes its siblings clear (`EDIT-NODE-6`), and the geometry is
  reconstructed on request. The `[U]` geometric laws hold after every operation because this
  recovery maintains them.
- **Inspection mode**: the document is read-only and drawn as faithfully as possible to its
  stored geometry — the recovery is suspended, so a loaded diagram may break the `[U]`
  geometric laws and is shown as-is (see §4.10). No operation runs, so nothing enforces or
  breaks a law.

A `[U]` geometric law is therefore an **editing-mode** invariant, and the test system checks
it in editing mode; inspection mode is where a non-compliant diagram is read without change.

## 3. Where the specification sits

```
  authority                   this specification              checks
 ┌───────────┐  semantics    ┌──────────────────┐  law kind  ┌──────────────┐
 │ ПНСТ 984  │──structure───▶│ STRUCT  SEM      │───[U]─────▶│  model+scene │
 │ UML2      │               ├──────────────────┤            │  dump        │
 ├───────────┤  geometry     │ NODE  EDGE  TEXT │───[U]/[A]─▶│              │─▶ polygon laws
 │ editor    │──interaction─▶│ TOOL  INSPECT    │───[A]─────▶│  graph +     │   (every round)
 │ design    │  history      │ HIST             │───[I]─────▶│  rectangle   │
 ├───────────┤               ├──────────────────┤            │  arithmetic  │─▶ ctest cases
 │ ПНСТ 1044 │──round-trip──▶│ ROBUST IO META   │───[I]/[C]─▶│              │   (per requirement)
 └───────────┘               └──────────────────┘            └──────────────┘
```

**Dump implementation.** The batch dump already exposes every graphical object offscreen —
`== document` (the model tree: ids, names, actions, geometry) and `== scene` (every scene
item with its role and `pos`/`rect`, absolute rect computable). A [U] law is a predicate
over those rows; nothing more than the existing dump is needed.

## 4. Requirements

### 4.1 Robustness — ROBUST

- `EDIT-ROBUST-1` MUST [C]: every editing operation on a valid document
  completes without crashing or hanging. *design.*
- `EDIT-ROBUST-2` MUST [C]: after any sequence of operations the document stays
  internally consistent and can be saved and reopened. *design.*
- `EDIT-ROBUST-3` MUST [C]: an operation that cannot apply is refused with no partial
  document mutation. The cancellation could be followed by an error (a second initial
  pseudostate per level) or be silent (a bad/repeated state name). *design.*

### 4.2 Structure and semantics — STRUCT / SEM

- `EDIT-STRUCT-1` MUST [U]: every element id is unique in the document. *PNST 1044*
- `EDIT-STRUCT-2` MUST [U]: a state's name is non-empty and unique among its
  siblings; a pseudostate may keep an empty name; if set, it must follow the same
  uniqueness rule. *PNST 984*
- `EDIT-STRUCT-3` MUST [U]: the containment tree is a forest rooted at the state
  machines — one parent per element, no cycle. *PNST 1044*
- `EDIT-STRUCT-4` MUST [U]: a transition's source and target resolve to elements
  of the same state machine. *PNST 1044*
- `EDIT-STRUCT-5` MUST [U]: deleting an element removes its whole subtree and
  every incident transition — no dangling endpoint. The only exception is a state
  machine. Deleting a state machine just removes the geometry and makes the element
  invisible. *design*
- `EDIT-STRUCT-6` MUST [U]: a state with children is composite, a leaf is
  simple and is a state or a pseudostate. *PNST 984*
- `EDIT-STRUCT-7` MUST [U]: a state might contain several blocks: a header block with the
  state name, entry/exit behavior blocks with actions, internal transition blocks with
  actions, and for composite states - a content block containing a region with the
  children. *PNST 984*
- `EDIT-STRUCT-8` MUST [U]: a comment may carry subjects — a link to a target element, to
  a fragment of a target's name, or to a fragment of its data. *PNST 984*
- `EDIT-STRUCT-9` MUST [U]: deleting an element strips every subject that referenced it
  (the link goes, the comment stays); a copy or a reparent rebinds the subject to the
  moved or copied element. The link itself is drawn as a line ending in a black box
  (`EDIT-EDGE-6`). *design*
- `EDIT-SEM-1` MUST [U]: at most one initial pseudostate per region (per level);
  a second is refused. *PNST 984*
- `EDIT-SEM-2` MUST [U]: transition endpoints follow the rules — a source is a state,
  an initial pseudostate or a choice; a target is a state, a final state, a choice or a
  terminate. *PNST 984*
- `EDIT-SEM-3` SHOULD [U]: a choice pseudostate's outgoing transitions are
  guarded. Only one guard is allowed to have `else` guard. *PNST 984*

### 4.3 State machines/states/pseudostates/comments geometry and layout — NODES

The `[U]` laws below are editing-mode invariants: the editor's recovery maintains them after
each operation (grow-to-fit, push-siblings). Inspection mode suspends them and shows the stored
geometry as-is (see Modes and §4.10).

- `EDIT-NODE-1` MUST [U]: a parent's rect contains every child's rect with a
  margin. The parent's rect should consider the actual region rect available for children
  (the whole rect minus a header, other blocks, etc.) *design*
- `EDIT-NODE-2` MUST [U]: when an operation would leave a child outside its parent, the
  editor grows the parent to fit; a parent never shrinks below its content. Adding new
  content (by `move` or `new-state`) grow the parent (see also `EDIT-NODE-5`). *design*
- `EDIT-NODE-3` MUST [A]: an element's point is its centre; a top-level element
  is in absolute coordinates, a nested one relative to its parent's centre.
  *design*
- `EDIT-NODE-4` SHOULD [U]: a state machine without a rect frames the union of
  its content and sits at the origin. *design*
- `EDIT-NODE-5` MUST [A]: a state, a comment, and a state machine resize directionally - to
  the side of growth, clamped to the content. *design*
- `EDIT-NODE-6` MUST [U]: sibling elements do not overlap. *design*
- `EDIT-NODE-7` MUST [U]: If an element is separated from parent it should be placed on
  the top level of hierarchy not overlapping the parent and the rest of the elements, but
  close to the parent. *design*
- `EDIT-NODE-8` MUST [U]: a state's blocks are stacked vertically top-top-bottom (if
  available): the name block, the entry action block, the internal transition blocks, the
  content block with a region for composite states, the exit action block. If a simple
  state has no actions the header block is not drawn and the state name is drawn in the
  center of a state. *PNST 984*
- `EDIT-NODE-9` MUST [U]: a comment name is shown in the top of the element if set. *PNST
  984*
- `EDIT-NODE-10` MUST [U]: a node with a colour is drawn in it — the outline of a state, a
  composite, a state machine, a choice, a comment or a terminate; the fill of an initial;
  the outer-circle outline and the inner-circle fill of a final. The selection highlight
  overrides it while selected. *design*

### 4.4 Transitions/comment links geometry and layout — EDGES

- `EDIT-EDGE-1` MUST [U]: an endpoint of a rect elements (a state, a comment) lies on the
  elements border. An endpoint without a stored point lies on the border of its node
  toward the other end (a vertex at its centre), within tolerance. *design*
- `EDIT-EDGE-2` MUST [U]: an endpoint of a point-based circle elements (initial
  pseudostate or final state) lies on the border of the drawn element, not the
  center. *design*
- `EDIT-EDGE-3` MUST [U]: an endpoint of the rest point-based elements (terminator) lies
  on the center. *design*
- `EDIT-EDGE-4` MUST [U]: an endpoint of a choice element lies on vertexes of the
  rhombus. *design*
- `EDIT-EDGE-5` MUST [U]: a transition line finishes with an arrow. *PNST 984* 
- `EDIT-EDGE-6` MUST [U]: a comment link line finishes with a black box. *PNST 984* 
- `EDIT-EDGE-7` MUST [A]: a rebind moves the endpoint to the new node; the
  transition then connects the new pair. The identity of endpoints, not the id,
  which may be renamed). *design*
- `EDIT-EDGE-8` MUST [U]: a self-loop stays a loop unless an endpoint is dragged
  onto a non-ancestor node. *design*
- `EDIT-EDGE-9` MUST [A]: adding, moving or removing a polyline point changes
  only that point; the endpoints stay. *design*
- `EDIT-EDGE-10` SHOULD [U]: transition (lines and polylines) should not cross state
  borders if possible *design*
- `EDIT-EDGE-11` MUST [U]: a transition's label geometry is described by a rect while
 editing. *design*
- `EDIT-EDGE-12` SHOULD [U]: a transition's label sits near its path by default.
- `EDIT-EDGE-13` MAY [A]: the label position and size can be changed. *design*
- `EDIT-EDGE-14` MUST [U]: a transition with a colour draws its line and arrow in it; the
  selection highlight overrides it while selected. *design*

### 4.5 Text of names, actions, and labels — TEXT

- `EDIT-TEXT-1` MUST [U]: a state's or a state machine's title lies inside its owner; when
  editing a taller header grows the element and the other roles reflow, none moving
  sideways.  *design*
- `EDIT-TEXT-2` MUST [U]: an action keeps its `entry/ `, `exit/ ` or trigger
  prefix. If the behaviour text fits within the element width it is drawn in a single line,
  otherwise the behaviour text is drawn from the next line. *design*
- `EDIT-TEXT-3` MUST [A]: a title, an action text, or an edge label is edited in place. *design*
- `EDIT-TEXT-4` SHOULD [A]: if an edge label text no longer fits the label rect, the
  text is drawn wrapped. *design*

### 4.6 Tools and interaction — TOOL

- `EDIT-TOOL-1` MUST [A]: a creation tool click creates exactly one element of
  its kind, parented to the element under the press point (a state or a state machine).
  *design*
- `EDIT-TOOL-2` MUST [A]: a rect tool draws a rubber-band rect or places a
  default size on a bare click. *design*
- `EDIT-TOOL-3` MUST [A]: a creation tool reverts to select after one use.
  *design*
- `EDIT-TOOL-4` MUST [A]: copy-paste places a sibling with a fresh id, cleared of
  the source's box so the two do not overlap (`EDIT-NODE-6`), and grows the parent
  to keep the copy inside it. *design*
- `EDIT-TOOL-5` MUST [A]: a state machine is not copyable. *design*
- `EDIT-TOOL-6` MUST [A]: while the selection tool is used a drag moves the element;
  dragging a child past the parent border grows the parent (the interactive side of
  `EDIT-NODE-2`).  *design*
- `EDIT-TOOL-7` MUST [I]: the view tools (pan, zoom) change only the viewport; the
  document model and the scene geometry in the dump are unchanged. *design*

### 4.7 History — HIST

- `EDIT-HIST-1` MUST [I]: undo of an operation restores the exact prior model
  and geometry. *design*
- `EDIT-HIST-2` MUST [I]: redo re-applies it exactly. *design*
- `EDIT-HIST-3` MUST [I]: undo-all returns to the initial document; redo-all to
  the final. Undo is bounded by the first version of the document opened. Redo line is
  dropped as soon as the document was changed right after the undo operation. *design*
- `EDIT-HIST-4` MUST [I]: a canvas gesture (press … release) is one undo step.
  *design*

### 4.8 Persistence — IO

- `EDIT-IO-1` MUST [I]: save then reopen yields the same model and geometry, within the
  format's expressiveness. *PNST 1044.* Note: the format does not distinguish local and
  external transitions, so the transition type is excluded from the identity.
- `EDIT-IO-2` MUST [C]: export to PNG and SVG succeeds for any valid document.
  *design*
- `EDIT-IO-3` MUST [I]: geometry round-trips losslessly *design.*
- `EDIT-IO-4` MUST [I]: an element's colour is a persisted attribute and round-trips
  through save and reopen. *PNST 1044.*
- `EDIT-IO-5` MUST [A]: an element's colour is editable — from the property menu and the
  `set-color` verb — for every node and edge (state, composite, state machine, choice,
  initial, final, terminate, comment, transition); the change is one undo step. *design*

### 4.9 Metainformation — META

- `EDIT-META-1` MUST [U]: the machine-readable metainformation node (the CGML_META formal
  comment: standard version, transition order, event propagation, geometry mode) is not
  drawn on the scene. *design*
- `EDIT-META-2` MUST [A]: the metainformation is edited only through the metainformation
  editor (`update-meta`); it is not an ordinary comment and its body is not editable
  through the comment-body path. *design*
- `EDIT-META-3` MUST [I]: editing the metainformation changes no diagram element and no
  geometry, and the metainformation round-trips through save and reopen. *PNST 1044*

### 4.10 Inspection mode — INSPECT

- `EDIT-INSPECT-1` MUST [C]: in inspection mode the document is read-only — every editing
  operation (create, delete, rename, move, resize, reparent, action edit, paste,
  metainformation) is refused and changes nothing. *design*
- `EDIT-INSPECT-2` MUST [A]: a diagram is drawn from its stored geometry — a composite state's
  region follows the document's region rect, not the layout reconstructed from its title and
  actions; the geometric recovery (the `[U]` NODE/EDGE laws) is suspended, so a diagram whose
  stored geometry breaks them is shown as-is. *design*
- `EDIT-INSPECT-3` MUST [A]: inspection and geometry reconstruction are mutually exclusive — an
  inspected document is never given the geometry it does not have. *design*
- `EDIT-INSPECT-4` SHOULD [I]: leaving inspection mode re-applies the editing layout and the
  recovery — the region follows the text again and the `[U]` compliance laws regain force.
  *design*

## 5. Coverage

Every requirement is covered by at least one case, named by the requirement id,
following the compat suite's discipline: an editing case that drives one
operation and asserts the requirement's law, plus a polygon standing law that
holds it over every exploratory run. A violation names both the requirement and
the operation that broke it.
