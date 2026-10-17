#!/usr/bin/env python3
"""verify.py -- analysis of a Deforming Bubble run for human examination.

    python3 dev/workflows/verify/verify.py OUTDIR [--ref REFDIR] [--src SRCDIR]
                                                  [--cell I J] [--dest DIR]
                                                  [--criteria verification.yaml]

OUTDIR is a run output directory (history.csv + dfun_*.pvti/.vti).  The error
is phi - phi_fp64, taken from the run's own FP64 shadow (`dfun_ref`, written for
precision != fp64) or, with --ref, from a second run's `dfun` at matching steps.

Writes into DEST (default OUTDIR/verify/):

  report.md              everything below, in prose + tables
  metrics.json           machine-readable numbers
  norms_vs_time.png      L1, L2 (RMS), Linf of the error per snapshot
  error_map.png          signed error at the final frame, with both phi = 0 contours
  stencil_schematic.png  cell (i,j): cell-centre vs face-centre variables and the
                         WENO5 stencils, as parsed from src/advect.cpp

Sections of the analysis:

  1. Error norms (formal).  For e_ij = phi_ij - phi64_ij over the N owned cells
       ||e||_1 = (1/N) sum |e|,  ||e||_2 = sqrt((1/N) sum e^2),  ||e||_inf = max |e|
     with ||e||_1 <= ||e||_2 <= ||e||_inf.  Reported per snapshot, recomputed from
     the fields and cross-checked against history.csv; plus the localisation
     ratio Linf/L2, the interface-band L2 (|phi64| <= 5h), the location of the
     max error and its distance to the interface, the error in units of the
     FP32 unit roundoff, and a log-linear growth fit of L2(t).
  2. Stencils.  The g.index(i+a, j+b) offsets of every WENO5 call in the fp64
     and lowprec kernels are parsed from the source and checked: same stencils
     in both kernels, upwind bias, reach <= NGUARD.  The candidate polynomials
     and linear weights of wenoFaceValue / wenoFaceValueLowPrec are parsed and
     checked exactly (fractions): each candidate is consistent and exact for
     quadratics (3rd order), the d-weighted combination is exact for quartics
     (5th order).
  3. Schematic of point (i,j) (default: the cell of max final error).
  4. Acceptance (with --criteria).  Each `<metric>_max` / `<metric>_min` entry
     of the file's `criteria:` mapping is checked against the top-level number
     of that name in metrics.json (e.g. trajectory_rms_max, stencil_fail_max);
     a metric that is missing counts as a failure.

Standard library + numpy + matplotlib only.
"""

import argparse
import csv
import json
import math
import os
import re
import sys
import xml.etree.ElementTree as ET
from fractions import Fraction

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.colors import LinearSegmentedColormap  # noqa: E402
from matplotlib.patches import FancyBboxPatch, Rectangle  # noqa: E402

NGUARD = 3
U_FP32 = 2.0 ** -24  # unit roundoff, round-to-nearest

# Reference palette (dataviz skill, light mode): categorical slots 1-3 validate
# all-pairs; diverging blue <-> red with a gray midpoint.
SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK2 = "#52514e"
GRID = "#e4e3df"
BLUE, ORANGE, AQUA = "#2a78d6", "#eb6834", "#1baf7a"
RED = "#e34948"
DIVERGING = LinearSegmentedColormap.from_list("blue_gray_red", ["#184f95", BLUE, "#f0efec", RED, "#a12d2c"])

plt.rcParams.update({
    "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
    "axes.edgecolor": INK2, "axes.labelcolor": INK, "text.color": INK,
    "xtick.color": INK2, "ytick.color": INK2, "axes.grid": True, "grid.color": GRID,
    "grid.linewidth": 0.8, "axes.spines.top": False, "axes.spines.right": False,
    "font.size": 10, "legend.frameon": False,
})


# ---------------------------------------------------------------------------
# I/O
# ---------------------------------------------------------------------------

def read_history(path):
    with open(path) as f:
        rows = list(csv.DictReader(f))
    if not rows:
        return {}
    return {k: np.array([float(r[k]) for r in rows]) for k in rows[0]}


def _read_vti(path):
    piece = ET.parse(path).getroot().find("ImageData").find("Piece")
    ext = [int(v) for v in piece.get("Extent").split()]
    nx, ny = ext[1] - ext[0], ext[3] - ext[2]
    arrays = {}
    for da in piece.find("CellData").findall("DataArray"):
        arrays[da.get("Name")] = np.array(da.text.split(), dtype=np.float64).reshape((ny, nx))
    return ext, arrays


