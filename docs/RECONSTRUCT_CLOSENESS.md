# Reconstruction closeness metric

The geometry reconstruction (Edit ▸ Geometry Reconstruction, the `reconstruct` batch verb,
`libhtreegeom`) strips a diagram's geometry and lays it out from scratch. A good reconstruction
reproduces the *arrangement* a human drew — the same left-to-right / top-to-bottom flow, the same
relative placement — even though the absolute coordinates and the node sizes change. This metric scores
how close a reconstructed layout is to the original, so the algorithm can be improved to raise it.

```
  original.graphml ─┐                         K ∈ [0,1]   1 = same arrangement
                    ├─► closeness(orig, recon) ─────────► 0 = arrangement lost
  reconstruct ─► recon.graphml ┘   per container: ½ order + ½ position, child-count weighted
```

The metric is **scale-, translation- and size-invariant**: reconstruction assigns default node sizes,
so only the relative arrangement is judged, never the absolute position or the box dimensions. The two
documents share the same elements (reconstruction never changes the structure), matched by id.

## The score

For every **container** `g` (a state machine, composite or submachine state) and its direct node
children (states, pseudostates, submachine states, comments — not transitions) that carry geometry in
both documents, take each child's centre `(x, y)` and normalise it inside `g`'s child bounding box to
`(u, v) ∈ [0,1]²` (a zero span maps to `0.5`):

- **order** — over every sibling pair, the fraction whose left→right order (`u`) and top→bottom order
  (`v`) is the same in the reconstruction as in the original; the two axes are averaged. A pair with
  fewer than two children scores `1`. This is the primary signal: it captures the reading flow of the
  diagram and ignores small shifts.
- **position** — `1 − mean(‖(u,v)_orig − (u,v)_recon‖) / √2` over the children (√2 is the diagonal of
  the unit square, so the term is in `[0,1]`). This adds the granularity the order test misses.

`K(g) = ½·order + ½·position`. The document score is the mean of `K(g)` weighted by each container's
child count, so the multi-child containers — where a layout can actually go wrong — dominate and the
single-child ones (trivially `K=1`) contribute little.

## Bands

| K | reading |
|---|---|
| ≥ 0.9 | faithful — the arrangement is essentially preserved |
| 0.75–0.9 | close — same flow, local shifts |
| 0.5–0.75 | loose — the order mostly holds, positions drift |
| < 0.5 | poor — the arrangement is lost |

## Use

`tools/reconstruct_closeness.py <original.graphml> <reconstructed.graphml>` prints `K` and the per-
container breakdown; `--table` takes several pairs. The polygon `reconstruct` mode computes it for every
corpus diagram and reports the mean as the campaign's **closeness rate** — the number the `libhtreegeom`
layout work aims to raise (alongside clearing the render-soundness failures the same mode finds).

## Calibration

Validated on synthetic cases: an identical layout scores `K = 1.0`; a fully reversed axis `K ≈ 0.51`
(order `0.5`); a row relaid as a column `K ≈ 0.33` (order `0`).

Baseline over the corpus (`./run-polygon.sh reconstruct`, the current shelf packer), all 19 diagrams
structurally intact:

| band | K | diagrams |
|---|---|---|
| faithful | 1.00 | comment-links, labels, propagation, worker-submachine (few multi-child containers) |
| close | 0.79–0.88 | submachine, orchestrate-home-ext, orchestrate-robot-ext |
| loose | 0.52–0.72 | most (semaphore 0.58, microwave 0.60, dog 0.53, orchestrate-* 0.56–0.66, vacuum-robot 0.72) |
| poor | 0.46 | maze-solver (deep nesting) |

**Closeness rate 0.719.** The shelf packer places children in child-list order, so it keeps the flow
only when that order already matches the drawing; the deeper and wider the diagram, the more the
arrangement drifts. Raising this rate is the Phase C target for the `libhtreegeom` layout.
