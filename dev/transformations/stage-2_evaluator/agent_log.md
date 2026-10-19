# Agent Execution Log — `stage-2_evaluator`


## Handoff Context Received from `stage-1_profile` (Run `20261019T053349Z-turbo`, Approved by `Stephen Jacobsohn (Interactive HITL Gate 1)` at `2026-10-19T05:44:17.773683+00:00`)
- **Gate Mode**: `human-hitl-signed`
- **Handoff Grill**: `dev/transformations/stage-1_profile/handoff_grill_to_next_stage.yaml` (`b4cb8d824ec4`)
- **Frozen Evaluator SHA256**: `c17276900ec83eb7`

## Run `20261019T054417Z-turbo` (`2026-10-19T05:44:18.005474+00:00`)
- **Budget Mode**: `turbo`
- **Staged Source**: `dev/agy/src/stage-2_evaluator/`
- **Manifest**: `dev/tmp/runs/stage-2_evaluator/20261019T054417Z-turbo/manifest.json`
- **Metrics**: `{"evaluator_sha256": "c17276900ec83eb72afaeb69ebda2d8291bd14cfcb7f486f36ca951b4d5a7ce9", "toolchain_compiler": "/usr/bin/g++", "baseline_fp64_score": 1.0, "seed_fp32_score": 2.412, "evolved_coupled_score": 15.6374, "cheats_rejected": "4/4", "all_cheats_rejected": true, "deterministic": true}`
- **Contextual Next-Stage `/grill-me` Generated**: `dev/transformations/stage-2_evaluator/handoff_grill_to_next_stage.yaml`

## Handoff Context Received from `stage-1_profile` (Run `20261019T175941Z-turbo`, Approved by `Antigravity-Argon-SingleAgent (gemini-launch-candidate-high)` at `2026-10-19T17:59:41.056968+00:00`)
- **Gate Mode**: `auto-gate-single-agent`
- **Handoff Grill**: `dev/transformations/stage-1_profile/handoff_grill_to_next_stage.yaml` (`b4cb8d824ec4`)
- **Frozen Evaluator SHA256**: `f55655351603b127`

## Run `20261019T175941Z-turbo` (`2026-10-19T17:59:41.369394+00:00`)
- **Budget Mode**: `turbo`
- **Staged Source**: `dev/agy/src/stage-2_evaluator/`
- **Manifest**: `dev/tmp/runs/stage-2_evaluator/20261019T175941Z-turbo/manifest.json`
- **Metrics**: `{"evaluator_sha256": "f55655351603b127647bade744860a02980ea7b0986a3cadde00cf03f7d50bfa", "toolchain_compiler": "/opt/rh/gcc-toolset-15/root/usr/bin/g++", "baseline_fp64_score": 1.0, "seed_fp32_score": 2.412, "evolved_coupled_score": 15.6374, "cheats_rejected": "4/4", "all_cheats_rejected": true, "deterministic": true}`
- **Contextual Next-Stage `/grill-me` Generated**: `dev/transformations/stage-2_evaluator/handoff_grill_to_next_stage.yaml`
