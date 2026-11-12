# The Plugin Polygon

The plugin polygon extends the test polygon (`POLYGON.md`) from editing diagrams to the
editor's plugins (`PLUGINS.md`, `PLUGIN-API.md`). A plugin run is one more line of the
accumulated script, so the script stays the reproduction. The external state a plugin works on,
such as a git repository or a model API, is its **world**, and it is rebuilt for every editor
run. The oracles judge a run by the plugin requirements of `EDITOR-SPEC` §4.12 and by the
invariants the plugin declares in its **profile**. The LLM takes both roles:

* the **tester** drives the plugins through long edit histories;
* a plugin driven by a model is the **subject**: its model answers are recorded and replayed,
  so its runs reproduce.

```
  <plugin repo>/editor-plugin/                tests/polygon/polygon/
 +--------------------------------+          +--------------------------------+
 | plugin.json   the descriptor   | -------> | plugins/profile.py             |
 | polygon.json  the profile      |   read   | the profile, or one derived    |
 +--------------------------------+          | from the descriptor            |
                                             +--------------------------------+
                                                world   |        | actions, values,
                                                script  |        | invariants, model
                                                        v        v
 +--------------------------------+          +--------------------------------+
 | worlds/   fs | git | model     | <------- | producers                      |
 | a fresh copy for every run;    |  build   | drill --which plugin           |
 | model: the traffic proxy       |          | fuzz --plugin                  |
 +--------------------------------+          | run --mission plugin    (LLM)  |
              | folder, env, url             | plugin-campaign  (LLM plugin)  |
              v                              +--------------------------------+
 +----------------------------------------------------------------+     |
 | Round: the world + start.graphml + the accumulated script      | <---+ script
 |  tab run      --dump --dump-tabs --dump-actions                |
 |  diagram run  [+ switch <diagram tab>]: tier 1, laws, render   |
 +----------------------------------------------------------------+
              | dumps, world facts
              v
 +----------------------------------------------------------------+
 | oracles: the plugin laws, the profile invariants, tiers 1-3    | ---> register P-<n>/
 +----------------------------------------------------------------+
```

## The batch verbs

The editor runs plugins from the edit script (`EDITOR-SPEC` `EDIT-DOC-11`,
`EDIT-PLUG-61..64`). One editor run can edit, save, run plugins and edit again:

```
rename n1 Ready
save
plugin git:stage
plugin git:commit message="rename the idle state"
plugin git:compare revision=HEAD~1
switch 0
plugin polygon-world:do verb="write diagram.graphml @start"
plugin git:stage
plugin git:commit-session
  answer restore the start document
  wait ended
end
```

| Line | Effect |
|---|---|
| `save [<file>]` | Save, or Save As `<file>`; afterwards the `saved` token holds |
| `plugin <id>[:<action>] [<name>=<value> ...]` | Runs the action on the active tab and waits for it. The values use the `--plugin-param` syntax and are double-quoted when they hold spaces. An action that is refused is a script error that carries the reason. A failed run does not stop the script: it is dumped as `state: failed`, and the batch exits 6 at the end. |
| `plugin <id>:<session action>` ... `end` | The lines up to `end` are the session verbs of `--session-script`; inside the block, `save FILE` is the session's own verb |
| `--dump-actions` | `== actions`: one line per action on the active tab, `<id>:<action> enabled` or `<id>:<action> disabled: <reason>` |
| `--dump` | `== plugin <n>`, `== diagnostics <n>` and `== session <n>` for the n-th run of the script, whatever the active tab |

## The world

An editor run starts in a fresh copy of the world, so a plugin's side effects (a commit, a
written file) cannot leak into the next run. The world is built once per round from the
**world script** into a template, and every run of the round copies that template.

```
  world script                          the round                     a run
 +----------------------------+        +---------------------+       +----------------------+
 | the profile's history      | -----> | template/           | ----> | <run>/world/         |
 | write diagram.graphml      |  build |   diagram.graphml   |  copy |   diagram.graphml    |
 |   @start                   |  once  |   .git/             |  each |   .git/              |
 | commit init                |        |                     |  run  | <run>/home/          |
 +----------------------------+        +---------------------+       +----------------------+
                                                                       env: HOME, XDG_*, GIT_*,
                                                                       POLYGON_WORLD, profile env
```

