# The Plugin System — Architecture

**Document version:** 0.11 (2026-11-11)

A plugin adds a feature to the editor without rebuilding it: a diagram checker, a code
generator, a compiler front end, an LLM diagram maker. A plugin is a directory with a
`plugin.json` descriptor and the program it runs. The tools ship their plugins in their
own packages; the editor ships none. The editor never loads plugin code: it runs the
program in a separate process, hands it a copy of the active document and takes back what
the action declares — a document, diagnostics, files, a log or the base of a diff. The
editor gains `QProcess` and the Qt JSON classes only: no interpreter, no network library, no
plugin ABI.

This document is the editor side. The contract a plugin author follows — the descriptor,
the variables, the files and the formats — is `PLUGIN-API.md`; the requirements are
`EDIT-PLUG-1..49` in `EDITOR-SPEC.md` §4.12, the tests the plugin layer of `TESTING.md`.

```
 smeditor_window.cpp, batch_driver.cpp
+----------------------------------------------------------------------------------------+
| Plugins menu, plugin toolbars, --run-plugin   plugin dock: Diagnostics, plugin outputs |
+----------------------------------------------------------------------------------------+
       |                                                     ^
       | action, parameter values                            | findings, log, progress
       v                                                     |
+----------------------------------------------------------------------------------------+
| PluginRunner        stage the run dir > start > wait > validate > apply the result     |
+----------------------------------------------------------------------------------------+
       |          ^                              |                         ^
       | result   | snapshot                     | argv, env               | exit code,
       v          |                              v                         | stdout/stderr
+-----------------------------+          +-----------------------------------------------+
| CyberiadaSMModel            |          | plugin process                                |
| the active document         |          | a script run by its interpreter, or any tool  |
+-----------------------------+          +-----------------------------------------------+
                                                 |                         ^
                                                 | write                   | read
                                                 v                         |
                                         +-----------------------------------------------+
                                         | run dir   in:  input.graphml, request.json    |
                                         |           out: output.graphml,                |
                                         |                diagnostics.json, files        |
                                         +-----------------------------------------------+
```

## Levels

| Level | Process | Exchange | For |
|---|---|---|---|
| 1 (this design) | one process per run | argv, environment and files in; exit code and files out | checkers, generators, the compiler adapter, one-prompt LLM drawing |
| 2 | a session for the whole interaction | JSON-RPC 2.0 over stdin and stdout | a diagram run step by step, a replay, an LLM agent asking the user, a bridge to a running machine |

Both levels use the same descriptor; level 2 adds the `session` block (see Level 2:
sessions).

## Discovery

```
 search path, first match wins                     PluginRegistry::scan()
+--------------------------------------+         +--------------------------------------+
| 1  --plugin-path DIR ...             |         | for every <dir>/<name>/plugin.json:  |
|    (replaces 2..4; tests, batch)     |  dirs   |   parse and check the fields         |
| 2  $CYBERIADA_PRO_PLUGIN_PATH        | ------> |   "api" major supported?             |
| 3  the user plugin folder            |         |   id seen earlier on the path? skip  |
| 4  the system plugin folder          |         |   interpreter found? else disabled   |
|                                      |         |   requires hold? else disabled       |
+--------------------------------------+         +--------------------------------------+
                                                     | rejected              | accepted
                    +--------------------------------+                       |
                    v                                                        v
+--------------------------------------+         +--------------------------------------+
| not listed: the Plugin Manager shows |         | registry: id -> descriptor, folder,  |
| the path and the reason; the editor  |         | enabled or disabled + the reason;    |
| starts normally                      |         | read by the menu, the toolbars, the  |
|                                      |         | plugin manager and --list-plugins    |
+--------------------------------------+         +--------------------------------------+
```

The user folder is `QStandardPaths::writableLocation(AppDataLocation)/plugins`; the
system folder is `share/cyberiada-pro/plugins` one level above the executable (a
Debian install) or next to it (the Windows zip). The exact paths are in `PLUGIN-API.md`.
A user plugin overrides a system plugin with the same id; the manager lists the shadowed
one. The scan runs at start and on Rescan in the plugin manager; it also reads the
plugin's `i18n/<lang>.json` of the interface language and translates the plugin texts
(`EDIT-UI-4`). A run gets the language as `${language}`, `CYBERIADA_PRO_LANGUAGE` and
`language` in `request.json` — `en` in the batch mode (`EDIT-PLUG-33`).

The scan also reads the `languages` of the plugins (`EDIT-PLUG-60`): a new language for
the highlighting and the text modes needs no rebuild of the editor.

