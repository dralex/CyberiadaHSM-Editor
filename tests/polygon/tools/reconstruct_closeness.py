#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor - polygon tools
#
# The reconstruction closeness metric (see docs/RECONSTRUCT_CLOSENESS.md): how
# close a reconstructed layout is to the original arrangement, scale/size free.
#
# Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see https://www.gnu.org/licenses/
# -----------------------------------------------------------------------------
"""Compute K, the closeness of a reconstructed layout to the original, per
docs/RECONSTRUCT_CLOSENESS.md, using the CyberiadaML Python library. The two
documents share the same elements; only the geometry differs. Usage:
    reconstruct_closeness.py <original.graphml> <reconstructed.graphml>
"""
import sys, math, argparse
import CyberiadaML as C


def center(e):
    """The centre (x, y) of a node element, or None when it has no geometry."""
    if not e.has_geometry():
        return None
    try:
        r = e.get_geometry_rect()
        return (r.x, r.y)
    except Exception:
        try:
            p = e.get_geometry_point()
            return (p.x, p.y)
        except Exception:
            return None


def collect(path):
    """{container id -> {child id -> centre}} of every collection's node children."""
    d = C.LocalDocument()
    d.open(path, C.formatDetect, C.geometryFormatQt)
    boxes = {}

    def walk(coll):
        kids = {}
        for c in coll.get_children():
            if c.get_type() == C.elementTransition:
                continue
            ctr = center(c)
            if ctr is not None:
                kids[c.get_id()] = ctr
            if c.has_children():
                walk(c)
        if kids:
            boxes[coll.get_id()] = kids

    for sm in d.get_state_machines():
        walk(sm)
    return boxes


def sign(x):
    return 0 if abs(x) < 1e-9 else (1 if x > 0 else -1)


def normalise(kids):
    xs = [p[0] for p in kids.values()]
    ys = [p[1] for p in kids.values()]
    minx, maxx, miny, maxy = min(xs), max(xs), min(ys), max(ys)
    out = {}
    for k, (x, y) in kids.items():
        u = (x - minx) / (maxx - minx) if maxx > minx else 0.5
        v = (y - miny) / (maxy - miny) if maxy > miny else 0.5
        out[k] = (u, v)
    return out


def container_score(o_kids, r_kids):
    """(K, order, position, child count) of one container, or None when empty."""
    ids = [k for k in o_kids if k in r_kids]
    if not ids:
        return None
    o = normalise({k: o_kids[k] for k in ids})
    r = normalise({k: r_kids[k] for k in ids})
    position = 1.0 - sum(math.dist(o[k], r[k]) for k in ids) / len(ids) / math.sqrt(2)
    pairs = [(a, b) for i, a in enumerate(ids) for b in ids[i + 1:]]
    if pairs:
        def agree(ax):
            return sum(1 for a, b in pairs
                       if sign(o[a][ax] - o[b][ax]) == sign(r[a][ax] - r[b][ax])) / len(pairs)
        order = (agree(0) + agree(1)) / 2.0
    else:
        order = 1.0
    return 0.5 * order + 0.5 * position, order, position, len(ids)


def band(k):
    for lo, name in ((0.9, "faithful"), (0.75, "close"), (0.5, "loose")):
        if k >= lo:
            return name
    return "poor"


def closeness(original, reconstructed):
    bo, br = collect(original), collect(reconstructed)
    total = 0.0
    acc = order_acc = pos_acc = 0.0
    per = []
    for cid in bo:
        if cid not in br:
            continue
        res = container_score(bo[cid], br[cid])
        if res is None:
            continue
        k, order, position, w = res
        acc += k * w
        order_acc += order * w
        pos_acc += position * w
        total += w
        per.append((cid, k, order, position, w))
    K = acc / total if total else 1.0
    return dict(K=K, band=band(K), order=order_acc / total if total else 1.0,
                position=pos_acc / total if total else 1.0, weight=int(total), containers=per)


def main():
    ap = argparse.ArgumentParser(description="reconstruction closeness metric")
    ap.add_argument("original")
    ap.add_argument("reconstructed")
    a = ap.parse_args()
    r = closeness(a.original, a.reconstructed)
    print("K=%.3f (%s)  order=%.3f position=%.3f  nodes=%d" %
          (r["K"], r["band"], r["order"], r["position"], r["weight"]))
    for cid, k, order, position, w in sorted(r["containers"], key=lambda c: c[1]):
        print("  %-24s K=%.3f order=%.3f pos=%.3f (%d children)" % (cid, k, order, position, w))
    return 0


if __name__ == "__main__":
    sys.exit(main())
