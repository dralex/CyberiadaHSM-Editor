# CyberiadaPro, the Cyberiada HSM Editor — Behavioural Specification

**Overview:** The document contains the requirements for the possible/required
editor behaviour. This document is used as the root specification for the test systems
used within the project.

**Document version:** 0.80 (2026-11-23)

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
  Adding new content (by `move`, `new-state` or `new-comment`) grows the parent (see also
  `EDIT-NODE-5`, `EDIT-NODE-11`): a comment shown inside a state is given its text-sized geometry so
  the state grows to contain it. A drag that
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
  prefix (EDIT-TEXT-2) are not changed. A preference turns it off. A text tab draws the colors
  of its color scheme (`EDIT-COLOR-1`). *design*

  ```
  entry/ foo(1);        ─► after the keyword "/"          (entry, exit, do behaviour)
  EV [x > 0] / f();     ─► the guard and after the "/"    (internal and edge transitions)
  formal comment body   ─► the whole text, unless its own dMarkup is markdown (EDIT-TEXT-11)
  titles, names, informal comments ─► plain
  ```

  Languages: the built-in ones (`syntax/*.json`: C, C++ (Arduino), C#, JSON, Python) and those the
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
  the document `platformLanguage`, else in one code colour. An inline element goes on over the
  line feeds of its paragraph as the reader joins them, not over a blank line, a table row or
  another block; an edit re-colours the earlier lines it changes. Colour only, as EDIT-TEXT-7: the
  text and its size are not changed. *PNST 1044 9.3; compat spec 1.9 CGML-9.3-1/3*

  ```
  informal   own dMarkup ─► markupLanguage ─► plain
  formal     CGML_META, CGML_COMPONENT ─► code
             own dMarkup markdown ─► Markdown      otherwise ─► code
  ```

  ```
  # Heading            ─► heading (the whole line)
  > quote              ─► the marker, the rest of the quote and its lazy lines as quote
  - item  1.           ─► the markers
  ---  ***             ─► a rule
  **strong** *em*      ─► emphasis
  `code`               ─► code
  [text](url) <url>    ─► link; the target underlined in a text tab (EDIT-MODE-28)
  *over a              ─► one element over the lines of a paragraph; the quote markers
  > line* [a⏎b](url)      and the indents between them keep their colours
  $x^2$  $$ … $$       ─► the dollars as markers, the TeX as a formula, where the reader
                          finds one (EDIT-MODE-14): not in code, \$ or $ next to a letter
  [text]: #  <!-- … --> ─► comment: the lines a rendered document does not show
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
- `EDIT-IO-7` MUST [A]: Save is enabled only for an open document in an editable tab
  (`EDIT-MODE-4`) that is modified or has a canvas text being edited (`EDIT-KEYS-10`); Save-As is enabled in every tab but a diff tab, Export in every diagram tab.
  *design*
- `EDIT-IO-8` MUST [A]: an SVG export records the diagram it is drawn from when asked. File >
  Export has *Record the source diagram in the SVG metadata*, off by default and remembered
  (`export/svgMetadata`), enabled for SVG of a saved, unmodified diagram and greyed with the
  reason otherwise; in batch mode `--svg-metadata` beside `--export <file>.svg`, a usage error
  (exit 1) for a modified diagram. On, the SVG has the `<title>` of the document name (else the
  file's base name), the `<desc>` `Generated by CyberiadaPro from <file>.graphml` and a
  `<metadata>` RDF description of itself (`rdf:about=""`): `dc:source` the diagram file
  relative to the SVG, `dc:format` `image/svg+xml`, `cyb:sourceDigest` `sha256:<hex>` of the
  diagram file and `cyb:font` the font family drawn (`cyb:` =
  `https://cyberiada.dev/ns/svg#`); off, the SVG is written as before. The drawing is the
  same either way. Specified in 0.72. *SVG 1.1 §21; design*

  ```xml
  <title>Traffic light</title>
  <desc>Generated by CyberiadaPro from semaphore.graphml</desc>
  <metadata><rdf:RDF><rdf:Description rdf:about="">
    <dc:source>semaphore.graphml</dc:source>
    <dc:format>image/svg+xml</dc:format>
    <cyb:sourceDigest>sha256:9f2c…</cyb:sourceDigest>
    <cyb:font>Courier New</cyb:font>
  </rdf:Description></rdf:RDF></metadata>
  ```

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

The window holds several documents; the structure is drawn in `docs/WINDOW.md`. The check of
the files changed on disk (`EDIT-DOC-13..18`) is specified in 0.80, the architecture in
`docs/GIT.md`, The disk check.

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
  a refused `save` and a failed write are script errors. *design*
- `EDIT-DOC-12` MUST [A]: File > Open Terminal, also the main toolbar button after Export,
  opens the system terminal in the folder of the active tab's file: `cmd` on Windows, Terminal
  on macOS, elsewhere the program of `TERMINAL` or the first found of `x-terminal-emulator`,
  `gnome-terminal`, `konsole`, `xfce4-terminal`, `mate-terminal`, `lxterminal`, `xterm`. It is
  enabled only for a tab with a file — an untitled tab, a plugin's `new-document` result
  before its save and a diff tab have none; a folder gone or a terminal that cannot start is
  reported. It has no default key (0.70). *design*
- `EDIT-DOC-13` MUST [A]: a tab with a file remembers the size, the modification time and the
  digest of the bytes it last read from the file or wrote to it. The file is checked when it
  changes on disk (a watcher on the files of the open tabs), on the window activation, after
  a git command of the editor (`EDIT-GIT-17`) and by the batch verb `disk-check`; the batch
  mode has no watcher, so a script checks by its verbs only. A size and a time as remembered
  end the check, unless the bytes were read within two seconds of the time of the file (the
  time step of a file system, as git's racy check); a digest as remembered only takes the new
  state (a touch, a save of the editor). An untitled, a diff and a text diff tab are not
  checked. *design*
- `EDIT-DOC-14` MUST [A]: a tab without unsaved edits whose file changed is read again from it,
  in its mode and with its read-only flag, the undo history emptied, the zoom and the scroll
  kept where they still apply. A file that cannot be read keeps the tab as it was, with the
  bar of `EDIT-DOC-15` saying `<name> changed on disk and cannot be read`. *design*
- `EDIT-DOC-15` MUST [A]: a tab with unsaved edits, or with a plugin running on it, whose file
  changed gets a bar above its page,
  `<name> changed on disk`: *Reload* reads the file, the edits lost and the undo history
  emptied; *Keep my edits* closes the bar and remembers the file as it is now, so a Save
  writes over it without a question; *Compare* opens a diff tab — a text diff tab for a text —
  of the file on disk against the tab, labelled `<name> ↔ on disk`, the bar kept. The bar stays until a choice, a Save or
  a reload. *design*

  ```
  +- traffic.graphml* ------------------------------------------------------+
  | traffic.graphml changed on disk      [Reload] [Keep my edits] [Compare] |
  +-------------------------------------------------------------------------+
  |   the canvas or the source, the edits kept until a choice               |
  ```
- `EDIT-DOC-16` MUST [A]: a tab whose file is gone gets the bar `<name> is not on disk` —
  *Close* closes the tab without a question, *Keep* closes the bar — and counts as modified, so
  closing it asks and Save writes the file again. The file back with the remembered bytes
  closes the bar and the tab is unmodified again, unless it was edited. *design*
- `EDIT-DOC-17` MUST [A]: a Save of a tab whose file changed since it was remembered — the bar
  shown or the change not seen yet — asks `<name> changed on disk. Overwrite it?`, Cancel the
  default; the batch `save` of such a tab is a script error. *design*
- `EDIT-DOC-18` MUST [A]: the batch verb `disk-check` checks the open tabs as the window
  activation does; `disk reload`, `disk keep`, `disk compare` and `disk close` answer the bar of
  the active tab, a script error without one, or with an answer the bar does not offer;
  `--dump-tabs` adds `disk changed`, `disk unreadable` or `disk absent` to a tab with a bar.
  *design*

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
  fails with the reason when that tab is closed (`EDIT-MODE-12`). In a text mode the base
  is `${output}` read as UTF-8 text and the tab is a text diff tab (`EDIT-MODE-19`, 0.65). A
  `diff` action lists only the diagram modes `hsm`, `fa`, `inspection` and the text modes,
  else the descriptor is rejected. *design*
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
  error ends it. *design*
- `EDIT-PLUG-62` MUST [A]: the `plugin` verb of a session action takes the lines up to a line
  `end` as its session script (`EDIT-PLUG-47`): the session verbs, run in order after the first
  state, `save FILE` inside the block being the session verb; a failed `expect`, a timeout or a
  failed session fails the run (`EDIT-PLUG-61`), and a block without `end` is a script error.
  *design*
- `EDIT-PLUG-63` MUST [A]: `--dump-actions` prints `== actions` before the plugin sections
  (`EDIT-PLUG-64`): every action of every plugin listed `enabled` (`EDIT-PLUG-1`), session
  actions included, in the search path order, one line each — `<id>:<action> enabled`, or
  `<id>:<action> disabled: <reason>` with the reason of `EDIT-PLUG-54`; on a diff tab every
  action is disabled. *design*
- `EDIT-PLUG-64` MUST [A]: `--dump` and `--dump-plugin` print `== plugin <n>` and
  `== diagnostics <n>` for the n-th `plugin` run of the script, and `--dump-session` prints
  `== session <n>` for a session one, in the formats of `EDIT-PLUG-28/48`, where the plugin
  sections stand for every kind of the active tab; the `--run-plugin` stage keeps its
  unnumbered sections, printed after them. The numbered sections and `== actions` are
  deterministic and name no machine path (`EDIT-PLUG-28`). *design*

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
- `EDIT-UI-6` MUST [A]: the structure tree, the properties panel, the change list and the
  outline are docks at
  the right of the canvas (as the plugin dock: a title bar with the float and close buttons,
  movable; every dock title is bold, the dock contents keep the regular weight), each with a
  checkable View entry — *Structure tree*, *Properties*, *Changes*, *Outline* — all shown by
  default;
  closing a dock unchecks its entry and the entry shows it again; the choice is kept per user
  between sessions. A dock is shown only in the tabs that have it, as the user chose: a
  diagram tab (HSM, inspection, FA) the tree and the properties, a diff and a text diff tab
  the changes, a Markdown tab the outline (`EDIT-MD-13`, 0.76), another text tab none; in the
  other tabs it is hidden and its View entry greyed, the choice unchanged (0.69). A hidden
  tree or panel keeps following the active document and the canvas selection, so showing it
  again needs no refresh; a tab switch keeps the sizes of the docks, the tree only grows once
  to fit a wider structure. The Repository and the History docks (`EDIT-GIT-3/15`, 0.80) are
  at the left of the canvas, tabbed together with the Repository in front, in every tab, with
  their View entries *Repository* and *History*, shown by default; without a git program or
  with *Use git* off (`EDIT-GIT-1`) they are hidden and their entries greyed. *design*

  ```
  tab                         Structure tree   Properties   Changes   Outline
  HSM, inspection, FA               x              x
  diff, text diff                                               x
  Markdown                                                                x
  text, a language
  ```
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
has a read-only flag; the architecture is `docs/MODES.md`. Specified in 0.47, the JSON check
(`EDIT-MODE-26`) in 0.66, implemented by the stages of `docs/MODES.md`.

- `EDIT-MODE-1` MUST [A]: every tab has one mode — `hsm` (HSM diagram), `inspection` (HSM
  inspection), `fa` (FA diagram), `diff` (HSM diff), `text-diff` (Text diff, 0.65), `text` (Plain text), `markdown`
  (Markdown) or a language — a built-in one (`c`, `cpp`, `csharp`, `json`, `python`) or one a plugin
  brings (`EDIT-PLUG-60`) — and a read-only flag, off in every new or opened tab; changing
  them in one tab leaves the others as they are (`EDIT-DOC-6`). *design*
- `EDIT-MODE-2` MUST [A]: the tab text and the window title carry the mode after the title —
  nothing for HSM, `[inspection]`, `[FA]`, `[diff]`, `[text diff]`, `[text]`, `[Markdown]`, `[<language
  name>]` — and the flag as `ro` in the tab, `read-only` in the window: `a.graphml [FA, ro]*`,
  `a.graphml [FA, read-only][*]`, `a.graphml [ro]` for HSM. *design*
- `EDIT-MODE-3` MUST [A]: Document > Mode lists the modes, the active tab's checked; an entry
  the tab cannot reach is disabled: an HSM or FA tab reaches HSM, FA and HSM inspection
  (`EDIT-MODE-6`), an inspection tab HSM and FA, a text tab every text mode, a diff or a text
  diff tab none.
  *design*
- `EDIT-MODE-4` MUST [A]: Document > Read-only sets the flag of the active tab. A tab is
  *editable* when it is HSM, FA or a text mode without the flag and not locked by a plugin
  (`EDIT-PLUG-22/41`); in a tab that is not, every edit is refused as in `EDIT-INSPECT-1` (a
  text takes no input), Undo, Redo and Save are disabled, and selecting, zooming, scrolling and
  copying keep working. The flag keeps the editing layout and the recovery of a diagram. In an
  inspection, a diff or a text diff tab the entry is checked and disabled. The main toolbar shows the entry
  as a toggle button with the inspection icon; the inspection itself is reached through
  Document > Mode only. *design*
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
  says there is none; it is disabled in the other tabs but a Markdown one (`EDIT-MD-8`).
  *EXT-FA 5; design*
- `EDIT-MODE-10` MUST [A]: Document > Compare with... on a diagram tab opens a diff tab: the
  target is the tab's snapshot, the base another open diagram tab, a file, or the tab's saved
  file when it is modified; a plugin opens one with a `diff` result (`EDIT-PLUG-58`). Both
  sides are read as inspected (`EDIT-INSPECT-2`) in any format the library reads, an empty base
  being an empty document; the tab is titled `<target> ↔ <base>`; the source tabs are
  untouched. On a text tab it opens a text diff tab (`EDIT-MODE-19`). *design*
- `EDIT-MODE-11` MUST [A]: a diff tab shows the base and the target side by side, read-only,
  their elements marked added (the target only), removed (the base only) and changed, a state
  machine by its border and title tab; in a changed pair the drawn text of what differs is
  tinted on both sides — the name, the trigger, guard and behaviour of a transition, the
  actions of a state that differ from their pairs (paired by type, the others by trigger, one
  without a pair whole), the body of a comment (0.64). The change list in the Changes dock
  (`EDIT-UI-6`) lists the pairs by state machine with what differs, that column kept in view (the
  element elided, whole in its tooltip, 0.64), a row selecting and centring the element in
  both views. The pairs and what differs are the
  library's revision diff with the geometry option of Document > Geometry (off): without it
  the ids, the geometry and the `cyberiadaEditorView` and `regionGeometry` metainformation
  are not shown; a pair is named as the target names it. *design*
- `EDIT-MODE-12` MUST [A]: Document > Refresh takes both sides of the active diff or text diff
  tab again from their origins: the target from its source tab (kept when the tab is closed), the base from
  its tab, its file, the saved file, or a run of its plugin action with the same values.
  *design*
- `EDIT-MODE-13` MUST [A]: File > New > Text file opens an untitled text tab in Plain text,
  Markdown or a language, the Markdown one also by the main toolbar button after New HSM
  diagram; Open reads a file into a text tab in the mode its extension names
  (the `extensions` of the languages, `.json` for JSON, `.md` and `.markdown`, `.txt`), Plain text for an
  extension no mode names, a diagram for `.graphml`, its dialog offering the CyberiadaML
  graphs, the Markdown files, the text files, the files of each language and all files - the
  type of the active tab chosen first (a diagram or a diff the graphs, a Markdown or a plain text
  tab its own, a language tab its files, a text diff tab its language) and the folder of the
  active tab's file, when it has one, opened first (0.74); the text is UTF-8 and Save writes it back as it is, Save As
  proposing the mode's first extension; Undo and Redo step through the text's own edits; the
  text is highlighted by the language (`EDIT-TEXT-7`) or as Markdown (`EDIT-TEXT-11`) in the
  colors of the current scheme (`EDIT-COLOR-1`), and a switch between the text modes changes
  only the highlighting and the marker. *design*
- `EDIT-MODE-14` SHOULD [A]: a read-only Markdown tab shows the rendered document instead of the
  source; clearing the flag shows the source again. The rendering (`docs/MARKDOWN.md`) reads
  CommonMark with the GitHub tables, task lists and strikethrough; a TeX formula `$..$` stands
  on the text baseline, `$$..$$` is a centred line, one the engine cannot read shows its source
  in red monospace; an image is found relative to the directory of the tab's file (an untitled
  tab has none), an SVG one is drawn as vectors with its markers (the arrowheads), no wider than
  the text, a missing one as its alt text and path; a comment (`[text]: #`, `<!-- … -->` over
  its lines too) shows nothing; a `#anchor` scrolls to its heading, a link to a file the editor
  opens (`EDIT-MODE-13`, `.graphml`) goes to its tab, `file.md#anchor` at its heading
  (`EDIT-MD-10`, 0.72), another link opens outside the editor. A heading has 1.5 text
  lines of space above it and 0.5
  below, a paragraph 0.75 above and below (the larger of two neighbouring spaces counts), an
  item of a tight list 0.15, the first block none; a quote is indented by a level, drawn with a
  bar at the left of each level and in a grey between the text and the background. The page is
  black text on white whatever the color scheme and the theme (`EDIT-COLOR-7`). *design*
- `EDIT-MODE-15` MUST [A]: the parts of the window follow the mode of the active tab
  (`docs/MODES.md`, The modes): a text tab hides the element and the diagram toolbars, showing
  the text toolbar in their place (`EDIT-MD-1`, 0.72), and disables the zoom,
  Language, Markup, Properties..., the geometry actions, Export (a Markdown tab exports its
  rendered PDF, `EDIT-MODE-18`) and the switches of the canvas
  view (the grid, the snap, the service objects, the transition text, the FA symbols, the
  component comments), their menu entries greyed, and the tree and properties docks are
  hidden (`EDIT-UI-6`, 0.69); Geometry and Refresh act on a diff tab, Check on an FA and a
  Markdown tab only (`EDIT-MD-8`). A text diff tab hides the element and the diagram
  toolbars, disables the zoom and Geometry, shows its change list in the
  Changes dock and enables Refresh, Next Change, Previous Change and Ignore Whitespace (0.65).
  *design*
- `EDIT-MODE-16` MUST [A]: the batch verbs `new-document [<mode>]`, `mode <id>` (`inspection`
  opens the inspection tab), `read-only on|off`, `compare <index|title|file>`, `geometry
  on|off` and `refresh` act as the menus do, `compare` on a text tab opening a text diff tab
  (`EDIT-MODE-25`); `--dump-tabs` prints `i <title>[*] <mode>[ ro]
  [active]`; `--dump` of a text tab prints `== text` (the mode and the lines; a JSON tab also
  `error: none|<line>:<column> <message>`, `EDIT-MODE-26`), of a diff tab
  `== diff` (the base label and the pairs with what differs, by id); `--export <file>.pdf` of a
  Markdown tab writes its rendered PDF (`EDIT-MODE-18`), any other export of a text tab is a
  usage error. *design*
- `EDIT-MODE-17` MUST [A]: the Markdown of `EDIT-MODE-14` and `EDIT-TEXT-14` is read by the
  editor's md4c reader on any Qt, not by Qt's `setMarkdown`: every image is drawn, one without
  an alt text or with a formatted one too; a header cell takes its column's alignment; the text
  inside raw HTML keeps its order, and none is lost after a `<br>`, an unclosed tag or one that
  is not HTML text formatting (`<rundir>`, shown as text). *design*
- `EDIT-MODE-18` MUST [A]: File > Export in a Markdown tab, read-only or not, saves the
  rendered current text as a PDF (`docs/MARKDOWN.md`, PDF): A4 portrait pages, 20 mm margins,
  the page number centred at the bottom; black text on white whatever the theme, the formulas,
  the images and the quote bars as on the rendered page, at its font and resolution; a line
  never splits between two pages; the images are found next to the file. *design*
- `EDIT-MODE-19` MUST [A]: Document > Compare with... on a text tab opens a text diff tab: the
  target is the tab's text with its unsaved edits, the base the tab's saved file when it is
  modified, another text tab, or a file read as UTF-8 — a missing or an empty one an empty
  text; a plugin opens one with a `diff` result in a text mode (`EDIT-PLUG-58`). The tab is
  titled `<target> ↔ <base>`, highlights both sides by the mode of the target tab, and leaves
  the source tabs untouched; Refresh takes the sides again as `EDIT-MODE-12`. Specified in
  0.65. *design*
- `EDIT-MODE-20` MUST [A]: a text diff tab shows the base on the left and the target on the
  right, read-only, each under a label naming it. The lines are compared as `git diff` does
  by default (the Myers difference); in a run of changed lines the base and the target lines
  pair in order while they are similar — at least half of their characters in common — and
  the others are removed (the base) or added (the target). A line and its pair stand on one
  row; the side without a line of a row has an empty filler row without a number; each side
  numbers the lines of its own text; a text without a final line feed loses no line. *design*
- `EDIT-MODE-21` MUST [A]: the removed lines are marked red on the base, the added ones green on
  the target, the pairs blue on both sides with their changed characters, extended to whole
  words, in a stronger blue; the language highlighting stays under the marks. The sides
  scroll together, vertically and horizontally; an overview ruler at the right of the target
  shows where the changes lie in the whole text, a click scrolling both sides there.
  *design*
- `EDIT-MODE-22` MUST [A]: the change list in the Changes dock lists the hunks of a text diff tab —
  a run of rows that are not equal — as `<base lines> → <target lines>` with what they are:
  `changed <n> → <m>`, `removed <n>`, `added <m>`; a row scrolls both sides to its hunk and
  makes it current. Document > Next Change (`Alt+Down`) and Previous Change (`Alt+Up`) make
  the next or the previous row of the change list current, in a text diff and in a diff tab,
  showing it as a click does; they stop at the ends. *design*
- `EDIT-MODE-23` SHOULD [A]: Document > Ignore Whitespace (off, per tab, not stored) compares
  the lines with their runs of spaces and tabs as one space and none at their ends; the lines
  are shown as they are, and the changed characters ignore the spaces too. *design*
- `EDIT-MODE-24` MUST [A]: in a text diff tab Copy and Select All act on the side the focus was
  in last, and Find searches that side — the target before any focus — marking its matches
  over the diff marks (`EDIT-FIND-5`). *design*
- `EDIT-MODE-25` MUST [A]: the batch verbs `compare <index|title|file>` on a text tab,
  `next-change`, `previous-change` and `ignore-whitespace on|off` act as the menus do;
  `--dump-tabs` prints the tab's mode as `text-diff`; `--dump` of a text diff tab prints `==
  text diff`: `base: <label>`, `target: <label>`, `ignore whitespace: yes|no`, `hunks: <n>,
  added: <n>, removed: <n>, changed: <n>` (the lines without a pair and the pairs), `current:
  <n>` (from 1, 0 without a current hunk), then each hunk as `@@ <base lines> | <target
  lines>` (`13`, `13-15`, `.` for none) and its rows `<base number> <target number> <mark>
  <text>` — the marks `=` equal, `-` removed, `+` added, `~` a pair as `<base text> | <target
  text>`, `.` the number of a filler side; of the equal rows only the 3 next to a hunk.
  *design*
- `EDIT-MODE-26` SHOULD [A]: a JSON tab (the built-in `json` language) checks its text as strict
  JSON (RFC 8259: one value, a scalar too; no comment, no trailing comma, no single quote, no
  `NaN`) on load, on a mode switch, on a whole-text replace and 300 ms after an edit; an empty
  or blank text is no error. The first error is shown by a red wavy underline under its token,
  the red number of its line in the margin and a bar under the text, `Line 3, column 7: missing
  comma`, a click on the bar putting the cursor at the error; the find marks (`EDIT-FIND-4`)
  stay. The check never changes the text, the history or the file; another mode drops the
  marks and the bar; a read-only JSON tab shows its source, checked, as the other languages do.
  The rules and the messages are in `docs/MODES.md`, JSON. Specified in 0.66. *design*
- `EDIT-MODE-27` MUST [A]: the source editor of a text tab, the sides of a text diff tab and the
  preview of the color schemes draw in the text editor font, apart from the diagram font: its
  family and size are set on Preferences > Text, *Text editors* (the monospaced families
  offered), the bundled Cyberiada Mono 12 pt by default. A change applies to the open tabs at
  once, the tab stop being 4 spaces of it; the rendered Markdown page, its PDF and the text
  dialogs keep their fonts. The font is stored per user (`display/editorFontFamily`, empty for
  the bundled one, `display/editorFontSize`); Reset of the Preferences restores it. Specified in
  0.67. *design*
- `EDIT-MODE-28` SHOULD [A]: in the source editor of a Markdown tab, highlighted (`EDIT-TEXT-11`),
  the target of a link, of an image, of an autolink and a bare URL is underlined in the link
  color; with Ctrl held the pointer over it is a hand, and a tooltip names it. Ctrl+click opens
  it: a `#anchor` puts the cursor on the line of its heading, a file the editor opens
  (`EDIT-MODE-13`, `.graphml`) goes to its tab (an open one is activated, a failure is a
  warning), `file.md#anchor` to the line of its heading (`EDIT-MD-10`, 0.72), another file
  and a site go to the system (an image to its viewer, a page to the
  browser). A relative path is taken from the directory of the tab's file; in an untitled tab it
  opens nothing. A plain click edits as before. A formula is underlined too and Ctrl+click edits
  it (`EDIT-MD-25`, 0.79). Specified in 0.68. *design*

  ```
  Ctrl+click on a target ─► #anchor            ─► the heading's source line
                           ─► .md .cpp .graphml ─► an editor tab
                           ─► pic.png doc.pdf   ─► the system's viewer
                           ─► https:// mailto:  ─► the browser, the mail client
  ```
- `EDIT-MODE-29` SHOULD [A]: the source editors - the text tabs, the sides of a text diff tab,
  the preview of the schemes - draw the cursor as a line, by default, or as a block over the
  character at the cursor, one space of the editor font wide, the character shown in inverted
  colors (a shell, Emacs); it is chosen on Preferences > Text, *Text editors*, `Cursor: Line |
  Block`, the preview showing the choice before Apply. A change applies to the open editors at
  once and a font change keeps the block one character wide; the choice is stored per user
  (`display/editorCursor`: `line` or `block`), and Reset of the Preferences restores the line.
  The line edits, the find bar and the canvas keep their cursors. Specified in 0.73. *design*
- `EDIT-MODE-30` SHOULD [A]: the long lines of a source editor scroll sideways, by default, or
  are wrapped at its width: a row breaks at any character, a line keeps one number at its first
  row, and a narrow right fringe shows a hook arrow, in the color of the line numbers, at the
  end of each row the line continues from (as the continuation lines of Emacs). It is chosen on
  Preferences > Text, *Text editors*, `Long lines: Scroll | Wrap`, the preview showing it before
  Apply, and by View > Wrap Long Lines, checkable, enabled in a text tab; a change applies to
  the open editors at once; stored per user (`display/editorWrap`: `scroll` or `wrap`), Reset
  of the Preferences restores scroll. The sides of a text diff tab always scroll: their rows
  align. Lines stay lines for Go to Line, Find, the errors and the links. Specified in 0.75.
  *design*
- `EDIT-MODE-31` SHOULD [A]: the source editor of a text tab, also beside the preview, has an
  Emacs-like mode line under its text and its error bar (`EDIT-MODE-26`): one row in the editor
  font, a long one elided at the right, of the parts

  | Part | Shows |
  |---|---|
  | state | `--` saved, `**` modified, `%%` read-only or locked by a plugin run |
  | name | the title of the tab without its marker |
  | `(line,column)` | of the cursor, from 1; the column counts a tab to the next stop of 4, as the text width does (`EDIT-MD-19`) |
  | mode | the name of the mode: `Markdown`, `Plain text`, `C++`, `JSON` |
  | `Fill <width>` | in a Markdown and a Plain text tab: bold while Fill as You Type is on, in the color of the line numbers while off; a click switches it (`EDIT-MD-22`) |
  | `<n> selected` | with a selection only: its characters, a line break one |

  in the line-number colors of the scheme, the text in the text color, a rule above. A
  rendered Markdown page and the sides of a text diff tab have none. View > Mode Line,
  checkable, on by default, shows or hides it in every editor; stored per user
  (`display/showModeLine`), Reset of the Preferences shows it. Specified in 0.78. *design*

  ```
   12 | The light has three states: red, yellow and green; the tim|er
   13 |
  ** notes.md   (12,61)   Markdown   Fill 90   24 selected
  ```

### 4.15 Find — FIND

The find bar searches the text the active tab shows and marks the matches over it; the
document is never changed. The architecture is `docs/FIND.md`. Specified in 0.60.

- `EDIT-FIND-1` MUST [A]: Edit > Find (Ctrl+F) shows the find bar under the document area with
  the focus in its field, all of it selected; the text selected in the active tab fills it when
  it is one line. Esc in the bar or its close button hides it, drops the marks and gives the
  focus back to the page. *design*
- `EDIT-FIND-2` MUST [A]: every change of the field or of an option searches the active tab
  again: the field as written in any case unless *Match case* is on; *Whole words* takes a match
  whose ends cut no word (`/reset` in `entry/reset()`, not `LED` in `LED_BUILTIN`), a word
  being letters, digits and `_` of any script; *Regular expression* reads the field
  as a Perl-compatible expression, an invalid one matching nothing; an empty match is never
  taken. The bar shows `n of m` (`m+` past 10000 matches), or `No matches` with the field
  tinted red. *design*
- `EDIT-FIND-3` MUST [A]: Find Next (F3, Enter in the field, the down arrow) and Find Previous
  (Shift+F3, Shift+Enter, the up arrow) make the next or the previous match current, the last
  wrapping to the first and back; with the bar hidden they show it first. A new search makes
  current the first match at or after the current one, else after the text cursor of a text
  tab or the first selected element of a diagram, else the first match. *design*
- `EDIT-FIND-4` MUST [A]: every match is marked by a yellow background (`#ffeb3b`), the current
  one by an orange one (`#ff9800`), under the glyphs, which a text tab draws black; the marks
  are not a part of the document: its text, its undo history, its modified state and every
  export stay as they are. *design*
- `EDIT-FIND-5` MUST [A]: a text tab searches the page it shows: the source, or the rendered
  document of a read-only Markdown tab (`EDIT-MODE-14`), whose formulas and images are not
  searched. The current match is scrolled into view; hiding the bar leaves it selected. *design*
- `EDIT-FIND-6` MUST [A]: a diagram tab (HSM, FA, inspection) searches the texts its canvas
  draws — the names, the actions, the transition labels, the comment titles and bodies, a
  rendered comment by its rendered text — in the order of `--dump-text`: the document tree,
  the texts of an element top-down. A text the canvas does not draw (the transition text off,
  a hidden component comment, the metainformation) is not searched. *design*
- `EDIT-FIND-7` MUST [A]: on the canvas the mark is a box behind the matched characters, one per
  line of a wrapped match; a match hidden beyond the ellipsis of a cut text (`EDIT-NODE-16`)
  marks the ellipsis. The element of the current match is selected and its mark scrolled into
  view at the current zoom. *design*
- `EDIT-FIND-8` MUST [A]: the marks follow the text: an edit, an undo or redo, a reload, a plugin
  result, a change of the mode, of the font or of the drawn texts searches again; the current
  match becomes the first at or after its old place, and the view, the selection and the text
  cursor stay where they are. *design*
- `EDIT-FIND-9` MUST [A]: the bar and its query are global (`docs/WINDOW.md`): a tab switch drops
  the marks of the tab left and searches the new one. In a diff tab Find, Find Next and Find
  Previous are disabled and the bar is greyed; a text diff tab searches one side
  (`EDIT-MODE-24`). *design*

### 4.16 Keyboard shortcuts — KEYS

Every built-in action has default keys the user may change; a text being edited takes its
keys first. The architecture and the default keys are in `docs/SHORTCUTS.md`. Specified in 0.62.

- `EDIT-KEYS-1` MUST [A]: every built-in action of the menus and the toolbars — the session
  entries and the dock toggles too — has the default keys of `docs/SHORTCUTS.md` or none; the
  menu shows the first key of an action and all its keys act. The entries that come and go
  (the plugin actions, Open Recent, the text modes, Language and Markup) have no keys. *design*
- `EDIT-KEYS-2` MUST [A]: the Shortcuts page of the Preferences lists the actions by their
  menus with their keys, the changed ones in bold; a filter narrows the list by action, menu
  or key. A key typed into the key field replaces the keys of the chosen action (Assign) or
  joins them (Add); Clear leaves it without a key; Reset gives it its default keys, Reset all
  every action. Ok and Apply put the keys to work at once; Cancel changes nothing. *design*
- `EDIT-KEYS-3` MUST [A]: no two actions hold one key: the page names the action that holds
  the key typed and, on a confirmation, takes it from that action. *design*
- `EDIT-KEYS-4` MUST [A]: only the changes are stored, per user (`shortcuts/<action>`): an
  action never changed follows the default keys of the editor it runs in; an action left
  without a key stays so. Reset of the Preferences drops the changes. *design*
- `EDIT-KEYS-5` MUST [A]: a key the focused text takes does not reach its action: while a text
  is edited on the canvas, in a text tab or in a field, the typing and editing keys — a tool
  letter, Del, the clipboard keys of a field — go to the text. *design*
- `EDIT-KEYS-6` MUST [A]: the key of an action shown only on a toolbar (Select, Pan, Zoom, Zoom
  to SM) acts while its toolbar is hidden; a disabled action ignores its key. *design*
- `EDIT-KEYS-7` MUST [A]: Edit > Go to Line (Alt+G) asks a line number of the text tab, the
  current one offered, and puts the cursor at the start of that line, scrolled into view. It
  is disabled in a diagram, a diff and a rendered Markdown tab; the text toolbar has it too
  (`EDIT-MD-1`, 0.72). *design*
- `EDIT-KEYS-8` SHOULD [A]: the default keys follow the platform where Qt names them (Find,
  Find Next, the next document) and keep `Ctrl+Y` beside `Ctrl+Shift+Z` for Redo. *design*
- `EDIT-KEYS-9` MUST [A]: Del on the focused point of a transition removes the point
  (`EDIT-EDGE-9`), not the transition selected with it. *design*
- `EDIT-KEYS-10` MUST [A]: an action that reads or replaces the document — New, Open, Save,
  Save as, Export, Close, Exit, a tab switch, a mode change, Check, Compare, the geometry
  entries, a plugin run, a session start — first ends the edit of a canvas text as a click
  outside does, so the text typed is in the document it works on; a Save after an edit that
  changed nothing writes nothing. *design*
- `EDIT-KEYS-11` MUST [I]: Esc in a canvas text being edited abandons the edit: the text is as
  it was before, the document and its undo history unchanged. Esc there, and in the find bar
  (`EDIT-FIND-1`), does not reach the action it is a key of. *design*
- `EDIT-KEYS-12` MUST [A]: in a text tab the keys of Undo and Redo, whichever they are, drive
  the Undo and Redo of the window (`EDIT-HIST-1/2`), `Ctrl+Y` among them. *design*

### 4.17 Autoreplace — AUTO

A typed pattern becomes its typographic character in a text for people, never in code. The
architecture, the default rules and the Markdown exclusions are in `docs/AUTOREPLACE.md`.
Specified in 0.62.

- `EDIT-AUTO-1` MUST [A]: when the characters typed complete the pattern of an enabled rule,
  they are replaced by its text at once (`--` by `–`, `<<` by `«`, `(c)` by `©`); of several
  patterns ending at the cursor the longest wins; a pattern is compared as written. *design*
- `EDIT-AUTO-2` MUST [A]: the rules replace only in the names (a state, a state machine, a
  pseudostate, a comment, a document), the bodies of the informal comments, the description
  of the metainformation and the Markdown texts (a comment body in Markdown, a Markdown tab)
  — on the canvas, in a text tab, in the properties panel, the text dialog and the structure
  tree. *design*
- `EDIT-AUTO-3` MUST [U]: nothing is replaced in code and values: the actions, the transition
  labels, the triggers, the guards, the behaviours, the formal and component comments not in
  Markdown, the ids and the other metainformation, the tabs of a language and of Plain text,
  the find field, the fragment of a comment link, the plugin parameters and session lines.
  *design*
- `EDIT-AUTO-4` MUST [A]: in a Markdown text nothing is replaced inside a code span or block, a
  formula, an HTML tag or comment, a link target or URL — an unclosed one counting to the end
  of its line or, a fence and a comment, of the text — nor in a line of a rule or a table
  border, nor the `>` opening a quote. *design*
- `EDIT-AUTO-5` MUST [U]: only typing is replaced: a paste, a drop, an undo or redo, a loaded
  file, a plugin result and the batch `edit-text` leave it as it is; the batch `type` is
  typing. *design*
- `EDIT-AUTO-6` MUST [I]: Undo right after a replacement, or Backspace with the cursor not
  moved, gives back the characters typed, the cursor after them; they then stay as typed.
  *design*
- `EDIT-AUTO-7` MUST [A]: the Autoreplace page of the Preferences has the switch *Replace as
  you type* and the table of the rules: each one switched, changed or removed, new ones added
  in the last row, a pattern never twice; Restore defaults gives the default rules. Ok and
  Apply act on the open texts at once; Cancel changes nothing. *design*
- `EDIT-AUTO-8` MUST [A]: the switch and the rules are stored per user (`autoreplace/enabled`,
  `autoreplace/rules`), the rules only when they differ from the default ones. *design*

### 4.18 Color schemes of the text editors — COLOR

The source editor of a text tab draws in the colors of a scheme; the schemes are edited in
the Preferences. The architecture, the colors and the storage are in `docs/COLOR-SCHEMES.md`.
Specified in 0.63.

- `EDIT-COLOR-1` MUST [A]: the source editor of a text tab (`EDIT-MODE-13`) draws its
  background, its text, the selection, the line numbers, the highlighting (`EDIT-TEXT-7/11`)
  and its mode line (`EDIT-MODE-31`, 0.78) in the colors of the current scheme; the canvas, the
  text dialogs, the
  exported images and the rendered Markdown (`EDIT-COLOR-7`) keep the colors of the White
  scheme as built in, whatever the current one. *design*
- `EDIT-COLOR-2` MUST [I]: two schemes are built in: *White*, the default — black text on white
  with the highlighting colors of the canvas — and *Dark* — yellow text (`#ffff00`) on blue
  (`#000099`); a scheme has all the colors of `docs/COLOR-SCHEMES.md`, The colors. *design*
- `EDIT-COLOR-3` MUST [A]: Preferences > Text has the group *Text editors* beside the diagram
  font: the editor font (`EDIT-MODE-27`), the list of the schemes, a swatch per color that opens
  a color dialog, and a preview of a Markdown text with a C++ block in the font and the scheme
  shown. Ok and Apply make the scheme shown current and store the changes; Cancel changes
  nothing. *design*
- `EDIT-COLOR-4` MUST [A]: *Copy...* adds a user scheme with the colors shown, under a name
  asked that is not empty and is no other scheme's in any case; *Rename...* and *Delete* act on
  a user scheme only, White taking the place of a deleted one; *Restore* gives a built-in
  scheme its own colors back and is disabled while it has them. *design*
- `EDIT-COLOR-5` MUST [A]: a change of the current scheme or of its colors re-colors every open
  text tab at once; its text, cursor, undo history and find marks stay as they are, the marks
  in their own colors in every scheme (`EDIT-FIND-4`). *design*
- `EDIT-COLOR-6` MUST [U]: the current scheme and the schemes are stored per user
  (`textSchemes/current`, `textSchemes/list`): a built-in scheme by its changed colors only, a
  user scheme by its name and all its colors, the list removed when nothing differs from the
  built-ins. An unknown current scheme is White; a missing or malformed stored color is the
  built-in one's, White's for a user scheme. *design*
- `EDIT-COLOR-7` MUST [A]: the rendered page of a read-only Markdown tab (`EDIT-MODE-14`) draws
  black text on white whatever the scheme and the theme, as its PDF does (`EDIT-MODE-18`).
  *design*
- `EDIT-COLOR-8` SHOULD [A]: Reset of the Preferences restores the built-in schemes, drops the
  user ones and makes White current. *design*

### 4.19 Markdown tools — MD

The authoring tools of a Markdown tab: the text toolbar, the image and the diagram inserts,
the diagram images kept up to date, the link check, the link insert, the live preview, the
outline, the fill of the paragraphs and the formula insert. The architecture is `docs/MARKDOWN.md`,
Tools, Outline, Fill and Insert Formula. Specified in 0.72, the outline (`EDIT-MD-13..17`) in 0.76,
the fill (`EDIT-MD-18..23`, of a Plain text tab too) in 0.77, the formulas (`EDIT-MD-24..27`) in 0.79.

- `EDIT-MD-1` MUST [A]: the toolbars follow the active tab: *Main* (the file, history and
  clipboard actions, Read-only) always; *Diagram* (Delete, the geometry, the centring, Grid,
  Snap, Service objects, Log session) in a diagram or a diff tab; *Elements* in a diagram tab;
  *Text* in a text tab, in the place of Elements: Go to Line in every text tab, Fill Paragraph
  in a Markdown and a Plain text one (0.77), Insert Image, Insert Diagram, Insert Link, Insert
  Formula (0.79) and Update Diagram Images in a Markdown one; a text diff tab has none of the
  three. The entries of a hidden toolbar stay in the menus, greyed; a toolbar the user closed
  stays closed. In a rendered (read-only) Markdown tab Go to Line, Fill Paragraph and the
  inserts are greyed, Update Diagram Images is not.
  *design*

  ```
  diagram tab   [Main ............ Read-only][Diagram: Delete Geometry Centre Grid Snap ...]
                [Elements (left)]
  Markdown tab  [Main ............ Read-only]
                [Text (left): Go to Line | Fill Paragraph | Image Diagram Link Formula | Update Diagram Images]
  Plain text    [Text (left): Go to Line | Fill Paragraph]
  ```
- `EDIT-MD-2` MUST [A]: Edit > Insert Image... (`Ctrl+Shift+I`), the text toolbar, an image
  file dropped on the source and an image pasted from the clipboard open the Insert Image
  dialog: the preview, the edits (`EDIT-MD-3`), the alt text (the file's base name proposed),
  the folder and the name it is saved as and the line it inserts. Insert puts `![<alt>](<path>)`
  as a paragraph of its own at the cursor, one undo step. An image inside the directory of the
  tab's file (a subfolder too) and not edited is linked where it is; another one is saved into
  the folder (`images/` by default, the last one used remembered, `markdown/imageFolder`); a
  pasted one as `image-<n>.png`, the first free n. A target that exists with another content
  asks Replace, Keep both (`<name>-<n>.<ext>`) or Cancel, which goes back to the dialog.
  *design*
- `EDIT-MD-3` MUST [A]: a raster image (a format Qt reads) is edited in the dialog: a crop frame
  with eight handles and its numbers, a rotation by 90° either way, a width limit scaling it
  down in its proportions, Reset. The edited image is saved as a new file in the format of its
  source (a JPEG at quality 90), the source file is never written; an image of the directory,
  edited, is proposed as `<name>-<n>.<ext>`. An SVG is copied as it is, without the edits.
  *design*
- `EDIT-MD-4` MUST [A]: the paths are relative to the directory of the tab's file, with `/`; a
  path with a space or a parenthesis is written as `<path>`, a `\`, `[` or `]` of an alt or link
  text with a `\` before it. An untitled tab has no directory: Insert Image, Insert Diagram and Insert Link
  first ask Save and Insert or Cancel; the batch verbs refuse it (exit 4). Insert Formula writes
  no file and inserts into an untitled tab, its verbs too (`EDIT-MD-24`). *design*
- `EDIT-MD-5` MUST [A]: Edit > Insert Diagram... opens the Insert Diagram dialog: the diagram —
  an open diagram tab with a file (its saved file is drawn, a modified tab is told so) or a
  `.graphml` file —, the folder it is copied to when it lies outside the directory of the tab's
  file (`diagrams/` by default, the last one remembered, `markdown/diagramFolder`), the font
  (the export font, `EDIT-IO-2`), the alt text (the document name proposed, else the title of
  the first state machine) and the line it inserts. Insert copies the diagram when outside (a
  file of the same content is reused, another asks as `EDIT-MD-2`), writes `<name>.svg` beside
  it with the source metadata (`EDIT-IO-8`) and puts `[![<alt>](<svg>)](<graphml>)` at the
  cursor, one undo step. A diagram the library cannot read is reported, nothing is written.
  *design*
- `EDIT-MD-6` MUST [A]: a diagram image of a Markdown text is, in this order: an image linked
  to a `.graphml` (`[![…](x.svg)](y.graphml)`, the diagram `y`); an SVG image whose metadata
  names its source (`EDIT-IO-8`); an SVG image beside a `.graphml` of its base name (`x.svg`,
  `x.graphml`). It is up to date when its `cyb:sourceDigest` is the SHA-256 of its diagram
  file, out of date otherwise, without a digest too. *design*
- `EDIT-MD-7` MUST [A]: Document > Update Diagram Images exports again every out-of-date
  diagram image of the text with the source metadata, each in the font of the SVG it replaces
  (its `cyb:font`, else its first `font-family`, else the export font), from the saved diagram
  file; the text stays as it is. The images updated and the ones failed, with the reason, go to
  Diagnostics; with none out of date it says so and offers Re-export All. *design*
- `EDIT-MD-8` SHOULD [A]: Document > Check (`F7`) of a Markdown tab, read-only too, shows in
  Diagnostics each finding with its `line:column`, a click putting the cursor there (in the
  source): an image or a link to a missing local file; a `#anchor` no heading of the text has; a
  `file.md#anchor` no heading of that file has (of its tab's text when it is open); an
  out-of-date diagram image (`EDIT-MD-6`); an image linked to a diagram other than the source
  its metadata names. A site is never fetched. Without a finding it says *The text has no broken
  link.* *design*
- `EDIT-MD-9` SHOULD [A]: Edit > Insert Link... (`Ctrl+K`) opens the Insert Link dialog: the
  text (the selection proposed, replaced by the link), the target — a heading of this text or of
  another Markdown file (this text, the `.md` files under the directory of the tab's file,
  Browse...), a file, or an address — and the line it inserts. The headings are listed by level
  from the text of the file's tab when it is open, else from the file; a heading's anchor is
  the one the rendered view scrolls to (`EDIT-MODE-14`): lower case, the spaces as `-`, the
  letters of any script kept, the other punctuation dropped, a repeated one `-1`, `-2`. Insert
  puts `[<text>](<target>)`, one undo step. *design*
- `EDIT-MD-10` MUST [A]: a link `file.md#anchor` opens the file's tab (an open one is
  activated) at the line of the heading in the source or scrolled to it on a rendered page —
  by Ctrl+click in the source (`EDIT-MODE-28`) and by a click on a rendered page
  (`EDIT-MODE-14`); an anchor no heading has only opens the tab. *design*
- `EDIT-MD-11` SHOULD [A]: View > Preview, off by default and remembered
  (`display/markdownPreview`), shows each writable Markdown tab as its source and its rendered
  page side by side in a splitter; the page is rendered again 300 ms after the last edit with
  its scroll kept, and scrolled to the block of the cursor's line when the cursor goes to
  another block. A read-only tab shows the rendered page alone (`EDIT-MODE-14`); Find, Go to
  Line and the inserts act on the source; `--dump` of such a tab adds `preview: yes`. *design*
- `EDIT-MD-12` MUST [A]: the batch verbs act on the active Markdown tab as the dialogs do,
  inserting before line `n` of `line <n>`, else at the end; the trailing text is the alt, the
  link text or the TeX; the diagram is drawn in the `--font` family, the bundled one without it:

  | Verb | Acts as |
  |---|---|
  | `insert-image <file> [crop <x>,<y>,<w>,<h>] [rotate 90\|180\|270] [width <px>] [to <dir>] [line <n>] [<alt>]` | Insert Image |
  | `insert-diagram <graphml> [to <dir>] [line <n>] [<alt>]` | Insert Diagram |
  | `update-diagram-images [all]` | Update Diagram Images; `all` = Re-export All; `== update` and a `<line> updated <svg>` or `<line> failed <svg>: <reason>` per image, or `none`, on stdout |
  | `insert-link <target> [line <n>] [<text>]` | Insert Link; an anchor no heading has is refused (exit 4) |
  | `insert-formula [display] [line <n>] <tex>` | Insert Formula as a paragraph of its own (`EDIT-MD-26`); the TeX is the rest of the line as written, no `\n` escape |
  | `edit-formula <line> <column> [inline\|display] <tex>` | Edit Formula of the formula holding the character at `line`:`column` (from 1), its kind kept unless given; none there is refused (exit 4) |
  | `check` | Check: `== check` and a `<line>:<column> <message>` per finding, or `none`, on stdout |
  | `preview on\|off` | View > Preview |
  | `fill-paragraph [line <n>\|lines <n>-<m>\|all]` | Fill Paragraph: the unit of the cursor, of line `n`, of the lines `n` to `m`, every unit (`EDIT-MD-20`) |
  | `unfill-paragraph [line <n>\|lines <n>-<m>\|all]` | Unfill Paragraph, the same units |
  | `text-width <n>` | the text width for the run, 20..200 (`EDIT-MD-22`) |

  A name clash keeps both (`<name>-<n>.<ext>`); a verb on another tab is refused (exit 4); the
  fill verbs act on a Plain text tab too, a read-only one or a line it has not is refused.
  *design*
- `EDIT-MD-13` SHOULD [A]: the Outline dock of a Markdown tab, read-only too (`EDIT-UI-6`, View >
  Outline, `display/showOutline`), shows the headings of the text as a tree and its images as
  leaves. The top row is the document (the file name, `untitled` without a file); a heading is
  a child of the nearest earlier heading of a lower level, a skipped level allowed; an image is
  a leaf of the heading whose section holds its line, in the order of the lines and the
  columns, an image before the first heading a leaf of the document row. A leaf shows the alt
  text, else the file name; a diagram image (`EDIT-MD-6`) has the diagram icon and, out of
  date, a warning mark; a tooltip gives the line, the anchor or the paths and the diagram
  state. The headings and the anchors are the ones of Check and Insert Link (`EDIT-MD-8/9`),
  so a reference image `![a][ref]`, an HTML `<img>` and a heading in a quote or a list are not
  in the tree. The text is the source of the tree: the tree is read-only (no rename, no drag,
  no edit) and nothing in it changes the text. *design*

  ```
  Outline                                                [_][x]
  ▾ 02-traffic-light-sm.md
    ▾ 2. Машины состояний для управления светофором на Arduino
      ▾ Теоретическая часть
        ▾ 2.3. Машины состояний
          ▾ Что такое машины состояний?
              ◇ Машина состояний для микроволновки  ⚠
      ▸ Практическая часть
  ```
- `EDIT-MD-14` SHOULD [A]: the outline is read from the text at most once a second: the first
  change after a reading starts a one-second wait that the next changes do not restart, so a
  text typed without a pause updates the tree every second. A hidden dock and a tab in the
  background read nothing; their outline is only marked out of date, and is read at once when
  the dock is shown or the tab is activated. A reading keeps a row whose heading or image is
  the same or similar (half of its characters) under the same parent, a heading of another
  level too, so its expanded state and the selection stay; a row that moved under another
  heading (its level changed past a sibling, its section moved) or was retyped beyond similar
  is replaced. *design*
- `EDIT-MD-15` SHOULD [A]: the tree and the text follow each other. A row made current (a click,
  the arrow keys) puts the cursor of the source at the heading's line or the image's
  `line:column`, scrolled into view, the focus kept in the tree; Enter does the same and
  focuses the source; on a rendered page the heading or the block of the line is scrolled to
  (`EDIT-MD-10`). A double click on a diagram leaf opens its diagram in a tab, as a click on
  the link does (`EDIT-MODE-28`); the row's context menu has Go to, Open Diagram, Open Image (the
  system viewer), Expand All and Collapse All. A cursor moved in the source makes the row of
  its section or image current, scrolled into view in the tree, the focus kept; an outline out
  of date does not follow the cursor until it is read. *design*
- `EDIT-MD-16` MUST [A]: a row made current while the outline is out of date reads the text
  first and goes to what the row named, not to the row now under the pointer: to the same
  heading or image when the reading kept it (`EDIT-MD-14`); else to the only heading of the
  same title, or the only image of the same file, that the reading added (a level changed, a
  section moved); else nowhere — the cursor stays, a tooltip at the tree says *The heading
  "<title>" is no longer in the text* (an image: *The image "<file>" is no longer in the text*)
  and the row at the cursor becomes current. A heading of that title that the text had before
  the reading is never chosen. *design*

  | In the text since the last reading | Row after the reading | The row made current |
  |---|---|---|
  | lines added or removed elsewhere | kept | goes to its new line |
  | the heading retitled | kept (similar) | goes to the renamed heading |
  | the heading removed | removed | nowhere, the tooltip |
  | the heading retyped beyond similar | replaced | nowhere, the tooltip |
  | the level changed, the same parent | kept | goes to the heading |
  | the level changed past a sibling, the section moved | replaced | goes to the added heading |
- `EDIT-MD-17` MUST [A]: `--dump-outline` reads the outline of the active tab after the script and
  writes a `== outline` section: the document row, then a line per row indented by two spaces a
  level — `h<level> <line> "<title>" #<anchor>`, `image <line>:<column> "<alt>" <target>`,
  `diagram <line>:<column> "<alt>" <svg> <graphml> up-to-date|out-of-date` — or `(none)` when
  the active tab is no Markdown one. *design*

  ```
  == outline
  document 02-traffic-light-sm.md
    h1 5 "2. Машины состояний для управления светофором на Arduino" #2-машины-состояний-для-управления-светофором-на-arduino
      h2 12 "Теоретическая часть" #теоретическая-часть
        h3 126 "2.3. Машины состояний" #23-машины-состояний
          h4 128 "Что такое машины состояний?" #что-такое-машины-состояний
            diagram 141:1 "Машина состояний для микроволновки" diagrams/2/microwave.svg diagrams/2/microwave.graphml out-of-date
  ```
- `EDIT-MD-18` MUST [A]: a Markdown text is filled by units: a unit is the lines of one
  paragraph, in a list item or a quote too; it ends at a blank line, before a line opening
  another block (a list item, a quote, a heading, a fence, a tag, a rule) and after a hard break
  (two trailing spaces or a `\`). Headings (ATX and setext), fenced and indented code, tables
  (a block with a delimiter row), HTML blocks, thematic breaks, link reference definitions and
  `$$` formulas are never filled. The first line of a unit keeps its prefix; the next ones take
  the quote marks of the first one, then the width of the marker of a list item, else the
  indent of the second line (of the first one when it is alone). *design*
- `EDIT-MD-19` MUST [A]: a line breaks only at a run of spaces outside a code span, a formula, a
  tag, a link target and a URL (as `EDIT-AUTO-4`), and not where the next line would open a
  block: `-`, `*` or `+` and a space, `1.` or `1)` and a space, `#`, `>`, a fence, a tag, a line
  of `=` or `-` alone. A filled line holds at most the text width, a tab counting to the next
  multiple of 4 columns; a longer word stays whole on a line of its own. *design*
- `EDIT-MD-20` MUST [A]: Edit > Fill Paragraph (`Alt+Q`, the text toolbar) fills the unit of the
  cursor, or each unit the selection touches: its words joined by one space and broken again
  at the text width, the cursor at the same character of a word; Edit > Unfill Paragraph
  (`Alt+Shift+Q`) joins each unit into one line. Each is one undo step (`fill paragraph`,
  `unfill paragraph`); a text that stays the same takes none. They act in the writable source
  of a Markdown or a Plain text tab and are greyed in another tab, a rendered, a read-only or a
  locked one. *design*

  ```
  width 30, Alt+Q                           ->
  - A list item whose text runs well           - A list item whose text runs
  past the width and on                          well past the width and on
  > quoted words that go on and on and on      > quoted words that go on and
                                               > on and on
  ```
- `EDIT-MD-21` SHOULD [A]: with Fill as you type, off by default, a Space or a Return typed in
  such a source (after the autoreplace, `EDIT-AUTO-1`) breaks a line longer than the text width
  at its last break point within the width, else at the first one after it before the key
  typed, none: nothing; the rest goes to a new line with the prefix of the unit and the
  following lines are not touched (Fill Paragraph refills them). The break is an undo step of
  its own: Undo right after it joins the line and the key typed stays. Nothing breaks in a
  block never filled, nor by a paste, a drop, an undo or redo, a load, a plugin result or a
  batch verb (as `EDIT-AUTO-5`). *design*
- `EDIT-MD-22` SHOULD [A]: the text width, 90 characters by default (20..200), and the switch
  are set on Preferences > Text, *Text editors*, `Text width: [n] characters [ ] Fill as you
  type`, applied by Ok and Apply; Edit > Fill as You Type, checkable and enabled in a Markdown
  or a Plain text tab, is the same switch. Both are stored per user (`editor/textWidth`,
  `editor/fillAsYouType`); Reset of the Preferences restores 90 and off. The width changes no
  display: Wrap Long Lines (`EDIT-MODE-30`) stays apart. *design*
- `EDIT-MD-23` MUST [A]: a Plain text tab fills the same way without the Markdown rules: a unit
  is the lines between blank lines, a line breaks at any run of spaces, the next lines take
  the indent of the second line (of the first one when it is alone). *design*
- `EDIT-MD-24` SHOULD [A]: Edit > Insert Formula... (`Ctrl+Shift+M`, the text toolbar) opens the
  Insert Formula dialog: the palette (`EDIT-MD-27`), the TeX (the selection proposed), the kind
  — *inline* `$..$` or *display* `$$..$$` —, the preview, black on white as the rendered page
  draws it, 200 ms after the last edit, with the errors of the TeX under it, and the text it
  inserts (`EDIT-MD-26`). Insert, disabled while the TeX is blank, puts the formula at the cursor,
  the selection replaced, one undo step (`insert formula`); a display formula without a
  selection goes as a paragraph of its own after the paragraph of the cursor, as an image does
  (`EDIT-MD-2`). A TeX with errors is inserted too, the page draws it as its red source. With
  the cursor, or an end of the selection, inside a formula (between its first and its last
  dollar) the action edits that formula (`EDIT-MD-25`). Greyed in a rendered, a read-only or a
  locked tab and in the other tabs. *design*

  ```
  Insert Formula ──────────────────────────────────────────────────────
  [Greek][Operators][Relations][Arrows][Structures][Brackets][Matrices][Functions][Other]
  │ α β γ δ ε θ λ μ π σ φ ω  Γ Δ Θ Λ Π Σ Φ Ω │   the buttons drawn as their formulas
  TeX:      [\frac{a}{b} + \sqrt{x^2+y^2}                    ]
  Kind:     (•) inline $..$   ( ) display $$..$$
  Preview:  [          a/b + √(x²+y²)  drawn            ]
            the errors of the TeX, red
  Inserts:  $\frac{a}{b} + \sqrt{x^2+y^2}$
                                                 [Insert] [Cancel]
  ```
- `EDIT-MD-25` SHOULD [A]: in the source editor of a Markdown tab, highlighted (`EDIT-TEXT-11`),
  a formula — its dollars and its TeX, never in code — is underlined in its colors; with Ctrl
  held the pointer over it is a hand and the tooltip says *Ctrl+click to edit the formula*, as
  over a link target (`EDIT-MODE-28`). Ctrl+click opens the dialog as *Edit Formula* with its
  TeX and its kind; Replace puts the new formula in place of the old one, its dollars included,
  one undo step (`edit formula`); Cancel or the same text changes nothing. A locked tab only
  underlines; a plain click edits as before. *design*
- `EDIT-MD-26` MUST [A]: the text of a formula: inline `$<tex>$` on one line, the line breaks of
  the TeX as spaces; display `$$<tex>$$` for a TeX of one line, else `$$`, the lines of the TeX
  and `$$`, each on a line of its own. The TeX is trimmed, its blank lines dropped (a blank line
  ends the paragraph), a `$` without a `\` before it written `\$`, a line Markdown would read as
  the start of a block (a list item, a quote, a heading, a fence, a rule, a setext underline)
  joined to the line before it with a space. A space goes before the formula when the
  character before it in its line is neither a space nor a punctuation or a symbol (Unicode P
  and S, as md4c reads a dollar), or is a `$`; the same after it. *design*

  ```
  at the cursor   TeX      written
  area|is         S        area $S$ is
  «|»             x        «$x$»
  $a$|            b        $a$ $b$
  ```
- `EDIT-MD-27` SHOULD [A]: the palette of the dialog: the tabs *Greek*, *Operators*,
  *Relations*, *Arrows*, *Structures*, *Brackets*, *Matrices*, *Functions* and *Other* of buttons
  drawn as their formulas (their TeX as text in a build without the formulas), the TeX in the
  tooltip. A click puts the entry's TeX at the cursor of the TeX field: the selection of the
  field goes into the first slot of a template (`a+b` selected, `\frac{}{}` gives
  `\frac{a+b}{}`), the cursor to the first empty slot, else after the entry; a command ending
  the entry takes a space before a letter (`\alpha b`, not `\alphab`). An entry is a
  command of LaTeX math as MathJax and KaTeX read it (`\mathbb{R}`, never JKQTMathText's own
  `\bbR`) that JKQTMathText draws without errors. *design*

### 4.20 Git — GIT

The git repository of the active tab in the editor itself: the Repository and the History
docks, the Git menu, the commit, the compares with a revision and the local branches. The
architecture is `docs/GIT.md`. Specified in 0.80; it supersedes the git plugin of
cybersphere-git-plugin, whose textconv driver is `cybparser listing` of libcyberiadaml.

```
 File Edit Document View Git Plugins Help
                         Stage File                      Ctrl+Alt+A
                         Unstage File
                         Discard Changes of the File...
                         -------------------------------------------
                         Compare with HEAD               Ctrl+D
                         Compare with the Index
                         Compare with a Revision...
                         File History
                         -------------------------------------------
                         Commit...                       Ctrl+Shift+K
                         Branch                        > [x] devel  ↑1
                         Refresh                           main
                                                           -------------
                                                           New Branch...
                                                           Delete Branch...

 +- Repository -------------------------------------------------------------------+
 | hsm-arduino-course  [devel ↑1 v]                                              |
 | [Tree|Groups] [Changed only] [Ignored]   [Stage] [Unstage] [Commit] [Refresh] |
 | Name                                              Index  Tree                 |
 | v diagrams/                                              *                    |
 |     blink.graphml  ●                                     M                    |
 |     traffic.graphml                               M                           |
 |   v 6/                                                                        |
 |       greenhouse.graphml                          A                           |
 |   notes.md                                               ?                    |
 |   README.md                                                                   |
 +-------------------------------------------------------------------------------+
```

- `EDIT-GIT-1` MUST [A]: the git program is the one of Preferences > Git, else `git` found in
  `PATH`; a git before 2.23 counts as not found. The Git page has the program (a path, empty
  for `PATH`, with Browse...), the line of the program found — `git version 2.43.0
  (/usr/bin/git)` or `git is not found` — and *Use git*, on by default; both are stored per user
  and act at Ok and Apply. Without a program or with *Use git* off the editor starts no git
  process: the docks are hidden (`EDIT-UI-6`) and the Git menu greyed with the reason.
  *design*
- `EDIT-GIT-2` MUST [A]: the repository of a tab is the work tree of its file — `git rev-parse
  --show-toplevel` in the folder of the file, so a linked work tree and a submodule are
  repositories of their own; a diff and a text diff tab have the repository of the file of
  their target. An untitled tab and a file outside any work tree have none. The work tree of
  a folder is looked up once and kept; a Save As and the window activation look it up again.
  *design*
- `EDIT-GIT-3` MUST [A]: the Repository dock shows the repository of the active tab: its title
  stays `Repository`, its first line is `<the folder of the work tree> (<branch>)` with the
  branch button (`EDIT-GIT-18`), so a long name never widens the dock. A switch
  to a tab of the same repository keeps the dock as it is, the tab's file made the current row
  and scrolled into view; a switch to another repository shows it with the folders expanded
  and the scroll it had when last shown. Without a repository the dock holds one line:
  `<file> is not in a git repository`, `The document has no file`, `git is not found` or
  `git is off`. *design*
- `EDIT-GIT-4` MUST [A]: the rows are the files of the work tree: the tracked ones (`git
  ls-files`), the untracked ones (`git status --untracked-files=all`) and, with *Ignored*, the
  ignored ones. A file has two letters of `git status --porcelain=v2`, the index and the work
  tree: `M` modified, `T` type changed, `A` added, `D` deleted, `R` renamed (the old path in the
  tooltip), `U` conflicted (both letters), `?` untracked, `!` ignored, none unchanged. The row
  of a file whose tab has unsaved edits shows `●` after the name. *design*
- `EDIT-GIT-5` MUST [A]: the dock toolbar switches the layout. *Tree*: the folders and the
  files, the folders first, the changed entries before the unchanged ones, then by name; a
  folder holding a change has `*` in the Tree column. *Groups*: *Conflicts*, *Staged*,
  *Changes*, *Untracked* and, with *Ignored*, *Ignored*, the paths flat in each, a file staged
  and changed again in both Staged and Changes, the empty groups and the unchanged files left
  out. *Changed only* keeps in the tree the changed files and the folders holding them. The
  layout, *Changed only* and *Ignored* are stored per user. *design*
- `EDIT-GIT-6` MUST [A]: the status is read again after a save of a tab of the repository,
  after every git command of the editor, when the git folder changes (the index, `HEAD`, the
  refs), on the window activation and on Refresh (the dock toolbar, Git > Refresh): at most
  once a second, only while the Repository or the History dock is shown, never blocking the
  window — the rows change when the read ends, and a read of a repository no longer shown is
  dropped. The reads pass `--no-optional-locks`, so they never hold the index lock against a
  git the user runs. *design*
- `EDIT-GIT-7` MUST [A]: a double-click, Return or *Open* on a file row opens it as Open does
  (`EDIT-DOC-2`) — a diagram or a text of a known mode — or activates its tab; another file
  opens in the program of the system. On a folder or a group they expand or collapse it.
  *design*
- `EDIT-GIT-8` MUST [A]: the context menu of the dock acts on the selected rows, a folder or a
  group standing for its files: *Open*; *Stage*, *Unstage*, *Discard Changes...*; *Compare with
  HEAD*, *Compare with the Index*, *Compare with a Revision...*, *History*; *Rename...* (F2),
  *Delete...* (Del); *Copy Path*, *Copy Relative Path*. An entry with nothing to act on is
  greyed: Stage without a changed or untracked file, Unstage without a staged one, Discard
  without a change in the work tree, the compares without exactly one file the editor opens,
  History and Rename without exactly one file. The dock toolbar has Stage, Unstage, Commit...
  and Refresh. *design*
- `EDIT-GIT-9` MUST [A]: Stage runs `git add -- <files>`: the files as saved — the unsaved edits
  of a tab are not staged and its row keeps `●`; Unstage runs `git restore --staged --
  <files>`, before the first commit `git rm --cached -q -- <files>`. Neither changes a file on
  disk. *design*
- `EDIT-GIT-10` MUST [A]: Discard Changes... asks first, No the default, naming the files and
  the open tabs whose unsaved edits are lost too; Yes runs `git restore -- <files>` — the work
  tree back to the index, the staged changes kept — and deletes the untracked files. The open
  tabs of the files are read again, their edits dropped (`EDIT-DOC-14`); a tab of a deleted
  file gets the bar of `EDIT-DOC-16`. A conflicted file is not discarded. *design*
- `EDIT-GIT-11` MUST [A]: Rename... asks the new name, a path relative to the folder of the
  file; a tracked file moves by `git mv`, another file is renamed, the folder made; a name
  taken or outside the work tree is refused. The tab of the file follows it: the title, the
  Save target, the repository; a text tab keeps its edits, a diagram tab with unsaved edits
  is refused with `save it before renaming it` (the library writes the file it read). Delete... asks first, No the default, naming the files and the open tabs it
  closes; a tracked file is removed by `git rm -q`, which refuses a file with changes not
  committed, another file is deleted; the tabs of the deleted files close without a question.
  The Markdown links and the submachine references to the file are not changed (`docs/GIT.md`,
  Later). *design*
- `EDIT-GIT-12` MUST [A]: Compare with HEAD, with the Index and with a Revision... open the diff
  tab of the file (`EDIT-MODE-10`), a text diff tab for a text (`EDIT-MODE-19`): the base is the
  file in the revision (`git cat-file blob <revision>:<path>`) or in the index (its stage 0
  entry), the target the file's tab, opened first when it is not open. A file the base lacks is
  compared with an empty document, a staged rename with its old path; a conflicted file has no
  index base, so Compare with the Index is greyed for it. The tab is labelled `<name> ↔ HEAD`,
  `<name> ↔ index` or `<name> ↔ <the revision as typed>`; Document > Refresh reads the base
  again, `HEAD` resolved anew. *design*
- `EDIT-GIT-13` MUST [A]: Compare with a Revision... asks the revision in an editable list of the
  last commits of the file, `HEAD` first; a branch, a tag, a hash or `HEAD~1` is taken as
  typed; a revision starting with `-` or one git does not know keeps the dialog open with the
  reason. *design*
- `EDIT-GIT-14` MUST [A]: Git > Commit... and Commit... of the dock open the commit dialog,
  titled `Commit to <branch>` (`Commit at <hash>` when detached): the staged files with their
  letters, a rename as `old → new`; a warning for each tab of the repository with unsaved edits
  (they are not committed); a message of several lines; *Amend the last commit*, which fills an
  empty message with the last one and names the commit it amends. Commit is greyed with a
  blank message, with nothing staged and Amend off, and with a conflicted file; during a merge
  the dialog says the commit concludes it. Commit runs `git commit -F -` (`--amend`), the
  message on stdin, the hooks without a terminal; a failure keeps the dialog open with git's
  message, a success closes it and reads the status. A message not committed is offered again
  at the next open, until the editor exits. *design*

  ```
  +- Commit to devel -----------------------+
  | Staged (2):                              |
  |   M  diagrams/traffic.graphml            |
  |   A  diagrams/6/greenhouse.graphml       |
  | ! blink.graphml has unsaved edits        |
  | Message:                                 |
  | +--------------------------------------+ |
  | | traffic light: night mode            | |
  | +--------------------------------------+ |
  | [ ] Amend the last commit                |
  |                      [Commit] [Cancel]   |
  +------------------------------------------+
  ```
- `EDIT-GIT-15` MUST [A]: the History dock lists the commits — the short hash, the date, the
  author, the subject — of the file of the current row of the Repository dock, else of the
  active tab, its renames followed (`git log --follow`), a staged rename by its old path; with
  *Repository* those of the whole repository; read again after each status read. It lists the last 100, a *More...* row adds 100. A double-click on a commit, or
  *Compare with This Revision* of its menu, compares the file — in the Repository scope the
  active tab's — with it (`EDIT-GIT-12`); *Copy Hash* copies the full hash. Without commits it
  holds `no commits`; Git > File History and History of the dock show it on the file. *design*
- `EDIT-GIT-16` MUST [A]: the Git menu (Alt+T: Alt+G is Go to Line), between View and Plugins, acts on the file of the active
  tab, a diff tab on the file of its target: *Stage File* (Ctrl+Alt+A), *Unstage File*,
  *Discard Changes of the File...*; *Compare with HEAD* (Ctrl+D), *Compare with the Index*,
  *Compare with a Revision...*, *File History*; *Commit...* (Ctrl+Shift+K), *Branch*
  (`EDIT-GIT-18`), *Refresh*. In a tab without a repository the entries are greyed, the reason
  in the tooltip. *design*
- `EDIT-GIT-17` MUST [A]: every git command of the editor that may change the repository, and
  every one that fails, is written with its output into the *Git output* tab of the plugin dock
  (`EDIT-PLUG-17`), headed `== git <command> - <work tree>`; a failure outside the commit and
  the branch dialogs is also shown with git's message in a message box. git runs with stdin
  closed (the commit message aside), `GIT_TERMINAL_PROMPT=0` and `GIT_LITERAL_PATHSPECS=1`;
  only its `-z` output is read, its messages are passed on as they are. *design*
- `EDIT-GIT-18` MUST [A]: the branch button in the first line of the Repository dock and Git >
  Branch, one menu, show the branch — `devel`, `devel ↑1 ↓2` when it is ahead or behind its upstream, `detached at
  3f2a9c1`, `devel (no commits)`, with `, merging` or `, rebasing` while one is in progress —
  and list the local branches, the current one checked, then *New Branch...* and *Delete
  Branch...*; New Branch... is greyed before the first commit, Delete Branch... with no branch
  but the current one. *design*
- `EDIT-GIT-19` MUST [A]: a click on another branch switches to it: the tabs of the work tree
  with unsaved edits are named in `Save <n> documents before switching?` — *Save and Switch*,
  *Cancel* — then `git switch <branch>` runs; a refusal (the local changes it would
  overwrite) shows git's message and changes nothing; after the switch the status is read
  and the open tabs are checked against the disk (`EDIT-DOC-13..16`). The branches are greyed
  while a merge or a rebase is in progress, the reason in the tooltip. *design*
- `EDIT-GIT-20` MUST [A]: New Branch... asks the name, the start — the current commit or a local
  branch — and *Switch to it*, on by default: `git switch -c <name> <start>`, or `git branch
  <name> <start>` without the switch; a start other than the current commit asks the question
  of `EDIT-GIT-19` first; a name git refuses keeps the dialog open with git's message. Delete
  Branch... lists the local branches but the current one and deletes the chosen one by `git
  branch -d`: a branch not merged is refused with git's message. *design*
- `EDIT-GIT-21` MUST [A]: the batch verbs act as the GUI without a dialog — a verb is the
  confirmed action — and wait for git; a verb without a repository, a refused one and a git
  failure are script errors (4), git's message on stderr. *design*

  | Verb | Does |
  |---|---|
  | `git refresh` | looks the repository of the active tab up again and reads its status |
  | `git stage <path>...`, `git unstage <path>...` | `EDIT-GIT-9`; a path relative to the work tree |
  | `git discard <path>...` | `EDIT-GIT-10`, the open tabs read again |
  | `git mv <from> <to>`, `git rm <path>...` | `EDIT-GIT-11` |
  | `git commit "<message>" [amend]` | `EDIT-GIT-14`; `\"`, `\\` and `\n` in the message |
  | `git compare <path> head\|index\|<revision>` | `EDIT-GIT-12` |
  | `git history <path>\|repository` | `EDIT-GIT-15` |
  | `git switch <branch>` | `EDIT-GIT-19`, the unsaved tabs of the work tree saved |
  | `git branch <name> [from <start>] [switch]`, `git branch-delete <name>` | `EDIT-GIT-20` |
  | `repository layout tree\|groups`, `repository filter all\|changed`, `repository ignored on\|off` | `EDIT-GIT-5` |
- `EDIT-GIT-22` MUST [A]: `--dump-repository` writes `== repository` of the active tab:
  `root <the folder of the work tree>`; `branch <name>`, ` (no commits)` before the first one,
  or `detached <short hash>`; `upstream <name> ahead <n> behind <n>` and `state
  merging|rebasing` when they apply; a line per local branch, `* ` before the current one;
  then a line per file of the status in the order of the paths, `<index letter><tree
  letter> <path>` with `.` for no letter, ` from <old path>` after a rename, ` open` and
  ` unsaved` after the path of an open file, the ignored files only with *Ignored*; `none
  <the line of the dock>` without a repository. `--dump-history` writes `== history`, a line
  `<short hash> <subject>` per row, `more` for the *More...* row, `none <the title>` without a
  commit. *design*
- `EDIT-GIT-23` MUST [A]: the plugin token `git` keeps its `.git` lookup (`EDIT-PLUG-55`): the
  plugins do not depend on the git of the editor, nor on *Use git*. *design*

## 5. Coverage

Every requirement is covered by at least one case, named by the requirement id,
following the compat suite's discipline: an editing case that drives one
operation and asserts the requirement's law, plus a polygon standing law that
holds it over every exploratory run. A violation names both the requirement and
the operation that broke it.
