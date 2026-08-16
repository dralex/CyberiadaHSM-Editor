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
| L1    | load/dump: canonical model + scene dumps vs good text        | planned     |
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

## Test suite layout

```
tests/
  CMakeLists.txt        one ctest case per diagram
  cmake/RunBatchTest.cmake   runs the batch mode, checks the exit code
  diagrams/*.graphml    input documents (see below)
run-tests.sh            build-and-run wrapper: ctest --output-on-failure
```

Diagram conventions:

* positive diagrams are valid CyberiadaML-1.0 documents named by their purpose
  (`hierarchy.graphml`, `two-sms.graphml`, ...); most originate from the
  libcyberiadamlpp and hsm-console-viewer test corpora;
* negative diagrams are named `broken-<reason>.graphml` and must fail with
  exit code 2;
* future good files follow the sibling-library convention:
  `<name>-output.txt` (canonical dump), `<name>-output.graphml` (saved
  document).

Good files are reference data: they are never regenerated from the
implementation just to make a failing test pass; any change to a good is a
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