```
 scan(), Rescan                          CodeStyle (code_highlighter.cpp)
+------------------------------+       +---------------------------------------+
| a plugin enabled in the      | files | 1 the built-ins, :/Syntax/syntax      |
| manager (its interpreter or  |------>| 2 the plugin languages, in scan order |
| requires may still keep its  |       |   a clash of an id, a name or an      |
| actions off): `languages`    |       |   alias: the earlier wins, the later  |
+------------------------------+       |   is a warning                        |
                                       +---------------------------------------+
                                         | styleChanged: the tabs re-highlighted,
                                         v the language menus rebuilt
     Document > Mode > Code, File > New > Text file, Document > Language, the Open filter
```

## The interface

```
+--------------------------------------------------------------------------------------+
| File  Edit  View  Plugins  Help                                                      |
+--------------------------------------------------------------------------------------+
| [New][Open][Save] ... [Log] | [Check][Strict] | [Compile] | [Draw]                   |
|  mainToolBar                  checker           lapki       HSM Maker                |
+------+-------------------------------------------------------+-----------------------+
| tool |                                                       | structure tree        |
| bar  |                       canvas                          |                       |
|      |                                                       | properties            |
+------+-------------------------------------------------------+-----------------------+
| Lapki compiler: Compile: running 00:07                                     [Cancel]  |
| [Diagnostics 2] [Diagram checker output x] [Lapki compiler output x]                 |
| warning  CGML-6.9-4-4   nMeta   transitionOrder 'transitionFirst' is not admissible  |
| info     CGML-6.4-4-2   G0      state machine 'G0' contains no initial pseudostate   |
+--------------------------------------------------------------------------------------+
```

```
 Plugins
+--------------------------+
| Diagram checker        > |---+-------------------+
| Lapki compiler         > |   | Check             |
| HSM Maker              > |   | Check (strict)    |
| Python program...        |   | ----------------- |
| ------------------------ |   | Settings...       |
| Output panel             |   +-------------------+
| Plugin manager...        |
+--------------------------+
```

* **Plugins menu** — a top-level menu before Help (Edit > Tools already holds the element
  tools). A submenu per plugin, a flat entry for a plugin with one action and no stored
  settings; every submenu ends with Settings... for the persistent parameters. It is built
  from the registry, like the `MetaChoiceMenu` menus.
* **Plugin toolbars** — the actions marked `toolbar` form one small toolbar per plugin in
  the top area, to the right of `mainToolBar`; a narrow window folds them into the `»`
  extension. View > Plugin toolbars shows or hides each one.
* **A click** — an action with `run` parameters, a `document` parameter not yet set for
  this document, or a required `persistent` parameter without a value opens its dialog,
  and its label ends with `...`; any other action runs at once with the stored values.
* **The parameter dialog** — generated from the action's parameters (`QDialog` +
  `QFormLayout`, one widget per type); the plugin draws no window, so the same action runs
  in batch mode. The mockups and the type table are in `PLUGIN-API.md`.
* **Plugin dock** — under the canvas, hidden until the first run: the running plugins with
  their time and Cancel, the Diagnostics tab (a click selects and centres the element) and
  an output tab per plugin, "<plugin> output": one block per run headed by the action and
  the document; a starting run makes its tab current. An output tab closes with its `x`,
  dropping the text, and comes back empty with the plugin's next output; its context
  menu (on the text or the tab) adds Save to File... (plain UTF-8 text,
  `<plugin id>-output.txt` in the document folder) and Clear... (after a confirmation) to
  Copy and Select All.
* **Plugin manager** — the plugins found: folder, version, enabled or the reason why not;
  enable/disable, Rescan.
* **Disabled actions** — a missing interpreter, a tab whose mode the action does not list
  (`modes`), a tab that is not editable (for `replace`), a run already going on the same
  document, a condition of the plugin or the action (see Conditions); the tooltip gives the
  reason.

## Conditions

A plugin declares when its actions can run (`EDIT-PLUG-50..55`, the fields in
`PLUGIN-API.md`): `requires` is the system, checked by the scan, `when` the document, checked
by every refresh of the actions. A failing condition greys the action out with the reason.

```
 scan(), Rescan, Preferences                   updatePluginActions(): undo, updateEditActions()
+--------------------------------------+      +---------------------------------------------+
| parse: requires, when, reason, modes |      | pluginDisabledReason(entry, action, doc)    |
| interpreter found?                   |entry |  1 the status, the manager switch           |
| requires: programs, env              |----->|  2 entry.unavailable[action]                |
| requires.plugins: passes until no    |      |  3 no document, a run, the mode, read-only  |
|   status changes                     |      |  4 PluginConditions::check(when, facts)     |
+--------------------------------------+      +---------------------------------------------+
  the plugin: disabled + the reason               ^ facts                        | reason
  an action: unavailable[action]                  |                              v
                                     +-----------------------------+   menu, toolbar: enabled,
                                     | doc->pluginFacts()          |   tooltip; plugin manager;
                                     |  live: file, saved,         |   --list-plugins;
                                     |        selection            |   --run-plugin: exit 6
                                     |  cached: machines, initial, |
                                     |        fa, findings, meta   |
                                     |  per path: git, the .git    |
                                     |        folder walk          |
                                     +-----------------------------+
                                       dropped on indexChanged, modelReset;
                                       git: on save, Save As, Rescan
```

