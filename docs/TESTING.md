# The Testing System

The editor is tested as a black box: the full application — window, scene and
model wired exactly as shipped — runs under the Qt offscreen platform and is
driven through the batch mode instead of the GUI event loop. Test verdicts come
from exit codes, canonical stdout dumps and produced files compared against
good references.

The behaviour under test is written down in `docs/EDITOR-SPEC.md` — the root
specification of the editor's required behaviour, its requirements identified
`EDIT-<AREA>-<n>`. Every layer here checks those requirements; a verdict traces
to the requirement it holds.

## Architecture

```
  run-tests.sh  /  ctest
    |   per test: diagram .graphml [+ command script] + good files
    v
  CyberiadaEditor --batch <file>   (QT_QPA_PLATFORM=offscreen)
  +------------------------------------------------------+
  | full application: window + scene + model             |
  | batch driver instead of app.exec():                  |
  |   open <file>      -> model + scene loading          |
  |   dump             -> canonical text on stdout  (L1) |
  |   edit <op> ...    -> model/scene mutators      (L2) |
  |   save / export    -> document / image files    (L2+)|
  |   errors           -> stderr + exit code, no dialogs |
  +------------------------------------------------------+
        |                |                 |
   stdout dump      saved .graphml     exported image
   vs *-output.txt  vs *-output.graphml   vs good image
                    (or cybparser diff)   (with tolerance)
```

### Undo/redo

```
  gesture (scene press ... release)     single-shot edit (dialog, property, script)
          |                                        |
   beginUndoStep()  ... N mutations ...  endUndoStep()   (a mutator opens its own
          |                                                step when none is open)
          v
   first mutation of a step: encode the document "before"; on the step end:
   encode "after"; push DocumentStep{before, after} if they differ
          |
   undo/redo: decode the snapshot into the document -> model reset -> the
   scene rebuilds its items (view kept, selection restored by id), the
   properties widget clears, the tree is re-rooted; the clean state of the
   stack marks the window title and drives the save prompt
```

The snapshot is the Cyberiada 1.0 encoding in memory; a restored document
keeps its file identity, so the `file:`/`format:` fields of the dump do not
move across an undo. The undo tier proves the fidelity end to end: the
`undo-all` script edits the hierarchy diagram with the `add-elements`
commands and undoes them all - the dump must equal the untouched L1 good
file; `redo-all` redoes them all and must equal the `add-elements` L2 dump
and saved document. Neither case owns a reference file.

## Test layers

Each layer checks a class of `EDITOR-SPEC` requirements. The layers below are the
mechanism; the matrix after them maps each specification area to the layers that hold it.

| Layer | What is checked                                                | Status      |
|-------|----------------------------------------------------------------|-------------|
| L0    | smoke: every diagram opens offscreen, the process exits clean  | implemented |
| L1    | load/dump: canonical model + scene dumps vs good text        | implemented |
| L2    | editing: scripted mutations, then dump/save vs good files         | implemented |
| L3    | render: offscreen image export vs good images with tolerance | implemented |
| L4    | in-process: model contract and scene structure (QtTest)        | implemented |
| text  | text metrics: font and layout of every text item vs good text | implemented |
| reconstruct | reconstruction: the rebuilt geometry, shown and saved, vs good files | implemented |
| undo  | undo/redo: a script edits and undoes, the dump vs the existing good files | implemented |
| polygon-unit | the unit tests of the test polygon package (`tests/polygon/`, see `POLYGON.md`); opt-in with `-DPOLYGON_TESTS=ON` | implemented |
| polygon-<id> | one case per open problem of the polygon register, green while it reproduces; opt-in with `-DPOLYGON_TESTS=ON` | implemented |

Which layer checks which `EDITOR-SPEC` area (the check kind is the requirement's, see the
specification §2):

| Area (`EDITOR-SPEC` §4) | Check kind | ctest layers                     |
|-------------------------|------------|----------------------------------|
| ROBUST                  | [C]        | L0, L2, undo                     |
| STRUCT                  | [U]        | L1, L4                           |
| SEM                     | [U]        | L4                               |
| NODE                    | [U]/[A]    | L1, L3, reconstruct              |
| EDGE                    | [U]/[A]    | L2, L3, reconstruct              |
| TEXT                    | [U]/[A]    | text, L3                         |
| TOOL                    | [A]        | L2, L4                           |
| HIST                    | [I]        | undo                             |
| IO                      | [I]/[C]    | L1, reconstruct, L2 save/reject  |
| META                    | [U]/[A]/[I]| L1, L2                           |