def read_pvti(path):
    """Reassemble one frame -> (spacing, time, {name: (ny, nx) array})."""
    pimage = ET.parse(path).getroot().find("PImageData")
    whole = [int(v) for v in pimage.get("WholeExtent").split()]
    spacing = [float(v) for v in pimage.get("Spacing").split()]
    time = 0.0
    fd = pimage.find("FieldData")
    if fd is not None:
        for da in fd.findall("DataArray"):
            if da.get("Name") == "TIME":
                time = float(da.text)
    names = [p.get("Name") for p in pimage.find("PCellData").findall("PDataArray")]
    ny, nx = whole[3] - whole[2], whole[1] - whole[0]
    fields = {n: np.full((ny, nx), np.nan) for n in names}
    folder = os.path.dirname(path)
    for piece in pimage.findall("Piece"):
        ext, arrays = _read_vti(os.path.join(folder, piece.get("Source")))
        for n in names:
            fields[n][ext[2]:ext[3], ext[0]:ext[1]] = arrays[n]
    return spacing, time, fields


def frames(outdir):
    out = {}
    for name in os.listdir(outdir):
        m = re.match(r"^dfun_(\d+)\.pvti$", name)
        if m:
            out[int(m.group(1))] = os.path.join(outdir, name)
    return dict(sorted(out.items()))


# ---------------------------------------------------------------------------
# 1. error norms
# ---------------------------------------------------------------------------

def norms(err, ref, h):
    a = np.abs(err)
    band = np.abs(ref) <= 5.0 * h
    k = np.unravel_index(int(np.argmax(a)), a.shape)  # (j, i)
    l2 = float(np.sqrt(np.mean(err ** 2)))
    return {
        "l1": float(np.mean(a)),
        "l2": l2,
        "linf": float(a.max()),
        "band_l2": float(np.sqrt(np.mean(err[band] ** 2))) if band.any() else 0.0,
        "linf_over_l2": float(a.max() / l2) if l2 > 0 else 0.0,
        "argmax_ij": [int(k[1]), int(k[0])],
        "argmax_dist_cells": float(abs(ref[k]) / h),
        "linf_in_fp32_ulps": float(a.max() / (U_FP32 * max(np.abs(ref).max(), 1e-300))),
    }


def analyse_errors(outdir, refdir):
    fr = frames(outdir)
    rf = frames(refdir) if refdir else None
    rows, last = [], None
    for step, path in fr.items():
        spacing, t, f = read_pvti(path)
        if refdir:
            if step not in rf:
                continue
            ref = read_pvti(rf[step])[2]["dfun"]
        elif "dfun_ref" in f:
            ref = f["dfun_ref"]
        else:
            continue
        err = f["dfun"] - ref
        h = max(spacing[0], spacing[1])
        r = {"step": step, "time": t, **norms(err, ref, h)}
        rows.append(r)
        last = (step, t, spacing, f["dfun"], ref, err)
    return rows, last


def growth_fit(rows):
    """L2(t) ~ C exp(rate t), fitted over snapshots with t > 0 and L2 > 0."""
    pts = [(r["time"], r["l2"]) for r in rows if r["time"] > 0 and r["l2"] > 0]
    if len(pts) < 3:
        return None
    t, e = np.array(pts).T
    rate, logc = np.polyfit(t, np.log(e), 1)
    return {"rate_per_unit_time": float(rate), "C": float(math.exp(logc)),
            "doubling_time": float(math.log(2) / rate) if rate > 0 else None}


def cross_check(rows, hist):
    """Max relative difference between field-recomputed and history.csv norms."""
    if not hist or "err_l2" not in hist:
        return None
    by_step = {int(s): k for k, s in enumerate(hist["step"])}
    worst = 0.0
    for r in rows:
        k = by_step.get(r["step"])
        if k is None:
            continue
        for mine, theirs in (("l2", "err_l2"), ("linf", "err_linf"), ("band_l2", "err_band_l2")):
            a, b = r[mine], float(hist[theirs][k])
            if max(abs(a), abs(b)) > 0:
                worst = max(worst, abs(a - b) / max(abs(a), abs(b)))
    return worst


# ---------------------------------------------------------------------------
# 2. stencils
# ---------------------------------------------------------------------------

def _function_body(src, signature_regex):
    m = re.search(signature_regex, src)
    if not m:
        return None
    i = src.index("{", m.end() - 1)
    depth = 0
    for k in range(i, len(src)):
        depth += {"{": 1, "}": -1}.get(src[k], 0)
        if depth == 0:
            return src[i:k + 1]
    return None


def _offset(expr, var):
    expr = expr.replace(" ", "")
    if expr == var:
        return 0
    m = re.fullmatch(var + r"([+-]\d+)", expr)
    return int(m.group(1)) if m else None


def parse_stencils(body, fn_name):
    """[{face, positive, dx:[..], dy:[..]}] for every `face = fn(...)` call in body."""
    out = []
    for m in re.finditer(r"(\w+)\s*=\s*" + fn_name + r"\s*\((.*?)\)\s*;", body, re.DOTALL):
        args = m.group(2)
        idx = re.findall(r"g\.index\(\s*([^,]+?)\s*,\s*([^)]+?)\s*\)", args)
        flag = re.search(r"\b(true|false)\s*$", args.strip())
        if len(idx) != 5 or not flag:
            continue
        out.append({
            "face": m.group(1), "positive": flag.group(1) == "true",
            "dx": [_offset(a, "i") for a, _ in idx], "dy": [_offset(b, "j") for _, b in idx],
        })
    return out


