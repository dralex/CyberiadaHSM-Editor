# The Cyberiada HSM Editor — Behavioural Specification

**Overview:** The document contains the requirements for the possible/required
editor behaviour. This document is used as the root specification for the test systems
used within the project.

**Document version:** 0.8 (2026-09-26)

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
  an initial pseudostate or a choice; a target is a state, a final state, a choice, a
  terminate or a history pseudostate (shallow or deep). A history pseudostate may also be
  the source of at most one optional default transition; it is not required. *PNST 984*
- `EDIT-SEM-3` SHOULD [U]: a choice pseudostate's outgoing transitions are
  guarded. Only one guard is allowed to have `else` guard. *PNST 984*
- `EDIT-SEM-4` MUST [U]: a submachine state references a state machine by id (internal) or
  URI (external); the only children it may hold are entry/exit points, which act as its
  connection-point references, bound by name to the referenced machine's entry/exit. *PNST 1044 8.1*
- `EDIT-SEM-5` MUST [U]: entry/exit endpoint direction is the mirror of the container — on a
  submachine-state connector an entry point is a transition target only and an exit point a source
  only; a standalone entry/exit point in a state machine is reversed (entry = source only, exit =
  target only). *PNST 984*
- `EDIT-SEM-6` MUST [U]: a shallow history pseudostate restores the last active substate of its
  enclosing region; a deep history restores the full nested active configuration; the single
  optional default transition is taken when the region was never entered. A region holds at most
  one shallow and at most one deep history (one of each); a second of a kind is refused. *PNST 984*
- `EDIT-STRUCT-10` MUST [U]: a state carries at most one `entry/` and at most one `exit/`
  behaviour block (and at most one `do/` where the model supports it). *PNST 1044 6.8.1*
- `EDIT-SEM-7` MUST [U]: an entry point and an exit point carry a non-empty name; a new point
  gets a unique default name. *PNST 1044 8.3.1*
- `EDIT-SEM-8` MUST [U]: a submachine state never references the state machine that contains it;
  an internal reference names a state machine of the document, otherwise the reference is an
  external URI. *PNST 1044 8.1.1*

### 4.3 State machines/states/pseudostates/comments geometry and layout — NODES

The `[U]` laws below are editing-mode invariants: the editor's recovery maintains them after
each operation (grow-to-fit, push-siblings). Inspection mode suspends them and shows the stored
geometry as-is (see Modes and §4.10).

Three geometries are distinct and must not be conflated (PNST 1044 §7.2 defines the first two;
the third is a derived display value the format does not store):

- **element geometry** — an element's own rect (top-left + size) or centre point (`EDIT-NODE-3`)
  + known radius; rect sizes are advisory and the editor may size them to the content (PNST 1044).
- **state-machine geometry** — a state machine's own `dGeometry` rect: its drawn border and the
  frame that lays out its content (`EDIT-NODE-1`, `EDIT-NODE-4`). It is the machine's own rect,
  **not** a box that must enclose everything drawn.
- **document bounding rect** — the union of *everything drawn* across the state machines. It is
  derived, the format stores nothing for it, and it may extend beyond the state-machine rects
  (`EDIT-NODE-12`, `EDIT-NODE-13`).

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
- `EDIT-NODE-5` MUST [A]: a state and a state machine resize directionally - to the side of
  growth, clamped to the content; a comment (a leaf box) resizes on any of its four borders,
  the opposite edge held (no content clamp - it holds no children). *design*
- `EDIT-NODE-6` MUST [U]: sibling elements do not overlap. *design*
- `EDIT-NODE-7` MUST [U]: If an element is separated from parent it should be placed on
  the top level of hierarchy not overlapping the parent and the rest of the elements, but
  close to the parent. *design*
- `EDIT-NODE-8` MUST [U]: a state's blocks are stacked vertically top-to-bottom (if
  available): the name block, the entry action block, the internal transition blocks, the
  content block with a region for composite states, the exit action block. If a simple
  state has no actions the header block is not drawn and the state name is drawn in the
  center of a state. *PNST 984*
- `EDIT-NODE-9` MUST [U]: a comment name is shown in the top of the element if set. *PNST
  984*