* A plugin `requires` disables the plugin, as a missing interpreter does; an action
  `requires` fills `PluginEntry::unavailable`. The plugin dependencies come last, in passes
  until no status changes, so a chain of plugins is disabled in turn.
* `updatePluginActions()` runs on every move of the undo group (an edit, an undo, a redo)
  and from `updateEditActions()` (a save, a selection change, a tab switch), and when the
  title of the current document changes (a Save As of an unmodified document). The content
  facts are kept by the document until its undo stack moves or its model is reset;
  `fa-valid` costs one EXT-FA check per change of an automaton.
* `git` walks from the file's directory up to the root looking for a `.git` directory
  (`EDIT-PLUG-55`); the answer is kept for the path until a save, a Save As or a scan, so an
  edit or a selection costs no file system lookup.
* `meta:` sees the document as the action's `meta` fill leaves it (`EDIT-PLUG-34`).
* An unknown token or key keeps the action (the plugin) off and is a warning; `reason`
  replaces only the reason of a known `when` token.
* `--list-plugins` prints `unavailable: <action>: <reason>`; `--run-plugin` of an
  unavailable action fails with the reason.

## The run

```
   menu, toolbar, or --run-plugin
          |
          v
   +-------------+
   |  configure  | -- cancel ------------------------------------------> idle
   +-------------+
          | the values (batch: the defaults and --plugin-param)
          v
   +-------------+
   |    stage    | -- no snapshot, unresolved variable, no interpreter ---+
   +-------------+                                                        |
          | run dir written; a replace action locks the document          |
          v                                                               |
   +-------------+                                                        |
   |   running   | -- failed to start, timeout ---------------------------+
   |             | -- cancel: terminate, kill after a grace -> cancelled  |
   +-------------+                                                        |
          | finished; stdout and stderr streamed to its output tab        |
          v                                                               |
   +-------------+                                                        |
   |  validate   | -- exit code not in exit_ok, crash, output missing ----+
   |             |    or not decodable, diagnostics malformed             |
   +-------------+                                                        |
          | a valid result                                                |
          v                                                               |
   +-------------+                                                        |
   |    apply    | -- the document closed or read-only -------------------+
   +-------------+                                                        |
          | undo step, new tab, findings, files listed                    v
          v                                                        +-------------+
   +-------------+                                                 |   failed    |
   |    done     |                                                 +-------------+
   +-------------+
   done, failed, cancelled: unlock the document, remove the run dir
```

* The run dir is a fresh `QTemporaryDir`, removed after the run unless
  `CYBERIADA_PRO_PLUGIN_KEEP=1`. `input.graphml` is the model snapshot (Cyberiada 1.0,
  unsaved edits included) — never the file on disk; an empty document gives an empty file.
  A text tab writes its text as `input.<ext>` instead (`EDIT-PLUG-57`).
* An action's declared `meta` fills the keys the document lacks first, as one undo step; a
  present different value is kept and noted (`EDIT-PLUG-34`).
* An action with `save` (or `cwd: document`) saves a modified or untitled document first:
  an untitled one asks for a name, and a cancel there cancels the run; in batch mode an
  untitled document fails the run. `cwd: document` runs the plugin in the document's
  directory, for tools that read the files next to the diagram.
* One run per document; runs in different documents may go at the same time.
* A run belongs to the document it started from (a `QPointer`), not to the active tab.
* A `replace` run locks its document: no edits, no undo/redo; closing the tab asks, then
  cancels the run. Closing the editor cancels every run.
* Cancel and timeout terminate the process and kill it after a grace period. Only the
  direct child is killed: an adapter that starts a server stops it itself.
* The output text is capped per run; undecodable bytes are shown in the local 8-bit
  encoding.

## Applying the result

```
 output.graphml -- decode (load.strict, load.reconstruct) --> a temporary LocalDocument
        |                                                     |
        | not decodable, empty                                | valid
        v                                                     v
+--------------------------+        +--------------------------------------------------+
| failed: the document is  |        | replace:      beginUndoStep("Plugin: <label>")   |
| untouched, the output    |        |               restoreSnapshot(encoded)           |
| path is logged           |        |               endUndoStep()                      |
+--------------------------+        | new-document: a modified untitled document       |
                                    | diff:         a diff tab, the output its base,   |
                                    |               the input snapshot its target      |
                                    +--------------------------------------------------+
```

* A `diff` output may be empty: the base is then an empty document (a file not in the
  revision), not a failure. The editor computes the diff itself (`docs/MODES.md`, The diff
  tab); the tab keeps the run's plugin, action and values, so Refresh runs it again.