_NUM = r"(\d+(?:\.\d*)?(?:[eE][-+]?\d+)?)[fF]?"
_BARE = r"\d+(?:\.\d*)?(?:[eE][-+]?\d+)?[fF]?"


def _frac(text):
    m = re.fullmatch(r"\(?\s*" + _NUM + r"(?:\s*/\s*" + _NUM + r")?\s*\)?", text.strip())
    if not m:
        return None
    v = Fraction(m.group(1))
    return v / Fraction(m.group(2)) if m.group(2) else v


def parse_candidates(body):
    """{True: [c1, c2, c3], False: [...]} with c = {k: coeff of s_k}, from fT1..fT3 lines."""
    branches = {}
    pos = re.search(r"if\s*\(\s*positive\s*\)\s*\{(.*?)\}\s*else\s*\{(.*?)\}", body, re.DOTALL)
    if not pos:
        return None
    for flag, block in ((True, pos.group(1)), (False, pos.group(2))):
        cands = []
        for n in (1, 2, 3):
            m = re.search(r"fT%d\s*=\s*(.*?);" % n, block, re.DOTALL)
            if not m:
                return None
            expr = m.group(1).replace("\n", " ")
            coeffs = {}
            for sign, num, k in re.findall(r"([+-]?)\s*(\([^()]*\)|" + _BARE + r")?\s*\*?\s*s(\d)", expr):
                c = _frac(num) if num else Fraction(1)
                if c is None:
                    return None
                coeffs[int(k)] = coeffs.get(int(k), 0) + (-c if sign == "-" else c)
            cands.append(coeffs)
        branches[flag] = cands
    return branches


def parse_linear_weights(body):
    """{True: [d1, d2, d3], False: [...]} from `dk = positive ? a : b` / `dk = a`."""
    d = {True: [None] * 3, False: [None] * 3}
    for k in (1, 2, 3):
        m = re.search(r"\bd%d\s*=\s*positive\s*\?\s*([^:]+?)\s*:\s*([^;]+?)\s*;" % k, body)
        if m:
            d[True][k - 1], d[False][k - 1] = _frac(m.group(1)), _frac(m.group(2))
            continue
        m = re.search(r"\bd%d\s*=\s*([^;?]+?)\s*;" % k, body)
        if m:
            d[True][k - 1] = d[False][k - 1] = _frac(m.group(1))
    if any(v is None for vs in d.values() for v in vs):
        return None
    return d


def _cell_avg_moment(c, p):
    """Average of x^p over the unit cell centred at c."""
    return (Fraction(2 * c + 1, 2) ** (p + 1) - Fraction(2 * c - 1, 2) ** (p + 1)) / (p + 1)


def exactness_order(coeffs, face_x):
    """Largest P such that the reconstruction is exact for x^0..x^P (cell averages)."""
    p = 0
    while p < 8:
        val = sum(c * _cell_avg_moment(k - 3, p) for k, c in coeffs.items())  # s3 at x = 0
        if val != Fraction(face_x) ** p:
            return p - 1
        p += 1
    return p - 1


def check_kernel(src, fn_sig, loop_sig, loop_fn):
    res = {"function": fn_sig}
    body = _function_body(src, fn_sig)
    if body is None:
        res["error"] = "function not found"
        return res
    cands, d = parse_candidates(body), parse_linear_weights(body)
    if cands and d:
        res["candidates"] = {}
        for flag, face_x in ((True, Fraction(1, 2)), (False, Fraction(-1, 2))):
            comb = {}
            for w, c in zip(d[flag], cands[flag]):
                for k, v in c.items():
                    comb[k] = comb.get(k, 0) + w * v
            res["candidates"]["positive" if flag else "negative"] = {
                "face_x_rel_s3": str(face_x),
                "coeffs": [{f"s{k}": str(v) for k, v in sorted(c.items())} for c in cands[flag]],
                "consistency": [str(sum(c.values())) for c in cands[flag]],
                "candidate_exact_degree": [exactness_order(c, face_x) for c in cands[flag]],
                "linear_weights": [str(w) for w in d[flag]],
                "weights_sum": str(sum(d[flag])),
                "optimal_combination": {f"s{k}": str(v) for k, v in sorted(comb.items())},
                "optimal_exact_degree": exactness_order(comb, face_x),
            }
    else:
        res["candidates"] = None
        res["note"] = "candidate polynomials / linear weights not in the parsable fT/dk form"
    loop = _function_body(src, loop_sig)
    res["stencils"] = parse_stencils(loop, loop_fn) if loop else []
    return res