The exploratory polygon runs the same requirements as standing laws over long histories;
`POLYGON.md` maps the check kinds to its oracles.

### Requirement coverage

Representative cases per `EDITOR-SPEC` area (not exhaustive; a case may cite the requirement
it checks in a comment):

| Area   | Cases (examples) |
|--------|------------------|
| ROBUST | `l0-*` smoke, `l4 test_crash_marker`, `test_batch_action_guard`, `test_edit_action_gating`, the `inspect-reject-*` cases |
| STRUCT | `l1-*` / `l4 test_item_hierarchy`, `test_load_scene`; names `test_name_edit`, `test_default_name_unique`, `test_name_only_state`; delete `l2 delete` + `test_delete`; subjects `l2 subjects`, `transition-subjects`, `test_move_subjects` |
| SEM    | `test_choice_edge_rule`, `test_choice_tip_attach`; endpoint kinds via the transition cases |
| NODE   | grow `test_move_grows_parent`, `test_nested_state_grows_parent`, `test_grow_cascades_to_ancestors`, `test_grow_skips_rectless_sm`; resize `test_border_resize`, `test_directional_grow`, `test_container_resize_clamp`; place `test_new_element_place`; comment name `test_comment_name`; blocks `test_action_layout`; frame `reconstruct-sm-*` |
| EDGE   | attach `test_auto_attach`, `test_ctrl_snap_endpoint`, `test_choice_tip_attach`; points `l2 edge-points`, `test_point_edit`, `test_loop_polyline`; rebind `test_retarget_id`, `test_box_transition`; label `test_label_move`, `test_label_drag_tracks`, `label-geometry-*` |
| TEXT   | `test_action_edit`, `test_action_multiline`, `test_double_click_action`, `test_double_click_label`, `l2 text-edit`, `transition-notation`, the `text` metrics layer |
| TOOL   | `test_creation_tools`, `test_creation_tools_arm`, `test_new_{state,choice,comment,sm}_place`, paste `test_paste_state`, `test_paste_transition`, `l2 copy-paste`, drag `test_body_drag` |
| HIST   | `undo`/`redo-all` layer, `undo-all`, `test_gesture_recording`, `gestures-undo` |
| IO     | `save-*`, `l1-*` dumps, `reconstruct-*`, export `test_export_image`, `l3-*` |
| META   | `l2 update-meta`, `inspect-reject update-meta`, the `meta` diagram; META-1 (node hidden) via the `l1` scene dump |

Requirements with **no dedicated case yet** — the gaps to fill: `SEM-1` (one initial per
level — only the polygon pseudostate drill and the standing law cover it), `NODE-6` (sibling
no-overlap — the polygon standing law only), `EDGE-6` (the comment-subject link is not drawn
yet), `IO-4` (colour — no batch path yet), `TOOL-7` (pan/zoom not drivable in batch). The
first two are held by the polygon; the last three await the editor additions.

## In-process tests (L4)

The editor sources are built into the `CyberiadaEditorCore` static
library; the executable adds only `main.cpp`. The resources are a part of the
library as well, so the tests get the bundled font and the icons. The L4
tests (`tests/l4/`) link the library and drive the model and the scene
directly under QtTest, offscreen.

`l4-model` keeps a `QAbstractItemModelTester` attached to the model for the
whole run - every reset and mutation is checked against the
QAbstractItemModel contract - and verifies the editing API: the signals
each mutation emits, the id fixup of transition endpoints, the comment
subject cleanup and the transition cascade on element deletion. A comment
subject may address a transition as well; a new element is inserted before
the transitions of its collection, where the library keeps it. The inspected
model refuses every mutation and strips the editable, draggable and droppable
item flags, so an editing handler missing the mode check still cannot damage
the document. The states of one level are told apart by name: the model
refuses an empty or a taken state name on every rename path (the property
view, the tree, the canvas title, the batch `rename`), while the vertices
keep their empty names.

`l4-text` checks the per-role fonts: the bundled font is found through the
resources of the core library, every role carries its own point size, the
header is bold and the formal comment keeps the bundled family, a size change
reaches the items of that role alone, and the header is re-wrapped afterwards.
The sizes are compared with each other, never with an absolute value.