* A new document takes the active tab when that is an empty untitled one, as Open does
  (`EDIT-DOC-2`), and a new tab otherwise; its tab is titled by the document name the plugin
  wrote, `[Name]`, or `untitled` without one (`EDIT-DOC-10`).
* The output is decoded before anything is touched: `restoreSnapshot` only warns about a
  bad buffer. `load.reconstruct` lays out a document that comes without geometry (an LLM
  answer); `load.strict` rejects a non-conforming one.
* An output equal to the input pushes no undo step; the output tab says "no
  changes".
* The document keeps its file path and format; `readOnly()` is checked here, because
  `restoreSnapshot` does not.
* The session log writes the comment `# plugin <id>:<action>`: a plugin is not a script
  verb, so after a `replace` or a `new-document` result the log starts a new session from
  the new document and stays replayable.

## Settings

Everything goes through `SettingsManager`; nothing is written into the GraphML file.

| Key | Holds |
|---|---|
| `plugins/<id>/values/<param>` | a `persistent` parameter value |
| `plugins/<id>/documents/<sha1>/<param>` | a `document` parameter value; the key is the SHA-1 of the canonical file path |
| `plugins/<id>/enabled`, `plugins/<id>/toolbar` | the plugin manager switch, the toolbar visibility |
| `plugin-interpreters/<name>` | an interpreter path set in the Preferences (`python3` → `C:\Python312\python.exe`) |

A `document` value of an untitled document lives for the session only. A secret value is
stored as a reference (`env:NAME`, `file:PATH`); the key itself is read at the start of a
run and exported to the child process only — never to the argv, `request.json`, the
output tabs, the session log or the settings.

## Batch mode and tests

* The plugin stage runs after the script stage, before the dumps, the export and the save:
  open, script, **plugin**, tabs, dump, text, stack, export, save. A run without a
  document starts from an empty one.
* No dialog and no stored values: the defaults and `--plugin-param` only.
* The `plugin` verb runs an action in the middle of the script, a session action with
  the verbs up to `end` (`EDIT-PLUG-61/62`): a refused action is a script error, a failed
  run goes on with the script and exits 6 at the end; `--dump` numbers the sections of
  these runs, `== plugin <n>` (`EDIT-PLUG-64`). `batch_plugin.cpp` runs both the stage
  and the verb.
* `--dump-actions` prints `== actions`, the actions on the active tab with the reasons of
  the disabled ones (`EDIT-PLUG-63`).
* The runner is asynchronous in both modes; the batch mode waits for it in a local event
  loop, so the timeout and the kill work as in the GUI.
* A failed or timed-out run exits 6 (`batchPluginError`); findings do not change the exit
  code; a usage error (an unknown plugin, a missing or invalid value) exits 1.
* `--list-plugins` prints a `== plugins` section; `--dump` adds `== plugin` (the run and
  its stdout) and `== diagnostics`; `--dump-plugin` prints only these two. The plugin's
  stderr goes to the editor's stderr. The sections name the folders by origin, never by a
  path.
* Example plugins live in `tests/plugins/<id>/` and run a small helper built with the
  tests, so the cases need no interpreter and pass on the Windows test zip too. The cases
  use `RunBatchTest.cmake` with new `PLUGIN`, `PLUGIN_PARAMS` and `PLUGIN_PATH` variables.

## Code layout

```
  batch_plugin.cpp              plugins_menu.cpp              plugin_dock.cpp
 +------------------------+    +------------------------+    +------------------------+
 | the plugin stage and   |    | PluginsMenu, plugin    |    | PluginDock             |
 | the plugin verb of the |    | toolbars, the plugin   |    | Diagnostics, outputs,  |
 | script; no dialog;     |    | manager; the generated |    | progress and Cancel    |
 | exit 6 on a failed run |    | PluginParameterDialog  |    |                        |
 +------------------------+    +------------------------+    +------------------------+
             |                              |                             ^
             | id, action, values           | id, action, values          | findings, log
             v                              v                             |
 +------------------------------------------------------------------------------------+
 | CyberiadaSMEditorWindow::runPlugin()                           smeditor_window.cpp |
 |  one run per document; locks a replaced document; the result is applied here       |
 +------------------------------------------------------------------------------------+
             |                              |      ^                      |
             | descriptor                   | run  | finished(result)     | apply
             v                              v      |                      v
 +------------------------+    +------------------------+    +------------------------+
 | PluginRegistry         |    | PluginRunner           |    | CyberiadaSMModel       |
 | PluginDescriptor       |    | the run dir, QProcess, |    | one undo step:         |
 | the search path        |    | timeout, validation    |    | begin, restore, end    |
 +------------------------+    +------------------------+    +------------------------+
  plugin_registry.cpp           plugin_runner.cpp             cyberiadasm_model.cpp
  plugin_descriptor.cpp         no widgets: QtCore and
  plugin_values.cpp             libcyberiadamlpp
  plugin_conditions.cpp
```