def analyse_stencils(srcdir):
    path = os.path.join(srcdir, "advect.cpp")
    src = open(path, encoding="utf-8").read()
    fp64 = check_kernel(src, r"double\s+wenoFaceValue\s*\(", r"void\s+advectWenoWith\s*\(", "weno")
    low = check_kernel(src, r"float\s+wenoFaceValueLowPrec\s*\(", r"void\s+advectWenoLowPrec\s*\(",
                       "wenoFaceValueLowPrec")
    findings = []

    def key(s):
        return (s["face"], s["positive"], tuple(s["dx"]), tuple(s["dy"]))

    if fp64["stencils"] and low["stencils"]:
        a, b = {key(s) for s in fp64["stencils"]}, {key(s) for s in low["stencils"]}
        findings.append(("PASS" if a == b else "FAIL",
                         "lowprec and fp64 kernels read identical stencils" if a == b else
                         "stencil mismatch fp64 vs lowprec: %s" % sorted(a ^ b)))
    elif not low["stencils"]:
        findings.append(("INFO", "lowprec stencils not parsable (kernel restructured); compare by hand"))

    for s in fp64["stencils"]:
        offs = [o for o in s["dx"] + s["dy"] if o is not None]
        reach = max(abs(o) for o in offs) if offs else 0
        if reach > NGUARD:
            findings.append(("FAIL", f"{s['face']} reaches {reach} > NGUARD = {NGUARD}"))
        axis = "dx" if any(s["dx"]) else "dy"
        vel = "u" if axis == "dx" else "v"
        o = s[axis]
        # face of a high reconstruction (frx/fry) is at +1/2, low (flx/fly) at -1/2
        face = 0.5 if s["face"].startswith("fr") else -0.5
        centre = sum(o) / 5.0
        upwind_ok = (centre < face) if s["positive"] else (centre > face)
        findings.append(("PASS" if upwind_ok else "FAIL",
                         f"{s['face']} ({vel + ('>0' if s['positive'] else '<=0')}): offsets {o} centred at "
                         f"{centre:+.1f}, face at {face:+.1f} -> {'upwind-biased' if upwind_ok else 'NOT upwind'}"))

    for name, k in (("fp64", fp64), ("lowprec", low)):
        c = k.get("candidates")
        if not c:
            continue
        for br, info in c.items():
            ok = (all(x == "1" for x in info["consistency"]) and info["weights_sum"] == "1"
                  and all(d >= 2 for d in info["candidate_exact_degree"])
                  and info["optimal_exact_degree"] >= 4)
            findings.append(("PASS" if ok else "FAIL",
                             f"{name} {br}: candidates exact to degree {info['candidate_exact_degree']}, "
                             f"weights sum {info['weights_sum']}, optimal combination exact to degree "
                             f"{info['optimal_exact_degree']} (5th order needs 4)"))
    return {"fp64": fp64, "lowprec": low, "findings": findings}


# ---------------------------------------------------------------------------
# plots
# ---------------------------------------------------------------------------

def plot_norms(rows, dest, label):
    # exact zeros (t = 0) have no place on a log axis: mask them
    rows = [r for r in rows if r["linf"] > 0]
    t = [r["time"] for r in rows]
    fig, ax = plt.subplots(figsize=(7.2, 4.2))
    for key, color, name in (("linf", BLUE, "$L_\\infty$"), ("l2", ORANGE, "$L_2$ (RMS)"),
                             ("l1", AQUA, "$L_1$ (mean)")):
        ax.semilogy(t, [r[key] for r in rows], color=color, lw=2, marker="o", ms=5, label=name)
    ax.set_xlabel("t")
    ax.set_ylabel("$\\|\\phi - \\phi_{64}\\|$")
    ax.set_title("Error norms per snapshot (%s)" % label, loc="left", fontsize=10)
    ax.legend(loc="best", fontsize=9)
    fig.tight_layout()
    path = os.path.join(dest, "norms_vs_time.png")
    fig.savefig(path, dpi=130)
    plt.close(fig)
    return path


def plot_error_map(last, cell, dest):
    step, t, spacing, phi, ref, err = last
    ny, nx = err.shape
    x = (np.arange(nx) + 0.5) * spacing[0]
    y = (np.arange(ny) + 0.5) * spacing[1]
    lim = float(np.abs(err).max()) or 1.0
    fig, ax = plt.subplots(figsize=(6.4, 5.6))
    ax.grid(False)
    im = ax.pcolormesh(x, y, err, cmap=DIVERGING, vmin=-lim, vmax=lim, shading="auto")
    fig.colorbar(im, ax=ax, label="$\\phi - \\phi_{64}$ (cell centres)")
    ax.contour(x, y, ref, levels=[0.0], colors=INK, linewidths=1.2, linestyles="--")
    ax.contour(x, y, phi, levels=[0.0], colors=INK2, linewidths=0.8)
    ci, cj = cell
    ax.plot(x[ci], y[cj], marker="s", ms=9, mfc="none", mec=INK, mew=1.5)
    ax.annotate("(i,j) = (%d,%d)" % (ci, cj), (x[ci], y[cj]), xytext=(8, 8),
                textcoords="offset points", color=INK, fontsize=9)
    ax.set_aspect("equal")
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    ax.set_title("Signed error, step %d (t = %.4f)\ndashed: FP64 $\\phi=0$, solid: reduced precision"
                 % (step, t), loc="left", fontsize=10)
    fig.tight_layout()
    path = os.path.join(dest, "error_map.png")
    fig.savefig(path, dpi=130)
    plt.close(fig)
    return path


