# The test polygon

The exploratory test system of the editor: an LLM agent and a fuzzer edit
diagrams through the batch mode, reference-free oracles judge every run,
the found problems are kept with their reproductions and become regression
cases. The architecture is in `docs/POLYGON.md`.

Python 3.11+ and the standard library only. The editor must be built
(`build/CyberiadaInspector`); the batch environment is read from the build
tree, so no variables need setting.

```
cp polygon.example.toml polygon.toml       # fill in the backends; never committed
./run-polygon.sh brief lift                 # the reproduction brief of a diagram
./run-polygon.sh fuzz --diagram geometry --seed 1 --rounds 30
./run-polygon.sh run --backend local --mission reproduce --diagram lift --seed 1
./run-polygon.sh run --backend local --mission combine --seed 4711 --rounds 8
./run-polygon.sh replay sessions/<folder>   # the same verdicts without the backend
./run-polygon.sh check-script --diagram geometry --script my.script [--expectations my.facts]
./run-polygon.sh register list|add|cases    # the problem register
./run-polygon.sh check --problem P-1        # the regression case of a problem
./run-polygon.sh register set-status --problem P-3 --status format --note "..."
cd ../../build && cmake -DPOLYGON_TESTS=ON . && ctest -R polygon   # opt-in ctest tier
```

A backend names its protocol (`chat-completions` for any server speaking
it, `messages`), the model, the base URL and where the key lives: an
environment variable (`key_env`) or a file (`key_file`). Keys never enter
the configuration, the sessions or the register.

Layout: `polygon/` the package, `catalog/` the operations, the model card,
the themes and the hand-written briefs, `corpus/` the start documents and
the accepted reproductions, `tests/` the unit tests. The results are local
and not committed: `problems/` the register and one folder per problem,
`coverage.json` the coverage store, `sessions/` the recordings.
