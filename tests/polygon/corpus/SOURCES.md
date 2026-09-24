# Corpus diagram provenance

Diagrams adopted from the sibling Cyberiada-project repositories as start documents
for the edit missions. Each was loaded, re-saved to the editor's native
Cyberiada-GraphML, and verified law-clean and save-reopen stable before adoption.

| corpus file | origin |
|---|---|
| `vacuum-robot.graphml` | `cyberiadaml-compat-tests/fixtures/standard/F-STD-G2.graphml` (PNST example; refreshed 2026-09-24 after the final standard text) |
| `worker-submachine.graphml` | `cyberiadaml-compat-tests/fixtures/ext/F-EXT-SUB.graphml` — a submachine state whose named points match the referenced machine (PNST 1044 8.1) |
| `labels.graphml` | `cyberiadaml-compat-tests/fixtures/core/F-LABEL.graphml` — transition labels with guards, `propagate`/`block`, a completion transition and an internal `defer` reaction (6.8.1) |
| `propagation.graphml` | `cyberiadaml-compat-tests/fixtures/field/F-FIELD-propagation.graphml` — event propagation keywords in their final form (6.8.1, 6.8.2) |
| `comment-links.graphml` | `cyberiadaml-compat-tests/fixtures/core/F-CMT.graphml` — comment links to a name fragment and to a state (6.7) |
| `turtle-square.graphml` | `hsm_robot_ros_generator/examples/turtle-square.graphml` (yEd → converted) |
| `maze-solver.graphml` | `hsm_robot_ros_generator/examples/right_hand_maze_solver.graphml` (yEd → converted) |
| `microwave.graphml` | `hsm-arduino-course/diagrams/2/microwave.graphml` |
| `semaphore.graphml` | `hsm-arduino-course/diagrams/2/semaphore.graphml` |
| `semaphore-hierarchy.graphml` | `hsm-arduino-course/diagrams/2/semaphore-hierarchy.graphml` |
| `dog.graphml` | `hsm-arduino-course/diagrams/2/dog.graphml` |
| `simple-hoover.graphml` | `hsm-arduino-course/diagrams/3/simple-hoover.graphml` |
| `submachine.graphml` | `CyberiadaHSM-Editor/tests/good/submachine-output.graphml` (editor l2-submachine test; the only diagram using the new submachine + entry/exit elements — PSiCC2 has no formal submachine states) |
| `orchestrate-robot.graphml` | generated (`tools/combine.py orchestrate-robot`) — an orchestrator whose submachine states run vacuum-robot + maze-solver + turtle-square, embedded |
| `orchestrate-traffic.graphml` | generated (`tools/combine.py orchestrate-traffic`) — semaphore + semaphore-hierarchy, embedded |
| `orchestrate-home.graphml` | generated (`tools/combine.py orchestrate-home`) — microwave + simple-hoover, embedded |
| `orchestrate-grand.graphml` | generated (`tools/combine.py orchestrate-grand`) — vacuum-robot + maze-solver + turtle-square + dog, embedded (stress) |
| `orchestrate-robot-ext.graphml` | generated (`tools/combine.py orchestrate-robot-ext`) — the robot orchestrator referencing the machines by file name (external) |
| `orchestrate-home-ext.graphml` | generated (`tools/combine.py orchestrate-home-ext`) — the home orchestrator with external file references |

The gate is `tools/gate.py` (load, save to native, save/reopen stable, standing laws silent,
strict reload through libcyberiadaml); the corpus was re-gated on 2026-09-24 against the
final PNST 1044-2025 text (mirror revision `3f58ea7`): the orchestrators gained top-level
`in`/`out` points in every embedded machine and distinct machine names, `submachine.graphml`
follows the editor's regenerated good file (named points).
