"""Evaluator for experiment 2026-10-27/2-optimize (bf16-truncated advectWeno)."""

import math
import re
import os
import sys
from pathlib import Path

if os.environ.get("LAB_ROOT"):
    pass

for directory in [Path.cwd(), *Path(__file__).resolve().parents]:
    if (directory / "app").is_dir() and (directory / "dev/workflows/optimize").is_dir():
        ROOT = directory
        break
else:
    ROOT = Path.cwd()

sys.path.insert(0, str(ROOT / "dev/workflows/optimize"))
import evolve  # pylint: disable=wrong-import-position,import-error

OUTDIR = Path(__file__).resolve().parent / "results/eval"


class Bf16Evaluator(evolve.Evaluator):
    def audit(self):
        blocks = evolve.evolve_blocks(self.appdir / self.source)
        if not blocks:
            return [("MISSING_EVOLVE_BLOCK", f"{self.source} has no EVOLVE-BLOCK.")]
        for index, block in enumerate(blocks, start=1):
            code = evolve.COMMENTS.sub("", block)
            if re.search(r"(?m)^\s*#", code):
                return [("FORBIDDEN_TOKEN", f"preprocessor directive in EVOLVE-BLOCK #{index}.")]
        return []


EVALUATOR = Bf16Evaluator(
    root=ROOT,
    source="src/advect.cpp",
    objfile="build/advect.o",
    command=(
        "mpirun -n 1 ./bubble params/bubble.ini nx=64 ny=64 tmax=1.0 "
        "plot_interval=0 precision=bf16 outdir={out}"
    ).format(out=OUTDIR),
    outdir=OUTDIR,
    metrics=["score", "accuracy"],
)
ERRORS = OUTDIR / "history.csv"


def evaluate():
    failed = EVALUATOR.prepare()
    if failed:
        return failed

    errors = evolve.read_csv(ERRORS).get("err_rms", []) if ERRORS.is_file() else []
    if not errors or not all(math.isfinite(e) for e in errors):
        return EVALUATOR.fail("NAN_OR_INF", f"{ERRORS}: error missing or non-finite.")

    final = errors[-1]
    accuracy = -math.log10(final + 1e-30)

    return EVALUATOR.result(
        {"score": accuracy, "accuracy": accuracy},
        [("SUMMARY", f"{len(errors)} rows, final spatial err_rms vs FP64 shadow {final:.3e}.")],
    )


if __name__ == "__main__":
    EVALUATOR.main(evaluate)
