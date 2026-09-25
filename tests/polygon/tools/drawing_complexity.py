#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor - polygon tools
#
# The drawing-process complexity metric (see docs/DRAWING_COMPLEXITY.md).
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
"""Compute the process surplus P (= D - C_final) of a drawing session, per
docs/DRAWING_COMPLEXITY.md, from a batch/gesture session script. Stdlib only.

First-pass calibration scope: P and its components (O, H, E, R, U, G) plus an
undo/redo oscillation proxy for the directedness eta. C_final (inherited) and the
exact C(g)/trajectory terms (which need per-step replay dumps) are out of scope
here and flagged in the report. Usage:
    drawing_complexity.py <script> ...
    drawing_complexity.py --table <dir|glob|script> ...
"""
import sys, os, glob, argparse

CREATE = {"new-state", "new-comment", "new-formal-comment", "new-initial", "new-final",
          "new-choice", "new-terminate", "new-shallow-history", "new-deep-history",
          "new-submachine-state", "new-entry-point", "new-exit-point", "new-sm"}
REFINE = {"rename", "set-color", "new-action", "update-action", "delete-action",
          "new-subject", "delete-subject", "update-comment", "update-id", "update-meta"}
RESTRUCT = {"reparent", "move", "delete", "new-sm"}       # new-sm also a wrap/split
CLIP = {"copy", "cut", "paste"}
HIST = {"undo", "redo"}
EDGE = {"new-transition", "polyline", "label"}            # semantic transition work
GEST = {"press", "drag", "release", "click", "double-click", "tool",
        "delete-selected", "edit", "edit-text", "type", "key", "select-all", "commit"}
MODEL = CREATE | REFINE | RESTRUCT | CLIP | HIST | EDGE   # everything not a gesture


def parse(path):
    rows = []
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        rows.append(line.split())
    return rows


def measure(rows):
    verbs = [t[0] for t in rows]
    n = {}
    for v in verbs:
        n[v] = n.get(v, 0) + 1
    total = len(verbs)
    # only 'tool transition' is a transition draw ('tool select' is not)
    tool_tr = sum(1 for t in rows if t[0] == "tool" and len(t) > 1 and t[1] == "transition")
    model_verbs = [v for v in verbs if v in MODEL]
    gesture_verbs = [v for v in verbs if v in GEST]

    # attribute press/drag/release gestures to E (transition mode) or G (select) by
    # tracking the current tool; each 'release' completes one gesture
    tool = "select"
    e_gest = g_gest = 0
    for t in rows:
        v = t[0]
        if v == "tool":
            tool = t[1] if len(t) > 1 else "select"
        elif v == "release":
            if tool == "transition":
                e_gest += 1
                tool = "select"   # the transition tool is one-shot
            else:
                g_gest += 1
        elif v == "click" and tool == "transition":
            tool = "select"

    # O - operation breadth: variety of model verbs + capped volume
    O = 1.0 * len(set(model_verbs)) + 0.05 * min(total, 200)
    # H - how-used (proxy): a tool used several times ('several') vs once ('single')
    H = 0.0
    for v in set(model_verbs) | ({"tool"} if n.get("tool") else set()):
        H += 1.0 if n.get(v, 0) >= 2 else 0.5
    # E - transition-drawing craft (emphasised)
    E = (1.5 * n.get("new-transition", 0) + 1.5 * tool_tr
         + 0.5 * n.get("polyline", 0) + 0.3 * n.get("label", 0) + 0.5 * e_gest)
    # R - restructuring (count-based here; C(g) weighting needs replay)
    R = (1.0 * n.get("reparent", 0) + 1.0 * n.get("new-sm", 0)
         + 0.5 * n.get("move", 0) + 0.3 * n.get("delete", 0))
    # U - clipboard reuse (base; C(g) weighting needs replay)
    U = 0.2 * (n.get("copy", 0) + n.get("cut", 0)) + 0.4 * n.get("paste", 0)
    # G - non-transition gesture craft
    G = 0.3 * g_gest + 2.0 * (len(gesture_verbs) / total if total else 0.0)

    # eta proxy: undo/redo signals oscillation; damp the farmable credit
    undoish = n.get("undo", 0) + n.get("redo", 0)
    eta = 1.0 - min(0.8, undoish / max(1, len(model_verbs)))
    # farmable = O volume + U base + the gesture per-op counts
    farmable = 0.05 * min(total, 200) + U + G
    stable = O - 0.05 * min(total, 200) + H + E + R
    P = stable + eta * farmable

    return dict(P=P, band=band(P), O=O, H=H, E=E, R=R, U=U, G=G, eta=eta,
                ops=total, model=len(model_verbs), gestures=len(gesture_verbs),
                transitions=n.get("new-transition", 0) + tool_tr,
                undoredo=undoish)


def band(p):
    for lo, name in ((30, "intricate"), (12, "rich"), (3, "plain")):
        if p >= lo:
            return name
    return "minimal"


def script_of(path):
    """A path may be a .script file or a session directory holding a 'script'."""
    if os.path.isdir(path):
        s = os.path.join(path, "script")
        return s if os.path.exists(s) else None
    return path

def gather(args):
    out = []
    for a in args:
        if os.path.isdir(a) and os.path.exists(os.path.join(a, "script")):
            out.append(a)
        elif os.path.isdir(a):
            out += sorted(glob.glob(os.path.join(a, "**", "*.script"), recursive=True))
            out += [d for d in sorted(glob.glob(os.path.join(a, "*", "")))
                    if os.path.exists(os.path.join(d, "script"))]
        elif any(c in a for c in "*?["):
            out += sorted(glob.glob(a, recursive=True))
        else:
            out.append(a)
    return out

def main():
    ap = argparse.ArgumentParser(description="drawing-process complexity (P surplus)")
    ap.add_argument("--table", action="store_true")
    ap.add_argument("paths", nargs="+")
    a = ap.parse_args()
    rows = []
    for p in gather(a.paths):
        s = script_of(p)
        if not s or not os.path.exists(s):
            continue
        try:
            r = measure(parse(s))
            r["name"] = p
            rows.append(r)
        except Exception as e:
            sys.stderr.write("skip %s: %s\n" % (p, e))
    if a.table:
        rows.sort(key=lambda r: r["P"])
        print("%7s %-9s %5s %5s %5s %5s %5s %5s %4s  %4s %3s %s" %
              ("P", "band", "O", "H", "E", "R", "U", "G", "eta", "ops", "tr", "session"))
        for r in rows:
            print("%7.1f %-9s %5.1f %5.1f %5.1f %5.1f %5.1f %5.1f %4.2f  %4d %3d %s" %
                  (r["P"], r["band"], r["O"], r["H"], r["E"], r["R"], r["U"], r["G"],
                   r["eta"], r["ops"], r["transitions"],
                   os.path.relpath(r["name"]).replace("tests/polygon/sessions/", "")))
    else:
        for r in rows:
            print("%s\n  P=%.1f (%s)  O=%.1f H=%.1f E=%.1f R=%.1f U=%.1f G=%.1f  eta=%.2f"
                  "  | ops=%d model=%d gestures=%d transitions=%d undo/redo=%d" %
                  (r["name"], r["P"], r["band"], r["O"], r["H"], r["E"], r["R"], r["U"],
                   r["G"], r["eta"], r["ops"], r["model"], r["gestures"],
                   r["transitions"], r["undoredo"]))

if __name__ == "__main__":
    main()
