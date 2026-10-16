# AI Co-Scientist 6-Stage Workflow (`dev/`)

This directory implements the merged **Lab-Notebook / CodeScribe (`arXiv:2610.09995v2` & `adhruv/co-scientist-workflow`) + ArgonClaw Antigravity Swarm** architecture for `MxP-MiniApp-Bubble`.

## 6-Stage Tranche & Contextual HITL `/grill-me` Gate Architecture

| Stage | Staged Source Tree | What the Scientist / `/grill-me` Decides | What the Agents Produce | Contextual HITL Gate (`approvals.toml` + `handoff_grill_to_next_stage.yaml`) |
| :--- | :--- | :--- | :--- | :--- |
| **0. Bootstrap & Deps** | `dev/agy/src/stage-0_bootstrap/` | Physics invariants, hardware refactor check (`CPU/MPI/GPU/TPU`), budget mode (`cheap`/`balanced`/`turbo`), required checkers | `dependencies.lock.toml`, `grill_interview.yaml`, `budget_estimate.json` | *"Are the invariants, hardware refactor plan, budget mode, and checkers (`FPChecker`, `RAPTOR`, `ae` CLI) locked?"* |
| **1. Profile** | `dev/agy/src/stage-1_profile/` | Kernels in scope (`advect`, `redistance`, `init`, `driver`), target formats (`FP32`, `BF16`, `FP16`), grid sizes | `profile.json` (wall-clock shares, `FPChecker` cancellation sites, `RAPTOR` bit-width sweep, proposed `EVOLVE-BLOCK`s) | *"Given Stage 1 hotspot shares (`redistance` 54.8%, `advect` 31.4%) and naive FP32 `RMSE = 3.84e-3`, lock Stage 2 evaluator tolerances?"* |
| **2. Build Evaluator** | `dev/agy/src/stage-2_evaluator/` | Primary metric (`RMSE <= 1e-7`), hard rules (`0` FP64 ops), adversarial cheat vectors | Unified `dev/evaluators/evaluator.py` + `calibration.json` + 3/3 Red-Team cheats rejected (`score = 0.0`) | *"Does `evaluator.py` (`SHA256: c1727690...`) reject all 3 cheat vectors and calibrate baseline/seed deterministically?"* |
| **3. Run AlphaEvolve** | `dev/agy/src/stage-3_evolve/` | Evolution hints (`TwoSum`/`Ozaki`), generations/population budget, live endpoint (`603936599140`) | Top-k candidates in `dev/tmp/runs/stage-3_evolve/<run-id>/candidates/` (`score = 15.6374`, `RMSE = 2.41e-8`, `0` FP64 ops) | *"Promote winning candidate `cand_01_ozaki_twosum_fp32` (`score = 15.6374`) to Stage 4 multi-rank & proof verification?"* |
| **4. Verify & Analyze** | `dev/agy/src/stage-4_verify/` | MPI ranks (`np=1,2,4`), grid convergence (`64²..256²`), geometries, proofs | `mpi_check_np1_2_4.json`, script-driven studies, proofs, programmatic `CO_SCIENTIST_VERIFICATION_REPORT.md` | *"All MPI, convergence, and proof gates passed with zero number drift — proceed to Stage 5 packaging?"* |
| **5. Skill & Container Rollup** | `dev/agy/src/stage-5_package/` | Skill export scope & container base image | `dev/skill-distillation/mxp-co-scientist-workflow/SKILL.md` + `container_rollup/Containerfile` + `init-project` CLI | *"Are the portable Antigravity skill and container image ready to share or join a swarm?"* |

## Single CLI Entrypoint (`dev/workflow.py`)
```bash
# Estimate LLM token & AlphaEvolve budget across Cheap ($4.11), Balanced ($18.34), and Turbo ($72.97) modes
python3 dev/workflow.py estimate --mode turbo

# Run any individual stage (pauses at HITL gate by default)
python3 dev/workflow.py stage-1_profile run --mode balanced
python3 dev/workflow.py stage-1_profile gate      # View Stage 1 -> 2 contextual /grill-me questions
python3 dev/workflow.py stage-1_profile approve   # Sign approvals.toml

# Run all 6 stages end-to-end with Multi-Model Council (Gemini 4 Argon + Opus 5.5 + Fable 5.1) auto-gate
python3 dev/workflow.py run-all --mode turbo --auto-gate-with-council

# Bootstrap a brand-new scientific repository with this 6-stage Co-Scientist architecture
python3 dev/workflow.py init-project /path/to/new-scientific-repo
```
