# CyberiadaPro, the Cyberiada HSM Editor — Behavioural Specification

**Overview:** The document contains the requirements for the possible/required
editor behaviour. This document is used as the root specification for the test systems
used within the project.

**Document version:** 0.28 (2026-10-15)

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
  reconstructed on request - rebuilt from scratch, or *updated*: the drawn elements keep
  their places and the geometry-less ones are laid out around them. The `[U]` geometric laws hold after every operation because this
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
 │ editor    │──interaction─▶│ TOOL INSPECT DOC │───[A]─────▶│  graph +     │   (every round)
 │ design    │  history      │ HIST  PLUG       │───[I]/[A]─▶│  rectangle   │
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
- `EDIT-STRUCT-11` MUST [A]: a state machine with no geometry is not drawn on the scene; the
  element tree marks it — its name is shown in square brackets and its icon greyed — while the
  inline-rename value stays the plain name. *design*
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
  content (by `move` or `new-state`) grow the parent (see also `EDIT-NODE-5`). A drag that
  moves a child within its own parent grows the parent to keep it inside the same way
  (snap-to-grid included); a dragged element is never relocated away from where it is
  dropped. *design*
- `EDIT-NODE-3` MUST [A]: an element's point is its centre; a top-level element
  is in absolute coordinates, a nested one relative to its parent's centre.
  *design*
- `EDIT-NODE-4` SHOULD [U]: a state machine without a rect frames the union of
  its content and sits at the origin; several rect-less machines would share
  the origin, so the geometry reconstruction of a document with several
  machines gives every machine its rect and places the machines side by side.
  *design*
- `EDIT-NODE-5` MUST [A]: a state, a comment, and a state machine resize directionally - a drag
  of the right or bottom border (or the corner between them) grows that side; a drag of the top or
  left border moves the element. A state and a machine clamp to their content; a comment holds none.
  A container resize holds the absolute position of every contained element (a state, a pseudostate
  including a rect choice, and a comment alike); only the dragged edge extends, no child is
  relocated. A child without stored geometry is laid out by the scene and is neither counted in the
  content the resize clamps to nor relocated by it, so sizing and holding cover the same children;
  the editor never fabricates a rectangle for such a child (`EDIT-ROBUST-1`). *design*
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
  only that point; the endpoints stay. A point is removed by double-clicking its
  dot or pressing Delete on it. *design*
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

  Languages (`syntax/*.json`): C, C++ (Arduino), C#, Java, JavaScript (TypeScript), Python; the
  value is matched case-insensitively by the name or an alias.
- `EDIT-TEXT-8` SHOULD [A]: the new action and new internal transition dialogs highlight the
  entered text as EDIT-TEXT-7 does; an exported image is plain unless the export preference
  keeps the colours. *design*
- `EDIT-TEXT-9` SHOULD [A]: the Edit > Language menu (after Edit > Tools) sets the
  `platformLanguage` of the active document; choosing an entry writes the value, *Undefined*
  removes the parameter, each choice is one undo step; an empty document first gets the state
  machine the first drawn element would create (the format needs one). The checked entry follows the active document on load, tab switch, properties edit,
  undo/redo and batch `update-meta`. The menu is disabled in inspection mode. *design*

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
- `EDIT-TEXT-12` SHOULD [A]: the Edit > Markup menu (after Edit > Language) sets the
  `markupLanguage` of the active document with the rules of `EDIT-TEXT-9`: *Plain* removes the
  parameter (a `plain` value checks it too), *Markdown* writes `markdown`, an unknown value shows as
  an extra entry kept for the session, the check follows the active document, and the menu is
  disabled in inspection mode. A comment's own `dMarkup` still overrides the document value. *design*
- `EDIT-TEXT-13` MUST [A]: a model reset (a document load, an undo/redo, the geometry
  reconstruction or update) discards an unfinished canvas text edit before the old document is
  freed: nothing is committed and no undo step is added. *design*

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
- `EDIT-META-6` MUST [A]: a `CGML_COMPONENT` formal comment is a machine-readable component
  descriptor; it is not drawn on the scene by default — even when it carries geometry (the geometry
  reconstruction lays one out) — and is drawn only while the *show component comments* preference is
  on. It is always present in the element tree and editable through the properties. A sibling of
  `EDIT-META-1`, which keeps the `CGML_META` node off the scene unconditionally. *design*

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

### 4.11 Documents and tabs — DOC

The window holds several documents; the structure is drawn in `docs/WINDOW.md`.

- `EDIT-DOC-1` MUST [A]: the window starts with one untitled document and no tab line; the tab
  line appears with the second open document and disappears when one is left. *design*
- `EDIT-DOC-2` MUST [A]: New opens a new untitled document in a new tab without a prompt; Open
  loads into the active tab when it is a clean untitled document and into a new tab otherwise;
  opening a path that is already open activates its tab instead of loading it twice. *design*
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
- `EDIT-DOC-6` MUST [A]: inspection mode is an application mode — switching it on makes every
  open document read-only (`EDIT-INSPECT-1`) and the window title of the active document carries
  the marker. *design*
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

### 4.12 Plugins — PLUG

The plugins are external programs run on the active document: the architecture is
`docs/PLUGINS.md`, the contract a plugin follows is `docs/PLUGIN-API.md` (`api` 1).

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
  is listed as disabled with the reason; its actions are disabled, the reason in the tooltip.
  *design*
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
  edits (an empty file for an empty document), `${selection}` the selected element ids in the
  document order; `request.json` is written exactly when the argv uses `${request}`. *design*
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
  outdir in the path order. *design*
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
- `EDIT-PLUG-24` MUST [A]: inspection mode disables the `replace` actions, and a result that
  arrives after it is switched on is refused. *design*
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
  state; the enable switch is kept, and Rescan rebuilds the menu and the toolbars. *design*
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
  the canvas (as the plugin dock: a title bar with the float and close buttons, movable), each
  with a checkable View entry — *Structure tree*, *Properties* — both shown by default; closing a
  dock unchecks its entry and the entry shows it again; the choice is kept per user between
  sessions; a hidden tree or panel keeps following the active document and the canvas
  selection, so showing it again needs no refresh. *design*
- `EDIT-UI-7` MUST [A]: `CYBERIADA_PRO_CONFIG_HOME` keeps the preferences in the ini file
  `<folder>/Cyberiada/CyberiadaPro.ini` and `CYBERIADA_PRO_DATA_HOME` the user data, the user
  plugin folder `<folder>/Cyberiada/CyberiadaPro/plugins`, instead of the places of the system
  (the registry and `%APPDATA%` on Windows, `~/.config` and `~/.local/share` on Linux): a
  separate, portable configuration, the same on every system; the tests set both. *design*

## 5. Coverage

Every requirement is covered by at least one case, named by the requirement id,
following the compat suite's discipline: an editing case that drives one
operation and asserts the requirement's law, plus a polygon standing law that
holds it over every exploratory run. A violation names both the requirement and
the operation that broke it.
