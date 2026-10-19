# Plan: alphaevolve

**Goal:** evolve the `lowprec` seed under the agreed score, then apply one candidate to `app/src/`.

**Inputs:** `E/kernel.yaml`. **Outputs:** `E/experiment.yaml`, `E/evaluator.py`; generated
files in `E/results/`. Container commands use `bmx` (`AGENTS.md`),
which mounts the host's Google Cloud credentials.

AlphaEvolve runs through the `ae` CLI: GCP generates candidates, and `evaluator.py` scores each one
locally in the container.

## Steps

1. Fill `E/experiment.yaml` and `E/evaluator.py`
   from the templates. Use `kernel.yaml` for the paths and run command. The prompt states exactly
   the rules the evaluator enforces.
2. Calibrate: run `bmx make evaluate EXP=$E` twice; non-timing scores must match exactly.
   Save as `E/results/baseline.json`. **Checkpoint 1.**
3. Red-team in a copy, `rsync -a --exclude .git ./ /tmp/canary/`, one edit at a time
   inside the block:
   - a `double`
   - an inexact unsuffixed literal (`* 1.1`)
   - an `#include`
   - an untagged FP helper
   - skipped work

   Each must score null or below the seed; otherwise tighten the rules and redo step 2.
   **Checkpoint 2.**
4. **Checkpoint 3** (Gate 1).
5. Launch: `bmx make evolve EXP=$E`. This creates the experiment, uploads the seed with its baseline,
   and runs the evaluation loop until `max_programs`. Monitor from another shell with `AE_CONFIG_DIR=$E/results/ae`:
   `ae experiment describe <nick>`, `ae results best <nick>`. **Checkpoint 4** at each progress report.
6. For each top candidate: `ae program show <prog> --output-file $E/results/<prog>.cpp`, diff vs seed,
   re-score in a scratch copy, explain the numerics.
   **Checkpoint 5** (Gate 2).
7. Apply the chosen candidate: show `diff -u`, copy `$E/results/<prog>.cpp` over the seed, run `make check`.