The batch mode creates the window too, so `runPlugin()` is the one place that applies a
result. The dialogs are `dialogs/plugin_parameter_dialog.cpp` and
`dialogs/plugin_manager_dialog.cpp`.

## Level 2: sessions

A session action keeps its plugin running for the whole interaction:

* a diagram run step by step, with the active states marked on the canvas;
* a replay of a recorded log;
* an LLM agent that asks the user mid-run;
* a bridge to a machine running elsewhere.

The contract (the `session` block, the messages, the model) is the Sessions chapter of
`PLUGIN-API.md`; the requirements are `EDIT-PLUG-36..49`.

| Kind | The machine and its clock | The editor offers | Example |
|---|---|---|---|
| driven | run by the plugin on the editor's command | Step (units), Run (speeds), Pause, Restart, Stop; the event line; the stream | `hsm-to-python` step by step and replay; an FA plugin |
| supervised | run by the plugin on its own | Pause, Run (resume), Stop; the answers | `cybersphere-agents` |
| observed | run by an outside system; the plugin is a bridge | Stop; the event line; Save Log | an HSM-ROS2 controller on turtlesim |

The kind and the rest of the panel come from the plugin's answer to `initialize`, the
*model*. The editor knows no execution semantics: the FA tape, the HSM event queue and
the replay clock belong to the plugin, so a new kind of run needs no new editor.

### Architecture

```
 smeditor_window.cpp      Plugins > Session (the current document): Step, Run, Pause, ...
+--------------------------------------------------------------------------------------+
| PluginDock                                                                           |
|  run row: title, status, time; Step v, Run v, Pause, Restart, Stop; the answer bar   |
|  tabs: Diagnostics | "<document> session": event line, stream, history | outputs     |
+--------------------------------------------------------------------------------------+
     | control, event, input, save, answers                ^ model, state, history,
     v                                                     | log, ask, diagnostics
+--------------------------------------------------------------------------------------+
| PluginSession (QtCore)    the session of one run: the model, the latest state, the   |
|                           history, the open questions, the findings                  |
+--------------------------------------------------------------------------------------+
     | requests, notifications                             ^ messages; text lines
     v                                                     |
+--------------------------------------------------------------------------------------+
| PluginChannel (QtCore)    JSON-RPC 2.0, one object per line: ids, replies, errors    |
+--------------------------------------------------------------------------------------+
     | lines                                               ^ lines
     v                                                     |
+--------------------------------------------------------------------------------------+
| PluginRunner, session mode: the level-1 staging; stdin open; stdout split into lines |
| stderr -> the output tab; shutdown, then terminate and kill as a cancelled run       |
+--------------------------------------------------------------------------------------+
     | stdin                                               ^ stdout, stderr
     v                                                     |
+--------------------------------------------------------------------------------------+
| the session plugin process                                                           |
+--------------------------------------------------------------------------------------+

 PluginSession --(active, fired) at most every RUN_MARK_INTERVAL_MS-->
     CyberiadaSMEditorScene::setRunMarks() --> RunMark {None, Active, Fired} of the items
```

What is reused from level 1:

* the descriptor parsing and the registry;
* the parameter dialog and the stored values;
* the staging (`${...}`, `request.json`, `save`, `meta`);
* the output tab;
* the Diagnostics tab;
* `cancel()` with its grace;
* the result validation and apply;
* the one-run-per-document rule;
* the scene lookup `getMap()` and `idToElement`.

A stream file is never read by the editor: `input` passes its path or text to the plugin.
Edit Stream... uses the `MultilineTextDialog` of the properties (`EDIT-TEXT-10`).

### The session run

```
   a session action: the menu, the toolbar, --run-plugin
          |
          v
   configure, save, meta, stage -- as a run (EDIT-PLUG-7..9, 25, 34) ------------+
          | the document is locked from here                                     |
          v                                                                      |
   +-------------+                                                               |
   |  starting   | -- no answer within SESSION_INIT_TIMEOUT_MS, a bad model, ----+
   +-------------+    the process ended                                          |
          | initialize answered: the model; the controls and the session tab     |
          v                                                                      |
   +-------------+   control, event, input, save, replies to ask --> plugin      |
   |   session   |   state, history, log, ask, diagnostics       <-- plugin      |
   +-------------+                                                               |
          | Stop, the tab closed, the editor closed      | the plugin exits      |
          v                                              |                       |
   +-------------+                                       |                       |
   |  stopping   | cancel the open questions, shutdown,  |                       |
   |             | close stdin; terminate after the      |                       |
   |             | grace, kill after another             |                       |
   +-------------+                                       |                       |
          | the process ended                            |                       |
          v                                              v                       v
   validate (exit_ok) and apply the result (log, diagnostics, files) as a run   failed
   always: clear the marks, unlock the document, remove the run dir
```

