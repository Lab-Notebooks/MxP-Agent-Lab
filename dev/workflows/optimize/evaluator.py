"""Module with implementation of the evaluator for experiment <name>.

Copy to E/<n>-optimize/evaluator.py and fill every TODO from the decisions in
raptorize's plan.md and the scientist's metric. The Evaluator builds, audits and runs
the candidate; any failure scores null with the reason, so a broken or
cheating candidate never scores.

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

# TODO: source, run and errors from raptorize's plan.md, metrics from the scientist,
# primary first (ae optimizes the first). Source paths and the command are relative to app/.
OUTDIR = Path(__file__).resolve().parent / "results/eval"
EVALUATOR = evolve.Evaluator(
    root=ROOT,
    source="<kernel file>",
    objfile="<its object file>",
    command="<run>".format(mode="lowprec", out=OUTDIR),
    outdir=OUTDIR,
    metrics=["score", "accuracy"],
)
ERRORS = OUTDIR / "<errors csv>"


def evaluate():
    """
    Score the candidate in the current tree
    """
    failed = EVALUATOR.prepare()
    if failed:
        return failed

    # TODO: the error column against the FP64 shadow
    errors = evolve.read_csv(ERRORS).get("<column>", []) if ERRORS.is_file() else []
    if not errors or not all(math.isfinite(error) for error in errors):
        return EVALUATOR.fail("NAN_OR_INF", f"{ERRORS}: error missing or non-finite.")

    # TODO: the scientist's formula and gates, e.g. + w * log2(speedup) while rms <= tol
    rms = math.sqrt(sum(error * error for error in errors) / len(errors))
    accuracy = -math.log10(rms + 1e-30)

    return EVALUATOR.result(
        {"score": accuracy, "accuracy": accuracy},
        [("SUMMARY", f"{len(errors)} rows, rms error vs FP64 shadow {rms:.3e}.")],
    )


if __name__ == "__main__":
    EVALUATOR.main(evaluate)
