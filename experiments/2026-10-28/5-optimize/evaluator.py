"""Module with implementation of the evaluator for experiment 2026-10-28/5-optimize.

Builds, audits and runs advectWenoLowPrec (app/src/advect.cpp); any failure scores null
with the reason, so a broken or cheating candidate never scores.
"""

import math
import os
import sys
from pathlib import Path

def repo_root():
    if os.environ.get("LAB_ROOT"):
        return Path(os.environ["LAB_ROOT"]).resolve()
    for directory in [Path.cwd(), *Path(__file__).resolve().parents]:
        if (directory / "app").is_dir() and (directory / "dev/workflows/optimize").is_dir():
            return directory
    return Path.cwd()

ROOT = repo_root()
sys.path.insert(0, str(ROOT / "dev/workflows/optimize"))

import evolve

MODE = os.environ.get("PRECISION", "lowprec")
COLUMN = os.environ.get("ERROR_COLUMN", "err_band")

OUTDIR = Path(__file__).resolve().parent / "results/eval"
EVALUATOR = evolve.Evaluator(
    root=ROOT,
    source="src/advect.cpp",
    objfile="build/advect.o",
    command=(
        "mpirun -n 1 ./bubble params/bubble.ini nx=64 ny=64 tmax=2 "
        "plot_interval=0 precision={mode} outdir={out}"
    ).format(mode=MODE, out=OUTDIR),
    outdir=OUTDIR,
    metrics=["score", "accuracy"],
)
ERRORS = OUTDIR / "history.csv"


def evaluate():
    failed = EVALUATOR.prepare()
    if failed:
        return failed

    errors = evolve.read_csv(ERRORS).get(COLUMN, []) if ERRORS.is_file() else []
    if not errors or not all(math.isfinite(error) for error in errors):
        return EVALUATOR.fail("NAN_OR_INF", f"{ERRORS}: error missing or non-finite.")

    rms = math.sqrt(sum(error * error for error in errors) / len(errors))
    accuracy = -math.log10(rms + 1e-30)

    return EVALUATOR.result(
        {"score": accuracy, "accuracy": accuracy},
        [("SUMMARY", f"{len(errors)} rows, rms {COLUMN} vs FP64 shadow {rms:.3e}.")],
    )


if __name__ == "__main__":
    EVALUATOR.main(evaluate)
