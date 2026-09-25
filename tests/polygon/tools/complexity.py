#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor - polygon tools
#
# The HSM diagram complexity metric (see docs/COMPLEXITY.md).
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
"""Compute C(D), the structural complexity of a CyberiadaML diagram, per
docs/COMPLEXITY.md, using the CyberiadaML Python library. Reference-free; ignores
geometry. Usage:
    complexity.py <file.graphml> ...          # per-file breakdown
    complexity.py --table <dir|glob|file> ... # one sorted row per diagram
"""
import sys, os, glob, re, argparse
import CyberiadaML as C

BETA = 1.5      # nesting amplification per level (COMPLEXITY.md 4.2)
TAU  = 0.3      # referenced-machine inheritance (4.7)

KIND_W = {
    C.elementSimpleState: 1.0, C.elementCompositeState: 2.0,
    C.elementSubmachineState: 4.0, C.elementSM: 2.0,
    C.elementInitial: 0.5, C.elementFinal: 1.0, C.elementTerminate: 1.5,
    C.elementChoice: 4.0, C.elementShallowHistory: 3.0, C.elementDeepHistory: 4.0,
    C.elementEntryPoint: 2.0, C.elementExitPoint: 2.0,
    C.elementComment: 0.5, C.elementFormalComment: 0.25,
}
COLLECTIONS = (C.elementCompositeState, C.elementSubmachineState, C.elementSM)
STATES      = (C.elementSimpleState, C.elementCompositeState, C.elementSubmachineState)
PSEUDO      = (C.elementInitial, C.elementFinal, C.elementTerminate, C.elementChoice,
               C.elementShallowHistory, C.elementDeepHistory,
               C.elementEntryPoint, C.elementExitPoint)
EVERY_KIND  = list(KIND_W.keys()) + list(PSEUDO)


def parts(text):
    return len([s for s in re.split(r'[;\n]', text) if s.strip()]) if text else 0

def guard_cx(g):   return 0.5 + 0.02 * min(len(g), 50) if g else 0.0
def beh_cx(b):     return 0.3 * min(parts(b), 10) + 0.01 * min(len(b), 200) if b else 0.0

def name_cx(e):
    if e.get_type() in (C.elementComment, C.elementFormalComment):
        return 0.0
    n = e.get_name()
    return 0.5 + 0.1 * min(len(n.split()), 6) if n else 0.0

def act_cx(e):
    if e.get_type() not in (C.elementSimpleState, C.elementCompositeState):
        return 0.0
    s = 0.0
    for a in e.get_actions():
        b = a.get_behavior() if a.has_behavior() else ""
        t = a.get_type()
        if t in (C.actionEntry, C.actionExit):
            s += 0.4 + beh_cx(b)
        elif t == C.actionTransition:
            s += 1.0 + (0.3 if a.has_trigger() else 0.0) \
                 + guard_cx(a.get_guard() if a.has_guard() else "") + beh_cx(b)
    return s

def intrinsic(e):
    return KIND_W.get(e.get_type(), 0.0) + name_cx(e) + act_cx(e)

def structural_children(e):
    return [c for c in e.get_children() if c.get_type() != C.elementTransition] \
        if e.has_children() else []

def struct(e):
    """Bottom-up structural score with nesting amplification (4.2). A container
    amplifies its children by BETA; the state machine (depth-0 scope) does not."""
    s = intrinsic(e)
    if e.get_type() in COLLECTIONS:
        factor = 1.0 if e.get_type() == C.elementSM else BETA
        for c in structural_children(e):
            s += factor * struct(c)
    return s

def resolve(sm, eid):
    try:
        return sm.find_element_by_id(eid)
    except Exception:
        return None

def trans_cx(sm, t):
    a = t.get_action()
    s = 0.5
    if a.has_trigger():   s += 0.3
    if a.has_guard():     s += guard_cx(a.get_guard())
    if a.has_behavior():  s += beh_cx(a.get_behavior())
    src, tgt = t.get_source_element_id(), t.get_target_element_id()
    if src == tgt:                                s += 0.5
    if t.get_transition_type() == C.transitionLocal: s += 1.0
    for eid in (src, tgt):
        e = resolve(sm, eid)
        if e is not None and e.get_type() in PSEUDO:
            s += 1.0
    return s

def combo(sm, trs):
    s = 0.0
    out = {}
    for t in trs:
        out[t.get_source_element_id()] = out.get(t.get_source_element_id(), 0) + 1
    for src_id, k in out.items():
        e = resolve(sm, src_id)
        if e is None:
            continue
        if e.get_type() == C.elementChoice and k >= 2:
            s += k * (k - 1) / 2.0
        elif e.get_type() in STATES and k >= 3:
            s += 0.3 * k * (k - 1) / 2.0
    for t in trs:
        se, te = resolve(sm, t.get_source_element_id()), resolve(sm, t.get_target_element_id())
        if se is None or te is None:
            continue
        sp, tp = se.get_parent(), te.get_parent()
        if sp is not None and tp is not None and sp.get_id() != tp.get_id():
            s += 1.5
    return s

