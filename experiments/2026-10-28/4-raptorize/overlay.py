"""Overlay the phi = 0 contours of two single-rank runs, frame by frame, and make a GIF.

Beside the contours: err_band and sign_mismatch of the low run's history.csv up to each frame.

Usage: python overlay.py <fp64 dir> <low dir> <out dir> [label]
"""

import re
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import PillowWriter


def read_vti(path):
    """Read the dfun cell field and time of an ASCII .vti piece.

    Parameters
    ----------
    path : Path
        A dfun_<step>_0000.vti file.

    Returns
    -------
    tuple of (float, numpy.ndarray)
        Time and dfun as an (ny, nx) array.
    """
    text = path.read_text()
    ext = list(map(int, re.search(r'Piece Extent="(\S+) (\S+) (\S+) (\S+)', text).groups()))
    time = float(re.search(r'Name="TIME"[^>]*>\s*([^<\s]+)', text).group(1))
    data = re.search(r'Name="dfun"[^>]*>(.*?)</DataArray>', text, re.S).group(1)
    return time, np.array(data.split(), dtype=float).reshape(ext[3] - ext[2], ext[1] - ext[0])


def draw_contours(axes, ref, low, label, title):
    """Draw the phi = 0 contours of ref (fp64, solid) and low (dashed)."""
    axes.clear()
    axes.contour(ref, levels=[0], colors="k", extent=(0, 1, 0, 1))
    axes.contour(low, levels=[0], colors="tab:red", linestyles="--", extent=(0, 1, 0, 1))
    axes.plot([], [], "k", label="fp64")
    axes.plot([], [], "--", color="tab:red", label=label)
    axes.set(xlim=(0, 1), ylim=(0, 1), aspect="equal", title=title)
    axes.legend(loc="upper right")


def draw_curves(curve, mismatch, history, time):
    """Draw err_band (left axis) and sign_mismatch in % (right axis) up to time."""
    shown = history[history["time"] <= time + 1e-12]
    curve.clear()
    mismatch.clear()
    curve.plot(shown["time"], shown["err_band"], "tab:blue")
    mismatch.plot(shown["time"], 100 * shown["sign_mismatch"], "tab:orange")
    curve.set(xlim=(0, history["time"][-1]), ylim=(0, 1.1 * history["err_band"].max()))
    curve.set(xlabel="t", title="interface error vs fp64")
    curve.set_ylabel("err_band (rms, |phi| < 3 cells)", color="tab:blue")
    mismatch.set_ylim(0, 110 * history["sign_mismatch"].max())
    mismatch.set_ylabel("wrong-sign cells (%)", color="tab:orange")


def main(ref_dir, low_dir, out_dir, label):
    """Write overlay_<step>.png per common frame and overlay.gif to out_dir."""
    ref_dir, low_dir, out_dir = Path(ref_dir), Path(low_dir), Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    steps = sorted(
        {p.name[5:11] for p in ref_dir.glob("dfun_*_0000.vti")}
        & {p.name[5:11] for p in low_dir.glob("dfun_*_0000.vti")}
    )
    history = np.genfromtxt(low_dir / "history.csv", delimiter=",", names=True)
    fig, panels = plt.subplots(1, 2, figsize=(10, 5))
    mismatch = panels[1].twinx()
    gif = PillowWriter(fps=10)
    with gif.saving(fig, str(out_dir / "overlay.gif"), dpi=80):
        for step in steps:
            time, ref = read_vti(ref_dir / f"dfun_{step}_0000.vti")
            low = read_vti(low_dir / f"dfun_{step}_0000.vti")[1]
            draw_contours(panels[0], ref, low, label, f"step {int(step)}, t = {time:.3f}")
            draw_curves(panels[1], mismatch, history, time)
            fig.tight_layout()
            fig.savefig(out_dir / f"overlay_{step}.png", dpi=80)
            gif.grab_frame()
    plt.close(fig)


if __name__ == "__main__":
    ARGS = sys.argv[1:] + ["fp8e4m3"] * (len(sys.argv) == 4)
    main(ARGS[0], ARGS[1], ARGS[2], ARGS[3])
