# The Plugin System — Architecture

**Document version:** 0.3 (2026-10-14)

A plugin adds a feature to the editor without rebuilding it: a diagram checker, a code
generator, a compiler front end, an LLM diagram maker. A plugin is a directory with a
`plugin.json` descriptor and the program it runs. The tools ship their plugins in their
own packages; the editor ships none. The editor never loads plugin code: it runs the
program in a separate process, hands it a copy of the active document and takes back what
the action declares — a document, diagnostics, files or a log. The editor gains `QProcess`
and the Qt JSON classes only: no interpreter, no network library, no plugin ABI.

This document is the editor side. The contract a plugin author follows — the descriptor,
the variables, the files and the formats — is `PLUGIN-API.md`; the requirements are
`EDIT-PLUG-1..32` in `EDITOR-SPEC.md` §4.12, the tests the plugin layer of `TESTING.md`.

```
 smeditor_window.cpp, batch_driver.cpp
+----------------------------------------------------------------------------------------+
| Plugins menu, plugin toolbars, --run-plugin           plugin dock: Diagnostics, Output |
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
| 2 (reserved) | a session for the whole interaction | JSON-RPC 2.0 over stdio | an LLM conversation, incremental edits, live diagnostics |

Both levels use the same descriptor; level 2 adds a `session` block (see the end).

## Discovery

```
 search path, first match wins                     PluginRegistry::scan()
+--------------------------------------+         +--------------------------------------+
| 1  --plugin-path DIR ...             |         | for every <dir>/<name>/plugin.json:  |
|    (replaces 2..4; tests, batch)     |  dirs   |   parse and check the fields         |
| 2  $CYBERIADA_EDITOR_PLUGIN_PATH     | ------> |   "api" major supported?             |
| 3  the user plugin folder            |         |   id seen earlier on the path? skip  |
| 4  the system plugin folder          |         |   interpreter found? else disabled   |
+--------------------------------------+         +--------------------------------------+
                                                     | rejected              | accepted
                    +--------------------------------+                       |
                    v                                                        v
+--------------------------------------+         +--------------------------------------+
| not listed: one Output line with the |         | registry: id -> descriptor, folder,  |
| path and the reason; the editor      |         | enabled or disabled + the reason;    |
| starts normally                      |         | read by the menu, the toolbars, the  |
|                                      |         | plugin manager and --list-plugins    |
+--------------------------------------+         +--------------------------------------+
```

The user folder is `QStandardPaths::writableLocation(AppDataLocation)/plugins`; the
system folder is `share/cyberiada-editor/plugins` one level above the executable (a
Debian install) or next to it (the Windows zip). The exact paths are in `PLUGIN-API.md`.
A user plugin overrides a system plugin with the same id; the shadowed one gets an
Output line. The scan runs at start and on Rescan in the plugin manager; it also reads the
plugin's `i18n/<lang>.json` of the interface language and translates the plugin texts
(`EDIT-UI-4`). A run gets the language as `${language}`, `CYBERIADA_EDITOR_LANGUAGE` and
`language` in `request.json` — `en` in the batch mode (`EDIT-PLUG-33`).

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
| [Diagnostics 2] [Output]            Lapki compiler: running 00:07          [Cancel]  |
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
* **Plugin dock** — under the canvas, hidden until the first run: the Diagnostics tab (a
  click selects and centres the element), the Output tab (one block per run), and the
  running plugin with its time and Cancel.
* **Plugin manager** — the plugins found: folder, version, enabled or the reason why not;
  enable/disable, Rescan.
* **Disabled actions** — a missing interpreter, inspection mode (for `replace`), a run
  already going on the same document; the tooltip gives the reason.

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
          | finished; stdout and stderr streamed to Output                |
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
  `CYBERIADA_EDITOR_PLUGIN_KEEP=1`. `input.graphml` is the model snapshot (Cyberiada 1.0,
  unsaved edits included) — never the file on disk; an empty document gives an empty file.
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
* The Output text is capped per run; undecodable bytes are shown in the local 8-bit
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
                                    +--------------------------------------------------+
```

* A new document takes the active tab when that is an empty untitled one, as Open does
  (`EDIT-DOC-2`), and a new tab otherwise.
* The output is decoded before anything is touched: `restoreSnapshot` only warns about a
  bad buffer. `load.reconstruct` lays out a document that comes without geometry (an LLM
  answer); `load.strict` rejects a non-conforming one.
* An output equal to the input pushes no undo step; Output says "no changes".
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
Output, the session log or the settings.

## Batch mode and tests

* The plugin stage runs after the script stage, before the dumps, the export and the save:
  open, script, **plugin**, tabs, dump, text, stack, export, save. A run without a
  document starts from an empty one.
* No dialog and no stored values: the defaults and `--plugin-param` only.
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
  batch_driver.cpp              plugins_menu.cpp              plugin_dock.cpp
 +------------------------+    +------------------------+    +------------------------+
 | the plugin stage after |    | PluginsMenu, plugin    |    | PluginDock             |
 | the script; no dialog; |    | toolbars, the plugin   |    | Diagnostics, Output,   |
 | exit 6 on a failed run |    | manager; the generated |    | progress and Cancel    |
 |                        |    | PluginParameterDialog  |    |                        |
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
```

The batch mode creates the window too, so `runPlugin()` is the one place that applies a
result. The dialogs are `dialogs/plugin_parameter_dialog.cpp` and
`dialogs/plugin_manager_dialog.cpp`.

## Level 2 (outline)

A `session` block in the descriptor names the session command. The editor starts it once
and keeps it for the document; the messages are JSON-RPC 2.0 over stdio.

| Direction | Messages |
|---|---|
| editor → plugin | `initialize` (api, the snapshot, meta, values), `document/changed`, `shutdown` |
| plugin → editor | `edit` (a list of edit-script verbs, the batch script vocabulary; one undo step per list), `input` (a parameter list: the editor shows the same generated dialog mid-run), `diagnostics/publish`, `log`, `message` (a conversation pane) |

The alternative is the editor acting as an MCP server for an outside agent; the choice is
left for the level 2 design.

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
* The system folder of a macOS bundle is `Contents/share/cyberiada-editor/plugins`.

Deferred: a `network` flag with a first-run notice, a `${sm}` variable
for documents with several state machines, plugin shortcuts, element context-menu actions,
the toolbar positions (`saveState`), stdout as the output document, a JSON Schema of the
descriptor, level 2.
