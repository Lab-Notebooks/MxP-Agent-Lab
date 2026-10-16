# Spec: Stage 5: Distill Reusable Antigravity Skill & Containerized Swarm Rollup (`stage-5_package`)

## 1. Contract
- **Stage**: `stage-5_package` (Tranche 5 of 5)
- **Staged Source Directory**: `dev/agy/src/stage-5_package/`
- **Run Artifacts Directory**: `dev/tmp/runs/stage-5_package/<run-id>/`
- **Gate Question**: *"Are the portable Antigravity skill and container image ready to share or join a swarm?"*
- **Authority**: `desired_spec.md`, `current_plan.md`, `loop.toml`, and `approvals.toml` are human-owned (or locked at Stage 0 bootstrap). `agent_log.md` is the only file in `dev/transformations/stage-5_package/` that agents may modify during execution.

## 2. Output Shape
- Deterministic `dev/tmp/runs/stage-5_package/<run-id>/manifest.json` recording `git_sha`, `spec_sha256`, `plan_sha256`, `evaluator_sha256`, `budget_mode`, `commands_executed`, `metrics`, and `status`.
- Stage-specific code and verification artifacts staged cleanly inside `dev/agy/src/stage-5_package/` and `dev/tmp/runs/stage-5_package/<run-id>/`.

## 3. Type Conversions & Caveats
- All evolved hot-loop computations in `advect`, `redistance`, `init`, and `driver` must execute with **0 FP64 (`double`) operations** inside `EVOLVE-BLOCK`s.
- Compensated summation (`TwoSum`, `FastTwoSum`, Ozaki split-accumulators) in `float` (FP32) is permitted and encouraged to preserve high-order WENO5 and RK3 accuracy.

## 4. Correctness Bar
- Global level-set error vs. FP64 reference baseline: `RMSE(phi) <= 1.0e-7` at `T = 1.0` (`128x128` grid).
- Velocity divergence: `L_inf(div u) <= 1.0e-12`.
- All 3 adversarial red-team cheat candidates must score `0.0`.
