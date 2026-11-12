# The Nightly Run

`tests/nightly/run-nightly.sh` is the unattended run of the whole test suite of
the editor: it updates the development stack, builds the current `devel` out of
the source tree, runs every ctest tier (with the polygon tier, see `TESTING.md`)
and the polygon itself (`POLYGON.md`), the LLM missions included. It only
records: the problems go to the polygon register, the run to a report; it never
fixes, never changes the status of a problem, never re-records a good file,
never commits a fix and never pushes.

```
 cron ──> run-nightly.sh ── copy to <work>, re-exec, flock <work>/nightly.lock
            │
            │ 1 update   <sources>/update-dev-stack.sh -y -u libs gui plugins
            │              exit 3 (conflict, diverged main, uncommitted changes) ──> blocked, exit 2
            │ 2 check    the stack repositories on devel, no tracked changes; HEADs recorded
            │ 3 build ─> <work>/prefix        QtPropertyBrowser, libhtreegeom, libcyberiadaml,
            │                                 libcyberiadamlpp (build + install)
            │            <work>/build/binding libcyberiadamlpp-py
            │            <work>/venv          pydantic-ai; .pth: binding, cyberiadaml-py,
            │                                 hsm-dispatcher-py, cybersphere-agents
            │            <work>/build/editor  -DPOLYGON_TESTS=ON, Python3 = <work>/venv
            │ 4 ctest ─> every tier, polygon-unit and polygon-<id> included
            │ 5 polygon  tests/polygon, POLYGON_INSPECTOR=<work>/build/editor/CyberiadaPro
            │              tour, fuzz, fuzz --stress, drill all; run hunt, draw, stress, rebuild
            │              └─> problems/register.json, problems/P-<n>/, sessions/, coverage
            │ 6 report ─> tests/polygon/problems/nightly/<YYYY-MM-DD-HHMM>/
            v              report.md, update.log, build-<step>.log, ctest.log, polygon-<cmd>.log
          stdout: the summary (the cron mail); exit status        nightly/latest -> the last run
```

## Steps

1. **update** - `update-dev-stack.sh -y -u` fetches `origin/main` of every
   project of the sections, fast-forwards `main` and merges it into `devel`
   (`merge the updated main branch into devel`); nothing is pushed and `main`
   is never committed to. A conflict, a diverged `main` or a project with
   uncommitted changes blocks the run: the report says so and nothing is built.
   A fetch error is reported and the run goes on with the current `devel`.
2. **check** - the editor, the libraries, the binding and the Python packages of
   the harness must be on `devel` (QtPropertyBrowser on its own branch) with no
   tracked changes; the report records the HEAD of each.
3. **build** - an incremental out-of-tree build into `<work>`: the libraries are
   installed into `<work>/prefix`, the binding is built against them, the venv
   of the LLM harness is created once and pointed at the binding and the
   sources by a `.pth`, the editor is reconfigured every run so that the
   `polygon-<id>` cases of new problems are picked up.
4. **ctest** - `ctest --output-on-failure` in the editor build. A failing
   `polygon-<id>` case means that its problem no longer reproduces: it is
   listed for review, not counted as a regression.
5. **polygon** - the producers with the day number `N` (days since 1970) as the
   seed; the theme, the story and the diagrams rotate with `N` over
   `catalog/themes.json`, `catalog/stories.json` and `corpus/manifest.json`:

   | command | arguments |
   |---|---|
   | `tour` | `--seed N` |
   | `fuzz` | `--diagram <corpus N> --seed N --rounds 30` |
   | `fuzz` | `--diagram polyline --stress --seed N --rounds 40` |
   | `drill` | `--which all --seed N --rounds 20` |
   | `run --mission hunt` | `--theme <theme N> --seed N` |
   | `run --mission draw` | `--story <story N> --seed N` |
   | `run --mission stress` | `--seed N` |
   | `run --mission rebuild` | `--diagram <corpus N+1> --seed N` |

   The tour plays only the tool cells not tried yet (`toolcover.json`): once
   they are exhausted it runs no rounds. The register is listed before and
   after; the difference is the result. A
   rebuild reproduction (`corpus/<name>-repro-<seed>.graphml`) is moved into
   the run folder, so the source tree stays clean.
6. **report** - `report.md` holds the update summary, the HEADs, the build, the
   ctest totals and failures, a line per polygon command, the new problems and
   the problems hit again, and the backend ledger.

## Options

| option | default | |
|---|---|---|
| `--sources DIR` | two levels above the editor | the repositories and `update-dev-stack.sh` |
| `--work DIR` | `~/.cache/cyberiada-nightly` | the prefix, the build trees, the venv, the lock |
| `--ask` | | run the update interactively (without `-y`) |
| `--no-update` | | test the current `devel` as it is |
| `--sections LIST` | `libs gui plugins` | the sections of `update-dev-stack.sh` |
| `--missions LIST` | `hunt,draw,stress,rebuild` | the LLM missions |
| `--no-llm` | | skip the LLM missions |
| `--no-fuzz` | | skip the deterministic producers |
| `--backend NAME` | `deepseek` | the backend of `polygon.toml` |
| `--polygon-config FILE` | `tests/polygon/polygon.toml`, else `<work>/polygon.toml` | made once from `polygon.example.toml` with `key_env = "DEEPSEEK_API_KEY"` |
| `--env-file FILE` | `~/.profile` | sourced for the API key (cron has no login shell) |
| `--clean` | | remove the build trees first |
| `--jobs N` | `nproc` | parallel build jobs |

The LLM missions are skipped, and the report says why, when the harness does
not import or the key variable of the backend is empty.

## Exit status

| status | meaning |
|---|---|
| 0 | the suite passed and no new problem was found |
| 1 | ctest failures, new problems, or a step that did not run cleanly (an update error, a failed polygon command, the LLM missions skipped) |
| 2 | blocked: an update conflict or uncommitted changes, nothing tested |
| 3 | the build failed |
| 4 | a setup error |
| 5 | another run holds the lock |

## Scheduling

The script needs no terminal and no agent: the SSH remotes are fetched with
`BatchMode=yes`, so their keys must work without a passphrase prompt. A crontab
line (`crontab -e`; install `cron` first where it is missing):

```
0 3 * * * $HOME/sources/CyberiadaHSM-Editor/tests/nightly/run-nightly.sh >> $HOME/.cache/cyberiada-nightly/cron.log 2>&1
```

The register is not safe for concurrent writers: do not run the polygon by hand
while the nightly run is in its polygon step.
