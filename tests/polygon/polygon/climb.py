# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the complexity climb
#
# Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see https://www.gnu.org/licenses/
#
# -----------------------------------------------------------------------------

"""The climb mission steers the agent to raise the diagram complexity C
(docs/COMPLEXITY.md) and the drawing complexity D (docs/DRAWING_COMPLEXITY.md)
densely within an operation budget. See docs/POLYGON.md (the complexity climb).

C needs the CyberiadaML binding (via tools/complexity.py); when it is not
importable the climb still runs and steers by D alone."""

from . import runner

TARGET_BAND = "complex"
TARGET_C = 65.0            # the complex band (docs/COMPLEXITY.md)


def measure_c(graphml_path):
    try:
        from tools import complexity as CX
        return CX.compute(str(graphml_path))
    except Exception:
        return None


def measure_d(script_text):
    try:
        from tools import drawing_complexity as DX
    except Exception:
        return None
    rows = [line.split() for line in script_text.splitlines()
            if line.strip() and not line.strip().startswith("#")]
    return DX.measure(rows) if rows else None


def measure(env, config, start, script_text, work):
    """C (from a materialised graphml) and D (from the script) of the current
    accepted document; either may be None when its tool is unavailable."""
    d = measure_d(script_text)
    c = None
    try:
        work.mkdir(parents=True, exist_ok=True)
        script = runner.write_script(work / "climb.script", script_text)
        graphml = work / "climb.graphml"
        runner.run(env, start, script=script, save=graphml,
                   timeout=config.timeout, workdir=work)
        if graphml.exists():
            c = measure_c(graphml)
    except Exception:
        c = None
    return c, d


def nudges(c, d):
    """The thinnest dimensions of the two profiles, turned into instructions."""
    out = []
    if c is not None:
        if c.get("depth", 0) < 1:
            out.append("flat: nest states inside a composite state")
        if c.get("cycles", 0) < 1:
            out.append("no feedback loop: route a transition back to an earlier state")
        if c.get("combo", 0) <= 0:
            out.append("no branch: add a choice pseudostate with guarded outgoing transitions")
        if c.get("submachines", 0) < 1:
            out.append("single machine: factor a part into a submachine state referencing another machine")
    if d is not None:
        if d.get("R", 0) <= 0:
            out.append("never restructured: reparent a subtree, or wrap two states in a new composite")
        if d.get("U", 0) <= 0:
            out.append("never reused: copy and paste a substructure you already built")
        if d.get("E", 0) < 3:
            out.append("plain edges: draw a self-loop, a poly-line with a routing point, or a cross-boundary transition")
    return out


def standing(c, d, budget, commands):
    """The standing block appended to the next round's feedback."""
    _, hi = budget
    left = max(0, hi - commands)
    parts = []
    if c is not None:
        parts.append("C=%.1f (%s)" % (c["C"], c["band"]))
    if d is not None:
        parts.append("D-process P=%.1f (%s)" % (d["P"], d["band"]))
    head = ("Complexity so far: %s." % ", ".join(parts)) if parts else \
        "Complexity so far: (not measured)."
    lines = [head,
             "Target: %s (structural C >= %g). Budget: about %d of %d commands left."
             % (TARGET_BAND, TARGET_C, left, hi)]
    if c is not None and c["C"] >= TARGET_C:
        lines.append("You have reached the target band. Deepen and connect what you have "
                     "rather than adding flat elements; stop if a round no longer makes it "
                     "meaningfully more complex.")
    ns = nudges(c, d)
    if ns:
        lines.append("To climb, add this round where you are thinnest:")
        lines += ["  - " + n for n in ns[:4]]
    return "\n".join(lines)


def session_metrics(rounds, commands):
    """Peak C, peak D, the band at the peak and the density (peak C per
    accepted command) from a session's rounds carrying .cx / .dx."""
    cs = [(r.cx["C"], r.cx["band"]) for r in rounds if getattr(r, "cx", None)]
    ds = [r.dx["P"] for r in rounds if getattr(r, "dx", None)]
    peak_c = max((c for c, _ in cs), default=0.0)
    peak_d = max(ds, default=0.0)
    band = next((b for c, b in cs if c == peak_c), "")
    density = round(peak_c / commands, 2) if commands else 0.0
    return {"peak_c": round(peak_c, 1), "peak_d": round(peak_d, 1),
            "band": band, "density": density, "commands": commands}