* A plugin that answers `shutdown` and exits with a code in `exit_ok` has ended the
  session normally, and its result is applied.
* A plugin the editor has to terminate is a cancelled run, and its result is dropped.

### Run marks

* `RunMark {None, Active, Fired}` is an item state of its own, apart from the selection
  and from `setHighlighted` (the drag target).
* It is drawn by the state, transition, choice and vertex items; Fired is drawn over
  Active. A state gets a tint of the colour and a border of `RUN_MARK_WIDTH`; a selected
  element keeps the selection border, so both show.
* The colours are `RUN_ACTIVE_COLOR` and `RUN_FIRED_COLOR`.
* The scene resolves the ids through `getMap()`. An unknown id is ignored, and so is a
  state machine id (its border is not marked).
* The latest marks are applied at most every `RUN_MARK_INTERVAL_MS`, so a fast run shows
  the latest marks and never a backlog.
* The marks live in the scene of the session's document. The end of the session clears
  them, and the export never draws them.

### Lock

A session locks its document (`setLocked`), as a `replace` run does, but the view stays
interactive. `isEditable()` of the items also checks `model->readOnly()`, so a drag, a
resize, a text edit or a delete is refused. Selecting, zooming, scrolling, the structure
tree and the read-only properties still work. Undo and Redo are disabled.

### The dock

```
 driven (HSM step by step, replay)
+--------------------------------------------------------------------------------------+
| HSM to Python: Run step by step - coffee.graphml    paused  t = 1.300 s  #12  00:41  |
|   [Step: event v] [Run x1 v] [Pause] [Restart] [Stop]                                |
| [Diagnostics] [coffee.graphml session] [HSM to Python output x]                      |
|   Event [COIN amount=5   v] [Post]   [Load Stream...] [Edit Stream...] [Save Log...] |
|     time   event                    origin  outcome  fired                           |
|    0.000   POWER_ON                 input   fired    Off -> Idle                     |
|    0.512   button.CLICKED           input   fired    Idle -> Ready                   |
|    1.000   t1.TIMEOUT               model   fired    Ready: t1.TIMEOUT               |
|  > 1.300   sensor.VALUE value=23.5  input   pending                                  |
+--------------------------------------------------------------------------------------+

 supervised (an LLM agent)
+--------------------------------------------------------------------------------------+
| Cybersphere Agents: Run the agent - develop.graphml    waiting  round 7  03:41       |
|   [Pause] [Stop]                                                                     |
|   ? The tests fail in parser.c. Rewrite the lexer?                                   |
|   [answer ........................................] [yes] [no] [Answer] [Decline]    |
| [Diagnostics] [develop.graphml session] [Cybersphere Agents output x]                |
+--------------------------------------------------------------------------------------+
```

* The run row replaces Cancel with the controls the model offers.
* The answer bar appears under it while a question is open.
* The status text is the plugin's `status` and `text`, plus `time` and `position` when the
  plugin sends them.
* The session tab "<document> session" opens at the start of the session and becomes
  current. It can be closed only after the session ends. The plugin's name and action are
  in its tooltip.
* A history row names its fired elements: a transition as `source -> target`, a state's
  own reaction as `state: trigger`. A click selects and centres the element, as a finding
  does.
* An observed session shows Stop in the run row, and the event line, Save Log and the
  history in its tab.
* A stream-logic session (an FA tape) shows the `output` symbols above the history and the
  `verdict` in the status.

### The Session menu

```
 Plugins                                Plugins > HSM to Python
+-------------------------------+      +----------------------------+
| HSM to Python               > |      | Generate the code...       |
| Cybersphere Agents          > |      | Check the code             |
| ----------------------------- |      | Run the diagram...         |
| Session                     > |      | -------------------------- |
| Output panel                  |      | Run step by step...        |
| Plugin manager...             |      | -------------------------- |
+-------------------------------+      | Settings...                |
                                       +----------------------------+
 Plugins > Session (the current document)
+-------------------------------+
| Step                     F10  |
| Run                       F5  |
| Pause                     F6  |
| Restart                       |
| Stop                Shift+F5  |
| ----------------------------- |
| Post Event                    |
| Load Stream...                |
| Edit Stream...                |
| Save Log...                   |
+-------------------------------+
```

* The session actions follow the plugin's other actions, after a separator. The one-shot
  actions a session action `replaces` are hidden from the menu and the toolbar; the batch
  mode still runs them by id.
* Step repeats the last unit chosen in the dock.
* Post Event moves the focus to the event line.
* Every entry is enabled exactly as its dock control. Without a session on the current
  document, all of them are disabled.

### Batch mode

A session action in batch mode needs `--session-script FILE`; without it the run is a
usage error (exit 1). The verbs run in order, one per line, with `#` comments:

