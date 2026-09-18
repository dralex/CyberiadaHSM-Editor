# Corpus diagram provenance

Diagrams adopted from the sibling Cyberiada-project repositories as start documents
for the edit missions. Each was loaded, re-saved to the editor's native
Cyberiada-GraphML, and verified law-clean and save-reopen stable before adoption.

| corpus file | origin |
|---|---|
| `vacuum-robot.graphml` | `cyberiadaml-compat-tests/fixtures/standard/F-STD-G2.graphml` (PNST example) |
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