- `EDIT-NODE-10` MUST [U]: a node with a colour is drawn in it — the outline of a state, a
  composite, a submachine state, a state machine, a choice, a comment or a terminate; the fill of
  an initial, a history, or an entry/exit pseudostate; the outer-circle outline and the inner-circle
  fill of a final. A history pseudostate is drawn as a small circle like an initial, carrying an `H`
  (shallow) or `H*` (deep) glyph; an entry point is a small empty circle and an exit point a small
  empty circle with an inscribed cross, as on the tool icons (a terminate keeps its heavy cross to the
  corners); a submachine state is drawn like a state carrying a submachine marker.
  The selection highlight overrides it while selected. *design*
- `EDIT-NODE-14` MUST [U]: an entry/exit point that is a submachine state's connector lies on that
  state's border and stays on it when moved; a standalone entry/exit point in a state machine may lie
  on the border or anywhere inside it. A point is not content the border must contain: moving or
  creating one near the border never grows the parent (`EDIT-NODE-2` does not apply), and a point
  does not floor the parent's resize (`EDIT-NODE-5`), like a comment. *PNST 1044 8.1/8.3*
- `EDIT-NODE-11` MUST [A]: an auto-sized comment's box follows its body text; changing the body
  re-lays-out the box so its live geometry matches a save/reopen (keeps `EDIT-IO-1`). *design*
- `EDIT-NODE-12` MUST [U]: the document bounding rect is the union of everything drawn — the
  state-machine rects and every element geometry that reaches past a border: a point pseudostate's
  drawn circle/rhombus, a terminator marker, an entry/exit point, a transition `dLabelGeometry`
  rect, a comment link. It may exceed the state-machine rects. It is derived, not part of the
  serialization (PNST 1044), so the editor recomputes it on load; a stored or cached bounding
  that disagrees with the recomputed one is corrected, never a reason to refuse the document. A
  document with valid element geometry always loads (see `EDIT-ROBUST-2`). *PNST 1044*
- `EDIT-NODE-13` MAY [U]: an element's geometry may reach beyond the state-machine border — a point
  pseudostate (initial/final/choice/history) near the border whose drawn shape overhangs it, an
  `exitPoint`/entry point on the border (PNST 1044), a transition label. The standard imposes
  no containment at the state-machine level, so this is not a violation: `EDIT-NODE-1` containment
  is checked only for the rect children of a rect state, never against the state-machine frame.
  *PNST 984/1044*

### 4.4 Transitions/comment links geometry and layout — EDGES

- `EDIT-EDGE-1` MUST [U]: an endpoint of a rect elements (a state, a comment) lies on the
  elements border. An endpoint without a stored point lies on the border of its node
  toward the other end (a vertex at its centre), within tolerance. *design*
- `EDIT-EDGE-2` MUST [U]: an endpoint of a point-based circle elements (initial
  pseudostate, final state, history pseudostate, or entry/exit point) lies on the border of the
  drawn element, not the center. *design*
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
- `EDIT-EDGE-11` MUST [U]: a transition's label geometry is a rect — the live and persisted
  geometry of the label, migrated from a legacy label point when only a point is stored. *design*
- `EDIT-EDGE-12` SHOULD [U]: a transition's label sits near its path by default.
- `EDIT-EDGE-13` SHOULD [A]: the label position and size can be changed — dragged to move,
  resized by the corner handles under the selection tool, edited in the properties view, and
  driven by the `label <x y w h>` verb. *design*
- `EDIT-EDGE-14` MUST [U]: a transition with a colour draws its line and arrow in it; the
  selection highlight overrides it while selected. *design*

### 4.5 Text of names, actions, and labels — TEXT

- `EDIT-TEXT-1` MUST [U]: a state's or a state machine's title lies inside its owner; when
  editing a taller header grows the element and the other roles reflow, none moving
  sideways. A simple state becoming composite reflows its title the same way. *design*
- `EDIT-TEXT-2` MUST [U]: an action keeps its `entry/ `, `exit/ ` or trigger
  prefix. If the behaviour text fits within the element width it is drawn in a single line,
  otherwise it wraps to the next line at a word boundary. *design*