`l4-scene` checks the scene built from a loaded document: the item map
against the diagram structure, item positions, selection via
`slotElementSelected`, and the live sync — the scene follows the model
through its signals (dataChanged re-syncs an item, rowsInserted builds the
items of new elements, rowsAboutToBeRemoved tears them down before the
model frees the elements), so creation, deletion and reparenting are
driven through the connected scene, including the choice items and the
items of the comments without geometry. A reparent keeps the element's
absolute position in the document coordinates: the model re-expresses the
geometry relative to the new parent inside the same mutation (the scene
adds its own per-level region offset when rendering nested states). The batch mode runs the edit scripts
with the connected scene as well, so every L2 case exercises the sync.
The inspected region follows the document while the edited one is laid out
around the state title, so switching the mode re-derives it. `test_undo_redo`
checks the undo stack: every mutation is one step that brings the exact
document dump back (the file identity included), a refused mutation pushes
nothing, a bracketed gesture is one step, and the clean state follows the save.

`l4-window` drives the main window in process: the undo and redo actions
follow the stack (enabled state, text, effect), the tree follows a restored
document, the modified marker follows the clean state, and a clean document
closes without the unsaved changes prompt.

`l4-properties` checks the property view: an edited rect, point, endpoint or
polyline row writes the model and the scene item in the same call (the state
moves or resizes, its title is re-wrapped), the width and the height rows
refuse what the mouse resize refuses, the label point of a transition is
shown but disabled (the library cannot set it), the inspected view opens no
editor and drops a write, a model change refreshes the rows in place without
adding any, and the name row renames the element and the canvas title while a
name the model refuses reverts once the row is left.

`l4-dialog` checks the file dialogs. The open dialog: the file browser and the
option check boxes really share one window (the options are injected into the
dialog grid layout), the document is inspected by default, and the inspected
document is never given the geometry it does not have - the reconstruction box
is unchecked and disabled while the inspector box is checked. The save dialog:
every writable format is offered, the yEd formats are disabled with the reason
for a document they cannot express, they disable the geometry skipping, and the
skipped geometry disables the other options - the library allows no flag beside
it. The image export dialog derives the file suffix from the selected format.

`l4-log` checks the session logger (see *Session logging* below): a from-scratch
session records `new-sm` first and no start snapshot, and replays on a fresh
empty window to the identical document dump; a left-button gesture is recorded
as `press`/`drag`/`release` with the moves decimated; a model edit inside a
mouse gesture is suppressed (recorded once as the gesture) while an edit outside
one is logged; and a session left without its exit line - a crash - keeps the
`# start` marker with no `# exit`.

## Batch mode contract

`CyberiadaEditor --batch <file.graphml>` opens the document through the same
code path as the GUI (minus the dialogs) and exits. No dialog is ever shown in
batch mode; all diagnostics go to stderr.

Exit codes:

| Code | Meaning                                        |
|------|------------------------------------------------|
| 0    | success                                        |
| 1    | usage error (bad options, missing file arg)    |
| 2    | document load error (XML, format, semantics)   |
| 3    | internal error (assertion or exception thrown) |
| 4    | edit script error (bad command, unknown id)    |
| 5    | image comparison mismatch                      |

Assertion failures are reported with their `file:line` location (`MY_ASSERT`
throws it as the error message). The error flag is checked after every
stage - the script, the dumps, the export and the save - so an assertion
thrown while dumping or saving exits with code 3 as well.

`--no-text` hides all text elements (state titles and actions, transition
labels, comment bodies). Font metrics differ across Qt versions even for the
same font file, and the text sizes leak into the region layout, the transition
rectangles and the rendered pixels - so every test invocation runs with this
option, making the dumps and images identical on any machine. The option is
runtime-only: the GUI and normal exports always render text.

`--text` shows the text elements again, overriding an earlier `--no-text`. The
text metrics cases pass both, so they inherit the rest of the hermetic test
invocation and only turn the text back on.

`--dump-text` writes a `== text` section listing every text item: the owning
element, the role of the text, its font family, point size and boldness, its
position inside the element and its size, rounded to the pixel. It is
independent of `--dump`, so a text case compares that section alone.

`--dump-stack` writes a `== stack` section with the undo stack: the step
`count`, the current `index` and the `clean` state. It is independent of
`--dump` as well; the stack cases (`stack-<name>`) compare that section alone
with `good/<name>-stack-output.txt`, after the named script or with none.