| Kind | Holds | World verbs | World facts |
|---|---|---|---|
| `fs` | a folder holding the document, so `save`, `cwd: document` and the `file` token work | `write <file> @start\|<text>`, `remove <file>` | `file <name>`, `absent <name>` |
| `git` | an `fs` world inside a repository: a private configuration and a fixed author, committer and dates, so the hashes are stable | the `fs` verbs and `commit <message>`, `stage <file>`, `branch <name>`, `checkout <ref>`, `tag <name>`, `reset` | `clean`, `staged <file>`, `modified <file>`, `head-message <text>`, `commits <k>`, `branch <name>` |
| `model` | the traffic proxy of a model API (below) | - | `requests <k>` |

* **The document.** `@start` is the start document of the round, written into the world under
  the profile's `document` name (`diagram.graphml` by default). The editor opens it from the
  world, so the `git` and `saved` tokens hold as they do for a user.
* **World facts.** They are read from the world of the run after it ends.
* **Changing the world mid-script.** The helper plugin `polygon-world`
  (`tests/polygon/plugins/`) changes the world in the middle of a script: its action `do` runs
  one world verb on the folder named by `POLYGON_WORLD`. This is how the world changes behind
  the editor's back, as a terminal user would change it.
* **The environment.** A run overrides the ambient variables:
  * `HOME`, `XDG_CONFIG_HOME`, `XDG_DATA_HOME` and `XDG_RUNTIME_DIR` point into the run;
  * `CYBERIADA_PRO_PLUGIN_PATH` is empty;
  * the `GIT_` variables give the private configuration;
  * `--plugin-path` names the plugin under test and the helper.

## The profile

`editor-plugin/polygon.json` sits beside `plugin.json` in the plugin repository and is
versioned with the plugin. The editor never reads it and it is not installed with the plugin.

```json
{ "polygon": 1,
  "env": { "PYTHONPATH": "${profile_dir}/.." },
  "world": { "kind": "git", "document": "diagram.graphml",
             "history": ["write diagram.graphml @start", "commit init"] },
  "parameters": { "revision": { "valid": ["HEAD", "HEAD~1"], "invalid": ["nope"] },
                  "message": { "valid": ["one line"], "invalid": [""] } },
  "invariants": [
    { "name": "commit-then-compare-is-empty",
      "script": ["rename n1 Ready", "save", "plugin git:stage",
                 "plugin git:commit message=x", "plugin git:compare revision=HEAD"],
      "expect": ["plugin-state 3 done", "diff-changes 0", "world clean"] } ],
  "brief": "../docs/API.md" }
```

