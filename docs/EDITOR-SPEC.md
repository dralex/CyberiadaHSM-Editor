# CyberiadaPro, the Cyberiada HSM Editor — Behavioural Specification

**Overview:** The document contains the requirements for the possible/required
editor behaviour. This document is used as the root specification for the test systems
used within the project.

**Document version:** 0.51 (2026-11-12)

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

The mode of a tab (§4.14, `docs/MODES.md`) decides whether the geometric requirements are
enforced:

- **Editing** (an HSM or FA tab, the default): operations mutate the document and the editor's
  *recovery* keeps it spec-compliant — a container grows to fit its content (`EDIT-NODE-2`), a grown,
  dropped or reparented element pushes its siblings clear (`EDIT-NODE-6`), and the geometry is
  reconstructed on request - rebuilt from scratch, or *updated*: the drawn elements keep
  their places and the geometry-less ones are laid out around them. The `[U]` geometric laws hold after every operation because this
  recovery maintains them. The read-only flag of a tab refuses the operations and keeps the
  recovery.
- **Inspection** (an HSM inspection tab, also both sides of an HSM diff tab): the document is
  read-only, read and drawn as faithfully as possible to its stored geometry — the recovery is
  suspended, so a loaded diagram may break the `[U]` geometric laws and is shown as-is (see
  §4.10). No operation runs, so nothing enforces or breaks a law.

A `[U]` geometric law is therefore an **editing-mode** invariant, and the test system checks
it in editing tabs; an inspection tab is where a non-compliant diagram is read without change.

## 3. Where the specification sits

```
  authority                   this specification              checks
 ┌───────────┐  semantics    ┌──────────────────┐  law kind  ┌──────────────┐
 │ ПНСТ 984  │──structure───▶│ STRUCT  SEM      │───[U]─────▶│  model+scene │
 │ UML2      │               ├──────────────────┤            │  dump        │
 ├───────────┤  geometry     │ NODE  EDGE  TEXT │───[U]/[A]─▶│              │─▶ polygon laws
 │ editor    │──interaction─▶│ TOOL INSPECT DOC │───[A]─────▶│  graph +     │   (every round)
 │ design    │  history      │ HIST  PLUG  MODE │───[I]/[A]─▶│  rectangle   │
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
  of the same state machine. The invariant holds on creation and on an endpoint
  retarget alike: a retarget that would bind an end to an element of another machine
  is refused and the endpoint stays put. *PNST 1044*
- `EDIT-STRUCT-5` MUST [U]: deleting an element removes its whole subtree and
  every incident transition — no dangling endpoint. The only exception is a state
  machine. Deleting a state machine just removes the geometry and makes the element
  invisible. Clearing the border preserves the content's absolute position (the children
  are re-based back to global coordinates, the inverse of adding a border) and the now
  frameless machine is re-anchored to the origin, so the live scene matches a reload. *design*
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
  moved or copied element. The link itself is drawn by `EDIT-EDGE-6` and `EDIT-EDGE-17..21`.
  *design*
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
- `EDIT-STRUCT-11` MUST [A]: a state machine with no geometry is not drawn on the scene; the
  element tree marks it — its name is shown in square brackets and its icon greyed — while the
  inline-rename value stays the plain name. *design*
- `EDIT-STRUCT-12` MUST [U]: a subject's target is an element of the comment's state machine —
  a state, a pseudostate, a choice, another comment or a transition — never the state machine
  itself nor the comment itself; a name or data fragment is non-empty and lies inside one field
  of the target's text, searched in this order (the first occurrence is the commented one):

  ```
  target                     name fragment               data fragment
  state                      the name                    each action in turn: the event,
                                                         the guard, the behaviour
  transition (no name)       -                           the event, the guard, the behaviour
  comment                    the name                    the body
  pseudostate, choice        the name                    -
  ```

  A fragment spanning two fields or holding a keyword or a separator (`entry/`, `[`, `/`) is
  refused, as the strict reading of PNST 1044 does. A transition carries no name, so a **name**
  fragment on a transition is refused — only a data fragment of its label is valid (PNST 1044 8.5,
  `CGML-8.5-4`). A link or a retarget breaking these rules is refused and the subject stays as it
  was. *PNST 1044 6.7/8.5*
- `EDIT-STRUCT-13` MUST [U]: an edit of the target's text that leaves no field holding a
  fragment turns that subject into an element subject of the same target in the same undo
  step; the link keeps its source point and gets the box of `EDIT-EDGE-6`. *PNST 1044 6.7*
- `EDIT-SEM-7` MUST [U]: an entry point and an exit point carry a non-empty name; a new point
  gets a unique default name. *PNST 1044 8.3.1*
- `EDIT-SEM-8` MUST [U]: a submachine state never references the state machine that contains it;
  an internal reference names a state machine of the document, otherwise the reference is an
  external URI. *PNST 1044 8.1.1*

### 4.3 State machines/states/pseudostates/comments geometry and layout — NODES

The `[U]` laws below are editing-mode invariants: the editor's recovery maintains them after
each operation (grow-to-fit, push-siblings). An inspection tab suspends them and shows the stored
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
  editor grows the parent to fit; a parent never shrinks below its content — a resize through
  the model (the `move` verb) re-contains the children just as the interactive border drag does.
  Adding new content (by `move` or `new-state`) grow the parent (see also `EDIT-NODE-5`). A drag that
  moves a child within its own parent grows the parent to keep it inside the same way
  (snap-to-grid included); a dragged element is never relocated away from where it is
  dropped. *design*
- `EDIT-NODE-3` MUST [A]: an element's point is its centre; a top-level element
  is in absolute coordinates, a nested one relative to its parent's centre, and one nested
  in a composite state relative to the centre of its region (`EDIT-NODE-15`).
  *PNST 1044 7.2.1*
- `EDIT-NODE-4` SHOULD [U]: a state machine without a rect frames the union of
  its content and sits at the origin; several rect-less machines would share
  the origin, so the geometry reconstruction of a document with several
  machines gives every machine its rect and places the machines side by side.
  The reconstruction and the update lay the comments out inside their own
  containers - a linked comment beside the element of its container that holds
  the subject, the others on a shelf after the content - and the containers and
  the machine border grow around them (`EDIT-NODE-1`). *design*
- `EDIT-NODE-5` MUST [A]: a state, a comment, and a state machine resize directionally - a drag
  of the right or bottom border (or the corner between them) grows that side; a drag of the top or
  left border moves the element. A state and a machine clamp to their content; a comment holds none.
  A container resize holds the absolute position of every contained element (a state, a pseudostate
  including a rect choice, and a comment alike); only the dragged edge extends, no child is
  relocated. A child without stored geometry is laid out by the scene and is neither counted in the
  content the resize clamps to nor relocated by it, so sizing and holding cover the same children;
  the editor never fabricates a rectangle for such a child (`EDIT-ROBUST-1`). *design*
- `EDIT-NODE-6` MUST [U]: sibling elements do not overlap. A grown element pushes the siblings it
  grows into outward by its growth. A dropped element - a drag released within its parent or the
  `move` verb - stays where it is dropped (`EDIT-NODE-2`) and each sibling it overlaps moves out the
  shorter way, away from it, to the placement gap. A pushed sibling that lands on another settles
  in the nearest free slot, and the parent grows to re-contain them. *design*
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
- `EDIT-NODE-15` MUST [I]: a composite state's region is stored: the state rect less its bands -
  the header (the name, the entry, the internal transitions), the footer (the exit) and the side
  margins it was stored with - and the nested elements are placed in it (`EDIT-NODE-3`). A document
  with the meta parameter `regionGeometry/ stored` is drawn from its region rects. A document
  without it (an earlier editor's or another tool's) is drawn with the regions computed from the
  texts, as it was seen before, and the editor then holds them as stored: a save writes every
  region rect and the marker, so the other tools read the nested elements where the editor drew
  them. A state promoted to composite gets its computed region. A composite without a rect has no
  bands: its region is centred on it, so its nested elements are drawn where the model places them.
  A resize keeps the bands; the region never follows the font or the text visibility. *PNST 1044
  6.5.1, 6.9*

```
 a composite        the header band: name, entry, internal transitions
 +-----------+ --
 |  Moving   |   } the header  (state top .. region top)
 | entry/... |
 |+---------+| --
 ||  region || } the nested elements, relative to the region centre
 |+---------+| --
 | exit/...  |   } the footer  (region bottom .. state bottom)
 +-----------+ --