`--strict` loads the document with the library's strict standard checks: the
graph, identifier, marker, name and vertex order requirements are checked in
addition to the format, so a document the default mode accepts may fail with
exit code 2. The GUI open dialog exposes the same mode as the strict check
checkbox.

`--reconstruct` loads the document with the library's geometry reconstruction:
absent geometry is rebuilt, and node geometry violating the standard (e.g. a
comment with point instead of rect geometry) is dropped and rebuilt as well.
Without the option such documents fail with a format error (exit code 2) -
the strict default. The GUI open dialog exposes the same mode as the
reconstruction checkbox. `--reconstruct-sm` creates the absent state machine
border as well; the library honours it only together with `--reconstruct`,
and the dialog enables its checkbox only under the reconstruction one.

The reconstruction cases (`reconstruct-<diagram>`, `reconstruct-sm-<diagram>`)
load a diagram this way and compare both the scene dump and the saved
document with the good files, so a change in the library's layout - the
shelf placement of the states, the grow-only padding of the authored parents,
the border attachment of the transitions, the repair of malformed node
geometry - is seen in the editor. A complete document is covered too
(`geometry`): the library grows an authored composite state whose child
touches its border, and the good file records that.

`--inspect` opens the document read-only, as the GUI does through the open
dialog: every model mutation is refused, so an edit script fails with exit
code 4, and the region rectangles are taken from `dRegion` instead of the
layout around the state title. The dumps do not differ: with `--no-text` the
text heights are zero, so both region layouts coincide, which is why the mode
is covered by the renders and not by dumps.

`--service` draws the service objects: the region borders and the coordinate
origins of the scene, the states and the regions. They are a decoration and
say nothing about the mode - the region geometry follows `--inspect` alone -
so the two options are independent and the tests use both. The option is
runtime-only, like `--no-text`: the GUI toggle is a stored preference, and a
batch run must not change it.

`--save-format <format>` chooses the format of the saved document:
`cyberiada` (the default), `yed-ostranna` or `yed-berloga`. The yEd formats keep
a single state machine with the geometry and have no shape for the terminate
pseudostate, so a document they cannot express fails with exit code 3; the GUI
save dialog disables such a format and shows the reason instead.

## Edit scripts

`--batch <file.graphml> --script <file> [--dump] [--save <out.graphml>]` runs
the edit commands against the loaded document, then dumps and/or saves it.
The order is fixed: edits, dump, save — saving rewrites the in-memory file
path, so a dump after a save would embed a build-directory path.

One command per line; `#` starts a comment; tokens are whitespace-separated
and the trailing text field (name, body, action text) takes the rest of the
line; inside it the escape `\n` embeds a newline and `\\` a backslash, so
multi-line behaviours and comment bodies stay expressible. The commands map
1:1 to the model mutators:

| command | effect |
|---|---|
| `new-sm [x y w h] <name>` | create a state machine (rect optional) |
| `new-state <parent> [x y w h] <name>` | create a state (rect optional) |
| `new-initial <parent> [x y]` | create an initial pseudostate |
| `new-final <parent> [x y]` | create a final state |
| `new-comment <parent> <body>` | create a comment |
| `new-formal-comment <parent> <body>` | create a formal (machine-readable) comment |
| `new-choice <parent> [x y w h]` | create a choice pseudostate (rect optional) |
| `new-terminate <parent> [x y]` | create a terminate pseudostate |
| `new-transition <sm> <src> <tgt> [action]` | create an external transition; the trailing text is a bare trigger or the full action notation |
| `rename <id> <title>` | change the element title |
| `move <id> x y [w h]` | update point (2 args) or rect (4 args) geometry |
| `reparent <id> <new-parent-id>` | move the element to another parent |
| `delete <id>` | delete the element with its children |
| `new-action <id> <action-text>` | add a state action, or set the action of a transition that has none |
| `update-action <id> <i> <action-text>` | update state action `<i>`, or the transition action (`<i>` = 0) |
| `delete-action <id> <i>` | erase state action `<i>`, or clear the transition action |
| `update-comment <id> <body>` | replace the comment body |
| `update-meta <parameter> <value>` | set a document metainformation parameter |
| `update-id <id> <new-id>` | change the element id |
| `polyline <id> [x y ...]` | replace the transition polyline (no points clear it) |
| `label <id> [x y]` | pin the transition label point (no coordinates reset it to auto-placement) |
| `new-subject <comment> <target> [name\|data <fragment>]` | link the comment to an element |
| `delete-subject <comment> <i>` | remove subject `<i>` (0-based) |
| `undo` / `redo` | undo or redo one step (every other command is one step) |

