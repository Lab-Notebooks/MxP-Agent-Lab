# AI Co-Scientist 6-Stage Workflow — Antigravity + Argon Instructions

You are operating inside the **`MxP-MiniApp-Bubble` 6-Stage AI Co-Scientist Reproducibility Package**.
This package is designed for **Google Antigravity (`antigravity` CLI / `google-antigravity` Python SDK)** paired with **Argon (`gemini-launch-candidate-high`)** and has **zero dependency on internal `jetski` tooling**.

## Mandatory Operational Rules

1. **Skill Activation**:
   - Read and follow [`.agents/skills/mxp-co-scientist-workflow/SKILL.md`](.agents/skills/mxp-co-scientist-workflow/SKILL.md) before executing any stage.
2. **Stage-by-Stage HITL `/grill-me` Gates (`Stage 0` -> `Stage 5`)**:
   - Never jump straight to code mutation. Always progress through:
     - `Stage 0: Bootstrap & Intake` (`python3 dev/workflow.py stage-0_bootstrap run`)
     - `Stage 1: Profile (FPChecker + RAPTOR)` (`python3 dev/workflow.py stage-1_profile run`)
     - `Stage 2: Build & Red-Team Evaluator` (`python3 dev/workflow.py stage-2_evaluator run`)
     - `Stage 3: Run AlphaEvolve (Argon / AE)` (`python3 dev/workflow.py stage-3_evolve run`)
     - `Stage 4: Verify (17-Run MPI Check + Native Plots)` (`python3 dev/workflow.py stage-4_verify run`)
     - `Stage 5: Distill Skill & Container Rollup` (`python3 dev/workflow.py stage-5_package run`)
   - Or run the interactive terminal gate driver directly:
     ```bash
     python3 dev/workflow.py run-interactive --mode turbo
     ```
3. **Mandatory Gate 0 Deliverables Question & Standardized 6-Folder Layout**:
   - At Gate 0, always ask the scientist where deliverables should be stored, defaulting to **Local Machine (`./Deliverables/` or `~/Documents/MxP-MiniApp-Bubble-Deliverables/`)**.
   - Every run must produce the apples-to-apples 6-folder layout (`01_Executive_Summaries_and_Reports` through `06_Skills_and_Archives`) and run the repo's native `tools/plot_compare.py` visualization pipeline first.
