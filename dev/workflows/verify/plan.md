# Plan: verify

**Goal:** a pass/fail verdict on the applied candidate, from runs, checks and comparisons.

**Inputs:** the candidate commit and `E/kernel.yaml`.
**Outputs:** `E/verification.yaml`, `E/check_*.py`, `E/analysis.md`; generated files in `E/results/verify/`.

## Steps

1. Pin the candidate: `git log -1`, `git diff --stat <raptorize commit>`.
2. Fill `E/verification.yaml` from the template, with run commands from `kernel.yaml`.
3. For each mechanical property, write `E/check_<property>.py` (exit 0 = pass) and list
   it under `checks`. **Checkpoint 1.**
4. Run `bmx make verify SPEC=$E/verification.yaml`, which writes
   `E/results/verify/summary.md` and `metrics.json`. **Checkpoint 2.**
5. Write `E/analysis.md`: answer the scientist's questions, cite the file behind every
   number, and label hand arguments **informal**. `app/tools/` may help. **Checkpoint 3.**
