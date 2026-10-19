"""Module with implementation of the evaluator for experiment <name>.

Copy to runs/<name>/evaluator.py and fill every TODO from the raptorize
kernel.yaml and the scientist's metric. The Evaluator builds, audits and runs
the candidate; any failure scores every metric None with the reason, so a
broken or cheating candidate never scores.

ae runs `evaluator.py --program-dir <dir> --output-file <json>`; without
arguments it prints the result for the current tree.
"""

import math
import os
import sys
from pathlib import Path


def repo_root():
    """
    Repository root: $LAB_ROOT, else the first parent with app/ and dev/tools/local/
    """
    if os.environ.get("LAB_ROOT"):
        return Path(os.environ["LAB_ROOT"]).resolve()

    for directory in [Path.cwd(), *Path(__file__).resolve().parents]:
        if (directory / "app").is_dir() and (directory / "dev/tools/local").is_dir():
            return directory

    return Path.cwd()


ROOT = repo_root()
sys.path.insert(0, str(ROOT / "dev/tools/local"))

import evolve  # pylint: disable=wrong-import-position,import-error

# TODO: settings from dev/workflows/raptorize/runs/<kernel>/kernel.yaml,
# metrics from the scientist, primary first (ae optimizes the first).
# Paths and the command are relative to app/.
OUTDIR = "out_eval_<name>"
EVALUATOR = evolve.Evaluator(
    root=ROOT,
    source="src/<file>",
    objfile="build/<file>.o",
    command="<kernel.yaml run>".format(mode="lowprec", out=OUTDIR),
    outdir=OUTDIR,
    metrics=["score", "accuracy"],
)
ERRORS = ROOT / evolve.APP / OUTDIR / "<errors csv>"


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