- `EDIT-TEXT-3` MUST [A]: a title, an action text, or an edge label is edited in place. *design*
- `EDIT-TEXT-4` MUST [A]: if an edge label text no longer fits the label rect, it wraps at
  a word boundary. *design*
- `EDIT-TEXT-5` MUST [U]: an event name is not one of the reserved words `entry`, `exit`, `do`,
  `propagate`, `block`, `defer`, `else`; the reserved event names `ANY` and `UNKNOWN` are
  permitted. *PNST 1044 6.8.1*
- `EDIT-TEXT-6` MUST [U]: `defer` is the whole behaviour of an internal transition of a state and
  never labels a transition; `propagate` and `block` accompany a non-empty event name. *PNST 1044
  6.8.1, 6.8.2*

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
- `EDIT-TOOL-7` MUST [I]: the view tools (pan, zoom — first-class tools with the `pan`/`zoom`
  verbs) change only the viewport; the document model and the scene geometry in the dump are
  unchanged. Zoom-to-content frames a rect-less state machine, and the viewport is restored after
  a layout pass. *design*
- `EDIT-TOOL-8` MUST [A]: with snap-to-grid on (the toolbar toggle), a placed, moved or resized
  geometry snaps to the grid spacing; with it off the geometry is unrounded. *design*
- `EDIT-TOOL-9` MUST [A]: holding `Alt` suspends snap-to-grid while it is held. *design*
- `EDIT-TOOL-10` MUST [A]: holding `Ctrl` locks a move to a single axis (the dominant one). *design*
- `EDIT-TOOL-11` MUST [A]: the submachine-state, entry-point and exit-point creation tools each
  create one element of their kind under the press point; a new submachine state takes a default
  reference set afterward in the property panel. *design*

### 4.7 History — HIST

- `EDIT-HIST-1` MUST [I]: undo of an operation restores the exact prior model
  and geometry. *design*
- `EDIT-HIST-2` MUST [I]: redo re-applies it exactly. *design*
- `EDIT-HIST-3` MUST [I]: undo-all returns to the initial document; redo-all to
  the final. Undo is bounded by the first version of the document opened. Redo line is
  dropped as soon as the document was changed right after the undo operation. *design*
- `EDIT-HIST-4` MUST [I]: a canvas gesture (press … release) is one undo step; a
  transition-drawing gesture commits one undo step per completed transition. *design*

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
  `set-color` verb — for every node and edge (state, composite, submachine state, state machine,
  choice, initial, final, terminate, shallow history, deep history, entry point, exit point,
  comment, transition); the change is one undo step. *design*
- `EDIT-IO-7` MUST [I]: a submachine state's reference and its nested entry/exit points, and a
  standalone entry/exit point, round-trip through save and reopen. *PNST 1044 8.1/8.3*
- `EDIT-IO-6` MUST [A]: an exported image is the diagram's own size — the visible items'
  bounding rect padded by 1px, with no scene margin — and the `export frame` is reported on stderr
  in batch mode only (silent in the interactive editor). *design*
- `EDIT-IO-7` MUST [A]: Save is enabled only for an open, modified document that is not in
  inspection mode; Save-As and Export are enabled whenever a document is open. *design*

### 4.9 Metainformation — META

- `EDIT-META-1` MUST [U]: the machine-readable metainformation node (the CGML_META formal
  comment: standard version, transition order, event propagation, geometry mode) is not
  drawn on the scene. *design*
- `EDIT-META-2` MUST [A]: the metainformation is edited only through the metainformation
  editor (`update-meta`); it is not an ordinary comment and its body is not editable
  through the comment-body path. *design*
- `EDIT-META-3` MUST [I]: editing the metainformation changes no diagram element and no
  geometry, and the metainformation round-trips through save and reopen. *PNST 1044*
- `EDIT-META-4` MUST [A]: a free-form metainformation parameter can be added and removed
  through the properties view; the reserved parameters (standard version, transition order,
  event propagation, geometry mode, name) cannot be removed; each change is one undo step. *design*
- `EDIT-META-5` MUST [U]: the state machines of a document carry distinct non-empty names. *PNST
  1044 6.1.2*

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
