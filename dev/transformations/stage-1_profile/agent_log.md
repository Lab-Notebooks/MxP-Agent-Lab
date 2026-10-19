# Agent Execution Log — `stage-1_profile`


## Handoff Context Received from `stage-0_bootstrap` (Run `20261019T053255Z-turbo`, Approved by `Stephen Jacobsohn (Interactive HITL Gate 0)` at `2026-10-19T05:33:09.699501+00:00`)
- **Gate Mode**: `human-hitl-signed`
- **Handoff Grill**: `dev/transformations/stage-0_bootstrap/handoff_grill_to_next_stage.yaml` (`b2bb59d84a72`)
- **Frozen Evaluator SHA256**: `c17276900ec83eb7`

## Run `20261019T053349Z-turbo` (`2026-10-19T05:33:49.548270+00:00`)
- **Budget Mode**: `turbo`
- **Staged Source**: `dev/agy/src/stage-1_profile/`
- **Manifest**: `dev/tmp/runs/stage-1_profile/20261019T053349Z-turbo/manifest.json`
- **Metrics**: `{"hotspot_runtime_share_pct": {"redistance": 54.8, "advect": 31.4, "driver": 11.2, "init": 2.6}, "raptor_naive_fp32_rmse": 0.00384, "fpchecker_cancellation_sites": ["advect.cpp: WENO5 smoothness indicators beta_0, beta_1, beta_2 near interface", "redistance.cpp: Godunov Hamiltonian |grad(phi)| - 1 sign-change stencil", "driver.cpp: RK3 stage convex combination accumulation"], "proposed_evolve_blocks_count": 4}`
- **Contextual Next-Stage `/grill-me` Generated**: `dev/transformations/stage-1_profile/handoff_grill_to_next_stage.yaml`

## Handoff Context Received from `stage-0_bootstrap` (Run `20261019T175940Z-turbo`, Approved by `Antigravity-Argon-SingleAgent (gemini-launch-candidate-high)` at `2026-10-19T17:59:41.030076+00:00`)
- **Gate Mode**: `auto-gate-single-agent`
- **Handoff Grill**: `dev/transformations/stage-0_bootstrap/handoff_grill_to_next_stage.yaml` (`608e660686ed`)
- **Frozen Evaluator SHA256**: `f55655351603b127`

## Run `20261019T175941Z-turbo` (`2026-10-19T17:59:41.054833+00:00`)
- **Budget Mode**: `turbo`
- **Staged Source**: `dev/agy/src/stage-1_profile/`
- **Manifest**: `dev/tmp/runs/stage-1_profile/20261019T175941Z-turbo/manifest.json`
- **Metrics**: `{"hotspot_runtime_share_pct": {"redistance": 54.8, "advect": 31.4, "driver": 11.2, "init": 2.6}, "raptor_naive_fp32_rmse": 0.00384, "fpchecker_cancellation_sites": ["advect.cpp: WENO5 smoothness indicators beta_0, beta_1, beta_2 near interface", "redistance.cpp: Godunov Hamiltonian |grad(phi)| - 1 sign-change stencil", "driver.cpp: RK3 stage convex combination accumulation"], "proposed_evolve_blocks_count": 4}`
- **Contextual Next-Stage `/grill-me` Generated**: `dev/transformations/stage-1_profile/handoff_grill_to_next_stage.yaml`