def plot_schematic(stencils, cell, local, dest):
    """Cell (i,j) in index space: centres, MAC faces, and the parsed WENO5 stencils.

    Left panel: x direction, stencils drawn in lanes below the grid, all lying
    along row j (shaded).  Right panel: y direction, lanes right of the grid,
    along column i.
    """
    fig, axes = plt.subplots(1, 2, figsize=(14.0, 7.6))
    groups = (("x", [s for s in stencils if any(s["dx"])]), ("y", [s for s in stencils if any(s["dy"])]))
    for ax, (axis, sts) in zip(axes, groups):
        ax.grid(False)
        for side in ("left", "bottom"):
            ax.spines[side].set_visible(False)
        ax.set_aspect("equal")
        ax.set_xticks(range(-3, 4))
        ax.set_yticks(range(-3, 4))
        ax.set_xticklabels(["i%+d" % k if k else "i" for k in range(-3, 4)])
        ax.set_yticklabels(["j%+d" % k if k else "j" for k in range(-3, 4)])
        ax.tick_params(length=0)
        if axis == "x":
            ax.set_xlim(-3.9, 7.4)
            ax.set_ylim(-6.6, 3.9)
            ax.add_patch(Rectangle((-3.5, -0.5), 7, 1, fc=GRID, ec="none", alpha=0.6, zorder=0))
        else:
            ax.set_xlim(-3.9, 8.0)
            ax.set_ylim(-3.9, 5.6)
            ax.add_patch(Rectangle((-0.5, -3.5), 1, 7, fc=GRID, ec="none", alpha=0.6, zorder=0))
        for k in range(-3, 5):  # cell edges at half-integers
            ax.plot([k - 0.5, k - 0.5], [-3.5, 3.5], color=GRID, lw=1, zorder=1)
            ax.plot([-3.5, 3.5], [k - 0.5, k - 0.5], color=GRID, lw=1, zorder=1)
        ax.add_patch(Rectangle((-0.5, -0.5), 1, 1, fill=False, ec=INK, lw=2, zorder=3))
        cx, cy = np.meshgrid(range(-3, 4), range(-3, 4))
        ax.scatter(cx, cy, s=18, c=INK2, zorder=4)
        ax.annotate("$\\phi_{i,j}$, rhs$_{i,j}$", (0, 0), xytext=(0, 0.13), ha="center",
                    fontsize=8, color=INK, zorder=6)

        # MAC face velocities of cell (i,j): u(i,j) is the LOW x-face (i-1/2)
        for fx, fy in ((-0.5, 0), (0.5, 0)):
            ax.scatter([fx], [fy], marker=">", s=90, c=ORANGE, ec=SURFACE, lw=1.5, zorder=5)
        for fx, fy in ((0, -0.5), (0, 0.5)):
            ax.scatter([fx], [fy], marker="^", s=90, c=AQUA, ec=SURFACE, lw=1.5, zorder=5)
        if axis == "x":
            ax.annotate("$u_{i,j}$ @ i-1/2", (-0.5, 0), xytext=(-0.55, -0.32), ha="right", fontsize=8)
            ax.annotate("$u_{i+1,j}$ @ i+1/2", (0.5, 0), xytext=(0.55, -0.32), ha="left", fontsize=8)
        else:
            ax.annotate("$v_{i,j}$ @ j-1/2", (0, -0.5), xytext=(0.12, -0.78), fontsize=8)
            ax.annotate("$v_{i,j+1}$ @ j+1/2", (0, 0.5), xytext=(0.12, 0.62), fontsize=8)

        lanes = sorted(sts, key=lambda s: (not s["face"].startswith("fl"), not s["positive"]))
        for n, s in enumerate(lanes):
            offs = s["dx"] if axis == "x" else s["dy"]
            if any(o is None for o in offs):
                continue
            color = BLUE if s["positive"] else RED
            lo, hi = min(offs), max(offs)
            face = 0.5 if s["face"].startswith("fr") else -0.5
            vel = ("u" if axis == "x" else "v") + (">0" if s["positive"] else "<=0")
            if axis == "x":
                lane = -4.2 - 0.6 * n
                ax.add_patch(FancyBboxPatch((lo - 0.3, lane - 0.16), hi - lo + 0.6, 0.32,
                                            boxstyle="round,pad=0,rounding_size=0.1",
                                            fc=color, alpha=0.2, ec=color, lw=1.2, zorder=2))
                ax.scatter(offs, [lane] * 5, s=18, c=color, zorder=3)
                ax.plot([face, face], [lane - 0.24, lane + 0.24], color=INK, lw=2, zorder=3)
                ax.annotate("%s  (%s)  i%+d..i%+d" % (s["face"], vel, lo, hi), (3.8, lane),
                            va="center", fontsize=8.5, color=INK2)
            else:
                lane = 4.3 + 0.95 * n
                ax.add_patch(FancyBboxPatch((lane - 0.16, lo - 0.3), 0.32, hi - lo + 0.6,
                                            boxstyle="round,pad=0,rounding_size=0.1",
                                            fc=color, alpha=0.2, ec=color, lw=1.2, zorder=2))
                ax.scatter([lane] * 5, offs, s=18, c=color, zorder=3)
                ax.plot([lane - 0.24, lane + 0.24], [face, face], color=INK, lw=2, zorder=3)
                ax.annotate("%s\n%s\nj%+d..j%+d" % (s["face"], vel, lo, hi), (lane, 3.85),
                            ha="center", fontsize=8, color=INK2)
        where = "row j" if axis == "x" else "column i"
        ax.set_title("%s direction: WENO5 stencils along %s%s" % (
            axis, where, "" if cell is None else "   (i,j) = (%d,%d)" % cell), loc="left", fontsize=10.5)

    legend = [
        plt.Line2D([], [], ls="", marker="o", color=INK2, label="cell centre: $\\phi$ (dfun), rhs, dfun_ref, err"),
        plt.Line2D([], [], ls="", marker=">", color=ORANGE, ms=9, label="x-face centre: u (MAC; u[i] sits at i-1/2)"),
        plt.Line2D([], [], ls="", marker="^", color=AQUA, ms=9, label="y-face centre: v (MAC; v[j] sits at j-1/2)"),
        plt.Line2D([], [], color=BLUE, lw=7, alpha=0.4, label="stencil for positive face velocity"),
        plt.Line2D([], [], color=RED, lw=7, alpha=0.4, label="stencil for non-positive face velocity"),
        plt.Line2D([], [], color=INK, lw=2, label="face where phi is reconstructed"),
    ]
    fig.legend(handles=legend, loc="lower center", ncol=3, fontsize=9, bbox_to_anchor=(0.5, 0.075))
    note = ("rhs$_{i,j}$ = -(frx u$_{i+1,j}$ - flx u$_{i,j}$)/dx - (fry v$_{i,j+1}$ - fly v$_{i,j}$)/dy.   "
            "The .vti 'u','v' are face values averaged to the centre, u$_c$ = (u$_{i,j}$ + u$_{i+1,j}$)/2; "
            "raw face values are not written.   Guard cells: %d." % NGUARD)
    if local:
        note += "\nAt (i,j): $\\phi$ = %.9f, $\\phi_{64}$ = %.9f, err = %+.3e, u$_c$ = %+.4f, v$_c$ = %+.4f" % (
            local["dfun"], local["dfun_ref"], local["err"], local["u_c"], local["v_c"])
    fig.text(0.5, 0.012, note, ha="center", va="bottom", fontsize=9, color=INK2)
    fig.tight_layout(rect=(0, 0.17, 1, 0.96))
    path = os.path.join(dest, "stencil_schematic.png")
    fig.savefig(path, dpi=130)
    plt.close(fig)
    return path