def transitions(sm):
    return list(sm.find_elements_by_type(C.elementTransition))

def depth(e):
    if e.get_type() not in COLLECTIONS or not e.has_children():
        return 0
    dmax = max((depth(c) for c in structural_children(e)), default=0)
    return dmax + (0 if e.get_type() == C.elementSM else 1)

def distinct_kinds(machines):
    ks = set()
    for sm in machines:
        ks.add(sm.get_type())
        for e in sm.find_elements_by_types(EVERY_KIND):
            ks.add(e.get_type())
    ks.discard(C.elementTransition)
    ks.discard(C.elementFormalComment)
    ks.discard(C.elementSM)   # the machine itself is not a "feature" kind
    return ks

def band(c):
    for lo, name in ((150, "extreme"), (65, "complex"), (25, "moderate"), (5, "simple")):
        if c >= lo:
            return name
    return "trivial"

def compute(path):
    d = C.LocalDocument()
    d.open(path, C.formatDetect, C.geometryFormatNone)
    machines = list(d.get_state_machines())
    base = {}     # sm id -> (struct, trans, combo)
    for sm in machines:
        trs = transitions(sm)
        base[sm.get_id()] = (struct(sm),
                             sum(trans_cx(sm, t) for t in trs),
                             combo(sm, trs))
    cbase = {k: sum(v) for k, v in base.items()}
    # M: multi-machine and submachine references (4.7)
    M = 3.0 * max(0, len(machines) - 1)
    submachines = 0
    for sm in machines:
        for e in sm.find_elements_by_types([C.elementSubmachineState]):
            submachines += 1
            ref = e.get_submachine_reference() or ""
            if ref.startswith("file://"):
                M += 4.0
            else:
                M += 2.0 + (TAU * cbase[ref] if ref in cbase else 0.0)
    V = 1.0 * len(distinct_kinds(machines))
    st = sum(v[0] for v in base.values())
    tr = sum(v[1] for v in base.values())
    cb = sum(v[2] for v in base.values())
    total = st + tr + cb + M + V
    states = sum(len(sm.find_elements_by_types(list(STATES))) for sm in machines)
    trans_n = sum(len(transitions(sm)) for sm in machines)
    dep = max((depth(sm) for sm in machines), default=0)
    return dict(C=total, band=band(total), struct=st, trans=tr, combo=cb, M=M, V=V,
                machines=len(machines), states=states, transitions=trans_n,
                depth=dep, submachines=submachines)


def gather(args):
    files = []
    for a in args:
        if os.path.isdir(a):
            files += sorted(glob.glob(os.path.join(a, "**", "*.graphml"), recursive=True))
        elif any(ch in a for ch in "*?["):
            files += sorted(glob.glob(a, recursive=True))
        else:
            files.append(a)
    return files

def main():
    ap = argparse.ArgumentParser(description="HSM diagram complexity metric")
    ap.add_argument("--table", action="store_true", help="one sorted row per diagram")
    ap.add_argument("paths", nargs="+")
    a = ap.parse_args()
    files = gather(a.paths)
    rows = []
    for f in files:
        try:
            r = compute(f)
            r["file"] = f
            rows.append(r)
        except Exception as e:
            sys.stderr.write("skip %s: %s\n" % (f, e))
    if a.table:
        rows.sort(key=lambda r: r["C"])
        print("%8s %-9s %5s %5s %5s %5s %5s  %3s %3s %3s %3s  %s" %
              ("C", "band", "struc", "trans", "combo", "M", "V",
               "sm", "st", "tr", "dp", "diagram"))
        for r in rows:
            print("%8.1f %-9s %5.1f %5.1f %5.1f %5.1f %5.1f  %3d %3d %3d %3d  %s" %
                  (r["C"], r["band"], r["struct"], r["trans"], r["combo"], r["M"], r["V"],
                   r["machines"], r["states"], r["transitions"], r["depth"],
                   os.path.relpath(r["file"])))
    else:
        for r in rows:
            print("%s\n  C=%.1f (%s)  struct=%.1f trans=%.1f combo=%.1f M=%.1f V=%.1f"
                  "  | machines=%d states=%d transitions=%d depth=%d submachines=%d" %
                  (r["file"], r["C"], r["band"], r["struct"], r["trans"], r["combo"],
                   r["M"], r["V"], r["machines"], r["states"], r["transitions"],
                   r["depth"], r["submachines"]))

if __name__ == "__main__":
    main()
