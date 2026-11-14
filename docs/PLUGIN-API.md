# The Plugin Interface

**Document version:** 0.13 (2026-11-14), plugin contract `api` 1, session protocol 1. In
Russian: `PLUGIN-API-ru.md`.

This is the brief for a plugin author. A plugin gives CyberiadaPro, the Cyberiada HSM editor, a
new action — check a diagram, generate code, compile, draw — without changing the editor. The
plugin is usually written and packaged by the authors of the tool it runs, in the tool's
own repository; anyone may also package a plugin for a tool they do not own, or put one
into their user plugin folder. How the editor works inside is in `PLUGINS.md`.

A plugin is a directory with a `plugin.json` descriptor. For each run the editor writes a
copy of the active document into a temporary run directory, starts the declared command
there, waits for it and takes the result the action declares. A session action keeps the
plugin running and talks to it while the user works (see Sessions).

```
 the editor                                           the plugin
+------------------------------+                    +------------------------------+
| writes the run dir:          |  argv, env         | reads   ${input}             |
|   input.graphml              |  cwd = run dir     |         ${request}           |
|   request.json (if used)     | -----------------> |         ${param-file:x}      |
|   a file per param-file      |                    |                              |
|                              |                    | does its work                |
| shows stdout and stderr      | <-- stdout, stderr |                              |
| reads the declared result    | <-- exit code ---- | writes  ${output}            |
|                              |                    |         ${diagnostics}       |
|                              |                    |         files in ${outdir}   |
+------------------------------+                    +------------------------------+
```

## A minimal plugin

```
hello/
    plugin.json
    size.py
```

```json
{
  "api": 1,
  "id": "hello",
  "name": "Hello",
  "interpreter": "python3",
  "actions": [
    { "id": "size", "label": "Document size", "result": "log",
      "argv": ["${plugin_dir}/size.py", "${input}"] }
  ]
}
```

```python
import os
import sys
print(os.path.getsize(sys.argv[1]), 'bytes')
```

Put `hello/` into the user plugin folder (see Packaging) and restart the editor: the
Plugins menu gets the entry "Document size"; a click prints the size into the "Hello
output" tab of the plugin dock.

## The plugin directory

```
<plugin folder>/<id>/
    plugin.json        the descriptor, UTF-8
    icons/*.svg        the menu and toolbar icons (optional)
    ...                the scripts, an adapter, bundled modules
```

* The directory name should be the plugin `id`; the `id` in `plugin.json` is what counts.
* The `id` matches `[a-z0-9][a-z0-9._-]*` and is stable: the stored parameter values are
  kept under it. Name it after the tool (`hsm-ros2`, `lapki-compiler`).
* Paths inside the descriptor are relative to the plugin directory or use
  `${plugin_dir}`.

## plugin.json

```json
{
  "api": 1,
  "id": "hsm-maker",
  "name": "HSM Maker",
  "version": "0.1.0",
  "description": "Draws a new diagram from a description; sends it to an LLM service",
  "icon": "icons/maker.svg",
  "interpreter": "python3",
  "env": { "PYTHONPATH": "${plugin_dir}" },
  "parameters": [ ... ],
  "actions": [ ... ]
}
```

| Field | Required | Meaning |
|---|---|---|
| `api` | yes | the contract major version, `1`; the editor skips a descriptor with a major it does not know |
| `id` | yes | the plugin id (see above) |
| `name` | yes | the menu and plugin manager name |
| `version` | no | the plugin's own version, shown in the plugin manager |
| `description` | no | the tooltip and plugin manager text; say here if the plugin uses the network |
| `icon` | no | the plugin icon (SVG or PNG) |
| `interpreter` | no | a logical interpreter name (`python3`); the command is then `interpreter + argv` |
| `env` | no | extra environment variables; the values may use `${plugin_dir}` |
| `parameters` | no | the parameters, shared by the actions (see Parameters) |
| `requires` | no | the programs, variables and plugins the plugin needs (see Conditions) |
| `actions` | yes | at least one action; a plugin with session actions keeps one too, so that an editor before the sessions loads it; a plugin with `languages` may have none |
| `session` | no | the session actions (see Sessions); an editor before the sessions ignores it |
| `languages` | no | the language files the plugin brings (see Languages) |

An action:

| Field | Default | Meaning |
|---|---|---|
| `id` | required | the action id, unique in the plugin; `--run-plugin <plugin>:<action>` |
| `label` | required | the menu text; end it with `...` when the action asks for input |
| `result` | required | `replace`, `new-document`, `diagnostics`, `files`, `log` or `diff` (see Results) |
| `argv` | required | the command: a list of strings with variables, and conditional elements (see The command line) |
| `exit_ok` | `[0]` | the exit codes of a successful run |
| `timeout` | `60` | seconds; `0` means none |
| `parameters` | all | the ids of the parameters this action uses and shows |
| `outdir` | none | the id of the `dir` (or `file`) parameter that receives the files; required for a `files` action |
| `load` | `{}` | `strict`: reject an output that is not strictly conforming; `reconstruct`: lay out an output without geometry |
| `save` | `false` | save a modified or untitled document before the run, so `${document}` is current (an untitled one asks for a name) |
| `cwd` | `"run"` | the working directory: `run` (the run dir) or `document` (the document's directory; implies `save`) |
| `toolbar` | `false` | put the action on the plugin toolbar |
| `meta` | none | metainformation the action works with, e.g. `{"platformLanguage": "Python"}`; filled in when the document lacks it (see Metainformation) |
| `icon` | the plugin icon | the action icon for the toolbar and the menu |
| `description` | none | the tooltip |
| `requires` | none | what the action needs besides the plugin's `requires` (see Conditions) |
| `when` | none | the document conditions of the action (see Conditions) |
| `reason` | none | the tooltip when a `when` token fails |
| `modes` | `["hsm", "fa"]` | the modes of the tabs the action works in (see Modes) |

## How the editor shows a plugin

* **The Plugins menu** — a submenu named after the plugin with its actions, ending with
  Settings... when the plugin has stored parameters; a plugin with one action and no
  stored parameters is a single entry.
* **The plugin toolbar** — the actions with `toolbar: true` form a small toolbar named
  after the plugin, to the right of the main toolbar; the user can hide it.
* **A click** — when the action has `run` parameters, a `document` parameter not yet set
  for this document, or a required `persistent` parameter without a value, the editor
  opens a dialog generated from them; otherwise the action runs at once with the stored
  values.
* **A greyed-out action** — the plugin is switched off or unavailable, a run is going on
  the document, the tab is in a mode the action does not list (see Modes), the tab is not
  editable for a `replace` action, or a condition of the action fails (see Conditions); the
  tooltip says why.
* **The plugin dock** — every plugin has its own output tab, "<name> output", with the
  stdout and stderr of its runs; the user closes it, clears it or saves it to a text
  file. The Diagnostics tab holds the findings; the running plugin has a Cancel button.

```
+--------------------------------------------------+
| HSM Maker - Draw a new diagram               [x] |
+--------------------------------------------------+
| Description                                      |
| +----------------------------------------------+ |
| | A traffic light with a pedestrian button;    | |
| | the yellow light blinks at night.            | |
| +----------------------------------------------+ |
| Model      [deepseek-flash                    ]  |
| Server     [https://api.deepseek.com/v1       ]  |
| API key    [env:DEEPSEEK_API_KEY              ]  |
|                                 [Cancel]  [Run]  |
+--------------------------------------------------+

+--------------------------------------------------+
| HSM ROS2 - Generate the package...           [x] |
+--------------------------------------------------+
| Workspace [~/work/light/ros2_ws          ] [...] |
|                                 [Cancel]  [Run]  |
+--------------------------------------------------+
```

The plugin itself opens no windows and asks nothing on stdin: everything it needs is
declared as parameters, so the same action also runs in the batch mode. A session asks its
questions with `ask` (see Sessions).

## The command line

The command is `interpreter + argv`, or `argv` alone; without an interpreter `argv[0]` is
a program relative to the plugin directory, or found in `PATH`; on Windows a program
without a suffix also matches `<name>.exe`, and a `.bat` or `.cmd` file cannot be one.
There is no shell: each list element is one argument, quoting is never needed, and a
substituted value is never expanded again. stdin is the null device; a session action gets
the session channel instead (see Sessions). The environment is
the editor's, plus `env`, plus the exported secrets, plus `CYBERIADA_PRO_LANGUAGE`: the
interface language.

An argv element may be conditional: `{"if": "<param>", "argv": [...]}` adds its strings
when the parameter is `true` (a bool) or not empty (any other type), and nothing
otherwise.

```json
"argv": ["${plugin_dir}/check.py", "${input}", "${diagnostics}",
         {"if": "strict", "argv": ["--strict"]}]
```

| Variable | Value |
|---|---|
| `${input}` | `<rundir>/input.graphml`: the active document as CyberiadaML-GraphML 1.0, unsaved edits included; an empty file for an empty document. A text tab: `<rundir>/input.<ext>`, the text in UTF-8 (see Modes) |
| `${output}` | `<rundir>/output.graphml`: where a `replace`, `new-document` or `diff` action writes the document. A text tab: `<rundir>/output.<ext>` |
| `${diagnostics}` | `<rundir>/diagnostics.json`: where findings go |
| `${outdir}` | the value of the action's `outdir` parameter; the run dir when there is none |
| `${rundir}` | the temporary run directory; removed after the run |
| `${plugin_dir}` | the plugin directory |
| `${request}` | `<rundir>/request.json`, written only when the argv uses it |
| `${param:x}` | the value of the parameter `x`; a bool is `true` or `false` |
| `${param-file:x}` | a run dir file holding the value of `x`; use it for long texts such as prompts |
| `${meta:k}` | a document meta value: `platform`, `platformVersion`, `platformLanguage`, `markupLanguage`, `name`, ...; empty when absent |
| `${selection}` | the ids of the selected elements in the document order, comma-separated (the editor selects one element at a time) |
| `${language}` | the interface language of the editor: `en`, `ru`, ...; `en` in the batch mode |
| `${document}` | the document file on disk; empty when untitled; current only with `save` |
| `${document_dir}`, `${document_name}` | its directory; its base name without the extension |

An unknown variable makes the run fail before it starts. `$` not followed by `{` is kept
as is.

The interpreter name is resolved from the editor Preferences, then from `PATH`; on
Windows `python3` also tries `python` and `py -3`, skipping the Microsoft Store stub. The
Windows editor carries no Python: a Python plugin there stays disabled with a hint until
an interpreter is found.

## request.json

A plugin with many inputs may read one file instead of arguments. It is written when the
argv contains `${request}`:

```json
{
  "api": 1,
  "plugin": "hsm-maker",
  "action": "draw",
  "editor_version": "1.0.0",
  "language": "ru",
  "input": "/tmp/cyberiada-plugin-Xa12/input.graphml",
  "document": { "path": "/home/user/work/light.graphml", "format": "cyberiada",
                "modified": true },
  "meta": { "platform": "ArduinoUno", "platformVersion": "1.0",
            "platformLanguage": "C++" },
  "selection": ["n3", "t7"],
  "params": { "prompt": "A traffic light with a pedestrian button",
              "model": "deepseek-flash" },
  "outputs": { "output": "/tmp/cyberiada-plugin-Xa12/output.graphml",
               "diagnostics": "/tmp/cyberiada-plugin-Xa12/diagnostics.json",
               "outdir": "/tmp/cyberiada-plugin-Xa12" }
}
```

`document.format` is the format the document is saved in (`cyberiada`, `yed-ostranna`,
`yed-berloga`, `lapki`) — of a text tab its mode (`markdown`, `cpp`); the input of a diagram is
always CyberiadaML-GraphML 1.0. `meta` holds the document
metainformation strings except the editor's own view state. Secrets are never in it.

## Results

| `result` | The plugin writes | The editor |
|---|---|---|
| `replace` | the whole changed document to `${output}` | replaces the document as one undo step "Plugin: <label>" |
| `new-document` | a document to `${output}` | opens it as a modified unsaved document titled by its document name: in the active tab when that is an empty untitled one, in a new tab otherwise |
| `diagnostics` | `${diagnostics}` | shows the findings |
| `files` | files into `${outdir}` (the action's `outdir` parameter) | lists the new and changed files in the output tab; a diagram or a text file of a known mode opens in a tab on a click |
| `log` | stdout, stderr | nothing more |
| `diff` | the base document to `${output}`; an empty file is an empty document; in a text tab the base text in UTF-8 | opens a diff tab — a text diff tab in a text mode: the base is the output, the target the document as `${input}` had it; the tab is labelled by the values of the action's `string` parameters, else by its label; Refresh runs the action again on the same tab, and fails when that tab is closed |

* The output document may be CyberiadaML-GraphML 1.0 or a yEd dialect the library reads;
  the document keeps its own save format.
* An output without geometry is accepted with `load.reconstruct`: the editor lays it out.
  Without it, write the geometry.
* A `replace` output keeps the ids of the elements it does not change, so the selection
  and the findings still point at them. An output equal to the input changes nothing.
* An output that cannot be read, or an empty one, fails the run; the document stays as it
  was. A `diff` output may be empty.
* Any action may also write `${diagnostics}`; a `replace` action may report what it
  changed this way.
* A `replace` action is disabled in a tab that is not editable — read-only, inspection,
  diff — and the document is locked while it runs.
* A `diff` action supplies the base only: the editor pairs the elements itself, with the
  revision diff of libcyberiadaml, or the lines of a text with its own line diff, as in
  Document > Compare with.... Its `modes` may list the diagram modes `hsm`, `fa`,
  `inspection` and the text modes (0.13).
* An editor that does not know a `result` greys that action out as needing a newer editor;
  an editor before the diff tabs rejects the whole plugin, so a package with a `diff`
  action requires an editor with them; one before the text diff tabs rejects the whole
  plugin when a `diff` action lists a text mode.

## The diagnostics file

```json
[
  { "severity": "warning", "rule": "CGML-6.9-4-4", "element": "nMeta", "line": 31,
    "message": "transitionOrder 'transitionFirst' is not admissible" },
  { "severity": "info", "rule": "CGML-6.4-4-2", "element": "G0", "line": 28,
    "message": "state machine 'G0' contains no initial pseudostate" }
]
```

| Field | Required | Meaning |
|---|---|---|
| `severity` | yes | `error`, `warning` or `info`; the LSP numbers 1-4 are accepted too |
| `message` | yes | one line for the user; newlines are shown as spaces |
| `rule` | no | the rule or requirement id |
| `element` | no | the id of a node, an edge or a state machine; a click on the entry selects it |
| `line` | no | a line of `${input}`; in a text tab a click goes to it |

Other fields are ignored; a file that is not such an array fails the run. An `element`
that is not in the document is shown without a link. The entries turn grey once the
document changes after the run. The format is a subset of the LSP `Diagnostic`: severity
1 is `error`, 2 is `warning`, 3 and 4 are `info`, `code` is `rule`.

## Parameters

```json
"parameters": [
  { "id": "prompt", "type": "text", "label": "Description", "scope": "run",
    "required": true, "language": "markdown" },
  { "id": "model", "type": "string", "label": "Model", "scope": "persistent",
    "default": "deepseek-flash" },
  { "id": "dir", "type": "dir", "label": "Package directory", "scope": "document",
    "default": "${document_dir}/ros", "create": true },
  { "id": "key", "type": "secret", "label": "API key", "scope": "persistent",
    "export": "HSM_MAKER_API_KEY" }
]
```

Common fields: `id`, `type`, `label`, `default`, `scope` (default `run`), `required`
(default `false`), `description` (the tooltip).

| `type` | Widget | Options | Value |
|---|---|---|---|
| `string` | a line edit | | the text |
| `text` | a large text box on top of the dialog | `language`: highlighting by the editor's rules (`markdown`, `python`, ...) | the text, with newlines; pass it with `${param-file:x}` |
| `int`, `double` | a spin box | `min`, `max` | the number |
| `bool` | a check box | | `true` or `false` |
| `enum` | a combo box | `choices`: a list of strings | the chosen string |
| `file` | a line edit with `...` | `filter` (`"Python (*.py)"`), `save`: choose a new file | the path |
| `dir` | a line edit with `...` | `create`: make a missing directory | the path |
| `secret` | a line edit holding `env:NAME` or `file:PATH` | `export`: the variable that gets the key | — (never in the argv) |

| `scope` | Asked | Kept |
|---|---|---|
| `run` | every run, starting from the default | no |
| `persistent` | once; changed in Settings... | per user |
| `document` | on the first run for a document file; changed in Settings... | per user and document file |

* The values are kept in the editor settings, never in the diagram.
* A `default` may use `${document_dir}` and `${document_name}`, so a directory starts
  next to the diagram; for an untitled document they are empty, and so is such a default.
  It may also use `${language}`, so a language option follows the interface.
* A secret is resolved at the start of a run — the variable `NAME` of the editor's
  environment, or the first line of the file `PATH` — and exported to your process as the
  `export` variable. It never reaches the argv, `request.json`, the output tab or the
  settings. Do not print it.

## Translation

The texts of `plugin.json` are written in English. A plugin translates them with one file
per language, `i18n/<lang>.json`: a map from the English text to its translation.

```
hsm-to-python/
    plugin.json
    i18n/ru.json
```

```json
{
  "HSM to Python": "HSM в Python",
  "Check the code": "Проверить код",
  "Tick, ms": "Такт, мс",
  "update": "обновить"
}
```

* When it scans the plugins the editor reads the file of its interface language (chosen in
  the Preferences, applied at start) and translates the name and the description, the
  labels, the descriptions and the reasons of the actions, the labels and the descriptions
  of the parameters, and the shown enum choices; a choice keeps its value.
* A text without a translation is shown as written, a key that matches no text is
  ignored, and a file that is not a JSON object is listed as a warning.
* The batch mode is English and reads no translation.
* Your own messages may follow the same language: a run gets it as `${language}`, as
  `CYBERIADA_PRO_LANGUAGE` and as `language` in `request.json`.

## Metainformation

An action may declare the metainformation it works with:

```json
{ "id": "check", "label": "Check the code", "result": "diagnostics", "save": true,
  "meta": { "platformLanguage": "Python" }, "argv": [ ... ] }
```

* Before the run, and before the save of a `save` action, the editor fills each key the
  document lacks, all in one undo step; the run then sees the value (`${meta:k}`, the
  saved file).
* A key the document has keeps its value; a different one is noted in the output tab and
  the run goes on. The platform language counts as the same under any of its names
  (`python3`, `Python`).
* A document without a state machine, or a read-only one, is left as it is.
* A `replace` or `new-document` result carries its metainformation in the document itself.

## Conditions

An action may need a program, a saved file or a certain diagram. Declare it, and the editor
greys the action out with the reason in the tooltip instead of starting a run that fails.

```json
{
  "requires": { "programs": ["arduino-cli"], "plugins": ["cgml-checker>=0.3"] },
  "actions": [
    { "id": "upload", "label": "Upload", "result": "log",
      "requires": { "env": ["ARDUINO_PORT"] },
      "when": ["saved", "initial", "meta:platform=ArduinoUno"],
      "reason": "Save a diagram for the Arduino Uno",
      "argv": [ ... ] }
  ]
}
```

`requires` is the system, checked when the editor scans the plugins (the start, Rescan, the
Preferences). On the descriptor it disables the whole plugin, on an action the action alone.

| Key | Holds when |
|---|---|
| `programs` | each program is found on `PATH`; a path with `${plugin_dir}` is an executable file |
| `env` | each variable is set and not empty, in the editor environment or in `env` |
| `plugins` | each `<id>` or `<id>>=<version>` is a plugin found and enabled, of that version at least; the user's switch in the plugin manager does not count |

`when` is the document, checked again after every edit, undo, save and selection change. All
the tokens must hold; `!` negates one (`!fa`).

| Token | Holds when |
|---|---|
| `file` | the document has a file |
| `saved` | the document has a file and no unsaved changes |
| `selection` | an element or a state machine is selected: `${selection}` is not empty |
| `sm` | the document has a state machine |
| `initial` | every state machine has an initial pseudostate at its top level |
| `fa` | the document is a finite automaton: `type/ fa` in the metainformation (EXT-FA) |
| `fa-valid` | a finite automaton without EXT-FA violations |
| `meta:<key>` | the metainformation has the key |
| `meta:<key>=<value>` | the key has the value; the platform language under any of its names |
| `git` | the document file is in a git work tree: a `.git` directory in its directory or above; no program is run |

* `reason` replaces the reason of a failing token: tell the user what to do.
* `saved` greys the action out on a modified document; `save: true` saves it instead.
* A key the action's `meta` fills holds when the document lacks it.
* An unknown token or key keeps the action (the plugin) off, telling that it needs a newer
  editor, and is a warning in `--list-plugins`; an editor before the conditions ignores
  `requires` and `when`.
* A program installed while the editor runs counts after Rescan, a variable set after a
  restart of the editor.
* `git` sees a `.git` directory only: the `.git` file of a linked work tree does not count. A
  `.git` made while the document is open counts after a save or Rescan; whether the file is
  tracked, staged or ignored is your program's check.
* A check only your program can make stays in the run: fail with one clear line.

## Modes

A tab of the editor holds a diagram or a text in a mode (Document > Mode). `modes` lists the
modes an action works in; in a tab of another mode the action is greyed out.

```json
{ "id": "compare", "label": "Compare with a Revision...", "result": "diff",
  "modes": ["hsm", "fa", "inspection"], "when": ["git"], "parameters": ["revision"],
  "argv": ["-m", "hsm_git", "revision", "${document}", "--revision=${param:revision}",
           "--output", "${output}"] }
```

| Mode | A tab of | `${input}` |
|---|---|---|
| `hsm` | an HSM diagram | `input.graphml` |
| `fa` | a finite automaton diagram (`type/ fa`) | `input.graphml` |
| `inspection` | a diagram inspected, read-only | `input.graphml` |
| `diff` | two diagrams compared, read-only | - (no action runs there) |
| `text-diff` | two texts compared, read-only | - (no action runs there) |
| `text` | a plain text | `input.txt` |
| `markdown` | a Markdown text | `input.md` |
| `c`, `cpp`, `csharp`, `python` | a source file of the language | `input.c`, `input.cpp`, `input.cs`, `input.py` |
| a plugin's language | a source file of that language (see Languages) | `input.<its first extension>` |

* Without `modes` an action works in `hsm` and `fa`: a plugin written before the modes is
  where it was.
* In a text tab `${output}` is `output.<ext>`: a `replace` action writes the whole text
  there, a `new-document` action a new text of the same mode. The diagram tokens of `when`
  (`sm`, `initial`, `fa`, `fa-valid`, `meta:`) do not hold in a text tab.
* An unknown mode is a warning; an action that lists no mode the editor knows is greyed
  out as needing a newer editor. An editor before the modes ignores the field.

## Languages

A plugin may bring the programming languages of its tool: the editor highlights the code of
a diagram written in them (`platformLanguage`) and opens their source files in a text tab of
their own mode. No rebuild of the editor is needed.

```json
{
  "api": 1,
  "id": "lua-tools",
  "name": "Lua",
  "languages": ["syntax/lua.json"],
  "actions": [
    { "id": "run", "label": "Run the Script", "result": "log", "modes": ["lua"],
      "argv": ["lua", "${input}"] }
  ]
}
```

`syntax/lua.json`, in the plugin directory:

```json
{
  "name": "Lua",
  "aliases": ["lua5.4", "luajit"],
  "extensions": [".lua"],
  "lineComment": "--",
  "blockComment": ["--[[", "]]"],
  "quotes": "\"'",
  "keywords": ["and", "break", "do", "else", "elseif", "end", "false", "for", "function",
               "if", "in", "local", "nil", "not", "or", "repeat", "return", "then", "true",
               "until", "while"],
  "types": ["print", "pairs", "ipairs", "string", "table", "math"]
}
```

| Key | Required | Meaning |
|---|---|---|
| `name` | yes | the name in the menus and in the tab marker, `[Lua]`; matched with `platformLanguage` case-insensitively |
| `aliases` | no | the other names `platformLanguage` may give |
| `extensions` | no | the file extensions Open recognises, the first one proposed by Save As and used for `${input}` |
| `lineComment` | no | the line comment start |
| `blockComment` | no | the block comment start and end |
| `quotes` | no | the string quote characters |
| `preprocessor` | no | `true`: a line starting with `#` is a preprocessor line |
| `keywords`, `types` | no | the words drawn as keywords and as types |

* The id of the language is the file name without `.json` (`lua`): the mode id that
  `modes` names.
* The built-in languages (C, C++, C#, Python) come first, then the plugins in the order of
  the search path. A language whose id, name or alias another one already has is skipped
  with a warning in the plugin manager and `--list-plugins`.
* A language counts while its plugin is enabled in the plugin manager, even when the
  interpreter or `requires` keep the actions off. A file that is missing or is not a
  language is a warning; `languages` of a wrong type rejects the descriptor.
* The languages are read at start and on Rescan. A language gone after a Rescan turns its
  text tabs into Plain text and its `platformLanguage` into an unknown value.
* A plugin with `languages` may have no action. An editor before the languages ignores the
  field, and rejects a plugin without actions.

## Exit codes, errors, cancel

* The run succeeds when the exit code is in `exit_ok` and the declared output is there
  and readable. Anything else fails the run: the user sees the exit code and the end of
  stderr, the document is untouched.
* Write progress and messages to stdout and errors to stderr, in UTF-8; they appear in
  the plugin's output tab as the run goes, up to 1 MiB per run. A Python plugin on
  Windows sets `PYTHONUTF8=1` in `env` to write UTF-8 into a pipe.
* A timeout or Cancel terminates the process (SIGTERM on Unix) and kills it 3 seconds
  later; on Windows expect the kill. Only your process is stopped: stop the processes
  you started yourself, such as a local server.
* `CYBERIADA_PRO_PLUGIN_KEEP=1` in the editor's environment keeps the run directories
  for debugging.

## Testing a plugin

The batch mode runs an action without the window and without dialogs; the values are the
defaults and the `--plugin-param` options.

```
# in the tool repository: the folder . holds editor-plugin/plugin.json
CyberiadaPro --batch --plugin-path . --list-plugins
CyberiadaPro --batch light.graphml --plugin-path . \
    --run-plugin hsm-ros2:generate --plugin-param dir=/tmp/ros --dump
CyberiadaPro --batch --plugin-path . --run-plugin hsm-maker:draw \
    --plugin-param prompt=@light.txt --plugin-param key=env:DEEPSEEK_API_KEY \
    --save light.graphml
```

| Option | Meaning |
|---|---|
| `--run-plugin <id>[:<action>]` | run the action; `:<action>` may be omitted when the plugin has one |
| `--plugin-param <name>=<value>` | a parameter value, repeatable; `<name>=@<file>` reads it from a file, `@@` stands for a leading `@`; a bool is `true` or `false`; a secret takes `env:NAME` or `file:PATH` |
| `--plugin-path <dir>` | look for plugins in `<dir>` only, repeatable; relative to the current directory; also in the GUI |
| `--list-plugins` | print the plugins found, their state and why a plugin is disabled or rejected; needs no document |
| `--dump-plugin` | print only the `== plugin` and `== diagnostics` sections |
| `--dump-actions` | print `== actions`: your actions on the active tab, enabled or disabled with the reason |

Without a document on the command line the run starts from an empty one, which suits a
`new-document` action. `--dump` adds the `== plugin` section (the action, the state, the
exit code, your stdout, the files) and the `== diagnostics` section; without a dump your
stdout goes to the editor's stdout. stderr always goes to the editor's stderr. The
folders are named by their origin (`plugin-path 1`, `env 1`, `user`, `system`), never by
a path, so the output is the same on every machine. The exit code is 0 for a successful
run (findings or not), 1 for a usage error (an unknown plugin, a missing or invalid
value) and 6 for a failed run.

The edit script runs your actions too, between its edits, with the values on the line
(double-quoted when they hold spaces); `save` gives a document its file, as your `saved`
and `git` conditions need:

```
# compare.script, run as: CyberiadaPro --batch light.graphml --plugin-path . \
#     --script compare.script --dump --dump-tabs
rename n0 Ready
save
plugin git:compare revision=HEAD
```

Each run gets its own sections, `== plugin 1`, `== plugin 2`, ...; a refused action stops
the script (exit 4), a failed run does not and the batch exits 6 at the end.

## Packaging

Install the plugin directory as is, `i18n/` included:

| System | Folder |
|---|---|
| Linux, system | `<prefix>/share/cyberiada-pro/plugins/<id>/` — the editor's prefix, `/usr` for the Debian package |
| Linux, user | `~/.local/share/Cyberiada/CyberiadaPro/plugins/<id>/` |
| Windows, the editor zip | `<editor folder>\share\cyberiada-pro\plugins\<id>\` |
| Windows, user | `%APPDATA%\Cyberiada\CyberiadaPro\plugins\<id>\` |
| macOS, user | `~/Library/Application Support/Cyberiada/CyberiadaPro/plugins/<id>/` |
| macOS, the bundle | `CyberiadaPro.app/Contents/share/cyberiada-pro/plugins/<id>/` |

* Keep the directory in the tool repository as `editor-plugin/` and install it from the
  tool's package, or from a small separate package (`<tool>-editor-plugin`) when the tool
  should not know about the editor.
* A Debian package declares `Enhances: cyberiada-pro` and depends on what the plugin
  runs (`python3`, the tool).
* A user plugin overrides a system plugin with the same `id`.
* An upgrade replaces the directory. The stored values survive while the parameter ids
  stay the same; a renamed parameter starts from its default.
* Raise `api` only when the editor documents a new major; a new editor keeps running the
  majors it lists.

## Sessions

A session action keeps the plugin running while the user works with it:

* a diagram run step by step, with the active states marked on the canvas;
* a replay of a recorded log;
* an agent that asks the user mid-run;
* a bridge to a machine that runs elsewhere.

For the whole session the editor and the plugin exchange JSON-RPC 2.0 messages over
stdin and stdout.

```
 the editor                                            the session plugin
+-------------------------------+                    +------------------------------+
| the run dir, argv, env:       |    initialize      | reads ${input} and the rest  |
|   as for any action           |    control, event  | as any action                |
|                               |    input, save     |                              |
| marks the canvas, shows the   |    replies to ask  | runs the machine, the agent, |
| status, the history and the   | -----------------> | the replay or the bridge     |
| questions                     |       stdin        |                              |
|                               | <----------------- |                              |
|                               |       stdout       |                              |
|                               |    state, history  |                              |
|                               |    log, ask        |                              |
|                               |    diagnostics     |                              |
| the output tab                | <---- stderr ----- |                              |
+-------------------------------+                    +------------------------------+
```

| Kind | Who runs the machine and its clock | The user gets | Example |
|---|---|---|---|
| `driven` | the plugin, on the user's command | Step, Run, Pause, Restart, Stop; posting events; a stream to load, a log to save | `hsm-to-python`: a diagram step by step, a replay |
| `supervised` | the plugin, on its own | Pause, Run (resume), Stop; the questions | `cybersphere-agents`: an agent run |
| `observed` | an outside system; the plugin relays | Stop; posting events; Save Log | a bridge to an HSM-ROS2 controller |

### The session block

```json
{
  "api": 1,
  "id": "hsm-to-python",
  "actions": [ ... ],
  "session": {
    "version": 1,
    "actions": [
      { "id": "step", "label": "Run step by step...", "icon": "icons/step.svg",
        "toolbar": true, "save": true, "parameters": ["main", "tick"],
        "argv": ["-m", "hsm", "--session", "${document}", "--tick", "${param:tick}",
                 {"if": "main", "argv": ["--main", "${param:main}"]}] }
    ]
  }
}
```

| Field | Meaning |
|---|---|
| `session.version` | the session protocol, `1`; an editor that does not run this version leaves the session actions out and keeps the other actions |
| `session.actions` | the session actions, with the fields of an action except `timeout` and `load`; `result` is `log` (the default), `diagnostics` or `files`, taken when the session ends |
| `replaces` | in a session action: the ids of the one-shot actions it supersedes; an editor that runs the sessions hides them from the menu and the toolbar, an older editor shows them, and `--run-plugin` runs them by id everywhere |

* The action ids are unique across `actions` and `session.actions`.
* An editor before the sessions ignores the block, so the plugin's other actions keep
  working there. A one-shot fallback of a session action, named in its `replaces`, is
  what such an editor shows instead:

| | a level-1 plugin | `actions` + `session` | `actions` + `session` with `replaces` |
|---|---|---|---|
| an editor before the sessions | the actions | the actions | the actions, the fallbacks included |
| an editor with the sessions | the actions | the actions and the session actions | the session actions instead of the fallbacks |

* The parameters, the dialog, `save`, `meta`, the variables and `request.json` work as for
  any action. `${input}` is the document at the start, and the document stays locked
  until the session ends.

### The exchange

* One JSON-RPC 2.0 object per line, UTF-8, with no newline inside; flush after every
  line.
* stdin carries the editor's messages and stdout carries yours.
  * A stdout line that is not a JSON object goes to the output tab as text, so a stray
    `print` breaks nothing.
  * stderr goes to the output tab too.
* A request has an `id` and gets a reply with the same `id`; a notification has none and
  gets none.
* The editor answers a request it does not know with the error -32601. Unknown
  notifications and unknown fields are ignored on both sides.
* Reply to `initialize` within 30 seconds. Do the long work (a build, a connection) after
  that, and report it with `state`.

```
 editor                                                         plugin
   | -- initialize {session, editor, language, plugin, action} --> |
   | <-- the reply: the model ------------------------------------ |
   |                   the editor shows the controls; the session runs
   | -- control, event, input, save -----------------------------> |
   | <-- state, history, log, diagnostics ------------------------ |
   | <-- ask {text, choices} ------------------------------------- |
   | -- the reply {action, text} --------------------------------> |
   | -- shutdown (Stop, the tab closed, the editor closed) ------> |
   | <-- the reply {} --------------------------------------------  |  exit
```

The plugin may also end the session itself by exiting, for example when the agent has
finished. In every case the editor checks the exit code against `exit_ok` and takes the
`result`. Exit within 3 seconds of `shutdown`: after that the editor terminates the
process and kills it 3 seconds later, and such a session counts as cancelled.

A driven session, one message per line on the wire, wrapped here:

```
-> {"jsonrpc":"2.0","id":1,"method":"initialize","params":{"session":1,
    "editor":"1.1.0","language":"en","plugin":"hsm-to-python","action":"step"}}
<- {"jsonrpc":"2.0","id":1,"result":{"kind":"driven","steps":["event","input","time"],
    "speeds":[1,10,0],"events":["COIN","CANCEL"],"stream":true,"save":true}}
<- {"jsonrpc":"2.0","method":"state","params":{"status":"paused","active":["n1"],
    "time":0}}
-> {"jsonrpc":"2.0","method":"event","params":{"name":"COIN","params":{"amount":"5"}}}
-> {"jsonrpc":"2.0","method":"control","params":{"command":"step","unit":"event"}}
<- {"jsonrpc":"2.0","method":"log","params":{"text":"display: insert 1 more coin"}}
<- {"jsonrpc":"2.0","method":"history","params":{"event":"COIN","params":{"amount":"5"},
    "time":0,"origin":"user","outcome":"fired","fired":["n1-n2"]}}
<- {"jsonrpc":"2.0","method":"state","params":{"status":"paused","active":["n2"],
    "fired":["n1-n2"]}}
-> {"jsonrpc":"2.0","id":2,"method":"shutdown"}
<- {"jsonrpc":"2.0","id":2,"result":{}}
```

### The model

The reply to `initialize` describes the session, and the editor builds the controls from
it.

| Field | Kinds | Meaning |
|---|---|---|
| `kind` | all | `driven`, `supervised` or `observed`; required |
| `controls` | all | the buttons besides Stop, from `step`, `run`, `pause`, `restart`; by default all four for `driven`, `pause` and `run` for `supervised`, none for `observed` |
| `steps` | driven | the units of Step, the first by default: `event` (one event from the queue, handled to completion), `input` (the next stream item, after what falls due before it, with everything it causes), `time` (one tick of the clock); by default `["event"]` |
| `speeds` | driven | the speeds of Run: `1` real time, `N` N times faster on the virtual clock, `0` as fast as possible; by default `[1]` |
| `events` | driven, observed | the event names offered in the event line; without it there is no event line |
| `stream` | driven | `true`: Load Stream... and Edit Stream... |
| `save` | driven, observed | `true`: Save Log... |
| `logic` | driven | `async` (an event queue and a clock; the default) or `stream` (each input handled until nothing more happens, an end symbol, a verdict) |
| `end` | driven, `stream` | the end symbol, offered as a button and sent as an event (`EOF`) |
| `verdict`, `output` | driven, `stream` | `true`: the status shows the verdict, the session tab the output symbols |

```json
{"kind": "driven", "logic": "async", "steps": ["event", "input", "time"],
 "speeds": [1, 10, 0], "events": ["COIN", "CANCEL"], "stream": true, "save": true}
{"kind": "driven", "logic": "stream", "steps": ["input"], "controls": ["step", "run",
 "restart"], "events": ["0", "1"], "end": "EOF", "stream": true, "verdict": true,
 "output": true}
{"kind": "supervised"}
{"kind": "observed", "events": ["MOVE_COMPLETED", "TIMER_TICK"], "save": true}
```

The first is an HSM run step by step, the second a finite automaton on a tape, the third
an agent, the fourth a bridge to a robot.

### The editor's messages

| Method | Type | Params | What the plugin does |
|---|---|---|---|
| `initialize` | request | `{session, editor, language, plugin, action}` | replies with the model, then sends the first `state` |
| `control` | notification | `{command: "step", unit}` | one step of the unit; when it is done, a `state` whose status is not `running` |
| | | `{command: "run", speed}` | runs until Pause, the end of the machine or of the stream |
| | | `{command: "pause"}` | stops at the next step boundary and sends `state` `paused` |
| | | `{command: "restart"}` | starts over: the initial state, the stream from its first item, the clock at 0 |
| `event` | notification | `{name, params: {name: "value"}}` | posts the event (driven) or passes it on (observed); the values are strings as the user typed them |
| `input` | request | `{text}` or `{path}` | reads the stream and replies `{items: [{event, params?, time?}]}`; the stream replaces the previous one and the session restarts with it; a bad stream is the error -32602 with `data: [{line, message}]` |
| `save` | request | `{path}` | writes the inputs of this run (the stream items handled, the user's events, with their times) as a stream it can load again; replies `{}` |
| `shutdown` | request | | replies `{}` and exits |

### The plugin's messages

| Method | Type | Params | The editor |
|---|---|---|---|
| `state` | notification | the fields below | marks the canvas, updates the status and the controls |
| `history` | notification | `{event, params?, time?, origin, outcome, fired?, position?}` | adds a row to the history |
| `log` | notification | `{text, level?}` | the output tab; `warning` and `error` in the error colour |
| `ask` | request | `{text, choices?}` | shows the question; replies `{action: "accept", text}`, `{action: "decline"}` or `{action: "cancel"}` |
| `diagnostics` | notification | the array of the diagnostics file | replaces the session's findings |

| `state` field | Meaning |
|---|---|
| `status` | `running`, `paused`, `waiting` (for an answer, an event or the outside system), `finished` (the machine or the stream has ended; Restart may start over) |
| `text` | a short status of yours, e.g. `round 7, 23303 tokens` |
| `active` | the ids of the active states |
| `fired` | the ids of the last step: its transitions, the pseudostates passed, the state of an internal transition |
| `events` | the events the machine reacts to now; replaces the model's list in the event line |
| `time` | the clock, in seconds from the start |
| `position` | the index of the next stream item |
| `queue` | the pending events, in their order |
| `verdict` | `accepted` or `rejected` |
| `output` | the output symbols so far |

* A field present in a `state` replaces its earlier value, and a field left out keeps it;
  `[]` clears a list.
* `history` comes once per handled event, in order.
  * `origin` is one of `input` (the stream), `user` (the event line), `model` (the machine
    itself: timers, completion, its own events) or `system` (the outside system).
  * `outcome` is one of `fired`, `dropped` (nothing reacted), `deferred` or `rejected`
    (stream logic: the input is not accepted).
* `log` levels are `debug`, `info` (the default), `notice`, `warning` and `error`, the
  MCP logging names.
* Several questions may be open at once: the editor shows them one at a time, in order.
  * `decline` means the user refused to answer.
  * `cancel` comes for every open question when the user stops the session.
  * The shape follows MCP elicitation.

### Element ids

* `active`, `fired` and `history.fired` hold the ids of `${input}`. The editor ignores the
  ids it does not find.
* A runtime that uses names or ids of its own maps them back through the document:
  * an internal transition marks its state;
  * an element of an inlined submachine marks the submachine state.

### Steps and streams

The stream format is yours: the editor passes the text or the path and shows the items
you return. If your inputs are events, reuse the format of the `hsmd` dispatcher, which
the `hsm-to-python` session reads:

```
# timers: model                 <- or: recorded (the timeouts come from the log)
0.000   POWER_ON
0.512   button.CLICKED
+0.788  sensor.VALUE value=23.5 <- +delta from the previous line; an ISO time also works
SHOW                            <- no time: handled when the previous one is done
```

A worked example: at 0.1 s one tick of the `10-timer` diagram makes two timers due,
`t1.TIMEOUT` (priority 10) and `t2.TIMEOUT`.

| Unit | One Step does |
|---|---|
| `event` | `t1.TIMEOUT` in one step, `t2.TIMEOUT` in the next; each step sends a `history` and a `state` |
| `input` | both timeouts and the next stream item: everything due up to the item's time, then the item |
| `time` | the clock advances by one tick; the due events are handled one per `history` |

A finite automaton reads its tape one symbol per `input` step, sends `EOF` after the last
one, and reports `verdict` and `output`.

### Testing a session

The batch mode runs a session action from a script given by `--session-script`:

```
CyberiadaPro --batch coffee.graphml --plugin-path . --run-plugin hsm-to-python:step \
    --session-script coffee.session --dump-session
```

```
# coffee.session: the initial state, a coin, a step
expect-active n1
event COIN amount=5
step
expect-active n2
expect-fired n1-n2
stop
```

| Verb | Sends, waits |
|---|---|
| `step [UNIT]` | `control step`; waits for a state that is not `running` |
| `run [SPEED]`, `pause` | `control run`, `control pause` |
| `restart` | `control restart`; waits as `step` |
| `event NAME [name=value ...]` | `event` |
| `input FILE`, `save FILE` | `input {path}`, `save {path}`; an error reply fails the script |
| `wait STATUS [SECONDS]` | waits for the status; `ended` waits for the plugin to end the session |
| `answer TEXT`, `decline` | replies to the oldest open question, waiting for one |
| `expect-active ID ...`, `expect-fired ID ...` | the marks are exactly these ids, in any order |
| `expect-status STATUS`, `expect-verdict VERDICT` | the current status, the verdict |
| `stop` | `shutdown`, then waits for the exit |

* A wait lasts at most 30 seconds unless SECONDS is given.
* A failed `expect`, a timeout or a failed session exits with 6.
* A session action without a script is a usage error (1).
* `--dump-session` prints every message both ways, one line each; the editor's lines start
  with `sent`.
* An edit script holds the verbs itself: the lines after `plugin <id>:<session action>` up
  to `end`, and `--dump-session` prints them as `== session <n>`.

### A minimal session plugin

```
walker/
    plugin.json
    walker.py
```

```json
{
  "api": 1,
  "id": "walker",
  "name": "Walker",
  "interpreter": "python3",
  "actions": [
    { "id": "list", "label": "List the nodes", "result": "log",
      "argv": ["${plugin_dir}/walker.py", "${input}", "--list"] }
  ],
  "session": {
    "version": 1,
    "actions": [
      { "id": "walk", "label": "Walk the nodes",
        "argv": ["${plugin_dir}/walker.py", "${input}"] }
    ]
  }
}
```

```python
# walks the nodes of the diagram: every step marks the next one
import json
import sys
import xml.etree.ElementTree as ET

GRAPHML = '{http://graphml.graphdrawing.org/xmlns}'


def send(message):
    message['jsonrpc'] = '2.0'
    print(json.dumps(message), flush=True)


def main():
    nodes = [n.get('id') for n in ET.parse(sys.argv[1]).getroot().iter(GRAPHML + 'node')]
    if sys.argv[2:] == ['--list']:
        print('\n'.join(nodes))
        return
    step = 0
    for line in sys.stdin:
        message = json.loads(line)
        method, ident = message.get('method'), message.get('id')
        if method == 'initialize':
            send({'id': ident, 'result': {'kind': 'driven',
                                          'controls': ['step', 'restart']}})
        elif method == 'control':
            step = step + 1 if message['params']['command'] == 'step' else 0
        elif method == 'shutdown':
            send({'id': ident, 'result': {}})
            break
        else:
            if ident is not None:
                send({'id': ident, 'error': {'code': -32601, 'message': method}})
            continue
        active = [nodes[step % len(nodes)]] if nodes else []
        send({'method': 'state', 'params': {'status': 'paused', 'active': active}})


main()
```

Put `walker/` into the user plugin folder and choose Plugins > Walker > Walk the nodes;
F10 marks the next node.

### Session plugins

| Plugin | Kind, logic | The session | State |
|---|---|---|---|
| `hsm-to-python` "Run step by step..." | driven, async | the generated program on the `hsmd` dispatcher; posting events, timers on the virtual clock, replay of a stream, Save Log | available: hsm-to-python 2.1, hsm-dispatcher-py 0.2 (`hsmd.session`) |
| `cybersphere-agents` "Run the agent..." | supervised | the agent diagram marked as it runs; the agent's questions as `ask`; Pause before the next model request; `replaces` the one-shot run and its continuation | available: the session action `session` (`run --session`) |
| `git` "Commit..." | supervised | the branch and the staged files listed, the commit message asked as the answer, the index committed; `replaces` the one-shot commit with a message | available: cybersphere-git-plugin 0.1 (`commit --session`) |
| a finite automaton plugin | driven, stream | a tape of symbols, `EOF`, the verdict and the output (the FA extension, `type/fa`) | planned |
| `hsm-ros2` "Test with the fake robot...", `hsm-turtle` "Run the turtle..." | observed | the controller's steps relayed from its ROS 2 trace topic and marked on the diagram; Stop; `replaces` the one-shot test and run | available: hsm_generator (the `trace` parameter), hsm_tests (`--session`) |

## Examples

### A diagram checker over cgmlval

The checker of `cyberiadaml-compat-tests` writes its own JSON, so the plugin carries a
small adapter; the package puts the `cgmlval` module next to it.

```json
{
  "api": 1,
  "id": "cyberiada-checker",
  "name": "Diagram checker",
  "version": "0.1.0",
  "description": "Checks the document against CyberiadaML-GraphML (PNST 1044-2025)",
  "interpreter": "python3",
  "env": { "PYTHONPATH": "${plugin_dir}" },
  "actions": [
    { "id": "check", "label": "Check", "result": "diagnostics",
      "toolbar": true, "icon": "icons/check.svg",
      "argv": ["${plugin_dir}/check.py", "${input}", "${diagnostics}"] },
    { "id": "strict", "label": "Check (strict)", "result": "diagnostics",
      "toolbar": true, "icon": "icons/strict.svg",
      "argv": ["${plugin_dir}/check.py", "${input}", "${diagnostics}", "--strict"] }
  ]
}
```

```python
#!/usr/bin/env python3
"""The diagram checker plugin: cgmlval findings to editor diagnostics."""
import json
import re
import subprocess
import sys

ID = re.compile(r'\[id=([^\]]+)\]')


def main():
    graphml, diagnostics, options = sys.argv[1], sys.argv[2], sys.argv[3:]
    run = subprocess.run([sys.executable, '-m', 'cgmlval', 'validate', '--json']
                         + options + [graphml],
                         stdout=subprocess.PIPE, universal_newlines=True)
    if run.returncode not in (0, 1):    # 2: cgmlval could not read the file
        return 2
    entries = []
    for report in json.loads(run.stdout):
        for finding in report['findings']:
            entry = {'severity': finding['severity'].lower(),
                     'rule': finding['req'],
                     'message': finding['message'],
                     'line': finding['line']}
            ids = ID.findall(finding.get('path', ''))
            if ids:
                entry['element'] = ids[-1]
            entries.append(entry)
    with open(diagnostics, 'w', encoding='utf-8') as out:
        json.dump(entries, out, ensure_ascii=False, indent=1)
    print('{} finding(s)'.format(len(entries)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
```

The findings shown in The diagnostics file above are this adapter's output for the
editor test diagram `tests/diagrams/choice.graphml`.

### A ROS2 package generator

`hsm_robot_ros_generator` ships the `hsm-ros2` plugin in its `editor-plugin/`. The
workspace is remembered per diagram; generating and testing need a saved diagram with an
initial state and Python code, and the test runs as an observed session.

```json
{
  "api": 1,
  "id": "hsm-ros2",
  "name": "HSM ROS2",
  "version": "1.1.0",
  "interpreter": "bash",
  "requires": { "programs": ["bash"] },
  "parameters": [
    { "id": "setup", "type": "file", "label": "ROS 2 setup script", "scope": "persistent",
      "default": "/opt/ros/jazzy/setup.bash" },
    { "id": "ws", "type": "dir", "label": "Workspace", "scope": "document",
      "default": "${document_dir}/ros2_ws", "create": true, "required": true },
    { "id": "duration", "type": "int", "label": "Test duration, s", "scope": "document",
      "default": 30, "min": 1 }
  ],
  "actions": [
    { "id": "generate", "label": "Generate the package...", "result": "files",
      "outdir": "ws", "save": true, "meta": { "platformLanguage": "Python" },
      "when": ["saved", "sm", "!fa", "initial", "meta:platformLanguage=Python"],
      "parameters": ["setup", "ws"],
      "argv": ["${plugin_dir}/ros2_env", "--setup", "${param:setup}", "hsm_generator",
               "${input}", "-o", "${outdir}/src/hsm_controller", "-f", "-q"] },
    { "id": "test", "label": "Test with the fake robot...", "result": "diagnostics",
      "exit_ok": [0, 1], "timeout": 0, "save": true, "meta": { "platformLanguage": "Python" },
      "when": ["saved", "sm", "!fa", "initial", "meta:platformLanguage=Python"],
      "parameters": ["setup", "duration"],
      "argv": ["${plugin_dir}/ros2_env", "--setup", "${param:setup}", "--ros-exec",
               "hsm_tests", "hsm_robot_test", "${input}", "--json", "${diagnostics}",
               "--duration", "${param:duration}"] }
  ],
  "session": {
    "version": 1,
    "actions": [
      { "id": "test-live", "label": "Test with the fake robot...", "result": "diagnostics",
        "exit_ok": [0, 1], "replaces": ["test"], "save": true,
        "meta": { "platformLanguage": "Python" },
        "when": ["saved", "sm", "!fa", "initial", "meta:platformLanguage=Python"],
        "parameters": ["setup", "duration"],
        "argv": ["${plugin_dir}/ros2_env", "--setup", "${param:setup}", "--ros-exec",
                 "hsm_tests", "hsm_robot_test", "${input}", "--json", "${diagnostics}",
                 "--duration", "${param:duration}", "--session"] }
    ]
  }
}
```

The real descriptor adds a `check` action (`sm`, `!fa`: it reports a missing initial state),
a `build` action (`saved`) and the start pose and the scan of the fake robot. The bash
adapter `ros2_env` sources `${param:setup}`, because the editor is usually started without
ROS 2. In `test-live` the controller publishes its steps on `/hsm_ros_trace` in the ids of
the diagram and the runner relays them (`hsm_robot_ros_generator/docs/editor_plugin.md`);
`hsm-turtle` of `turtle_driver` drives turtlesim the same way, its `run-live` bringing back
the path of the turtle.

### An LLM diagram maker

`cybersphere-hsm-maker` draws a new diagram from a description and changes the open one
by a description of the change; its `editor-plugin/` holds the descriptor. The texts go
through files, the key through an exported variable the maker reads.

```json
{
  "api": 1,
  "id": "hsm-maker",
  "name": "HSM Maker",
  "version": "0.1.0",
  "description": "Draws a new diagram from a description or changes the open one with a language model; the description and the diagram are sent to the model server",
  "interpreter": "python3",
  "env": { "PYTHONUTF8": "1" },
  "parameters": [
    { "id": "prompt", "type": "text", "label": "Description", "scope": "run",
      "required": true, "language": "markdown" },
    { "id": "change", "type": "text", "label": "What to change", "scope": "run",
      "required": true, "language": "markdown" },
    { "id": "layout", "type": "enum", "label": "Layout", "scope": "run",
      "choices": ["update", "full"], "default": "update" },
    { "id": "model", "type": "string", "label": "Model", "scope": "persistent",
      "required": true, "default": "deepseek-flash" },
    { "id": "url", "type": "string", "label": "Server", "scope": "persistent",
      "required": true, "default": "https://api.deepseek.com/v1" },
    { "id": "key", "type": "secret", "label": "API key", "scope": "persistent",
      "default": "env:DEEPSEEK_API_KEY", "export": "HSM_MAKER_API_KEY" }
  ],
  "actions": [
    { "id": "draw", "label": "Draw a new diagram...", "result": "new-document",
      "toolbar": true, "icon": "icons/draw.svg", "timeout": 600,
      "load": { "strict": true },
      "parameters": ["prompt", "model", "url", "key"],
      "argv": ["-m", "hsm_maker", "-f", "${param-file:prompt}", "-o", "${output}",
               "--diagnostics", "${diagnostics}",
               "--model", "${param:model}", "--url", "${param:url}"] },
    { "id": "change", "label": "Change the diagram...", "result": "replace",
      "toolbar": true, "icon": "icons/change.svg", "timeout": 600,
      "load": { "strict": true },
      "parameters": ["change", "layout", "model", "url", "key"],
      "argv": ["-m", "hsm_maker", "--update", "${input}", "-f", "${param-file:change}",
               "--geometry", "${param:layout}", "-o", "${output}",
               "--diagnostics", "${diagnostics}",
               "--model", "${param:model}", "--url", "${param:url}"] }
  ]
}
```

The real descriptor adds the protocol, the language and the rounds as `persistent`
parameters and a `notation` action (`result` `log`) that prints the open diagram in the
notation the model reads. `change` keeps the identifiers and the drawing of what it does
not change, and its diagnostics list every change with the element. The maker exits 0
when the diagram is made, 1 when it is not, 2 on a usage error and 3 on a server error;
only 0 is success, and its stderr explains the rest.

### A server-based compiler

`lapki-compiler` is a WebSocket server, so its plugin is an adapter: it takes the server
address from a `persistent` parameter, sends `${input}` to `ws://<server>/main`, writes
the returned sources and binaries into `${outdir}`, prints the build logs and exits
non-zero on a failed build. The platform comes from the document meta
(`${meta:platform}`). An adapter that starts a local server stops it before it exits.

## A checklist

* No windows, no questions on stdin: declare parameters; a session asks with `ask`.
* Write only the declared outputs; leave `${input}` and the plugin directory alone.
* UTF-8 on stdout and stderr; one clear line per error.
* Exit codes that mean something, and `exit_ok` that lists the good ones.
* Declare in `requires` and `when` what an action needs, so it is greyed out, not failing.
* Stop every process you started.
* Never print a secret.
* Say in `description` when the document leaves the machine.
* Keep `i18n/<lang>.json` in step with the English texts: a changed text loses its
  translation until the file follows.
* A session: reply to `initialize` at once, flush every line, send a `state` after each
  step, and exit after `shutdown`.
