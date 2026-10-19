---
name: mxp-co-scientist-workflow
description: "6-Stage AI Co-Scientist Workflow for Scientific Software Engineering & Mixed-Precision / Accelerator Evolution. Combines Stage 0 tailored /grill-me intake, hardware refactor assessment, cheap/balanced/turbo budget estimation, and dependency provisioning with Stage 1 Profiling (FPChecker/RAPTOR), Stage 2 Red-Teamed Unified Evaluator, Stage 3 AlphaEvolve Discovery Engine search, Stage 4 MPI/Proof Verification, and Stage 5 Skill & Container Rollup — with a contextual HITL /grill-me gate at every stage boundary."
license: Apache-2.0
metadata:
  version: v2.0
  publisher: argonclaw-co-scientist
  paper_reference: "arXiv:2610.09995v2 + ArgonClaw Swarm Architecture"
---

# AI Co-Scientist 6-Stage Workflow (`mxp-co-scientist-workflow`)

Use this skill whenever a scientist or engineer wants to profile, refactor, evolve (via AlphaEvolve), or verify a scientific computing codebase with **provable numerical guarantees**, **contextual human-in-the-loop (`/grill-me`) stage gates**, and **reproducible manifest provenance**.

## Core Philosophy (Merging `arXiv:2610.09995` & ArgonClaw Swarm)
1. **Staged Code Freedom with Baseline Sanctity**:
   - Root `src/` stays the untouched reference baseline until Stage 4 promotion.
   - Agents have full code-modification freedom inside isolated stage trees (`dev/agy/src/stage-0_bootstrap` .. `dev/agy/src/stage-5_package`).
2. **Contextual HITL `/grill-me` Gate at Every Stage Boundary**:
   - Every stage $N$ produces `dev/tmp/runs/<stage-N>/<run-id>/manifest.json` and dynamically generates `dev/transformations/<stage-N>/handoff_grill_to_next_stage.yaml`, using Stage $N$'s actual empirical metrics to ask the scientist tailored questions that shape Stage $N+1$'s contract (`desired_spec.md` & `current_plan.md`).
   - By default, `dev/workflow.py` pauses at each HITL gate for scientist sign-off (`approvals.toml`), or can optionally skip/auto-approve via `--skip-hitl-gate` or `--auto-gate-with-council` (Multi-Model Council: Gemini 4 Argon + Claude Opus 5.5 + Claude Fable 5.1).
3. **Hardware Refactor & Dependency Capture (`Stage 0` -> `Stage 5`)**:
   - Stage 0 detects target hardware (CPU AVX-512, MPI, GPU, Cloud TPU), assesses whether pre-evolution structural refactoring is needed, estimates token & AlphaEvolve budgets (`cheap`, `balanced`, `turbo`), and locks all required checkers (`FPChecker`, `RAPTOR`, compilers, `ae` CLI) in `dependencies.lock.toml` so Stage 5 can bake them into a portable container image.

## Quickstart Commands
```bash
# 1. Estimate token & AlphaEvolve budget across Cheap, Balanced, and Turbo modes
python3 dev/workflow.py estimate --mode turbo

# 2. Run Stage 0..5 with interactive HITL /grill-me gates at each boundary
python3 dev/workflow.py stage-0_bootstrap run --mode turbo
python3 dev/workflow.py stage-0_bootstrap gate      # Inspect contextual /grill-me for Stage 0 -> 1
python3 dev/workflow.py stage-0_bootstrap approve   # Sign approvals.toml

# 3. Run all 6 stages end-to-end with Multi-Model Council auto-gate sign-off
python3 dev/workflow.py run-all --mode turbo --auto-gate-with-council

# 4. Scaffold this 6-stage Co-Scientist workflow into a brand-new scientific repository
python3 dev/workflow.py init-project /path/to/new-scientific-repo
```


## Mandatory Gate 0 Rule: Deliverables Location (Default = Local Machine)
- During **Stage 0 (`/grill-me` Intake)**, you **MUST** ask the user where all deliverables (final evolved code, speedup logs, simulation images/GIFs, raw field outputs, and Stage 5 packages) should live, with **Local Machine (`~/Documents/<project>-Deliverables/`)** as the `#1 (Recommended)` default option.
- Even when compute or AlphaEvolve runs execute on remote Cloudtop/Argolis/Swarm nodes, you **MUST** automatically `rsync` all stage deliverables, plots, VTI/CSV outputs, and evolved source files back to the local machine directory at the end of each stage and display the speedup metrics, terminal outputs, final code, and simulated images directly to the user.