The mouse gestures go through the same script. A gesture is delivered to the
scene as a view would send it - the left button at scene coordinates - so it
takes the path of the GUI: the item handlers, the tools, the border zones.

| command | effect |
|---|---|
| `press x y [ctrl\|shift\|alt ...]` | press the left button at the scene point, with the modifiers |
| `drag x y` | move the pointer with the button pressed |
| `release x y` | release the button |
| `click x y [mods]` | press and release at one point |
| `double-click x y [mods]` | double click at the point |
| `tool <name>` | select the scene tool: `select`, `pan`, `zoom`, `transition`, or a creation tool (`new-sm`, `new-state`, `new-initial`, `new-final`, `new-choice`, `new-terminate`, `new-comment`, `new-formal-comment`). The rect tools (`new-sm`, `new-state`) create from a drawn `press`/`drag`/`release` (a plain click uses a default size); the placement tools create on a click; every creation tool reverts to `select` afterwards |
| `delete-selected` | delete the selected element through the window action |
| `copy` / `cut` | copy or cut the selected element to the clipboard (a state copies its subtree; a state machine is not copyable) |
| `paste` | paste the clipboard element as a sibling of the original, with a fresh id, a unique name and a +20 geometry offset |
| `type <text>` | type the text into the text being edited, key by key (`\n` is Return) |
| `key <name> [mods]` | one key: return, escape, tab, backspace, delete, left, right, up, down, home, end, space, or a character |
| `select-all` | select the whole editable text (Ctrl+A) |
| `commit` | end the edit: the focus-out writes the text to the model |
| `edit <id> <role> [<i>]` | open the inline editor of a canvas text (role `title`, `action <i>`, `label`, `body`) and leave it open for the keystroke verbs |
| `edit-text <id> <role> [<i>] <text>` | open the editor, replace the editable text and commit, in one line (the `entry / ` prefix of an action and a transition label are re-parsed) |

A transition polyline is edited by gesture: click the transition to select it and
show its point dots, then drag a segment to add a point, drag a point dot to move
it, click a point dot and press `key delete` to remove it (the delete key on a
focused point dot mirrors the context menu), or drag an endpoint dot to move or
reattach it. The `key` verb reaches whatever holds the scene focus, a text editor
or a point dot.

A gesture is one undo step opened by the press and closed by the release, as
in the GUI, so `press`, `drag`, `release`, `click`, `double-click` and `tool`
open no script-level step; `delete-selected` is one step like the model
commands. The text verbs edit the texts on the canvas the way a user does: a
double click on a text under the select tool opens its inline editor, `type`
and `key` reach it through the scene as a keyboard would, `commit` ends the
edit and the owner writes the text to the model (a title renames, an action
updates its behaviour, a label re-parses its action, a comment its body). They
need `--text`: with the text hidden the items are not on the canvas and
`edit-text` fails with a script error. A refused title (empty or taken) is
reported on stderr in batch mode and the old title restored, where the GUI
shows a warning. The first gesture activates the scene. A `drag` or `release`
without a press, a `press` while the button is pressed and a script ending
with the button pressed are script errors (exit code 4); `delete-selected`
with nothing selected is one as well. The gesture case (`l2-gestures`) drags
a state by its body, resizes another from its border, draws a loop under
the transition tool (the first drag starts the loop on the pressed state,
the following drags move its target end) and deletes the moved state after
a click; its
undo case (`undo-gestures-undo`) undoes the four steps and must reproduce the
untouched dump.

The action text uses the CyberiadaML notation: `entry/ behaviour`,
`exit/ behaviour` or `TRIGGER [guard]/ behaviour`; on a transition the trigger
may be empty (`/ behaviour`, the initial or completion transition). State
actions are addressed
by their 0-based position in the state's action list; a transition holds a
single action addressed as index 0 - `new-action` only sets it while the
transition has none, `delete-action` clears it. Guards are not allowed for
entry/exit activities and a state reaction requires a trigger; the
violations are reported with specific messages before the model is touched.

