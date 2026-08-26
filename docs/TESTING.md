# The Testing System

The editor is tested as a black box: the full application — window, scene and
model wired exactly as shipped — runs under the Qt offscreen platform and is
driven through the batch mode instead of the GUI event loop. Test verdicts come
from exit codes, canonical stdout dumps and produced files compared against
good references.

## Architecture

```
  run-tests.sh  /  ctest
    |   per test: diagram .graphml [+ command script] + good files
    v
  CyberiadaInspector --batch <file>   (QT_QPA_PLATFORM=offscreen)
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

## Test layers

| Layer | What is checked                                                | Status      |
|-------|----------------------------------------------------------------|-------------|
| L0    | smoke: every diagram opens offscreen, the process exits clean  | implemented |
| L1    | load/dump: canonical model + scene dumps vs good text        | implemented |
| L2    | editing: scripted mutations, then dump/save vs good files         | implemented |
| L3    | render: offscreen image export vs good images with tolerance | implemented |
| L4    | in-process: model contract and scene structure (QtTest)        | implemented |

## In-process tests (L4)

The editor sources are built into the `CyberiadaInspectorCore` static
library; the executable adds only `main.cpp` and the resources. The L4
tests (`tests/l4/`) link the library and drive the model and the scene
directly under QtTest, offscreen.

`l4-model` keeps a `QAbstractItemModelTester` attached to the model for the
whole run - every reset and mutation is checked against the
QAbstractItemModel contract - and verifies the editing API: the signals
each mutation emits, the id fixup of transition endpoints, the comment
subject cleanup and the transition cascade on element deletion. A comment
subject may address a transition as well; a new element is inserted before
the transitions of its collection, where the library keeps it.

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
There is no undo stack in the editor yet, so undo/redo is not an L4
subject.

## Batch mode contract

`CyberiadaInspector --batch <file.graphml>` opens the document through the same
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
throws it as the error message).

`--no-text` hides all text elements (state titles and actions, transition
labels, comment bodies). Font metrics differ across Qt versions even for the
same font file, and the text sizes leak into the region layout, the transition
rectangles and the rendered pixels - so every test invocation runs with this
option, making the dumps and images identical on any machine. The option is
runtime-only: the GUI and normal exports always render text.

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
reconstruction checkbox.

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
| `new-state <parent> [x y w h] <name>` | create a state (rect optional) |
| `new-initial <parent> [x y]` | create an initial pseudostate |
| `new-final <parent> [x y]` | create a final state |
| `new-comment <parent> <body>` | create a comment |
| `new-formal-comment <parent> <body>` | create a formal (machine-readable) comment |
| `new-choice <parent> [x y w h]` | create a choice pseudostate (rect optional) |
| `new-terminate <parent> [x y]` | create a terminate pseudostate |
| `new-transition <sm> <src> <tgt> [trigger]` | create an external transition |
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
| `new-subject <comment> <target> [name\|data <fragment>]` | link the comment to an element |
| `delete-subject <comment> <i>` | remove subject `<i>` (0-based) |

The action text uses the CyberiadaML notation: `entry/ behaviour`,
`exit/ behaviour` or `TRIGGER [guard]/ behaviour`. State actions are addressed
by their 0-based position in the state's action list; a transition holds a
single action addressed as index 0 - `new-action` only sets it while the
transition has none, `delete-action` clears it. Guards are not allowed for
entry/exit activities and a transition-type action requires a trigger; the
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

Element ids of created elements are generated by the library and are
deterministic (`n0`, `n1`, nested `parent::nK`, transitions `src-tgt`), so
scripted results are reproducible. Errors are reported to stderr with the
script line number and the run exits with code 4. The script runs with the
scene connected to the model, so the dump shows the scene the live
synchronization built.

Saving uses the CyberiadaML-1.0 format with rounded geometry, keeping the
written floats stable for the good files. The test runner also re-opens every
saved document, so each L2 case doubles as a write-read round-trip check.

## Rendering and image comparison

`--batch <file.graphml> [--script <file>] --export <out.png>` renders the
scene offscreen into an image file: 1:1 scene units to pixels, white
background, the selection cleared first (exports never show the editing
selection). The grid and the scene frame are part of the picture.

`--compare <a.png> <b.png> [--epsilon <0-255>] [--max-diff <fraction>]`
compares two images and exits: a pixel differs when any channel delta exceeds
`--epsilon` (default 8); the images match when the differing pixel fraction
is not above `--max-diff` (default 0 - strict, since pinned-font renders are
reproducible; loosen per invocation when comparing across Qt versions). The
statistics are printed to stderr; exit code 0 on match, 5 on mismatch.

The tests render with `--no-text` (see the batch mode contract), so the
reference images are text-free and identical across machines and Qt versions.
The application still pins the bundled `fonts/courier.ttf` as the default
font at startup for the GUI and manual exports (the font dialog overrides it
interactively).

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
  transition looks like any other comment in the scene dump.

The L1 tests run the editor with `tests/` as the working directory and a
relative input path, so the `file:` field of the document dump stays
machine-independent. All batch output is locale-independent: the dump uses
locale-agnostic number formatting and the application forces `LC_NUMERIC` to
`C` (the graphml writer would otherwise follow the user's locale and print
decimal commas). The dumps are produced with `--no-text`, so no text metric
reaches the good files and they are valid across machines and Qt versions.

## Test suite layout

```
tests/
  CMakeLists.txt        the ctest cases (L0 smoke + L1 dump per diagram)
  cmake/RunBatchTest.cmake   runs the batch mode, checks the exit code and
                             compares the dump with the good file
  diagrams/*.graphml    input documents (see below)
  scripts/<case>.script      edit scripts for the L2 cases
  good/<name>-output.txt     reviewed good files for the L1/L2 dumps
  good/<case>-output.graphml reviewed good files for the L2 saved documents
  good/<name>-render.png     reviewed good images for the L3 renders
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
  `good/<case>-output.graphml` (saved document).

Good files are reference data: they are never regenerated from the
implementation just to make a failing test pass; any change to a good file is a
deliberate, reviewed part of a change.

## Running

```
mkdir build && cd build && cmake .. && make
cd .. && ./run-tests.sh          # or: cd build && ctest --output-on-failure
```

Each test carries its full environment, baked in at configure time: the
offscreen platform, a hermetic `XDG_CONFIG_HOME` inside the build directory,
and the library/plugin paths of the Qt actually found by CMake. Plain `ctest`
therefore works even when Qt lives outside the system paths (RUNPATH alone is
not enough there: it does not cover the transitive Qt dependencies nor the
dlopen'ed platform plugin). With a relocated Qt the *build* still needs
`LD_LIBRARY_PATH` pointing at its libraries, since the moc/uic code
generators run during compilation.