```

- `EDIT-NODE-16` MUST [A]: a change of a composite's text (its name or an action) gives the
  changed band the height of its text in the current font: the state border moves, the region and
  the nested elements keep their places. A simple state whose texts no longer fit after a change of
  its text grows down by the missing height, its top and texts keeping their places; it never
  shrinks on its own. A font change moves nothing: a text wraps to the state
  width and one taller than its band is cut at the band with an ellipsis, the whole text in its
  tooltip; the text being edited is shown whole. *design*

### 4.4 Transitions/comment links geometry and layout — EDGES

- `EDIT-EDGE-1` MUST [U]: an endpoint of a rect elements (a state, a comment) lies on the
  elements border; the rounded corners of a state are a part of it, so an endpoint released on
  one stays on its arc (`EDIT-EDGE-15`) and a load keeps it there. An endpoint without a stored point lies on the border of its node
  toward the other end (a vertex at its centre), within tolerance. Resizing a node holds each
  attached endpoint at its absolute position while its border did not move (a point on a
  perpendicular border stays put), and keeps an endpoint whose edge moved attached to the new
  border. This holds however the node geometry changes — an interactive border drag, a move or
  resize through the model, a grow to fit the children, a reparent — so a transition never hangs
  off a state that was moved, resized or reparented. A loaded document is recovered the same way:
  an endpoint stored off its border is snapped onto it on load (an inspection tab shows it as
  stored, `EDIT-INSPECT-*`). *design*
- `EDIT-EDGE-2` MUST [U]: an endpoint of a point-based circle elements (initial
  pseudostate, final state, history pseudostate, or entry/exit point) lies on the border of the
  drawn element, not the center. *design*
- `EDIT-EDGE-3` MUST [U]: an endpoint of the rest point-based elements (terminator) lies
  on the center. *design*
- `EDIT-EDGE-4` MUST [U]: an endpoint of a choice element lies on vertexes of the
  rhombus. The end of a comment link is the exception: its comment point lies anywhere on
  the rhombus border (`EDIT-EDGE-17`). *design*
- `EDIT-EDGE-5` MUST [U]: a transition line finishes with an arrow. *PNST 984* 
- `EDIT-EDGE-6` MUST [U]: a comment link is a dashed line from the comment to its subject; a
  link to an element (a node or a transition) finishes with a small black box at the target, a
  link to a fragment has no box: the fragment is underlined instead (`EDIT-EDGE-21`).
  *PNST 984 7.2.7*
- `EDIT-EDGE-7` MUST [A]: a rebind moves the endpoint to the new node; the
  transition then connects the new pair. The identity of endpoints, not the id,
  which may be renamed). *design*
- `EDIT-EDGE-8` MUST [U]: a self-loop stays a loop unless an endpoint is dragged
  onto a non-ancestor node; it is drawn outside its state (`EDIT-EDGE-22`). *design*
- `EDIT-EDGE-9` MUST [A]: adding, moving or removing a polyline point changes
  only that point; the endpoints stay. A point is removed by double-clicking its
  dot or pressing Delete on it. *design*
- `EDIT-EDGE-10` SHOULD [U]: transition (lines and polylines) should not cross state
  borders if possible *design*
- `EDIT-EDGE-11` MUST [U]: a transition's label geometry is a rect — the live and persisted
  geometry of the label, migrated from a legacy label point when only a point is stored. *design*
- `EDIT-EDGE-12` SHOULD [U]: a transition's label sits near its path by default.
- `EDIT-EDGE-13` SHOULD [A]: the label position and size can be changed — dragged to move,
  resized by the corner handles under the selection tool, edited in the properties view, and
  driven by the `label <x y w h>` verb. An in-place text edit re-fits the box to the new text's
  natural width (it shrinks as well as grows); a manual corner-handle width is superseded by the
  next text edit. *design*
- `EDIT-EDGE-14` MUST [U]: a transition with a colour draws its line and arrow in it; the
  selection highlight overrides it while selected. *design*
- `EDIT-EDGE-15` MUST [A]: with snap on, an endpoint placed by a drag or a release bind on a
  straight edge of a rect node stays on that edge and slides along it to the nearest grid line
  within the edge; on a rounded corner, or on an edge with no grid line, it keeps the plain
  projection. Pseudostates and choices keep `EDIT-EDGE-2/3/4`; the coordinate locked by `Ctrl`
  keeps the neighbour's. The node under the cursor itself takes the endpoint, so a snapped point
  never misses a small node. *design*
- `EDIT-EDGE-16` MUST [A]: with snap on, a polyline point placed, moved or inserted by a gesture
  lies on a grid crossing in scene coordinates, whatever the size of the drag steps. A seeded
  self-loop has its endpoints snapped by `EDIT-EDGE-15`, its out-levels on grid lines past the
  border and its segments orthogonal. The label box is not snapped. *design*
- `EDIT-EDGE-17` MUST [U]: a comment link starts on the comment border and ends on the target
  border by `EDIT-EDGE-1..3`, on a choice anywhere on the border of its rhombus (not only on the
  vertexes of `EDIT-EDGE-4`), or on the transition path for a transition target; an end without
  a stored point attaches toward the other end at display time, a drawn or dragged end slides
  along the border, and a resize of the target puts it back on the new border.
  *PNST 1044 9.1.1*

```
        /\            a transition ends on a vertex (EDIT-EDGE-4),
       /  \  ■ - - -  a comment point anywhere on an edge
       \  /
        \/
```
- `EDIT-EDGE-18` MUST [U]: a link drawn in the editor stores its source point and, unless it
  points at a fragment, its target point, so a saved document has a source point on every link;
  the geometry reconstruction computes the link points and the geometry update fills only the
  missing ones. *PNST 1044 9.1.2*
- `EDIT-EDGE-19` MUST [U]: a link is drawn only while both the comment and the target are drawn,
  and follows the moves and resizes of both ends as a transition does (`EDIT-EDGE-1`).
  *PNST 1044 9.1.2*
- `EDIT-EDGE-20` MUST [A]: a selected link shows a source handle sliding along the comment
  border and a target handle sliding along the target border; dropping the target handle on
  another element retargets the subject under `EDIT-STRUCT-12`. Delete or the link context menu
  removes the subject, never the comment. *design*
- `EDIT-EDGE-21` MUST [U]: a fragment link underlines the commented occurrence
  (`EDIT-STRUCT-12`) in the drawn text of its target — every line of a wrapped fragment — and
  ends at the middle of the first underline segment. While that text is hidden or edited in
  place, on a choice and for a fragment not drawn, the link ends on the target border by
  `EDIT-EDGE-17`, still without the box. *PNST 984 7.2.7, PNST 1044 9.1.1*

```
   ┌────────────────────────────┐
   │          Moving            │
   │ entry/ motor.on()          │        ┌───────────────┐
   │ TIMEOUT [count > 3] / stop()│- - - -│ Stops the     │
   │                       ‾‾‾‾‾‾       │ motor safely  │
   └────────────────────────────┘        └───────────────┘
```
- `EDIT-EDGE-22` MUST [U]: a self-loop is drawn outside its state as an orthogonal polyline
  leaving one border and returning into one, never as an arc and never across the state. A
  drawn loop starts on the side nearest the press that began the draw (on a tie, the side of
  the first move), at the press projected onto it, and returns into the side the release
  points to (the same side: a bump); the drawn route is stored (`EDIT-EDGE-16` with snap on).
  A loop without a stored polyline takes a default route at display time, nothing stored:
  stored endpoints keep their sides, otherwise the k-th such loop of the state takes the
  right, bottom, left, top side in document order, nesting farther out after four; the first
  drag of a bend or a segment stores the shown route. A resize moves every bend of a stored
  loop with the border it lies beyond. A loop label without a stored box lies outside, past
  the outer segment. *design*

```
   drawn: the press near the top            default, nothing stored
          ┌────┐                                   ┌────┐ loop 3
   ┌──────┴────▼──┐                         ┌──────┴────▼──┐
   │      ▪ press │                     ┌──►│              ├──┐
   │              │              loop 2 └───┤    State     │◄─┘ loop 0
   └──────────────┘                         └──────▲────┬──┘
                                                   └────┘ loop 1