A comment subject links a comment (formal or informal) to an element: the
bare `new-subject` form makes an element-type subject; `name`/`data` with a
fragment reference a part of the target's title or body. Any element except
the document and the state machines can be the target. Subjects are
addressed by their 0-based position in the comment's subject list. Deleting
an element also removes every subject referencing it or its children, so
the document never keeps dangling subject links.

`update-meta` addresses the document, not an element. The optional standard
parameters accept only their standard values (`transitionOrder`:
`actionFirst` / `exitFirst`, the legacy `transitionFirst` is accepted as
well; `eventPropagation`: `propagate` / `block`) plus `none`, which removes
the parameter from the document; any other parameter is a free-form string,
replaced in place or appended. The change is written
both to the decoded metainformation and to the serialized `nMeta` comment.
`update-id` also rewrites the transition source/target references to the
renamed element, so the saved document stays consistent. A choice pseudostate
is moved by its rect, so `move` requires the `<x y w h>` form for it.

`new-sm` addresses the document too: it creates a state machine, so a session
started from scratch (where the GUI auto-creates the machine on the first
element) is replayable. `label` pins the auto-placed transition label to a
point; without coordinates it clears the pin and the label returns to its
automatic position.

Element ids of created elements are generated by the library and are
deterministic (`n0`, `n1`, nested `parent::nK`, transitions `src-tgt`), so
scripted results are reproducible. Errors are reported to stderr with the
script line number and the run exits with code 4. The script runs with the
scene connected to the model, so the dump shows the scene the live
synchronization built.

Saving uses the CyberiadaML-1.0 format with rounded geometry, keeping the
written floats stable for the good files, and declares the geometry the editor
writes in the metainformation (`geometry/ full`, or `none` when the geometry
is skipped) - the standard resolves the geometry mode from that parameter. The test runner also re-opens every
saved document, so each L2 case doubles as a write-read round-trip check.

A document file is required, except with `--script`: a `--batch --script <file>`
with no document runs against an empty in-memory document, so a recorded
from-scratch session (whose first verb is `new-sm`) replays as a test.

## Session logging

The editor can record a live session as an edit script in this very language,
so a real bug - a segfault above all - becomes a replayable test. `View ->
Запись сессии` toggles it (off by default, persisted like the grid option);
`--batch` forces it off so a replayed test is never re-logged. Enabling it opens
a session folder (`CYBERIADA_SESSION_LOG_DIR`, else the per-user application
data location) holding:

- `start.graphml` - the document as it stood when logging began. A from-scratch
  launch has no document, so no snapshot is written and the session replays with
  no start document.
- `session.script` - a header (`# <app> <version> rev <sha>`), the document line
  (`# document start.graphml` or `# document (empty)`), a `# start <ISO>` line,
  the recorded verbs, and a `# exit <ISO>` line on a clean quit. A crash never
  writes the exit line, so a `# start` with no `# exit` is the crash marker.

The recording is hybrid. Raw mouse input is logged as the gesture verbs
(`press`/`drag`/`release`/`double-click`, the pointer moves decimated; `tool`
from the toolbar); menu, dialog and property edits are logged as the semantic
model verbs (`new-sm`, `new-state`, `move`, `rename`, `reparent`, `delete`,
`label`, `new-action`, `undo`/`redo`, ...). A model edit driven by a mouse
gesture is suppressed while the gesture is in flight and recorded once as the
gesture, so a replayed session never applies it twice. To replay a folder:
`CyberiadaEditor --batch --script <dir>/session.script <dir>/start.graphml`
(drop the file for a from-scratch session).

## Rendering and image comparison

`--batch <file.graphml> [--script <file>] --export <out.png>` renders the
scene offscreen into an image file: 1:1 scene units to pixels, white
background, the selection cleared first. The picture covers the visible
diagram with the scene margin around it, whatever the scene rect of the
loaded document was, so the elements an edit script added are in it. An exported image is the diagram
alone: the grid and the service objects (the region borders and the
coordinate origins) belong to editing and are forced off for the render, so
`--service` and the stored grid preference make no difference to a saved
image. There is no cosmetic scene frame: the editor draws no boundary
rectangle and the grid fills the whole edit area; a diagram boundary is the
optional state machine border (the standard `dGeometry` rect of the `<graph>`),
an ordinary element the user adds through the New State Machine action and
resizes or removes like any other.

