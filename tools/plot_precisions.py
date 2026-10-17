#!/usr/bin/env python3
"""plot_precisions.py -- one animation comparing runs at different precisions
for the Deforming Bubble mini-app.

    python3 tools/plot_precisions.py out_fp64 out_fp32 out_bf16 [--out DIR]
                                     [--stride N] [--no-gif]

Each argument is the outdir of one run (`./bubble ... precision=<p>
outdir=<dir>`).  A run is labelled by its directory name with a leading `out_`
dropped; `label=DIR` sets the label explicitly.  The runs must share the same
mesh and `plot_interval`; only steps present in every run are drawn.

For every common step the `phi = 0` contours of all runs are drawn on one
axis: the FP64 reference in black, each reduced-precision run in its own
colour and line style.  The reference is the run labelled `fp64` if one is
given, otherwise the FP64 shadow (`dfun_ref`) that a reduced-precision run
writes, if any.  With a reference, the legend carries each run's
max |phi - phi_fp64| for that frame.

Writes `precisions_<step>.png` per frame plus the animated GIF
`precisions.gif` into `--out` (default: `out_precisions`).

Standard library + numpy + matplotlib only (+ Pillow for the GIF); the reader
is shared with plot_fields.py.
"""

import os
import re
import sys

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402  (must follow matplotlib.use)
from matplotlib.lines import Line2D  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from plot_fields import read_pvti  # noqa: E402

# Categorical slots in fixed order (validated: CVD and normal-vision separation
# pass on a light surface).  Three of them are under 3:1 contrast, so identity
# is also carried by line style and the legend, never colour alone.
COLORS = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4"]
STYLES = ["-", "--", "-.", ":", (0, (5, 1, 1, 1))]
REF_COLOR = "#1a1a19"


def list_frames(outdir):
    pattern = re.compile(r"^dfun_(\d+)\.pvti$")
    frames = {}
    for name in os.listdir(outdir):
        m = pattern.match(name)
        if m:
            frames[int(m.group(1))] = os.path.join(outdir, name)
    return frames


def run_label(arg):
    if "=" in arg:
        label, path = arg.split("=", 1)
        return label, path
    name = os.path.basename(os.path.normpath(arg))
    return (name[4:] if name.startswith("out_") else name), arg


def frame_png(outdir, step, origin, spacing, time, runs, ref, dpi=110):
    """runs: [(label, dfun)], ref: dfun of the FP64 reference or None."""
    ny, nx = runs[0][1].shape
    x = origin[0] + (np.arange(nx) + 0.5) * spacing[0]
    y = origin[1] + (np.arange(ny) + 0.5) * spacing[1]
    X, Y = np.meshgrid(x, y)

    fig, ax = plt.subplots(figsize=(6.4, 5.8))
    handles = []
    if ref is not None:
        ax.contour(X, Y, ref, levels=[0.0], colors=REF_COLOR, linewidths=2.2)
        handles.append(Line2D([], [], color=REF_COLOR, lw=2.2, label="fp64 (reference)"))
    for k, (label, dfun) in enumerate(runs):
        color, style = COLORS[k % len(COLORS)], STYLES[k % len(STYLES)]
        ax.contour(X, Y, dfun, levels=[0.0], colors=color, linewidths=1.6,
                   linestyles=[style])
        text = label
        if ref is not None:
            text += "   max|err| %.2e" % float(np.abs(dfun - ref).max())
        handles.append(Line2D([], [], color=color, lw=1.6, ls=style, label=text))

    ax.legend(handles=handles, loc="lower left", fontsize=8, frameon=True,
              framealpha=0.9, edgecolor="0.8")
    ax.grid(True, color="0.92", linewidth=0.6)
    ax.set_axisbelow(True)
    ax.set_aspect("equal")
    ax.set_xlim(origin[0], x[-1] + 0.5 * spacing[0])
    ax.set_ylim(origin[1], y[-1] + 0.5 * spacing[1])
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    ax.set_title("step %d   t = %.4f   phi = 0 contour by precision" % (step, time),
                 fontsize=10)
    fig.tight_layout()
    png = os.path.join(outdir, "precisions_%06d.png" % step)
    fig.savefig(png, dpi=dpi)
    plt.close(fig)
    return png


def main(argv):
    args = argv[1:]
    stride = 1
    make_gif = True
    outdir = "out_precisions"
    inputs = []
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--stride":
            i += 1
            stride = int(args[i])
        elif a.startswith("--stride="):
            stride = int(a.split("=", 1)[1])
        elif a == "--out":
            i += 1
            outdir = args[i]
        elif a.startswith("--out="):
            outdir = a.split("=", 1)[1]
        elif a == "--no-gif":
            make_gif = False
        else:
            inputs.append(run_label(a))
        i += 1
    if not inputs:
        sys.stderr.write(__doc__)
        return 2
    for label, path in inputs:
        if not os.path.isdir(path):
            sys.stderr.write("plot_precisions.py: no such directory '%s'\n" % path)
            return 2

    ref_run = next((p for lab, p in inputs if lab == "fp64"), None)
    runs = [(lab, p) for lab, p in inputs if p != ref_run]
    if not runs:
        sys.stderr.write("plot_precisions.py: need at least one non-fp64 run\n")
        return 2

    frames = {p: list_frames(p) for _, p in inputs}
    steps = sorted(set.intersection(*(set(f) for f in frames.values())))
    for label, path in inputs:
        dropped = len(frames[path]) - len(steps)
        if dropped:
            print("plot_precisions: %s: %d frame(s) not in every run, skipped"
                  % (label, dropped))
    steps = steps[::stride]
    if not steps:
        sys.stderr.write("plot_precisions.py: no dfun_*.pvti step common to all runs\n")
        return 1

    os.makedirs(outdir, exist_ok=True)
    pngs = []
    for step in steps:
        dfuns = []
        ref = None
        origin = spacing = time = None
        for label, path in runs:
            origin, spacing, t, fields = read_pvti(frames[path][step])
            if time is not None and abs(t - time) > 1e-12 * max(1.0, abs(time)):
                print("plot_precisions: step %d: %s is at t = %.6f, not %.6f"
                      % (step, label, t, time))
            time = t if time is None else time
            dfuns.append((label, fields["dfun"]))
            if ref is None and "dfun_ref" in fields:
                ref = fields["dfun_ref"]
        if ref_run is not None:
            ref = read_pvti(frames[ref_run][step])[3]["dfun"]
        if any(d.shape != dfuns[0][1].shape for _, d in dfuns) or (
                ref is not None and ref.shape != dfuns[0][1].shape):
            sys.stderr.write("plot_precisions.py: step %d: runs differ in mesh size\n" % step)
            return 1
        pngs.append(frame_png(outdir, step, origin, spacing, time, dfuns, ref))
        print("plot_precisions: step %d, t = %.6f, %d run(s)%s"
              % (step, time, len(dfuns), "" if ref is not None else ", no fp64 reference"))

    print("plot_precisions: wrote %d PNG frame(s) into %s/" % (len(pngs), outdir))

    if make_gif:
        gif = os.path.join(outdir, "precisions.gif")
        try:
            from PIL import Image

            images = [Image.open(p).convert("P", palette=Image.ADAPTIVE) for p in pngs]
            images[0].save(
                gif, save_all=True, append_images=images[1:], duration=120, loop=0
            )
            print("plot_precisions: wrote %s (%d frames)" % (gif, len(images)))
        except ImportError:
            sys.stderr.write(
                "plot_precisions.py: Pillow is not available, skipping the GIF "
                "(PNG frames were written)\n"
            )
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
