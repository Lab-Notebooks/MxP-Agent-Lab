# Plan: verify

**Goal:** a pass/fail verdict on the applied candidate, from runs, checks and comparisons.

**Inputs:** the candidate commit and `../raptorize/runs/<kernel>/kernel.yaml`.
**Outputs:** `runs/<name>/` holding `verification.yaml`, `check_*.py` and `analysis.md`, plus `<dest>/`.

## Steps

1. Pin the candidate: `git log -1`, `git diff --stat <raptorize commit>`.
2. Fill `runs/<name>/verification.yaml` from the template, with run commands from `kernel.yaml`.
3. For each mechanical property, write `runs/<name>/check_<property>.py` (exit 0 = pass) and list
   it under `checks`. **Checkpoint 1.**
4. Run `bmx make verify SPEC=dev/workflows/verify/runs/<name>/verification.yaml`, which writes
   `<dest>/summary.md` and `<dest>/metrics.json`. **Checkpoint 2.**
5. Write `runs/<name>/analysis.md`: answer the scientist's questions, cite the file behind every
   number, and label hand arguments **informal**. `app/tools/` may help. **Checkpoint 3.**
