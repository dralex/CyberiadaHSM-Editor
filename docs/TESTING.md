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
| L2    | editing: scripted mutations, then dump/save vs good files         | planned     |
| L3    | render: offscreen image export vs good images with tolerance | planned     |
| L4    | in-process interaction tests (requires a library split)        | if needed   |

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

Assertion failures are reported with their `file:line` location (`MY_ASSERT`
throws it as the error message).

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
  dump records what the scene actually builds.

The L1 tests run the editor with `tests/` as the working directory and a
relative input path, so the `file:` field of the document dump stays
machine-independent. Some item rectangles derive from text metrics; if a
different font environment shifts them, regenerate the good files locally
with `tests/regen-good.sh` and review the diff.

## Test suite layout

```
tests/
  CMakeLists.txt        the ctest cases (L0 smoke + L1 dump per diagram)
  cmake/RunBatchTest.cmake   runs the batch mode, checks the exit code and
                             compares the dump with the good file
  diagrams/*.graphml    input documents (see below)
  good/<name>-output.txt     reviewed good files for the L1 dumps
  regen-good.sh         regenerates the good files and shows the diff
run-tests.sh            build-and-run wrapper: ctest --output-on-failure
```

Diagram conventions:

* positive diagrams are valid CyberiadaML-1.0 documents named by their purpose
  (`hierarchy.graphml`, `two-sms.graphml`, ...); most originate from the
  libcyberiadamlpp and hsm-console-viewer test corpora;
* negative diagrams are named `broken-<reason>.graphml` and must fail with
  exit code 2;
* good files follow the sibling-library convention:
  `good/<name>-output.txt` (canonical dump); L2 will add
  `good/<name>-output.graphml` (saved document).

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
