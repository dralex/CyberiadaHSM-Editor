#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor - polygon tools
#
# Summarise the complexity-climb sessions: per session the peak diagram
# complexity C, the peak drawing complexity D, the density and the defects,
# and the aggregate over a campaign (see docs/POLYGON.md, the complexity climb).
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
"""Summarise climb sessions. Reads sessions/*/session.json with a `climb`
block. Stdlib only. Usage:
    climb_summary.py                       # sessions/*climb* under the polygon
    climb_summary.py <session-dir|glob> ...
"""
import sys, os, json, glob, statistics

DEFECT_KINDS = ("crash", "oracle", "render")


def polygon_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def session_dirs(args):
    if args:
        dirs = []
        for a in args:
            dirs += sorted(glob.glob(a)) if any(c in a for c in "*?[") else [a]
        return dirs
    return sorted(glob.glob(os.path.join(polygon_root(), "sessions", "*climb*")))


def load(folder):
    path = os.path.join(folder, "session.json")
    if not os.path.exists(path):
        return None
    data = json.load(open(path, encoding="utf-8"))
    if not data.get("climb"):
        return None
    return data


def row(folder, data):
    cl = data["climb"]
    rounds = data.get("rounds") or []
    accepted = sum(1 for r in rounds
                   if r.get("accepted") and not str(r.get("verb", "")).startswith("burst:"))
    defects = len({tuple(f[:2]) for r in rounds for f in r.get("findings", [])
                   if f and f[0] in DEFECT_KINDS})
    return {"seed": data.get("seed"),
            "name": os.path.basename(folder.rstrip("/")),
            "peak_c": cl.get("peak_c", 0.0), "band": cl.get("band", ""),
            "peak_d": cl.get("peak_d", 0.0), "density": cl.get("density", 0.0),
            "commands": cl.get("commands", 0), "accepted": accepted,
            "rounds": len(rounds), "defects": defects,
            "error": data.get("error", "")}


def main():
    dirs = session_dirs(sys.argv[1:])
    rows = []
    for d in dirs:
        data = load(d)
        if data:
            rows.append(row(d, data))
    if not rows:
        print("no climb sessions found", file=sys.stderr)
        return 1
    rows.sort(key=lambda r: r["peak_c"], reverse=True)
    print("%6s %8s %-9s %8s %8s %6s %5s %5s %7s  %s" %
          ("seed", "peak-C", "band", "peak-D", "density", "cmds", "rnds", "def", "err", "session"))
    for r in rows:
        print("%6s %8.1f %-9s %8.1f %8.2f %6d %5d %5d %7s  %s" %
              (str(r["seed"]), r["peak_c"], r["band"], r["peak_d"], r["density"],
               r["commands"], r["rounds"], r["defects"],
               "yes" if r["error"] else "-", r["name"]))
    cs = [r["peak_c"] for r in rows]
    ds = [r["peak_d"] for r in rows]
    bands = {}
    for r in rows:
        bands[r["band"]] = bands.get(r["band"], 0) + 1
    total_def = sum(r["defects"] for r in rows)
    print("\n%d sessions" % len(rows))
    print("peak C: max %.1f  mean %.1f  median %.1f  min %.1f" %
          (max(cs), statistics.mean(cs), statistics.median(cs), min(cs)))
    print("peak D: max %.1f  mean %.1f  median %.1f" %
          (max(ds), statistics.mean(ds), statistics.median(ds)))
    print("bands: " + ", ".join("%s %d" % (b or "-", n) for b, n in
                                sorted(bands.items(), key=lambda x: -x[1])))
    print("defects (unique per session, summed): %d" % total_def)
    return 0


if __name__ == "__main__":
    sys.exit(main())