| Field | Meaning |
|---|---|
| `polygon` | the format version, 1 |
| `env` | the environment the plugin needs when it runs from its repository; `${profile_dir}` is the profile's folder |
| `world` | `kind` (`fs`, `git`), `document` (the document's file name in the world), `history` (the world script) |
| `parameters` | per parameter id, the `valid` and `invalid` values the producers try; a secret is never listed, it is always the dummy |
| `invariants` | named checks: a `script` run in a fresh world and the `expect` facts that must hold afterwards |
| `model` | for an LLM plugin, the parameters that carry the proxy (`url`, `model`, `key`) and the `env` it needs, where `{url}` is the proxy's address |
| `campaign` | for an LLM plugin as the subject: the `draw` and `change` actions with their prompt parameter, and the `quality` argv run on a made diagram and a reference one, which prints a JSON score |
| `brief` | a file of the repository, relative to the profile, that the tester reads as the plugin's specification |

A plugin without a profile gets a derived one:

* an `fs` world;
* the descriptor's values: the default, the enum values, `true`/`false`, the empty string and
  one invalid value for each type;
* no invariants.

The availability matrix of the drill still applies.

hsm-maker's profile, for example, carries
`"model": { "url": "url", "model": "model", "key": "key" }`. A Pydantic AI plugin carries
`"model": { "model": "model", "key": "key", "env": { "OPENAI_BASE_URL": "{url}" } }`.

## The round

A round whose script holds no tab verb (`plugin`, `compare`, `new-document`, `open`, `switch`,
`close`, `mode`) runs as in `POLYGON.md`, in a fresh world. A round with one is split, because
a diff or a text tab refuses the diagram dump flags (`EDIT-MODE-16`):

```
  template --copy--> tab run       script  --dump --dump-tabs --dump-actions
                                    == tabs, == actions, == plugin <n>, == diagnostics <n>,
                                    == diff | == text | == document, == scene
                                    -> the plugin laws, the facts, the world facts
  template --copy--> diagram runs  script [+ switch <i>]  -> the tiers of POLYGON.md
                                    <i>: the last diagram tab of == tabs when the active tab
                                    is not a diagram; with no diagram tab the tiers are skipped
```

The undo-all rerun counts the undo steps of `== stack`, not the script lines, because the
`plugin` and `save` lines push no step of their own.

## Facts

The plugin facts extend the expectation language (`POLYGON.md`). They serve the drill
invariants, the profile invariants and the tester's `expect`.

| Fact | Holds when |
|---|---|
| `plugin-state <n> done\|failed` | the n-th run of the script ended so |
| `plugin-exit <n> <code>` | the plugin of the n-th run exited with the code |
| `available <id:action>` / `refused <id:action>` | `== actions` lists the action as enabled / disabled |
| `tab <i> mode <m>` | tab `<i>` of `== tabs` is in mode `<m>` |
| `diff-changes <k>` | the active diff tab lists `<k>` changed pairs |
| `diff-has <id>` | the active diff tab lists a change of the element |
| `diag-count <n> <k>` | the n-th run reported `<k>` findings |
| `world <fact>` | the world fact holds after the run |

## Oracles and laws

| Check | Requirement | Kind | Signature |
|---|---|---|---|
| a failed run leaves the document as the script left it before that line | `EDIT-PLUG-18` | `oracle` | `plugin-failed:<id:action>:<field>` |
| `undo` after a `replace` run restores the document exactly | `EDIT-PLUG-13` | `oracle` | `plugin-undo:<id:action>:<field>` |
| a `replace` run without a change pushes no undo step | `EDIT-PLUG-12` | `law` | `law:PLUG-12:<id:action>` |
| the dummy secret appears in no output, no dump and no saved file | `EDIT-PLUG-10` | `law` | `law:PLUG-10:<where>` |
| `== plugin <n>` and `== actions` name no machine path (the world, the home, the run directory) | `EDIT-PLUG-28/64` | `law` | `law:PLUG-28:<line>` |
| the availability in `== actions` is the one the descriptor predicts for the tab | `EDIT-PLUG-52..56` | `invariant` | `invariant:available <id:action>` |
| a run with valid values only does not fail | the profile | `plugin` | `plugin:<id:action>:exit:<code>` |
| two fresh worlds give equal plugin sections and world facts | determinism | `oracle` | `plugin-rerun:<n>:<field>` |
| a profile invariant holds | the profile | `invariant` | `invariant:<name>:<fact>` |
| a result tab (`new-document`, `replace`) passes tier 1, the strict reload and the standing laws | `POLYGON.md` | as there | as there |

* **The `plugin` kind** is a defect of the plugin, not of the editor. Its reproduction keeps the
  plugin's stderr.
* **A failed run with an invalid value** is the expected outcome. Only its document identity
  (`EDIT-PLUG-18`) is judged.

## The traffic proxy

An LLM plugin reaches its model through the proxy, `polygon/worlds/model.py`, which is written
on the standard library only. The plugin gets the proxy's address and a dummy key; the real key
stays in the proxy.

```
  editor ---> plugin ---- url=http://127.0.0.1:<port>/v1, key=env:POLYGON_PROXY_KEY ----> proxy
                                                                                            |
     live     forward with the real key, record the exchange   ---------------------->  model API
     replay   answer from the cassette, no network, no key
     fault    a timeout, 429, 500, a truncated or a malformed answer
```

* **Endpoints.** The proxy passes through the chat completions (`/v1/chat/completions`) and the
  messages (`/v1/messages`) protocols, streamed answers included.
* **The cassette.** An exchange is stored as `cassette/<key>-<k>.json`: the normalised request,
  the status, the content type, and the body or the stream chunks. `<key>` is a hash of the
  request body with its keys sorted, and `<k>` counts the repeats of the same request.
* **In a round.** The first run goes in the session's mode, so a live session records it. The
  reruns replay, which keeps the identity oracles of tier 1 meaningful. A request missing from
  the cassette gets an error answer and the signature `cassette-miss`. During minimisation a
  miss counts as "not reproduced".
* **Keys and faults.**
  * The real key comes from the backend of `polygon.toml` (`key_env`, `key_file`) and never
    enters the editor's environment.
  * The dummy is the secret watched by the `EDIT-PLUG-10` law.
  * `fault:<kind>` drives `EDIT-PLUG-18/20`.

## Producers

### The drill

`drill --which plugin --plugin <id>` is deterministic and needs no model. Its rounds escalate:

1. **Availability.** The matrix of every mode of the descriptor, plus one mode outside it, by
   every `when` token held and not held. The world and the script make each token: `saved` by a
   `save` or by an edit after it, `git` by a `git` or an `fs` world, `file` by the world
   document or an untitled one. `== actions` is checked against the prediction.
2. **Valid runs.** Every action with its valid values, pairwise over the domains, judged by the
   oracles.
3. **Invalid runs.** The invalid values: the run fails and the document stays untouched.
4. **Invariants.** The profile invariants.

### The fuzzer

`fuzz --plugin <id>` mixes the fuzzer's groups with plugin lines and with the world verbs of the
helper. A plugin line is an available action with values from the domains. The rounds are
judged by the reference-free oracles and the plugin laws.

### The tester

`run --mission plugin --plugin <id>` is the LLM tester. The mission diagram
`diagrams/plugin.graphml` follows the `hunt` pattern:

```
 machine Plugin
 * -> Editing: entry/ tools.allow(*focus.families); prompt.set('plugin')
 Editing  --document.STARTED--> Applying --document.EDITED--> Editing | Checking   (as hunt)
 Applying --document.REJECTED/ prompt.add(document.summary)--> Editing            (a refusal)
 Checking --oracle.FOUND [new]--> Confirm --> ...                                 (as hunt)

 machine Focus     Actions -> Conditions -> Results -> Sessions -> World
 machine Watchdog  (as hunt)
```

* **The areas.**
  * `actions`: every action with its values;
  * `conditions`: `EDIT-PLUG-50..56`;
  * `results`: `EDIT-PLUG-12..19` and `57..59`;
  * `sessions`: `EDIT-PLUG-36..49`;
  * `world`: the plugin against a world changed by the helper.
* **The `plugin` family.** The tester sees the editing families and the `plugin` family: the
  `plugin` verb, the session block, `save`, `switch`, `compare`, `refresh`, `close` and
  `plugin polygon-world:do`. The family's description is generated from the descriptor and the
  profile: the actions, their parameters, conditions and results, and the brief.
* **The `Plugin` component.** It raises `RAN(n, action, state)` and keeps `runs`, `failed` and
  `last`.
* **An LLM plugin in the session.** When the plugin under test is an LLM plugin, the tester
  calls it through the proxy, so one session has both roles.

### The campaign

`plugin-campaign --plugin <id> --backend <b> [--story <name>]` makes an LLM plugin the subject:

```
 story (catalog/stories.json) --prompt--> plugin <draw> --proxy, record--> the made tab
   the made tab --> tier 1, strict reload, standing laws           --> defects, the register
                --> the quality argv against the story's reference --> plugin-quality.json
                --> the story's counts as facts                     --> candidates
   change prompts --> plugin <change> --> the same judgement
```

* **The backend** is the model behind the proxy. The backends are compared per plugin in
  `plugin-quality.json`: runs, made, defects, the mean quality and the mean `C`, requests and
  tokens.
* **Replay.** `plugin-campaign --replay <session>` replays a campaign's cassettes without a key.

## Register

A plugin problem's reproduction (`problems/P-<n>/`) adds these files to those of `POLYGON.md`:

* `world`, the world script;
* `cassette/`, for an LLM plugin;
* `profile`, the plugin's id and version and the digests of `plugin.json` and `polygon.json`.

`check` and the minimisation rebuild the world for every run and replay the cassette. The
minimisation drops script lines, never world lines.

## Nightly

The polygon step of `NIGHTLY.md` covers every plugin of the stack that has a profile:

* `drill --which plugin` and `fuzz --plugin` run daily;
* `run --mission plugin` takes its turn with the other missions;
* `plugin-campaign` replays the kept cassettes, and runs live only with a configured backend.

## Layout

```
tests/polygon/
  polygon/plugins/profile.py    the profile, the derived profile, the plugin facts
  polygon/worlds/               fs.py, git.py, model.py (the traffic proxy)
  polygon/drills/plugin.py      the plugin drill
  polygon/campaign.py           plugin-campaign
  plugins/polygon-world/        the helper plugin: plugin.json, world.py
  diagrams/plugin.graphml       the tester's mission
  plugin-quality.json           local, not committed
```

`polygon.toml` names the plugin folders as `[plugins] path = [...]`; `--plugin-path`
replaces them.

## Running

```
cd tests/polygon
./run-polygon.sh drill --which plugin --plugin git --plugin-path ../../../cybersphere-git-plugin
./run-polygon.sh fuzz --plugin git --rounds 30 --seed 1
./run-polygon.sh run --backend deepseek --mission plugin --plugin git --seed 1
./run-polygon.sh plugin-campaign --plugin hsm-maker --backend deepseek --story keyboard
./run-polygon.sh check --problem P-12
```
