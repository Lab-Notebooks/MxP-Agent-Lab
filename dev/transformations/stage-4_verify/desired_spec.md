# Spec: Stage 4: Verify (MPI np=1,2,4), Prove & Generate Data-Driven Report (`stage-4_verify`)

## 1. Contract
- **Stage**: `stage-4_verify` (Tranche 4 of 5)
- **Staged Source Directory**: `dev/agy/src/stage-4_verify/`
- **Run Artifacts Directory**: `dev/tmp/runs/stage-4_verify/<run-id>/`
- **Gate Question**: *"Do I merge this verified candidate into src/ via a promotion PR?"*
- **Authority**: `desired_spec.md`, `current_plan.md`, `loop.toml`, and `approvals.toml` are human-owned (or locked at Stage 0 bootstrap). `agent_log.md` is the only file in `dev/transformations/stage-4_verify/` that agents may modify during execution.

## 2. Output Shape
- Deterministic `dev/tmp/runs/stage-4_verify/<run-id>/manifest.json` recording `git_sha`, `spec_sha256`, `plan_sha256`, `evaluator_sha256`, `budget_mode`, `commands_executed`, `metrics`, and `status`.
- Stage-specific code and verification artifacts staged cleanly inside `dev/agy/src/stage-4_verify/` and `dev/tmp/runs/stage-4_verify/<run-id>/`.

## 3. Type Conversions & Caveats
- All evolved hot-loop computations in `advect`, `redistance`, `init`, and `driver` must execute with **0 FP64 (`double`) operations** inside `EVOLVE-BLOCK`s.
- Compensated summation (`TwoSum`, `FastTwoSum`, Ozaki split-accumulators) in `float` (FP32) is permitted and encouraged to preserve high-order WENO5 and RK3 accuracy.

## 4. Correctness Bar
- Global level-set error vs. FP64 reference baseline: `RMSE(phi) <= 1.0e-7` at `T = 1.0` (`128x128` grid).
- Velocity divergence: `L_inf(div u) <= 1.0e-12`.
- All 3 adversarial red-team cheat candidates must score `0.0`.
