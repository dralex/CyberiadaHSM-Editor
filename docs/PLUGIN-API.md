# The Plugin Interface

**Document version:** 0.2 (2026-10-14), plugin contract `api` 1 — a draft: the editor does
not implement it yet, and the contract may change until it does.

This is the brief for a plugin author. A plugin gives the Cyberiada HSM Editor a new
action — check a diagram, generate code, compile, draw — without changing the editor. The
plugin is usually written and packaged by the authors of the tool it runs, in the tool's
own repository; anyone may also package a plugin for a tool they do not own, or put one
into their user plugin folder. How the editor works inside is in `PLUGINS.md`.

A plugin is a directory with a `plugin.json` descriptor. For each run the editor writes a
copy of the active document into a temporary run directory, starts the declared command
there, waits for it and takes the result the action declares.

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
Plugins menu gets the entry "Document size"; a click prints the size into the Output tab
of the plugin dock.

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
| `actions` | yes | at least one action |
| `session` | no | reserved for the level 2 session protocol; ignored by `api` 1 |

An action:

| Field | Default | Meaning |
|---|---|---|
| `id` | required | the action id, unique in the plugin; `--run-plugin <plugin>:<action>` |
| `label` | required | the menu text; end it with `...` when the action asks for input |
| `result` | required | `replace`, `new-document`, `diagnostics`, `files` or `log` (see Results) |
| `argv` | required | the command: a list of strings with variables, and conditional elements (see The command line) |
| `exit_ok` | `[0]` | the exit codes of a successful run |
| `timeout` | `60` | seconds; `0` means none |
| `parameters` | all | the ids of the parameters this action uses and shows |
| `outdir` | none | the id of the `dir` (or `file`) parameter that receives the files; required for a `files` action |
| `load` | `{}` | `strict`: reject an output that is not strictly conforming; `reconstruct`: lay out an output without geometry |
| `save` | `false` | save a modified or untitled document before the run, so `${document}` is current (an untitled one asks for a name) |
| `cwd` | `"run"` | the working directory: `run` (the run dir) or `document` (the document's directory; implies `save`) |
| `toolbar` | `false` | put the action on the plugin toolbar |
| `icon` | the plugin icon | the action icon for the toolbar and the menu |
| `description` | none | the tooltip |

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
* **The plugin dock** — the Output tab shows stdout and stderr of every run, the
  Diagnostics tab the findings; the running plugin has a Cancel button.

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
| ROS2 package - Make the package              [x] |
+--------------------------------------------------+
| Package directory [~/work/light/ros      ] [...] |
|                                 [Cancel]  [Run]  |
+--------------------------------------------------+
```

The plugin itself opens no windows and asks nothing on stdin: everything it needs is
declared as parameters, so the same action also runs in the batch mode.

## The command line

The command is `interpreter + argv`, or `argv` alone; without an interpreter `argv[0]` is
a program relative to the plugin directory, or found in `PATH`; on Windows a program
without a suffix also matches `<name>.exe`, and a `.bat` or `.cmd` file cannot be one.
There is no shell: each list element is one argument, quoting is never needed, and a
substituted value is never expanded again. stdin is the null device. The environment is
the editor's, plus `env`, plus the exported secrets.

An argv element may be conditional: `{"if": "<param>", "argv": [...]}` adds its strings
when the parameter is `true` (a bool) or not empty (any other type), and nothing
otherwise.

```json
"argv": ["${plugin_dir}/check.py", "${input}", "${diagnostics}",
         {"if": "strict", "argv": ["--strict"]}]
```

| Variable | Value |
|---|---|
| `${input}` | `<rundir>/input.graphml`: the active document as CyberiadaML-GraphML 1.0, unsaved edits included; an empty file for an empty document |
| `${output}` | `<rundir>/output.graphml`: where a `replace` or `new-document` action writes the document |
| `${diagnostics}` | `<rundir>/diagnostics.json`: where findings go |
| `${outdir}` | the value of the action's `outdir` parameter; the run dir when there is none |
| `${rundir}` | the temporary run directory; removed after the run |
| `${plugin_dir}` | the plugin directory |
| `${request}` | `<rundir>/request.json`, written only when the argv uses it |
| `${param:x}` | the value of the parameter `x`; a bool is `true` or `false` |
| `${param-file:x}` | a run dir file holding the value of `x`; use it for long texts such as prompts |
| `${meta:k}` | a document meta value: `platform`, `platformVersion`, `platformLanguage`, `markupLanguage`, `name`, ...; empty when absent |
| `${selection}` | the ids of the selected elements in the document order, comma-separated |
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
`yed-berloga`); the input is always CyberiadaML-GraphML 1.0. `meta` holds the document
metainformation strings except the editor's own view state. Secrets are never in it.

## Results

| `result` | The plugin writes | The editor |
|---|---|---|
| `replace` | the whole changed document to `${output}` | replaces the document as one undo step "Plugin: <label>" |
| `new-document` | a document to `${output}` | opens it as a modified untitled document: in the active tab when that is an empty untitled one, in a new tab otherwise |
| `diagnostics` | `${diagnostics}` | shows the findings |
| `files` | files into `${outdir}` (the action's `outdir` parameter) | lists the new and changed files in Output |
| `log` | stdout, stderr | nothing more |

* The output document may be CyberiadaML-GraphML 1.0 or a yEd dialect the library reads;
  the document keeps its own save format.
* An output without geometry is accepted with `load.reconstruct`: the editor lays it out.
  Without it, write the geometry.
* A `replace` output keeps the ids of the elements it does not change, so the selection
  and the findings still point at them. An output equal to the input changes nothing.
* An output that cannot be read, or an empty one, fails the run; the document stays as it
  was.
* Any action may also write `${diagnostics}`; a `replace` action may report what it
  changed this way.
* A `replace` action is disabled while the editor is in the inspection mode, and the
  document is locked while it runs.

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
| `line` | no | a line of `${input}` |

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
    "export": "HSM_MAKER_KEY" }
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
* A secret is resolved at the start of a run — the variable `NAME` of the editor's
  environment, or the first line of the file `PATH` — and exported to your process as the
  `export` variable. It never reaches the argv, `request.json`, the Output or the
  settings. Do not print it.

## Exit codes, errors, cancel

* The run succeeds when the exit code is in `exit_ok` and the declared output is there
  and readable. Anything else fails the run: the user sees the exit code and the end of
  stderr, the document is untouched.
* Write progress and messages to stdout and errors to stderr, in UTF-8; they appear in
  the Output tab as the run goes, up to 1 MiB per run. A Python plugin on Windows sets
  `PYTHONUTF8=1` in `env` to write UTF-8 into a pipe.
* A timeout or Cancel terminates the process (SIGTERM on Unix) and kills it 3 seconds
  later; on Windows expect the kill. Only your process is stopped: stop the processes
  you started yourself, such as a local server.
* `CYBERIADA_EDITOR_PLUGIN_KEEP=1` in the editor's environment keeps the run directories
  for debugging.

## Testing a plugin

The batch mode runs an action without the window and without dialogs; the values are the
defaults and the `--plugin-param` options.

```
# in the tool repository: the folder . holds editor-plugin/plugin.json
CyberiadaEditor --batch --plugin-path . --list-plugins
CyberiadaEditor --batch light.graphml --plugin-path . \
    --run-plugin hsm-ros2:generate --plugin-param dir=/tmp/ros --dump
CyberiadaEditor --batch --plugin-path . --run-plugin hsm-maker:draw \
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

Without a document on the command line the run starts from an empty one, which suits a
`new-document` action. `--dump` adds the `== plugin` section (the action, the state, the
exit code, your stdout, the files) and the `== diagnostics` section; without a dump your
stdout goes to the editor's stdout. stderr always goes to the editor's stderr. The
folders are named by their origin (`plugin-path 1`, `env 1`, `user`, `system`), never by
a path, so the output is the same on every machine. The exit code is 0 for a successful
run (findings or not), 1 for a usage error (an unknown plugin, a missing or invalid
value) and 6 for a failed run.

## Packaging

Install the plugin directory as is:

| System | Folder |
|---|---|
| Linux, system | `<prefix>/share/cyberiada-editor/plugins/<id>/` — the editor's prefix, `/usr` for the Debian package |
| Linux, user | `~/.local/share/Cyberiada/Cyberiada State Machine Editor/plugins/<id>/` |
| Windows, the editor zip | `<editor folder>\share\cyberiada-editor\plugins\<id>\` |
| Windows, user | `%APPDATA%\Cyberiada\Cyberiada State Machine Editor\plugins\<id>\` |
| macOS, the bundle | `CyberiadaEditor.app/Contents/share/cyberiada-editor/plugins/<id>/` |

* Keep the directory in the tool repository as `editor-plugin/` and install it from the
  tool's package, or from a small separate package (`<tool>-editor-plugin`) when the tool
  should not know about the editor.
* A Debian package declares `Enhances: cyberiada-editor` and depends on what the plugin
  runs (`python3`, the tool).
* A user plugin overrides a system plugin with the same `id`.
* An upgrade replaces the directory. The stored values survive while the parameter ids
  stay the same; a renamed parameter starts from its default.
* Raise `api` only when the editor documents a new major; a new editor keeps running the
  majors it lists.

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

`hsm_robot_ros_generator` takes a diagram and an output directory; no adapter is needed.
The directory is remembered per diagram.

```json
{
  "api": 1,
  "id": "hsm-ros2",
  "name": "ROS2 package",
  "description": "Generates the hsm_controller ROS 2 package; needs ROS 2 and pysm",
  "interpreter": "python3",
  "parameters": [
    { "id": "dir", "type": "dir", "label": "Package directory", "scope": "document",
      "default": "${document_dir}/ros", "create": true, "required": true }
  ],
  "actions": [
    { "id": "generate", "label": "Make the package...", "result": "files",
      "outdir": "dir",
      "argv": ["${plugin_dir}/hsm_generator.py", "${input}", "-o", "${outdir}", "-f"] }
  ]
}
```

A tool that reads more files next to the diagram — `hsm-to-python` inlines the files the
diagram names — sets `"cwd": "document"` and takes `${document}` instead of `${input}`.

### An LLM diagram maker

`cybersphere-hsm-maker` makes a new diagram from a description. The description goes
through a file, the key through an exported variable.

```json
{
  "api": 1,
  "id": "hsm-maker",
  "name": "HSM Maker",
  "version": "0.1.0",
  "description": "Draws a new diagram from a description; sends it to an LLM service",
  "interpreter": "python3",
  "parameters": [
    { "id": "prompt", "type": "text", "label": "Description", "scope": "run",
      "required": true, "language": "markdown" },
    { "id": "model", "type": "string", "label": "Model", "scope": "persistent",
      "default": "deepseek-flash" },
    { "id": "url", "type": "string", "label": "Server", "scope": "persistent",
      "default": "https://api.deepseek.com/v1" },
    { "id": "key", "type": "secret", "label": "API key", "scope": "persistent",
      "default": "env:DEEPSEEK_API_KEY", "export": "HSM_MAKER_KEY" }
  ],
  "actions": [
    { "id": "draw", "label": "Draw a new diagram...", "result": "new-document",
      "toolbar": true, "icon": "icons/draw.svg", "timeout": 300,
      "argv": ["-m", "hsm_maker", "-f", "${param-file:prompt}", "-o", "${output}",
               "--model", "${param:model}", "--url", "${param:url}",
               "--key-env", "HSM_MAKER_KEY"] }
  ]
}
```

The maker exits 0 when the diagram is made, 1 when it is not, 2 on a usage error and 3 on
a server error; only 0 is success, and its stderr explains the rest.

### A server-based compiler

`lapki-compiler` is a WebSocket server, so its plugin is an adapter: it takes the server
address from a `persistent` parameter, sends `${input}` to `ws://<server>/main`, writes
the returned sources and binaries into `${outdir}`, prints the build logs and exits
non-zero on a failed build. The platform comes from the document meta
(`${meta:platform}`). An adapter that starts a local server stops it before it exits.

## A checklist

* No windows, no questions on stdin: declare parameters.
* Write only the declared outputs; leave `${input}` and the plugin directory alone.
* UTF-8 on stdout and stderr; one clear line per error.
* Exit codes that mean something, and `exit_ok` that lists the good ones.
* Stop every process you started.
* Never print a secret.
* Say in `description` when the document leaves the machine.
