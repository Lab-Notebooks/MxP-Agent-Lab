# Plan: alphaevolve

**Goal:** evolve the `lowprec` seed under the agreed score, then apply one candidate to `app/src/`.

**Inputs:** `../raptorize/runs/<kernel>/kernel.yaml`. **Outputs:** `runs/<name>/` holding
`experiment.yaml`, `evaluator.py` and `results/`. Container commands use `bmx` (`AGENTS.md`),
which mounts the host's Google Cloud credentials.

AlphaEvolve runs through the `ae` CLI: GCP generates candidates, and `evaluator.py` scores each one
locally in the container.

## Steps

1. With `X=dev/workflows/alphaevolve/runs/<name>`, fill `$X/experiment.yaml` and `$X/evaluator.py`
   from the templates. Use `kernel.yaml` for the paths and run command. The prompt states exactly
   the rules the evaluator enforces.
2. Calibrate: run `bmx make evaluate EXP=$X` twice; non-timing scores must match exactly.
   Save as `$X/results/baseline.json`. **Checkpoint 1.**
3. Red-team in a copy, `rsync -a --exclude .git ./ /tmp/canary_<name>/`, one edit at a time
   inside the block:
   - a `double`
   - an inexact unsuffixed literal (`* 1.1`)
   - an `#include`
   - an untagged FP helper
   - skipped work

   Each must score null or below the seed; otherwise tighten the rules and redo step 2.
   **Checkpoint 2.**
4. **Checkpoint 3** (Gate 1).
5. Launch: `bmx make evolve EXP=$X`. This creates the experiment, uploads the seed with its baseline,
   and runs the evaluation loop until `max_programs`. Monitor from another shell with `AE_CONFIG_DIR=$X/results/ae`:
   `ae experiment describe <nick>`, `ae results best <nick>`. **Checkpoint 4** at each progress report.
6. For each top candidate: `ae program show <prog> --output-file $X/results/<prog>.cpp`, diff vs seed,
   re-score in a scratch copy, explain the numerics.
   **Checkpoint 5** (Gate 2).
7. Apply the chosen candidate: show `diff -u`, copy `$X/results/<prog>.cpp` over the seed, run `make check`.
