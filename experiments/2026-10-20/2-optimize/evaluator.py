"""Module with implementation of the evaluator for experiment 2026-10-20/2-optimize.

Builds, audits and runs advectWenoLowPrec (app/src/advect.cpp); any failure scores null
with the reason, so a broken or cheating candidate never scores.

ae runs `evaluator.py --program-dir <dir> --output-file <json>`; without
arguments it prints the result for the current tree.
"""

import math
import os
import sys
from pathlib import Path


def repo_root():
    """
    Repository root: $LAB_ROOT, else the first parent with app/ and dev/workflows/optimize/
    """
    if os.environ.get("LAB_ROOT"):
        return Path(os.environ["LAB_ROOT"]).resolve()

    for directory in [Path.cwd(), *Path(__file__).resolve().parents]:
        if (directory / "app").is_dir() and (
            directory / "dev/workflows/optimize"
        ).is_dir():
            return directory

    return Path.cwd()


ROOT = repo_root()
sys.path.insert(0, str(ROOT / "dev/workflows/optimize"))

import evolve  # pylint: disable=wrong-import-position,import-error

OUTDIR = Path(__file__).resolve().parent / "results/eval"
EVALUATOR = evolve.Evaluator(
    root=ROOT,
    source="src/advect.cpp",
    objfile="build/advect.o",
    command=(
        "mpirun -n 1 ./bubble params/bubble.ini nx=128 ny=128 max_steps=100 "
        "plot_interval=0 precision={mode} outdir={out}"
    ).format(mode="lowprec", out=OUTDIR),
    outdir=OUTDIR,
    metrics=["score", "accuracy"],
)
ERRORS = OUTDIR / "history.csv"


def evaluate():
    """
    Score the candidate in the current tree
    """
    failed = EVALUATOR.prepare()
    if failed:
        return failed

    errors = evolve.read_csv(ERRORS).get("err_rms", []) if ERRORS.is_file() else []
    if not errors or not all(math.isfinite(error) for error in errors):
        return EVALUATOR.fail("NAN_OR_INF", f"{ERRORS}: error missing or non-finite.")

    rms = math.sqrt(sum(error * error for error in errors) / len(errors))
    accuracy = -math.log10(rms + 1e-30)

    return EVALUATOR.result(
        {"score": accuracy, "accuracy": accuracy},
        [("SUMMARY", f"{len(errors)} rows, rms error vs FP64 shadow {rms:.3e}.")],
    )


if __name__ == "__main__":
    EVALUATOR.main(evaluate)