| Verb | Sends, waits |
|---|---|
| `step [UNIT]` | `control step`; waits for a `state` whose status is not `running` |
| `run [SPEED]`, `pause` | `control run`, `control pause` |
| `restart` | `control restart`; waits as `step` |
| `event NAME [name=value ...]` | `event` |
| `input FILE`, `save FILE` | `input {path}`, `save {path}`; an error reply fails the script |
| `wait STATUS [SECONDS]` | waits for that status; `ended` waits for the plugin to end the session |
| `answer TEXT`, `decline` | replies to the oldest open question, waiting for one |
| `expect-active ID ...`, `expect-fired ID ...` | the marks are exactly these ids, in any order |
| `expect-status STATUS`, `expect-verdict VERDICT` | the current status, the verdict |
| `stop` | ends the session as Stop, then waits for the exit |

* Every wait is limited by `SESSION_WAIT_S`, or by the SECONDS given.
* A failed `expect`, a timeout or a failed session exits with 6.
* The end of the script stops a session that is still running.
* `--dump-session` prints the deterministic `== session` section: every message both
  ways in the order it went, one line each with the fields sorted. The editor's own lines
  start with `sent`; `initialize` is printed without its parameters and a path by its file
  name.

```
== session
sent initialize
model events=COIN,CANCEL kind=driven speeds=1,0 steps=event,time stream=true
state active=n1 status=paused time=0
sent event name=COIN params={amount=5}
sent control command=step unit=event
history event=COIN fired=n1-n2 origin=user outcome=fired params={amount=5} time=0.5
state active=n2 fired=n1-n2 status=paused time=0.5
request ask choices=yes,no text=Refund the coin?
sent reply action=accept text=yes
```

### Code layout, level 2

```
 plugin_channel.cpp       PluginChannel: JSON-RPC 2.0 over lines; ids, replies, errors
 plugin_session.cpp       PluginSession: the model, the state, the history, the questions
 plugin_runner.cpp        + the session mode: stdin open, lines, shutdown before terminate
 plugin_session_view.cpp  PluginSessionControls (the run row, the answer bar),
                          PluginSessionTab (the event line, the stream, the history)
 plugin_dock.cpp          + the session rows and tabs, the findings of a session
 plugins_menu.cpp         + the session actions; smeditor_window.cpp: Plugins > Session
 cyberiadasm_editor_*item.cpp, cyberiadasm_editor_scene.cpp   + RunMark, setRunMarks()
 cyberiadasm_render.cpp   + the export without the marks
 batch_session.cpp        --session-script, --dump-session
 tests/plugins/session/   the C++ helper in its session mode: scripted replies
```

| Constant (`cyberiada_constants.h`) | Value | Use |
|---|---|---|
| `SESSION_PROTOCOL_VERSION` | 1 | the `session.version` the editor runs |
| `SESSION_INIT_TIMEOUT_MS` | 30000 | the answer to `initialize` |
| `SESSION_HISTORY_LIMIT` | 10000 | the history rows kept per session tab |
| `SESSION_WAIT_S` | 30 | the default limit of a batch wait |
| `RUN_MARK_INTERVAL_MS` | 50 | the repaint throttle of the marks |
| `RUN_ACTIVE_COLOR`, `RUN_FIRED_COLOR` | `#2e7d32`, `#ef6c00` | the mark colours |

### Why JSON-RPC and not MCP

MCP is an application protocol on top of JSON-RPC 2.0, made for an LLM host that calls
the tools and reads the resources of a server. A session runs the other way: the user
drives through the editor, and the plugin pushes a stream of state.

| | Our session | MCP |
|---|---|---|
| who drives | the user, through the editor | the model in the host |
| a pushed state | one `state` notification | `resources/updated`, then `resources/read`: three messages |
| a question | `ask` | `elicitation/create` (the shape we copy) |
| a plugin in Python | `json` and stdin/stdout, any Python 3 | the official SDK needs Python 3.10+ and pydantic; the Windows 7/8.1 build has 3.8 |

The conventions are MCP's, so that a bridge stays cheap:

* the stdio framing;
* `initialize` first, with the capabilities;
* the `log` levels;
* the replies to `ask`.

MCP fits the reverse direction, planned later: the editor, or a bridge plugin, as an MCP
server whose tools are the edit verbs and whose resources are the open documents, used by
an outside agent.

### Later

* the expected values of a replay and the divergence marks;
* micro steps (exit, effect and entry one by one);
* breakpoints on the canvas;
* a variables view;
* run marks in the structure tree;
* simulated platform components;
* plugin `edit` requests (a conversation that changes the document);
* the MCP bridge.

## Trust

* Only plugins in the search path run; a document never names, starts or configures a
  plugin.
* A plugin runs with the rights of the user, without a sandbox, like any program the
  user starts.
* A plugin that talks to a network service (an LLM, a remote compiler) sends the document
  off the machine; its description must say so.