# ---------------------------------------------------------------------------
# report
# ---------------------------------------------------------------------------

def write_report(dest, outdir, label, rows, fit, xcheck, st, cell, local, pngs):
    L = ["# Verification report: `%s`" % outdir, "", "Comparison: %s." % label, ""]
    L += ["## 1. Error norms", "",
          "For e = phi - phi_fp64 over the N owned cells: "
          "||e||_1 = (1/N) sum|e|, ||e||_2 = sqrt((1/N) sum e^2), ||e||_inf = max|e|; "
          "always ||e||_1 <= ||e||_2 <= ||e||_inf. Linf/L2 near 1 means the error is spread "
          "uniformly; much larger than 1 means it is localised. "
          "'FP32 ulps' = Linf / (2^-24 max|phi_fp64|).", ""]
    if rows:
        L += ["| step | t | L1 | L2 | Linf | band L2 | Linf/L2 | argmax (i,j) | dist to phi=0 [cells] | Linf [FP32 ulps] |",
              "|---:|---:|---:|---:|---:|---:|---:|:---:|---:|---:|"]
        for r in rows:
            L.append("| %d | %.4f | %.3e | %.3e | %.3e | %.3e | %.1f | (%d,%d) | %.1f | %.1f |" % (
                r["step"], r["time"], r["l1"], r["l2"], r["linf"], r["band_l2"], r["linf_over_l2"],
                r["argmax_ij"][0], r["argmax_ij"][1], r["argmax_dist_cells"], r["linf_in_fp32_ulps"]))
        traj = math.sqrt(sum(r["l2"] ** 2 for r in rows) / len(rows))
        L += ["", "- Trajectory RMS (sqrt of the mean over snapshots of L2^2): **%.3e**" % traj,
              "- Max Linf over snapshots: **%.3e**" % max(r["linf"] for r in rows)]
        if fit:
            L.append("- Growth fit L2(t) ~ %.2e exp(%.3f t)%s" % (
                fit["C"], fit["rate_per_unit_time"],
                ", doubling time %.3f" % fit["doubling_time"] if fit["doubling_time"] else " (not growing)"))
        if xcheck is not None:
            L.append("- Cross-check against history.csv (max relative difference L2/Linf/band): %.2e %s" % (
                xcheck, "(consistent)" if xcheck < 1e-6 else "(**INCONSISTENT**)"))
    else:
        L.append("No error fields: the run is fp64 (no `dfun_ref`) and no `--ref` was given.")
    L += ["", "![norms](norms_vs_time.png)" if rows else "", "![error map](error_map.png)" if rows else "", ""]

    L += ["## 2. Stencils (parsed from src/advect.cpp)", "",
          "| check | result |", "|:---|:---|"]
    for status, text in st["findings"]:
        L.append("| %s | %s |" % (status, text.replace("|", "\\|")))
    for name in ("fp64", "lowprec"):
        c = st[name].get("candidates")
        if not c:
            continue
        L += ["", "### %s kernel (`%s`)" % (name, st[name]["function"].split("\\s+")[1].split("\\")[0]), ""]
        for br, info in c.items():
            L.append("- **%s** branch, face at x = %s relative to s3 (cell averages, unit spacing):" % (
                br, info["face_x_rel_s3"]))
            for n, (co, deg) in enumerate(zip(info["coeffs"], info["candidate_exact_degree"]), 1):
                L.append("  - q%d = %s  (exact to degree %d)" % (
                    n, " + ".join("%s %s" % (v, k) for k, v in co.items()), deg))
            L.append("  - d = (%s); optimal d-combination = %s, exact to degree %d" % (
                ", ".join(info["linear_weights"]),
                " + ".join("%s %s" % (v, k) for k, v in info["optimal_combination"].items()),
                info["optimal_exact_degree"]))
    L += ["", "## 3. Cell (i,j) schematic", "",
          "Variable placement (MAC grid, see src/grid.h, src/advect.h):", "",
          "| variable | location | index convention |", "|:---|:---|:---|",
          "| `dfun` (phi), `rhs`, `dfun_ref`, `err` | cell centre (i,j) | x = xmin + (i+1/2) dx |",
          "| `u` (solver) | x-face centre | u(i,j) is the LOW face i-1/2 of cell (i,j); u(nx,j) exists |",
          "| `v` (solver) | y-face centre | v(i,j) is the LOW face j-1/2 of cell (i,j); v(i,ny) exists |",
          "| `u`, `v` in .vti | cell centre | averages (u(i,j)+u(i+1,j))/2, not face values |",
          "| frx, flx / fry, fly | faces i+1/2, i-1/2 / j+1/2, j-1/2 | WENO5 reconstruction of phi at the face |",
          ""]
    if cell is not None:
        L.append("Cell shown: (i,j) = (%d,%d)%s." % (cell[0], cell[1],
                                                     " (max error at the final frame)" if local and local.get("auto") else ""))
    if local:
        L += ["", "Local error neighbourhood (rows j+2 .. j-2, columns i-2 .. i+2):", "", "```"]
        for row in local["patch"]:
            L.append("  ".join("%+.2e" % v for v in row))
        L.append("```")
    L += ["", "![schematic](stencil_schematic.png)", ""]
    with open(os.path.join(dest, "report.md"), "w") as f:
        f.write("\n".join(L) + "\n")