```

### 4.5 Text of names, actions, and labels — TEXT

- `EDIT-TEXT-1` MUST [U]: a state's or a state machine's title lies inside its owner; when
  editing a taller header grows the element and the other roles reflow, none moving
  sideways. A simple state becoming composite reflows its title the same way. *design*
- `EDIT-TEXT-2` MUST [U]: an action keeps its `entry/`, `exit/` or `event [guard]/`
  prefix. A single-line behaviour is drawn inline after the prefix and soft-wraps at the element
  width if too long; a behaviour spanning several lines starts on its own line under the prefix. The
  `entry/`, `exit/`, `do/` keyword prefix is drawn bold, and an event name (of an internal or an edge
  transition) is drawn bold when a guard or behaviour follows it (a lonely event name stays normal);
  a preference switches each of the three to normal weight (bold by default). *design*
- `EDIT-TEXT-3` MUST [A]: a title, an action text, or an edge label is edited in place. *design*
- `EDIT-TEXT-4` MUST [A]: if an edge label text no longer fits the label rect, it wraps at
  a word boundary. *design*
- `EDIT-TEXT-5` MUST [U]: an event name is not one of the reserved words `entry`, `exit`, `do`,
  `propagate`, `block`, `defer`, `else`; the reserved event names `ANY` and `UNKNOWN` are
  permitted. *PNST 1044 6.8.1*
- `EDIT-TEXT-6` MUST [U]: `defer` is the whole behaviour of an internal transition of a state and
  never labels a transition; `propagate` and `block` accompany a non-empty event name. *PNST 1044
  6.8.1, 6.8.2*
- `EDIT-TEXT-7` SHOULD [A]: the code parts of a text are highlighted by the language of the
  `platformLanguage` metainformation parameter of its own document, so the tabs are highlighted
  independently; an empty or unknown language leaves the text plain. The highlighting is a display colour only: the text, the saved document and the bold
  prefix (EDIT-TEXT-2) are not changed. A preference turns it off. *design*

  ```
  entry/ foo(1);        ─► after the keyword "/"          (entry, exit, do behaviour)
  EV [x > 0] / f();     ─► the guard and after the "/"    (internal and edge transitions)
  formal comment body   ─► the whole text, unless its own dMarkup is markdown (EDIT-TEXT-11)
  titles, names, informal comments ─► plain
  ```

  Languages: the built-in ones (`syntax/*.json`: C, C++ (Arduino), C#, Python) and those the
  plugins bring (`EDIT-PLUG-60`); the value is matched case-insensitively by the name or an
  alias. The same languages are the language modes of the text tabs (`EDIT-MODE-13`).
- `EDIT-TEXT-8` SHOULD [A]: the new action and new internal transition dialogs highlight the
  entered text as EDIT-TEXT-7 does; an exported image is plain unless the export preference
  keeps the colours. *design*
- `EDIT-TEXT-9` SHOULD [A]: the Document > Language menu (after Document > Read-only) sets the
  `platformLanguage` of the active document; choosing an entry writes the value, *Undefined*
  removes the parameter, each choice is one undo step; an empty document first gets the state
  machine the first drawn element would create (the format needs one). The checked entry follows the active document on load, tab switch, properties edit,
  undo/redo and batch `update-meta`. The menu is enabled only in an editable diagram tab — HSM or
  FA without the read-only flag (`EDIT-MODE-4`). *design*

  ```
  ( ) Undefined                 the parameter is absent or empty
  ( ) C  C++ … Python           the languages of EDIT-TEXT-7; an alias checks its language
  ─────────
  ( ) Lua (no highlighting)     an unknown value of a loaded file, or one replaced from the
                                menu: kept in the menu until the editor exits
  (•) Rust (no highlighting)    an unknown value typed in the properties: shown while current
  ```
- `EDIT-TEXT-10` SHOULD [A]: a multiline text — a state action's behaviour, a comment body and the
  document `description` metainformation parameter — is editable in the properties view through a
  multiline editor (a dialog opened from the row), and the newlines are preserved on write-back. The
  editor highlights the text as `EDIT-TEXT-7` does (behaviour and a formal/component comment body by
  the document language, an informal body by its markup as `EDIT-TEXT-11` does); the `description` is
  plain text and is not highlighted. *design*
- `EDIT-TEXT-11` SHOULD [A]: a comment body is highlighted by its markup language. An informal
  comment takes its own `dMarkup`, else the document `markupLanguage`, else `plain` (no
  highlighting). A formal comment has no document default: its own `dMarkup` `markdown` makes it
  Markdown, anything else keeps it code (EDIT-TEXT-7); the reserved `CGML_META` and
  `CGML_COMPONENT` comments are always code. `markdown` (alias `md`) colours the elements below; a
  fenced block is highlighted by the language of its tag (EDIT-TEXT-7 names and aliases), else by
  the document `platformLanguage`, else in one code colour. Colour only, as EDIT-TEXT-7: the text
  and its size are not changed. *PNST 1044 9.3; compat spec 1.9 CGML-9.3-1/3*

  ```
  informal   own dMarkup ─► markupLanguage ─► plain
  formal     CGML_META, CGML_COMPONENT ─► code
             own dMarkup markdown ─► Markdown      otherwise ─► code
  ```

  ```
  # Heading            ─► heading (the whole line)
  > quote  - item  1.  ─► the markers
  ---  ***             ─► a rule
  **strong** *em*      ─► emphasis
  `code`               ─► code
  [text](url) <url>    ─► link
  ```cpp … ```         ─► the fence lines as markers, the body as code
  ```
- `EDIT-TEXT-12` SHOULD [A]: the Document > Markup menu (after Document > Language) sets the
  `markupLanguage` of the active document with the rules of `EDIT-TEXT-9`: *Plain* removes the
  parameter (a `plain` value checks it too), *Markdown* writes `markdown`, an unknown value shows as
  an extra entry kept for the session, the check follows the active document, and the menu is
  enabled only in an editable diagram tab. A comment's own `dMarkup` still overrides the document
  value. *design*
- `EDIT-TEXT-13` MUST [A]: a model reset (a document load, an undo/redo, the geometry
  reconstruction or update) discards an unfinished canvas text edit before the old document is
  freed: nothing is committed and no undo step is added. *design*
- `EDIT-TEXT-14` SHOULD [A]: with *Render Markdown comments* (Preferences > Text, off by default)
  a comment body highlighted as Markdown (`EDIT-TEXT-11`) is drawn rendered as `EDIT-MODE-14`
  draws a tab, the images relative to the directory of the document's file; an edit of the body
  shows its highlighted source and the end of the edit renders it again; the box, its band and
  its cut (`EDIT-NODE-16`) measure the rendered text; a fragment link to a rendered body ends on
  the target border as for an edited one (`EDIT-EDGE-21`); the exports draw it as the canvas does. The option `--render-markdown` and the batch
  verb `render-markdown on|off` set it for the run without storing it. *design*

### 4.6 Tools and interaction — TOOL

- `EDIT-TOOL-1` MUST [A]: a creation tool click creates exactly one element of
  its kind, parented to the element under the press point (a state or a state machine).
  The first element on an empty canvas lands under the press point too: the canvas gains
  a hosting state machine and the element is placed where the cursor is, not centred in
  the viewport (which stranded a small pseudostate at the view corner). *design*
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
  geometry snaps to the grid spacing (the grid lines are its multiples in scene coordinates); with
  it off the geometry is unrounded. The transition points follow `EDIT-EDGE-15`/`16`. *design*
- `EDIT-TOOL-9` MUST [A]: holding `Alt` suspends snap-to-grid while it is held; the modifier is
  read from the gesture, so a scripted `alt` suspends it too. *design*
- `EDIT-TOOL-10` MUST [A]: holding `Ctrl` locks a move to a single axis (the dominant one). *design*
- `EDIT-TOOL-11` MUST [A]: the submachine-state, entry-point and exit-point creation tools each
  create one element of their kind under the press point; a new submachine state takes a default
  reference set afterward in the property panel. *design*
- `EDIT-TOOL-12` MUST [A]: a click selects the smallest (innermost) element under the cursor — a
  leaf, or a smaller state over a larger one — even when it overlaps a composite state; clicking the
  composite's own margin or title still selects the composite. *design*
- `EDIT-TOOL-13` MUST [A]: the element tree can drag any reparentable element (a state, a
  pseudostate or a comment) to a new parent, through the same reparent step as a scene drag; the
  state machine, a transition, an entry/exit connector and the metainformation node are not
  tree-draggable. *design*
- `EDIT-TOOL-14` MUST [A]: the batch mode starts with snap off and the default spacing, whatever
  the GUI settings hold; `--snap`, `--grid <n>` and the script verbs `snap on|off` and `grid <n>`
  set them for the run only, never persisted. A gesture log records a non-default snap state at its
  start and every change during the session, so a replay snaps as the recording did. *design*
- `EDIT-TOOL-15` MUST [A]: the transition tool started on a comment (a body drag or one of its
  boxes) draws a comment link: a release over an accepted target (`EDIT-STRUCT-12`) adds an
  element subject with the points of `EDIT-EDGE-18`, a release anywhere else creates nothing,
  and the tool reverts to select after one use. *design*
- `EDIT-TOOL-16` MUST [A]: the fragment dialog — *Comment a Fragment…* of the link context menu
  or the `…` of the Fragment row in the properties — shows the target's text one read-only box
  per field of `EDIT-STRUCT-12`; a selection in one box clears the others, fills the fragment and
  the type (the name box gives a name fragment, the others data), both editable and checked by
  `EDIT-STRUCT-12`; the occurrence that will be underlined is marked, and OK retargets the subject
  in one undo step. *Comment the Element* of the link menu turns a fragment subject into an element
  one; a type switched in the properties takes the first non-empty field as its fragment. *design*

```
 ┌ Comment a Fragment ──────────────────────────────────────────────┐
 │ Target: Moving (state)                                           │
 │ Name     [Moving                                               ] │
 │ entry    [motor.on()                                           ] │
 │ TIMEOUT  [TIMEOUT      ] [ count > 3      ] [ stop()            ] │
 │ exit     [▒stop()▒; log("out")                                 ] │
 │ Fragment [stop()                         ]  Type [Data ▾]        │
 │ The first occurrence (TIMEOUT, behaviour) will be underlined.    │
 │                                               [Cancel]  [OK]     │
 └──────────────────────────────────────────────────────────────────┘
```
- `EDIT-TOOL-17` MUST [A]: a selection in a text edited in place — a name, an action, a label,
  a comment body — offers *Comment the Selection* in its context menu: *New Comment* places a
  comment beside the element, clear of its siblings (the comments too), an existing comment of the machine is picked by its name; either
  gets a fragment subject of the selection in one undo step. A selection outside one field
  (`EDIT-STRUCT-12`) disables the entry; the comment's own body is not offered to itself. *design*
- `EDIT-TOOL-18` MUST [A]: *Center Horizontally* / *Center Vertically* (the toolbar and *Edit*)
  put the selected node's centre on its parent's vertical / horizontal axis (`x = 0` / `y = 0`,
  `EDIT-NODE-3`), the other coordinate kept, in one undo step; a sibling it lands on moves to the
  nearest free slot (`EDIT-NODE-6`) and the parent grows to fit (`EDIT-NODE-2`). Enabled only for a
  node with geometry in a state or a bordered state machine; disabled on the top level (a machine,
  a child of a frameless machine), for a submachine connector and in a tab that is not editable
  (`EDIT-MODE-4`). *design*

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
- `EDIT-IO-2` MUST [C]: export to PNG and SVG succeeds for any valid document. The export
  dialog proposes the open diagram's file base name as the default image name. *design*
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
- `EDIT-IO-7` MUST [A]: Save is enabled only for an open, modified document in an editable tab
  (`EDIT-MODE-4`); Save-As is enabled in every tab but a diff tab, Export in every diagram tab.
  *design*

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
- `EDIT-META-6` MUST [A]: a `CGML_COMPONENT` formal comment is a machine-readable component
  descriptor; it is not drawn on the scene by default — even when it carries geometry (the geometry
  reconstruction leaves a geometry-less one without geometry) — and is drawn only while the *show
  component comments* preference is
  on. It is always present in the element tree and editable through the properties; it is also
  deletable from the tree with Del even when it has no scene item (no geometry, or hidden). A
  sibling of `EDIT-META-1`, which keeps the `CGML_META` node off the scene unconditionally. *design*

### 4.10 Inspection — INSPECT

Inspection is the mode HSM inspection of a tab of its own (`EDIT-MODE-6`); the sides of an HSM
diff tab are inspected the same way (`EDIT-MODE-10`).

- `EDIT-INSPECT-1` MUST [C]: in an inspection tab the document is read-only — every editing
  operation (create, delete, rename, move, resize, reparent, action edit, paste,
  metainformation) is refused and changes nothing. *design*
- `EDIT-INSPECT-2` MUST [A]: an inspected diagram is read and drawn from its stored geometry —
  no endpoint is snapped on load (`EDIT-EDGE-1`), a composite state's region follows the
  document's region rect (the state rect when it has none), even in a document without the
  marker (`EDIT-NODE-15`); the geometric recovery (the `[U]` NODE/EDGE laws) is suspended, so a
  diagram whose stored geometry breaks them is shown as-is. *design*
- `EDIT-INSPECT-3` MUST [A]: inspection and geometry reconstruction are mutually exclusive — an
  inspected document is never given the geometry it does not have. *design*
- `EDIT-INSPECT-4` SHOULD [I]: turning an inspection tab into an editing one (Document > Mode >
  HSM diagram or FA diagram) re-applies the editing layout and the recovery — the endpoints are
  snapped (`EDIT-EDGE-1`), a document without the marker gets its computed regions
  (`EDIT-NODE-15`) and the `[U]` compliance laws regain force; the undo history starts
  there. *design*

### 4.11 Documents and tabs — DOC

The window holds several documents; the structure is drawn in `docs/WINDOW.md`.

- `EDIT-DOC-1` MUST [A]: the window starts with one untitled document and no tab line; the tab
  line appears with the second open document and disappears when one is left. *design*
- `EDIT-DOC-2` MUST [A]: New opens a new untitled document in a new tab without a prompt; Open
  loads into the active tab when it is a clean untitled document and into a new tab otherwise;
  opening a path that is already open activates its tab instead of loading it twice — an
  editing and an inspection tab of one path are counted apart (`EDIT-MODE-6`). *design*
- `EDIT-DOC-3` MUST [A]: the structure tree, the properties panel, undo/redo and their texts, the
  window title with the modified marker, the zoom level and the armed tool follow the active
  document on every tab switch; a background document is not changed by a switch. *design*
- `EDIT-DOC-4` MUST [A]: closing a tab with unsaved changes asks to save, discard or cancel for that
  document only; closing the last tab leaves a fresh untitled document; exiting the editor asks
  for every modified document in turn and a cancel keeps the editor open. *design*
- `EDIT-DOC-5` MUST [A]: the clipboard is shared by the documents — an element copied in one
  document is pasted into the active one with the rules of `EDIT-TOOL-4`; the paste target is the
  copied element's parent collection when the active document has it, else its first state
  machine. *design*
- `EDIT-DOC-6` MUST [A]: the mode and the read-only flag belong to a tab (`EDIT-MODE-1`) —
  changing them in one tab leaves the other tabs as they are; the Document menu, the tools, the
  docks and the plugin actions follow the active tab on every switch, and the window title
  carries its marker (`EDIT-MODE-2`). *design*
- `EDIT-DOC-7` MUST [A]: the files given on the command line in GUI mode open as tabs in order,
  the last one active; a file that fails to load is reported and skipped. *design*
- `EDIT-DOC-8` MUST [A]: a gesture-log session is bound to one document — a tab switch while
  logging ends the session and starts a new one on the newly active document. *design*
- `EDIT-DOC-9` MUST [A]: the batch verbs `open`, `new-document`, `switch` and `close` act on the
  tab line as the GUI does; the dumps, the save and the export describe the active document;
  `close` of a modified document is a script error unless it is `close discard`. *design*
- `EDIT-DOC-10` MUST [A]: a document without a file is titled by its document name (the meta
  `name`, the Name of the Document element) in square brackets — `[CoffeeMachine]` — in the
  window title and the tab text, `untitled` when it has no name; the title follows a rename, an
  undo/redo and a plugin result; a document with a file is titled by the file basename; Save As
  of a named unsaved document proposes `Name.graphml`. *design*
- `EDIT-DOC-11` MUST [A]: the batch verb `save [<file>]` saves the active tab as Save does, or
  as `<file>` (relative to the current directory) as Save As does; the tab is no longer
  modified, so the `saved` token holds (`EDIT-PLUG-52`). It follows the enabling of Save and
  Save As (`EDIT-IO-7`), except that `save` of an unmodified tab with a file writes nothing;
  a refused `save` and a failed write are script errors. *design*, *open*

### 4.12 Plugins — PLUG

The plugins are external programs run on the active document: the architecture is
`docs/PLUGINS.md`, the contract a plugin follows is `docs/PLUGIN-API.md` (`api` 1, the
session protocol 1). `EDIT-PLUG-36..49` are the sessions (plugin level 2), `EDIT-PLUG-50..55`
the conditions of an action, `EDIT-PLUG-56..59` the modes of an action and the diff result
(`docs/MODES.md`), `EDIT-PLUG-60` the languages a plugin brings, `EDIT-PLUG-61..64` the plugin
runs of the batch script, which the plugin polygon drives (`docs/POLYGON-PLUGINS.md`).

- `EDIT-PLUG-1` MUST [A]: the plugins are looked up in `--plugin-path` (which replaces the
  rest), `$CYBERIADA_PRO_PLUGIN_PATH`, the user folder (moved by `$CYBERIADA_PRO_DATA_HOME`,
  `EDIT-UI-7`) and the system folder, in this order;
  the first folder holding an id wins, a later one with the same id is listed as shadowed.
  *design*
- `EDIT-PLUG-2` MUST [C]: a descriptor that is not valid JSON, has an unknown `api` major or
  breaks a field rule of the contract is rejected with its folder and the reason; the editor
  starts normally. *design*
- `EDIT-PLUG-3` SHOULD [A]: an unknown descriptor field is ignored and listed as a warning.
  *design*
- `EDIT-PLUG-4` MUST [A]: a plugin whose interpreter is not found (the Preferences, then `PATH`)
  is listed as disabled with the reason; its actions are disabled, the reason in the tooltip;
  so is a plugin whose `requires` does not hold (`EDIT-PLUG-50`). *design*
- `EDIT-PLUG-5` MUST [A]: the Plugins menu stands before Help: a submenu per plugin, ending with
  Settings... when the plugin has stored parameters, or a single entry for a plugin with one
  action and none; Output panel and Plugin manager... close the menu. *design*
- `EDIT-PLUG-6` MUST [A]: the actions marked `toolbar` form one toolbar per plugin to the right
  of the main toolbar; View > Plugin toolbars shows or hides each, and the choice is kept.
  *design*
- `EDIT-PLUG-7` MUST [A]: an action opens the generated parameter dialog when it has `run`
  parameters, a `document` parameter not set for the document, or a required `persistent`
  parameter without a value, and runs at once otherwise; Cancel in the dialog starts nothing.
  *design*
- `EDIT-PLUG-8` MUST [A]: the command is the interpreter plus the argv, with no shell; every
  variable expands as the contract says, a conditional element is added only when its
  parameter is true or not empty, a `$` not followed by `{` is kept, and an unknown variable
  fails the run before it starts. *design*
- `EDIT-PLUG-9` MUST [A]: `${input}` holds the active document as Cyberiada 1.0 with its unsaved
  edits (an empty file for an empty document; a text tab: `EDIT-PLUG-57`), `${selection}` the
  selected element ids in the document order; `request.json` is written exactly when the argv
  uses `${request}`. *design*
- `EDIT-PLUG-10` MUST [U]: a secret value never appears in the argv, `request.json`, the Output,
  the dumps, the session log or the settings; only the environment variable named by `export`
  carries it to the plugin process. *design*
- `EDIT-PLUG-11` MUST [A]: a `run` value starts from its default, a `persistent` value is kept
  per user, a `document` value per user and document file — in the settings, never in the
  diagram; the values of an untitled document last for the session. *design*
- `EDIT-PLUG-12` MUST [A]: a `replace` result is one undo step "Plugin: <label>" that keeps the
  file path and the save format; an output equal to the input pushes no step and is reported
  as no changes. *design*
- `EDIT-PLUG-13` MUST [I]: undoing a `replace` step restores the document exactly as it was
  before the run. *design*
- `EDIT-PLUG-14` MUST [A]: a `new-document` result becomes the active modified untitled
  document — in the active tab when that is an empty untitled one (as `EDIT-DOC-2`), in a new
  tab otherwise, titled by its document name (`EDIT-DOC-10`); the source document is untouched.
  *design*
- `EDIT-PLUG-15` MUST [A]: the findings of a run are shown in Diagnostics; a click selects and
  centres the element, a finding whose element is absent has no link, and the findings turn
  grey once the document changes. *design*
- `EDIT-PLUG-16` MUST [A]: a `files` result lists the new and changed files under the action's
  outdir in the path order; a listed file whose extension a text mode knows (`EDIT-MODE-13`)
  is a link that opens it in a text tab, a `.graphml` one in a diagram tab. *design*
- `EDIT-PLUG-17` MUST [A]: the plugin's stdout and stderr stream during the run into the
  plugin's own output tab ("<plugin> output"), one block per run headed by the action and the
  document, capped per tab; a starting run makes its tab current. *design*
- `EDIT-PLUG-18` MUST [C]: a failed run — the program does not start, an exit code outside
  `exit_ok`, a crash, a timeout, a missing, empty or unreadable output, a malformed diagnostics
  file — leaves the document untouched and reports the reason, the exit code and the end of
  stderr. *design*
- `EDIT-PLUG-19` MUST [A]: the output document is decoded with the action's `load` options
  (`strict`, `reconstruct`) before the document is touched. *design*
- `EDIT-PLUG-20` MUST [A]: a timeout or Cancel terminates the plugin process and kills it after
  the grace period; only the direct child is stopped. *design*
- `EDIT-PLUG-21` MUST [A]: one run per document at a time; the runs of different documents may
  overlap; a result is applied to the document the run started from. *design*
- `EDIT-PLUG-22` MUST [A]: a `replace` run locks its document — editing, undo and redo are
  refused and the canvas takes no input until the run ends. *design*
- `EDIT-PLUG-23` MUST [A]: closing a tab with a run asks, then cancels the run; exiting the
  editor cancels every run; the result for a closed document is never applied. *design*
- `EDIT-PLUG-24` MUST [A]: a tab that is not editable (`EDIT-MODE-4`: read-only, inspection,
  diff) disables the `replace` actions, and a result that arrives after its tab became read-only
  is refused. *design*
- `EDIT-PLUG-25` MUST [A]: an action with `save` or `cwd: document` saves a modified or untitled
  document first — an untitled one asks for a name, a cancel cancels the run, and in batch mode
  an untitled document fails it; `cwd: document` runs the plugin in the document's directory.
  *design*
- `EDIT-PLUG-26` MUST [C]: each run has a fresh run directory, removed after the run unless
  `CYBERIADA_PRO_PLUGIN_KEEP=1`. *design*
- `EDIT-PLUG-27` MUST [A]: in batch mode the plugin stage runs after the script with the
  defaults and the `--plugin-param` values only; the exit code is 0 for a done run, 1 for a
  usage error and 6 for a failed run. *design*
- `EDIT-PLUG-28` MUST [A]: the batch sections `== plugins`, `== plugin` and `== diagnostics` are
  deterministic and name no machine path. *design*
- `EDIT-PLUG-29` MUST [A]: the session log records a run as the comment `# plugin <id>:<action>`
  and starts a new session after a `replace` or `new-document` result. *design*
- `EDIT-PLUG-30` MUST [A]: the plugin manager lists every plugin found with its folder and
  state; the enable switch is kept, and Rescan rebuilds the menu, the toolbars and the
  languages (`EDIT-PLUG-60`). *design*
- `EDIT-PLUG-31` SHOULD [A]: an interpreter path set in the Preferences takes effect at the
  next scan. *design*
- `EDIT-PLUG-32` MUST [A]: the plugin dock is hidden until the first run and shows every
  running plugin with its time and Cancel. *design*
- `EDIT-PLUG-33` MUST [A]: a run gets the interface language as `${language}`, as
  `CYBERIADA_PRO_LANGUAGE` in its environment and as `language` in `request.json`; a
  parameter default may use `${language}`; the batch mode gives `en` (`EDIT-UI-2`). *design*
- `EDIT-PLUG-34` MUST [A]: the metainformation an action declares (`meta`) fills the keys the
  document lacks before the run — before the save of a `save` action — as one undo step; a
  present different value is kept and noted (the platform language compared by its
  aliases); a document without a state machine, or a read-only one, is left as it is.
  *design*
- `EDIT-PLUG-35` MUST [A]: a plugin output tab can be closed, which drops its text, and
  comes back empty with the plugin's next output; its context menu clears the text after a
  confirmation and saves it to a file as plain UTF-8 text. *design*
- `EDIT-PLUG-36` MUST [A]: the session actions of a descriptor (`session.actions`) follow the
  plugin's other actions in its submenu, after a separator, and on its toolbar; the action ids
  are unique across both lists; the one-shot actions a session action `replaces` are hidden
  from the menu and the toolbars, while `--run-plugin` still runs them; a `session.version`
  the editor does not run leaves the session actions out with a warning in the plugin
  manager and `--list-plugins`, and the other actions stay; `actions` stays required
  (`EDIT-PLUG-2`). *design*
- `EDIT-PLUG-37` MUST [A]: a session run is configured, saved and staged as a run
  (`EDIT-PLUG-7..9`, `EDIT-PLUG-25`, `EDIT-PLUG-34`) and is the one run of its document
  (`EDIT-PLUG-21`); the plugin process gets stdin and stdout as the session channel; the
  editor sends `initialize` first, and no answer within `SESSION_INIT_TIMEOUT_MS`, a model
  without a known `kind` or the process ending first fails the session as a failed run
  (`EDIT-PLUG-18`). *design*
- `EDIT-PLUG-38` MUST [A]: the channel carries one JSON-RPC 2.0 object per line in UTF-8; a
  stdout line that is not a JSON object and the stderr go to the plugin's output tab; a
  request with an unknown method gets the error -32601; an unknown notification and the
  unknown fields are ignored. *design*
- `EDIT-PLUG-39` MUST [A]: the controls follow the model: Stop always; Step with the units of
  `steps`, the first one by default; Run with the speeds of `speeds`; Pause and Restart when
  `controls` lists them; the event line when `events` is given, the stream entries when
  `stream`, Save Log when `save`; each control is enabled by the current `status`. *design*
- `EDIT-PLUG-40` MUST [A]: the `active` and `fired` ids of the latest state mark their elements
  Active and Fired on the canvas, unlike the selection; an id the document lacks is ignored; a
  `state` message replaces only the fields it carries; the marks are repainted at most every
  `RUN_MARK_INTERVAL_MS` with the latest values and cleared when the session ends; an export
  draws none. *design*
- `EDIT-PLUG-41` MUST [A]: a session locks its document: editing, undo and redo are refused,
  while selecting, zooming, scrolling, the structure tree and the read-only properties keep
  working. *design*
- `EDIT-PLUG-42` MUST [A]: the session tab "<document> session" holds the event line, the
  stream entries and the history — a row per `history` message (the time, the event and its
  parameters, the origin, the outcome, the fired elements; a click selects and centres the
  element) and a pending row per item of a loaded stream, at most `SESSION_HISTORY_LIMIT`
  rows; it opens current with the session and closes only after the session ends. *design*
- `EDIT-PLUG-43` MUST [A]: an `ask` raises the dock and shows the question with an answer line,
  its `choices` as buttons, Answer and Decline; the reply is `accept` with the text, or
  `decline`; the questions are shown one at a time in their order; Stop replies `cancel` to
  every open question. *design*
- `EDIT-PLUG-44` MUST [A]: Post sends `event` with `NAME name=value ...` split into the name and
  the string parameters; Load Stream... and Edit Stream... send `input` with the path or the
  text, and an error reply is shown in the output tab; Save Log... sends `save` with the chosen
  path, `<document name>.log` in the document folder by default. *design*
- `EDIT-PLUG-45` MUST [A]: Stop, closing the tab (after a question) or exiting the editor ends
  the session: `cancel` to the open questions, `shutdown`, stdin closed, then terminate after
  `PLUGIN_KILL_GRACE_MS` and kill after another; the plugin may end the session by exiting; the
  exit code is then checked against `exit_ok` and the action's `result` is applied as a run's,
  a session the editor terminates being a cancelled run; the marks are cleared and the document unlocked in
  every case. *design*
- `EDIT-PLUG-46` MUST [A]: Plugins > Session acts on the session of the current document — Step
  (F10, the last unit chosen), Run (F5), Pause (F6), Restart, Stop (Shift+F5), Post Event (the
  focus to the event line), Load Stream..., Edit Stream..., Save Log... — each entry enabled as
  its dock control, all disabled without a session on the current document. *design*
- `EDIT-PLUG-47` MUST [A]: in batch mode a session action needs `--session-script`, else it is
  a usage error (exit 1); the verbs run in order after the first state; `step` and `restart`
  wait for a state whose status is not `running`, `wait` for the status given (`ended`: the
  plugin's own end), `answer` and `decline` for an open question, each within
  `SESSION_WAIT_S` or the seconds given; a failed `expect`, a timeout or a failed session
  exits 6; the end of the script stops the session. *design*
- `EDIT-PLUG-48` MUST [A]: `--dump-session` prints `== session`: every message both ways in
  the order it went, one line each with the fields sorted — the model, the plugin's
  messages, the editor's prefixed `sent`, the replies; `initialize` without its parameters
  and a path by its file name, so no line depends on the machine. *design*
- `EDIT-PLUG-49` MUST [A]: a `log` message goes to the output tab, `warning` and `error` in the
  stderr colour; a `diagnostics` message replaces the session's findings in Diagnostics
  (`EDIT-PLUG-15`). *design*
- `EDIT-PLUG-50` MUST [A]: a `requires` of the descriptor or of an action holds when every
  `programs` entry is found on `PATH` (a path with `${plugin_dir}`: an executable file) and
  every `env` variable is set and not empty in the editor environment or the descriptor
  `env`; the descriptor `requires` not holding disables the plugin (`EDIT-PLUG-4`), an action
  `requires` that action alone; both are checked at every scan. *design*
- `EDIT-PLUG-51` MUST [A]: a `requires.plugins` entry `<id>` or `<id>>=<version>` holds when
  the plugin of the id is listed as enabled — found, neither shadowed, rejected nor disabled —
  with at least the version given; its manager switch does not count; a plugin disabled this
  way disables the plugins requiring it in turn. *design*
- `EDIT-PLUG-52` MUST [A]: an action with `when` is enabled only while every token holds on the
  current document — `file`, `saved`, `selection`, `sm`, `initial`, `fa`, `fa-valid`,
  `meta:<key>`, `meta:<key>=<value>` as `PLUGIN-API.md` defines them and `git`
  (`EDIT-PLUG-55`), `!` negating a token; an absent key the action's `meta` fills
  (`EDIT-PLUG-34`) counts as holding the action's value; the tokens are checked again after
  every edit, undo, redo, save (Save As included) and selection change. *design*
- `EDIT-PLUG-53` MUST [A]: an unknown `when` token or `requires` key disables the action (the
  plugin, for the descriptor `requires`) with the reason that it needs a newer editor and is
  listed as a warning (`EDIT-PLUG-3`); a `when`, `requires` or `reason` of a wrong type rejects
  the descriptor (`EDIT-PLUG-2`). *design*
- `EDIT-PLUG-54` MUST [A]: the tooltip of a disabled action gives the reason — the action's
  `reason` for a failing `when` token when it has one, else the first condition failing; the
  plugin manager lists the actions disabled by their `requires` with the reasons and
  `--list-plugins` prints them as `unavailable` lines; `--run-plugin` of a disabled action
  fails with the reason (`EDIT-PLUG-27`). *design*
- `EDIT-PLUG-55` MUST [A]: the `git` token holds when the document has a file and a directory
  named `.git` is in the file's directory or in one of its ancestors, the path resolved
  through its links; a `.git` file does not count and no process is started; the lookup is
  made when the document gets its file (open, Save As), at every save and at every scan, and
  kept for the path between them. *design*
- `EDIT-PLUG-56` MUST [A]: an action works in the modes its `modes` lists (`EDIT-MODE-1` ids),
  `["hsm", "fa"]` when it has none; in a tab of another mode it is disabled with the reason
  naming the modes; an unknown id is a warning (`EDIT-PLUG-3`) and an action without a known id
  is disabled as needing a newer editor (`EDIT-PLUG-53`); a `modes` of a wrong type rejects the
  descriptor (`EDIT-PLUG-2`). The mode is checked before the `when` tokens. *design*
- `EDIT-PLUG-57` MUST [A]: in a text tab `${input}` is `<rundir>/input.<ext>` holding the text
  with its unsaved edits in UTF-8, `<ext>` the first extension of the tab's mode, and
  `${output}` is `<rundir>/output.<ext>`; a `replace` result replaces the text as one undo step
  (`EDIT-PLUG-12`), a `new-document` result opens a text tab of the same mode; the tokens of
  `EDIT-PLUG-52` that need a diagram (`sm`, `initial`, `fa`, `fa-valid`, `meta:`) do not hold;
  the text is locked while a `replace` runs, and a finding with a `line` goes to that line of
  the text. *design*
- `EDIT-PLUG-58` MUST [A]: a `diff` result opens a diff tab (`EDIT-MODE-10`): the base is
  `${output}` read in any format the library reads — an empty output is an empty document, not
  a failure — and the target is the run's `${input}`; the base is labelled by the values of the
  action's `string` parameters, else by the action label; the source document is untouched;
  Refresh of the tab runs the action again with the same values on the tab it ran on, and
  fails with the reason when that tab is closed (`EDIT-MODE-12`). A `diff`
  action lists only the diagram modes `hsm`, `fa`, `inspection`, else the descriptor is
  rejected. *design*
- `EDIT-PLUG-59` MUST [A]: an action with an unknown `result` is disabled with the reason that
  it needs a newer editor and listed as a warning, the plugin's other actions staying, as an
  unknown `when` token (`EDIT-PLUG-53`). *design*
- `EDIT-PLUG-60` MUST [A]: the files a descriptor's `languages` names in the plugin directory
  add languages, each file in the format of `syntax/*.json` with `extensions`, its id the file
  name without `.json`; they count while the plugin is enabled in the plugin manager, even
  when its interpreter or `requires` keep its actions off (`EDIT-PLUG-4/50`), for the
  highlighting (`EDIT-TEXT-7`), Document > Language (`EDIT-TEXT-9`) and the text modes
  (`EDIT-MODE-1/13`). The built-in languages come first, then the plugins in the search path
  order; a language whose id, name or alias is taken is skipped with a warning, as is a
  missing or malformed file; a `languages` that is not a list of strings rejects the
  descriptor (`EDIT-PLUG-2`). A plugin with `languages` may have no action. The languages are
  read at start and on Rescan; one gone turns its text tabs into Plain text and its
  `platformLanguage` values into unknown ones; `--list-plugins` prints a plugin's
  `languages:`. *design*
- `EDIT-PLUG-61` MUST [A]: the batch verb `plugin <id>[:<action>] [<name>=<value> ...]` runs
  the action on the active tab as `--run-plugin` does (`EDIT-PLUG-27`) and waits for its end:
  no dialog, the defaults and the values given in the `--plugin-param` syntax, a value holding
  spaces double-quoted (`\"` and `\\` inside it); the result is applied as in the GUI, a
  `new-document` or `diff` result activating its tab, and Refresh of a plugin diff tab runs the
  action again (`EDIT-PLUG-58`). An unknown plugin, action or value and an action disabled on
  the active tab (`EDIT-PLUG-52..56`, with the reason of `EDIT-PLUG-54`) are script errors; a
  failed run is not: the script goes on, and the batch exits 6 at its end unless another
  error ends it. *design*, *open*
- `EDIT-PLUG-62` MUST [A]: the `plugin` verb of a session action takes the lines up to a line
  `end` as its session script (`EDIT-PLUG-47`): the session verbs, run in order after the first
  state, `save FILE` inside the block being the session verb; a failed `expect`, a timeout or a
  failed session fails the run (`EDIT-PLUG-61`), and a block without `end` is a script error.
  *design*, *open*
- `EDIT-PLUG-63` MUST [A]: `--dump-actions` prints `== actions` after the sections of the active
  tab: every action of every plugin listed `enabled` (`EDIT-PLUG-1`), session actions included,
  in the search path order, one line each — `<id>:<action> enabled`, or `<id>:<action>
  disabled: <reason>` with the reason of `EDIT-PLUG-54`; on a diff tab every action is
  disabled. *design*, *open*
- `EDIT-PLUG-64` MUST [A]: `--dump` and `--dump-plugin` print `== plugin <n>` and
  `== diagnostics <n>` for the n-th `plugin` run of the script, and `--dump-session` prints
  `== session <n>` for a session one, in the formats of `EDIT-PLUG-28/48`, after the sections
  of the active tab whatever its kind; the `--run-plugin` stage keeps its unnumbered sections,
  printed last. The numbered sections and `== actions` are deterministic and name no machine
  path (`EDIT-PLUG-28`). *design*, *open*

### 4.13 Interface language and preferences — UI

The interface translations and their workflow are in `docs/TRANSLATION.md`.

- `EDIT-UI-1` SHOULD [A]: the *Interface language* of the Preferences (General) offers *System
  default* and every bundled language in its own name; the choice is stored per user and takes
  effect at the next start. *System default* takes the first of the system UI languages the
  editor has, else English. *design*
- `EDIT-UI-2` MUST [A]: the batch mode and the command line are English whatever the setting
  and the system locale — the dumps, the saved names of new elements and the messages of a
  batch run do not depend on the interface language. *design*
- `EDIT-UI-3` SHOULD [A]: in the GUI the names of new elements (state machine, state, comment,
  entry/exit point) and the Qt standard dialogs and buttons follow the interface language.
  *design*
- `EDIT-UI-4` SHOULD [A]: the texts a plugin supplies — its name and description, the labels
  and descriptions of its actions and parameters, the shown enum choices — follow the
  interface language through the plugin's `i18n/<lang>.json`; a text without a translation
  is shown as written, and a choice keeps its value. *design*
- `EDIT-UI-5` MUST [A]: the preferences are stored per user under the name `CyberiadaPro`; the
  first start without them adopts once every preference stored under the pre-release name
  *Cyberiada State Machine Editor* and leaves that store unchanged. *design*
- `EDIT-UI-6` MUST [A]: the structure tree and the properties panel are docks at the right of
  the canvas (as the plugin dock: a title bar with the float and close buttons, movable; every
  dock title is bold, the dock contents keep the regular weight), each
  with a checkable View entry — *Structure tree*, *Properties* — both shown by default; closing a
  dock unchecks its entry and the entry shows it again; the choice is kept per user between
  sessions; a hidden tree or panel keeps following the active document and the canvas
  selection, so showing it again needs no refresh. *design*
- `EDIT-UI-7` MUST [A]: `CYBERIADA_PRO_CONFIG_HOME` keeps the preferences in the ini file
  `<folder>/Cyberiada/CyberiadaPro.ini` and `CYBERIADA_PRO_DATA_HOME` the user data, the user
  plugin folder `<folder>/Cyberiada/CyberiadaPro/plugins`, instead of the places of the system
  (the registry and `%APPDATA%` on Windows, `~/.config` and `~/.local/share` on Linux): a
  separate, portable configuration, the same on every system; the tests set both. *design*
- `EDIT-UI-8` MUST [U]: the scene holds no graphics item that no live owner tracks — every item is
  an element or comment-link item, one of its owner's declared handles/labels/region, or a known
  transient draw item; a handle a rebuild replaces leaves the scene at once, never lingering to be
  painted after it is freed. The editor reports a violation through the `--dump-audit` scene audit
  and the polygon holds it as a standing law. *design*

### 4.14 Document modes — MODE

A tab holds its document in a mode — the format, the access, the tools and the plugins — and
has a read-only flag; the architecture is `docs/MODES.md`. Specified in 0.47, implemented by
the stages of `docs/MODES.md`.

- `EDIT-MODE-1` MUST [A]: every tab has one mode — `hsm` (HSM diagram), `inspection` (HSM
  inspection), `fa` (FA diagram), `diff` (HSM diff), `text` (Plain text), `markdown`
  (Markdown) or a language — a built-in one (`c`, `cpp`, `csharp`, `python`) or one a plugin
  brings (`EDIT-PLUG-60`) — and a read-only flag, off in every new or opened tab; changing
  them in one tab leaves the others as they are (`EDIT-DOC-6`). *design*
- `EDIT-MODE-2` MUST [A]: the tab text and the window title carry the mode after the title —
  nothing for HSM, `[inspection]`, `[FA]`, `[diff]`, `[text]`, `[Markdown]`, `[<language
  name>]` — and the flag as `ro` in the tab, `read-only` in the window: `a.graphml [FA, ro]*`,
  `a.graphml [FA, read-only][*]`, `a.graphml [ro]` for HSM. *design*
- `EDIT-MODE-3` MUST [A]: Document > Mode lists the modes, the active tab's checked; an entry
  the tab cannot reach is disabled: an HSM or FA tab reaches HSM, FA and HSM inspection
  (`EDIT-MODE-6`), an inspection tab HSM and FA, a text tab every text mode, a diff tab none.
  *design*
- `EDIT-MODE-4` MUST [A]: Document > Read-only sets the flag of the active tab. A tab is
  *editable* when it is HSM, FA or a text mode without the flag and not locked by a plugin
  (`EDIT-PLUG-22/41`); in a tab that is not, every edit is refused as in `EDIT-INSPECT-1` (a
  text takes no input), Undo, Redo and Save are disabled, and selecting, zooming, scrolling and
  copying keep working. The flag keeps the editing layout and the recovery of a diagram. In an
  inspection or a diff tab the entry is checked and disabled. *design*
- `EDIT-MODE-5` MUST [A]: HSM to FA sets the metainformation `type` to `fa`, FA to HSM removes
  it, each as one undo step; the mode follows the document: a loaded document with `type/ fa`
  opens in FA, and an undo, a redo, a properties edit or a plugin result that sets or removes it
  switches the tab. File > New > FA diagram opens an untitled FA tab whose document, made by
  the first edit (`EDIT-TEXT-9`), carries `type/ fa`. *EXT-FA 5; design*
- `EDIT-MODE-6` MUST [A]: HSM inspection opens a tab of its own: chosen on an HSM or FA tab it
  opens an inspection tab of the same document — read from its file when the tab is saved,
  else from the tab's snapshot — without the recovery (`EDIT-INSPECT-2`), titled as the source;
  the source tab is untouched; a file with an inspection tab already open activates it. The
  Open dialog's inspection box (off by default) and `--inspect` open the files in inspection
  tabs. HSM or FA on an inspection tab makes it an editing tab in place (`EDIT-INSPECT-4`),
  FA as `EDIT-MODE-5`. *design*
- `EDIT-MODE-7` MUST [A]: in an FA tab the creation tools are State, Initial, Final,
  Transition, Comment and Formal comment; the others — choice, terminate, the histories, the
  submachine state, the entry and exit points, the state machine — are disabled with the
  reason in the tooltip. A paste, a plugin result or a load is not filtered: an FA draft may
  hold any element. *EXT-FA 3; design*
- `EDIT-MODE-8` SHOULD [A]: View > FA symbols (off by default, kept per user) draws the entry
  action of a state in an FA tab as its behaviour alone, without `entry/`; the document is not
  changed. *EXT-FA 3.1; design*
- `EDIT-MODE-9` SHOULD [A]: Document > Check of an FA tab runs the EXT-FA check and shows each
  violation in Diagnostics with its requirement, linked to its element (`EDIT-PLUG-15`), or
  says there is none; it is disabled in the other tabs. *EXT-FA 5; design*
- `EDIT-MODE-10` MUST [A]: Document > Compare with... on a diagram tab opens a diff tab: the
  target is the tab's snapshot, the base another open diagram tab, a file, or the tab's saved
  file when it is modified; a plugin opens one with a `diff` result (`EDIT-PLUG-58`). Both
  sides are read as inspected (`EDIT-INSPECT-2`) in any format the library reads, an empty base
  being an empty document; the tab is titled `<target> ↔ <base>`; the source tabs are
  untouched. *design*
- `EDIT-MODE-11` MUST [A]: a diff tab shows the base and the target side by side, read-only,
  their elements marked added (the target only), removed (the base only) and changed; the
  change list in the structure tree dock lists the pairs by state machine with what differs, a
  row selecting and centring the element in both views. The pairs and what differs are the
  library's revision diff with the geometry option of Document > Geometry (off): without it
  the ids, the geometry and the `cyberiadaEditorView` and `regionGeometry` metainformation
  are not shown; a pair is named as the target names it. *design*
- `EDIT-MODE-12` MUST [A]: Document > Refresh takes both sides of the active diff tab again from
  their origins: the target from its source tab (kept when the tab is closed), the base from
  its tab, its file, the saved file, or a run of its plugin action with the same values.
  *design*
- `EDIT-MODE-13` MUST [A]: File > New > Text file opens an untitled text tab in Plain text,
  Markdown or a language; Open reads a file into a text tab in the mode its extension names
  (the `extensions` of the languages, `.md`, `.txt`), Plain text for an extension no mode
  names, a diagram for `.graphml`; the text is UTF-8 and Save writes it back as it is, Save As
  proposing the mode's first extension; Undo and Redo step through the text's own edits; the
  text is highlighted by the language (`EDIT-TEXT-7`) or as Markdown (`EDIT-TEXT-11`), and a
  switch between the text modes changes only the highlighting and the marker. *design*
- `EDIT-MODE-14` SHOULD [A]: a read-only Markdown tab shows the rendered document instead of the
  source; clearing the flag shows the source again. The rendering (`docs/MARKDOWN.md`) reads
  CommonMark with the GitHub tables, task lists and strikethrough; a TeX formula `$..$` stands
  on the text baseline, `$$..$$` is a centred line, one the engine cannot read shows its source
  in red monospace; an image is found relative to the directory of the tab's file (an untitled
  tab has none), an SVG one is drawn as vectors with its markers (the arrowheads), no wider than
  the text, a missing one as its alt text and path; a link opens outside the editor, a
  `#anchor` scrolls to its heading. *design*
- `EDIT-MODE-15` MUST [A]: the parts of the window follow the mode of the active tab
  (`docs/MODES.md`, The modes): a text tab hides the element toolbar and disables the zoom,
  Language, Markup, Properties..., the geometry actions and Export, and the tree and properties
  docks are empty; Geometry and Refresh act on a diff tab, Check on an FA tab only. *design*
- `EDIT-MODE-16` MUST [A]: the batch verbs `new-document [<mode>]`, `mode <id>` (`inspection`
  opens the inspection tab), `read-only on|off`, `compare <index|title|file>`, `geometry
  on|off` and `refresh` act as the menus do; `--dump-tabs` prints `i <title>[*] <mode>[ ro]
  [active]`; `--dump` of a text tab prints `== text` (the mode and the lines), of a diff tab
  `== diff` (the base label and the pairs with what differs, by id). *design*
- `EDIT-MODE-17` MUST [A]: the build option `CYBERIADA_MD_IMPORTER` chooses the Markdown reader
  of `EDIT-MODE-14` and `EDIT-TEXT-14`: `QT` (Qt's own, Qt 5.14 and newer), `MD4C` (the editor's,
  any Qt), `AUTO` (`QT` when the Qt has it); both give the same document structure — the
  headings, paragraphs, lists, quotes, code, tables, links, images and formulas. *design*

## 5. Coverage

Every requirement is covered by at least one case, named by the requirement id,
following the compat suite's discipline: an editing case that drives one
operation and asserts the requirement's law, plus a polygon standing law that
holds it over every exploratory run. A violation names both the requirement and
the operation that broke it.