## Prior art

| System | Taken | Left |
|---|---|---|
| Inkscape extensions (`.inx`) | parameters → a generated dialog; the document as a temp file | stdout as the document |
| Qt Creator, Kate external tools | `${variables}`; output modes; reload after a change | no parameter UI |
| KiCad 9 IPC API | `plugin.json`; out-of-process plugins | managed Python environments; the SWIG binding, deprecated as an unstable raw layer |
| GIMP 3, LSP/DAP, MCP | the level 2 models: a session over stdio | |
| QGIS Processing, VS Code `contributes` | typed parameter declarations | |
| SARIF 2.1.0 | the diagnostics subset maps onto it | too heavy for plugin authors |
| `QPluginLoader`, embedded Python/JS | | a Qt/compiler/ABI lock, a plugin crash is an editor crash, an interpreter in the Windows zip |

## Decisions of 0.2

* A `replace` run locks its document; there is no conflict check at apply.
* A failed run exits 6 in the batch mode.
* Unknown descriptor fields are ignored within a major and listed as warnings by
  `--list-plugins`; a minor version is not used.
* Conditional argv elements `{"if": "<param>", "argv": [...]}` are a part of `api` 1.
* A `files` action names its `outdir` parameter; the files are written in place and listed
  by comparing the paths, sizes and times before and after the run.
* The system folder of a macOS bundle is `Contents/share/cyberiada-pro/plugins`.

Deferred: a `network` flag with a first-run notice, a `${sm}` variable
for documents with several state machines, plugin shortcuts, element context-menu actions,
the toolbar positions (`saveState`), stdout as the output document, a JSON Schema of the
descriptor; the level 2 items under Later.

## Decisions of 0.7

* The conditions are declared, not probed: the editor checks `requires` and `when` itself,
  so a check starts no process and works the same in batch mode.
* `requires` acts at the scan, on the plugin or on an action; `when` acts per document.
* A required plugin counts when the scan enables it; the manager switch does not count.
* `api` stays 1: an editor before the conditions ignores the fields and shows the action.

## Decisions of 0.8

* `git` is a lookup of a `.git` directory, not `git rev-parse`: no process at every refresh,
  the same in batch mode. The `.git` file of a linked work tree does not count. Whether the
  file is tracked, staged or ignored is the plugin's check in the run.
* A Save As refreshes the actions even when the document stays unmodified: the path-based
  tokens (`file`, `git`) follow the new file.

Deferred: a probe command that reports the unavailable actions, an action available after
another one succeeded (`after`), a `param:<id>` token holding when the stored value of a
parameter (a `secret` through its `env:` or `file:` reference) is not empty — the model key
of hsm-maker and cybersphere-agents.

## Decisions of 0.9

* `modes` says which tabs an action serves, as the image types of a GIMP procedure; `when`
  stays the state of the document. The default `["hsm", "fa"]` keeps every earlier plugin
  where it was: on the diagrams, off the text and the diff tabs.
* A text tab gives its text, not a diagram: `input.<ext>`, `output.<ext>`.
* `diff` hands the editor the base only; the editor pairs the elements with the library, so
  every diff tab shows the same pairs, whoever supplied the base. The label is the action's
  string values (`HEAD`): no result file.
* An unknown `result` disables its action alone, so a plugin may offer a `diff` action and
  still work in an editor before it (from this version on).
* A generated text file opens from the `files` list; `new-document` stays in the mode of the
  tab it ran in.

## Decisions of 0.10

* A language comes with a plugin, not from a folder of its own: the tool that runs a
  language brings it, and the plugin manager is the one place to switch it off.
* A language is data: it counts while its plugin is enabled in the manager, whatever keeps
  the plugin's actions off.
* The built-in languages stay compiled in and come first: a plugin adds a language, it
  never replaces one, so the highlighting of a diagram does not depend on the plugins
  installed.
* The languages are read at the scan: at start and on Rescan.

## Decisions of 0.11

* The run belongs to the tab, not to the diagram: `EditorDocument` holds the run, the session
  values and the git fact, so a text tab runs a plugin as a diagram does. A diff tab runs none.
* The mode is checked before the `when` tokens; the registry checks the modes after the
  languages, so a mode a plugin language names is known at the same scan.
* A language replaced by a scan stays allocated until the editor exits: the scenes, the text
  tabs and the highlighters look the language up again on `languagesChanged`, and no pointer
  they hold dangles.
* Refresh of a plugin base runs the action on the tab it ran on: the plugin may read the
  document file, so a closed tab fails the Refresh instead of reusing the last input.
* A `files` link opens what the editor opens: a diagram, a text of a known mode; any other
  file stays a plain line.
* The test plugins that ran in an inspection tab before the modes declare
  `["hsm", "fa", "inspection"]`: the default leaves the inspection tabs out.
