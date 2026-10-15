# The test polygon

The exploratory test system of the editor: a tester agent and a fuzzer edit
diagrams through the batch mode, reference-free oracles judge every step,
the found problems are kept with their reproductions and become regression
cases. The architecture is in `docs/POLYGON.md`.

Python 3.11+. The deterministic commands (`fuzz`, `tour`, `drill`, `replay`,
`check`, `register`) need the standard library only. The tester (`run`) is an
agent of the cybersphere agents framework and needs `cybersphere-agents`,
`hsm-dispatcher-py`, `pydantic-ai` and the CyberiadaML binding importable
(the environment of `cybersphere-agents/README.md`). The editor must be built
(`build/CyberiadaPro`); the batch environment is read from the build tree,
so no variables need setting.

```
cp polygon.example.toml polygon.toml       # name the backends; never committed
./run-polygon.sh run --backend deepseek --mission hunt --seed 1          # the main mission
./run-polygon.sh run --backend deepseek --mission draw --story keyboard  # a drawing story
./run-polygon.sh run --backends deepseek,haiku --mission stress --seed 9 # build, burst, climb
./run-polygon.sh run --backend deepseek --mission rebuild --diagram lift --trace
./run-polygon.sh fuzz --diagram geometry --seed 1 --rounds 30
./run-polygon.sh fuzz --diagram polyline --stress --seed 3 --rounds 40   # edge and point ops
./run-polygon.sh drill --which all --seed 5 --rounds 40                   # the drill class
./run-polygon.sh tour --seed 7                                            # the deterministic tool tour
./run-polygon.sh report                     # the per-backend productivity ledger
./run-polygon.sh replay sessions/<folder>   # the same verdicts without the model
./run-polygon.sh brief lift                 # the brief of a diagram (the rebuild task)
./run-polygon.sh check-script --diagram geometry --script my.script [--expectations my.facts]
./run-polygon.sh register list|add|cases    # the problem register
./run-polygon.sh check --problem P-1        # the regression case of a problem
./run-polygon.sh register set-status --problem P-3 --status format --note "..."
cd ../../build && cmake -DPOLYGON_TESTS=ON . && ctest -R polygon   # opt-in ctest tier
```

A backend names a Pydantic AI model (`provider:model-name`) and where its
key lives: a file (`key_file`) or an environment variable (`key_env`, by
default the provider's own). The key is exported for the run only; it never
enters the configuration, the sessions or the register.

Layout: `polygon/` the package, `diagrams/` the mission diagrams (the
strategies of the tester, editable with the editor), `catalog/` the
operations with their families, the tool model, the model card, the themes,
the stories and the hand-written briefs, `corpus/` the start documents and
the accepted reproductions, `tests/` the unit tests. The results are local
and not committed: `problems/` the register and one folder per problem,
`coverage.json` and `toolcover.json` the coverage stores, `sessions/` the
recordings, `productivity.json` the ledger.