`--compare <a.png> <b.png> [--epsilon <0-255>] [--max-diff <fraction>]`
compares two images and exits: a pixel differs when any channel delta exceeds
`--epsilon` (default 8); the images match when the differing pixel fraction
is not above `--max-diff` (default 0 - strict, since pinned-font renders are
reproducible; loosen per invocation when comparing across Qt versions). The
statistics are printed to stderr; exit code 0 on match, 5 on mismatch.

The tests render with `--no-text` (see the batch mode contract), so the
reference images are text-free and identical across machines and Qt versions.
The application still pins the bundled Cyberiada Mono (`fonts/*.ttf`, a
renamed DejaVu Sans Mono - see `fonts/LICENSE`) as the default font at
startup for the GUI and manual exports (the font dialog overrides it
interactively).

## Text metrics

The text of an element is drawn in the font of its role - the state header, the
state body, the transition label and the comment have separate point sizes, set
in the Text tab of the preferences. The header is bold and the formal comment
keeps the bundled monospace family whatever the shared family is; both are fixed
properties of the role, not settings.

The glyphs are never compared. Their rasterization follows the freetype build,
the hinting and the antialiasing of the machine, so a reference image with text
would only match where it was made. The metrics are another matter: with the
bundled font pinned at startup, the point size converted to pixels inside the
font manager (a point is 1/72 inch at the fixed 96 dpi of the diagram plane -
the screen dpi and the scaling environment of the machine play no part) and
the hinting turned off (`QFont::PreferNoHinting`), the advance and the line
height come from the font file alone. The same conversion makes the exported
SVG carry the true pixel size of the drawn text. The bundled family
name is unique on purpose: a font requested by an ambiguous name (the former
`Courier`) is silently shadowed by a same-named system family with more real
faces, and the metrics then follow the machine again. A text case therefore
compares the `--dump-text` section, and the `l4-text` in-process test compares
the relations - a larger header is taller, the other roles do not move, the
region follows - rather than absolute values.

## Dump format

`--batch <file.graphml> --dump` prints the canonical dump on stdout in two
sections:

* `== document` — the loaded `Cyberiada::LocalDocument` streamed through the
  libcyberiadamlpp `operator<<`, verbatim: same format and determinism
  guarantees as that library's own test outputs;
* `== scene` — the scene items walked in document order, one line per item,
  indented by nesting depth:
  `Simple State: {id: 'n0::n1', pos: (x; y), rect: (x; y; w; h)}`.
  The element type names come from the model (the Qt item type collapses
  composite/simple states and the vertex kinds); coordinates are printed with
  a fixed 2-decimal format; elements without a scene item are skipped, so the
  dump records what the scene actually builds. Every element is drawn except
  a formal comment without geometry (the document metainformation node): a
  choice gets a diamond and an informal comment without geometry gets a
  default sized item, both centred on the parent origin. The link between a
  comment and its subject is not drawn yet, so a comment attached to a
  transition looks like any other comment in the scene dump. A transition
  follows its polyline point by point, a self-transition included; only a
  loop without a polyline is drawn as an arc between its endpoints, which
  is why its rect is not integral. An endpoint without a stored point is
  attached to the node border toward the other end (a vertex at its
  centre), as the library's reconstruction would place it, so the rect of
  such a transition is border-based while the document keeps no point.
  The children of a state machine without a rect carry global coordinates
  (7.2.1): the state machine item sits at the origin and its rect is the
  union of its content, in the scene as in the saved file.

The L1 tests run the editor with `tests/` as the working directory and a
relative input path, so the `file:` field of the document dump stays
machine-independent. All batch output is locale-independent: the dump uses
locale-agnostic number formatting and the application forces `LC_NUMERIC` to
`C` (the graphml writer would otherwise follow the user's locale and print
decimal commas). The dumps are produced with `--no-text`, so no text metric
reaches the good files and they are valid across machines and Qt versions. The
text cases are the exception and add `--text`: they compare the metrics, which
the pinned font, the pinned dpi and the disabled hinting keep machine-
independent (see the text metrics section).

## Test suite layout