# ---------------------------------------------------------------------------
# 4. acceptance
# ---------------------------------------------------------------------------

def load_criteria(path):
    with open(path) as f:
        text = f.read()
    if path.endswith(".json"):
        spec = json.loads(text)
    else:
        import yaml  # PyYAML, in the project image
        spec = yaml.safe_load(text)
    return (spec or {}).get("criteria", {})


def check_criteria(criteria, metrics):
    out = []
    for name, limit in criteria.items():
        if name.endswith("_max"):
            metric, ok = name[:-4], (lambda v, L: v <= L)
        elif name.endswith("_min"):
            metric, ok = name[:-4], (lambda v, L: v >= L)
        else:
            out.append({"criterion": name, "metric": name, "limit": limit, "value": None,
                        "status": "FAIL", "why": "criterion must end in _max or _min"})
            continue
        value = metrics.get(metric)
        if not isinstance(value, (int, float)):
            status, why = "FAIL", "metric not available for this run"
        else:
            status, why = ("PASS", "") if ok(value, float(limit)) else ("FAIL", "outside limit")
        out.append({"criterion": name, "metric": metric, "limit": limit, "value": value,
                    "status": status, "why": why})
    return out


def append_acceptance(dest, path, results):
    L = ["## 4. Acceptance", "", "Criteria from `%s`." % path, "",
         "| criterion | value | limit | status |", "|:---|---:|---:|:---|"]
    for c in results:
        v = c["value"]
        L.append("| `%s` | %s | %s | %s%s |" % (
            c["criterion"], ("%.3e" % v) if isinstance(v, float) else v, c["limit"], c["status"],
            (" (%s)" % c["why"]) if c["why"] and c["status"] != "PASS" else ""))
    with open(os.path.join(dest, "report.md"), "a") as f:
        f.write("\n".join(L) + "\n")


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("outdir")
    ap.add_argument("--ref", help="second run whose dfun is the reference (instead of dfun_ref)")
    ap.add_argument("--src", default="src")
    ap.add_argument("--cell", nargs=2, type=int, metavar=("I", "J"))
    ap.add_argument("--dest")
    ap.add_argument("--criteria", help="YAML/JSON file with a `criteria:` mapping (acceptance)")
    a = ap.parse_args(argv[1:])

    if not os.path.isdir(a.outdir):
        sys.stderr.write("verify.py: no such directory '%s'\n" % a.outdir)
        return 2
    dest = a.dest or os.path.join(a.outdir, "verify")
    os.makedirs(dest, exist_ok=True)

    hist_path = os.path.join(a.outdir, "history.csv")
    hist = read_history(hist_path) if os.path.isfile(hist_path) else {}
    label = ("dfun vs `%s` dfun" % a.ref) if a.ref else "dfun vs in-run FP64 shadow (dfun_ref)"

    rows, last = analyse_errors(a.outdir, a.ref)
    fit = growth_fit(rows)
    xcheck = None if a.ref else cross_check(rows, hist)
    st = analyse_stencils(a.src)

    cell, local = (tuple(a.cell) if a.cell else None), None
    pngs = []
    if last is not None:
        step, t, spacing, phi, ref, err = last
        auto = cell is None
        if auto:
            cell = tuple(rows[-1]["argmax_ij"])
        ci, cj = cell
        _, _, f = read_pvti(frames(a.outdir)[step])
        ny, nx = err.shape
        patch = [[float(err[jj, ii]) if 0 <= ii < nx and 0 <= jj < ny else float("nan")
                  for ii in range(ci - 2, ci + 3)] for jj in range(cj + 2, cj - 3, -1)]
        local = {"auto": auto, "dfun": float(phi[cj, ci]), "dfun_ref": float(ref[cj, ci]),
                 "err": float(err[cj, ci]), "u_c": float(f["u"][cj, ci]), "v_c": float(f["v"][cj, ci]),
                 "patch": patch}
        pngs.append(plot_norms(rows, dest, label))
        pngs.append(plot_error_map(last, cell, dest))
    pngs.append(plot_schematic(st["fp64"]["stencils"], cell, local, dest))

    write_report(dest, a.outdir, label, rows, fit, xcheck, st, cell, local, pngs)
    metrics = {
        "outdir": a.outdir, "comparison": label, "snapshots": rows, "growth_fit": fit,
        "history_cross_check_max_rel": xcheck,
        "stencil_findings": [{"status": s, "text": t} for s, t in st["findings"]],
        "stencils": {"fp64": st["fp64"], "lowprec": st["lowprec"]},
        "cell": list(cell) if cell else None, "cell_values": local,
    }
    for status in ("PASS", "INFO", "FAIL"):
        metrics["stencil_" + status.lower()] = sum(1 for s, _ in st["findings"] if s == status)
    if rows:
        metrics["trajectory_rms"] = math.sqrt(sum(r["l2"] ** 2 for r in rows) / len(rows))
        metrics["max_linf"] = max(r["linf"] for r in rows)
    if a.criteria:
        metrics["acceptance"] = check_criteria(load_criteria(a.criteria), metrics)
        append_acceptance(dest, a.criteria, metrics["acceptance"])
    with open(os.path.join(dest, "metrics.json"), "w") as f:
        json.dump(metrics, f, indent=2)

    fails = [t for s, t in st["findings"] if s == "FAIL"]
    for c in metrics.get("acceptance", []):
        print("verify: %s %s = %s (limit %s)" % (c["status"], c["metric"], c["value"], c["limit"]))
        if c["status"] == "FAIL":
            fails.append("acceptance: %s" % c["criterion"])
    print("verify: %d snapshot(s) analysed, %d stencil check(s), %d FAIL" % (
        len(rows), len(st["findings"]), len(fails)))
    for t in fails:
        print("verify: FAIL %s" % t)
    print("verify: wrote %s/{report.md, metrics.json, %s}" % (
        dest, ", ".join(os.path.basename(p) for p in pngs)))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