```
tests/
  CMakeLists.txt        the ctest cases (L0 smoke + L1 dump per diagram)
  cmake/RunBatchTest.cmake   runs the batch mode, checks the exit code and
                             compares the dump with the good file
  diagrams/*.graphml    input documents (see below)
  scripts/<case>.script      edit scripts for the L2 cases
  good/<name>-output.txt     reviewed good files for the L1/L2 dumps
  good/<name>-text-output.txt  reviewed good files for the text metrics
  good/<name>-stack-output.txt reviewed good files for the undo stack dumps
  good/<case>-output.graphml reviewed good files for the L2 saved documents
  good/<name>-reconstruct[-sm]-output.txt      reviewed good files for the reconstruction dumps
  good/<name>-reconstruct[-sm]-output.graphml  reviewed good files for the reconstructed documents
  good/<name>-render.png     reviewed good images for the L3 renders
  good/<name>-render.svg     reviewed good files for the vector renders
  good/<name>-inspect-render.png  reviewed good images for the inspect renders
  good/<name>-<format>-output.graphml  reviewed good files for the saved formats
  regen-good.sh         regenerates the good files and shows the diff
run-tests.sh            build-and-run wrapper: ctest --output-on-failure
```

Diagram conventions:

* positive diagrams are valid CyberiadaML-1.0 documents named by their purpose
  (`hierarchy.graphml`, `two-sms.graphml`, ...); most originate from the
  libcyberiadamlpp and hsm-console-viewer test corpora;
* negative diagrams are named `broken-<reason>.graphml` and must fail with
  exit code 2;
* the L2 case name and the diagram name must differ: both tiers write
  `good/<name>-output.txt`, so equal names would share one good file;
* good files follow the sibling-library convention:
  `good/<name>-output.txt` (canonical dump) and, for the L2 cases,
  `good/<case>-output.graphml` (saved document);
* a case may name the `EDITOR-SPEC` requirement it exercises (`EDIT-<AREA>-<n>`),
  so a failure names both the layer and the requirement it broke.

The scene is exported to the vector formats as well: `--export <file>.svg`
writes it through `QSvgGenerator` and `--export <file>.pdf` through
`QPdfWriter`, both by the same `scene->render()` call as the raster formats, so
the picture is identical. The `viewBox` of the svg and the `MediaBox` of the
pdf repeat the scene rect one unit per point. The svg cases
(`l3-svg-<diagram>`) compare the result **byte by byte** with
`good/<diagram>-render.svg`: the generator embeds no timestamp, and the export
sets the bundled font on the painter, without which the generator would stamp
the machine's default font family into every group element - even with no text
at all - and the good file would only match on the machine that made it. The
pdf case (`l3-pdf-<diagram>`) only checks
the shape of the file, because a pdf carries its creation date. Both are
produced under `--no-text`, so the good svg files contain no `<text>` element
at all: with the text shown an svg names the bundled font and renders
font-dependently elsewhere, unlike the raster exports.

The save format cases write the same diagram in every writable format:
`save-<format>-<diagram>` compares the result with
`good/<diagram>-<format>-output.graphml` and re-opens it, and
`save-reject-<format>-<diagram>` requires exit code 3 for a document the format
cannot express.

The inspection cases run the same diagrams with `--inspect`:
`inspect-render-<diagram>` compares the read-only render with
`good/<diagram>-inspect-render.png` (the service objects are off in an export,
so the image shows the inspected layout alone), and `inspect-reject-<case>` runs an
existing L2 script and requires exit code 4 - the refused mutation is reported
as a script error.

Good files are reference data: they are never regenerated from the
implementation just to make a failing test pass; any change to a good file is a
deliberate, reviewed part of a change.

## Running

```
mkdir build && cd build && cmake .. && make
cd .. && ./run-tests.sh          # or: cd build && ctest --output-on-failure
```

At every run ctest first prints the commit under test (hash, branch, subject
and a local-changes marker, via `tests/cmake/PrintRevision.cmake`), so a saved
test log identifies the exact version it exercised.

Each test carries its full environment, baked in at configure time: the
offscreen platform, a hermetic `XDG_CONFIG_HOME` inside the build directory,
a private fontconfig (`FONTCONFIG_FILE` generated from `tests/fonts.conf.in`)
that exposes only the bundled `fonts/` directory, so no system font ever
enters a match, and the library/plugin paths of the Qt actually found by
CMake. Plain `ctest`
therefore works even when Qt lives outside the system paths (RUNPATH alone is
not enough there: it does not cover the transitive Qt dependencies nor the
dlopen'ed platform plugin). With a relocated Qt the *build* still needs
`LD_LIBRARY_PATH` pointing at its libraries, since the moc/uic code
generators run during compilation.
