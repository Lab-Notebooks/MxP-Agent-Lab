# Plan: Stage 4: Verify (MPI np=1,2,4), Prove & Generate Data-Driven Report (`stage-4_verify`)

1. **What to do each round**:
   - Read `desired_spec.md`, `dev/params/toolchain.toml`, `dev/params/budget_modes.toml`, and `dev/params/guardrails.toml`.
   - Execute the bounded stage tasks inside `dev/agy/src/stage-4_verify/` and write outputs to `dev/tmp/runs/stage-4_verify/<run-id>/`.
2. **How to write the agent log file**:
   - Append structured round entries (timestamp, active agent/container, tool calls, numerical checks, open items) to `agent_log.md`.
3. **How to check for human / council approvals**:
   - Run `dev/workflow.py stage-4_verify gate` to verify `approvals.toml` before advancing to the next stage.
4. **How to use tools**:
   - Invoke `dev/workflow.py`, `dev/evaluators/evaluator.py`, and `/opt/argonclaw/.venv/bin/ae` using portable paths from `dev/params/toolchain.toml`.
5. **How to select units of work**:
   - Follow hotspot ranking (`advect` -> `redistance` -> `init` -> `driver` -> `coupled`).
6. **How to verify results**:
   - Validate all numerical metrics and SHA-256 hashes in `manifest.json`.
7. **When to stop**:
   - Stop immediately when all acceptance criteria in `desired_spec.md` pass or when `max_cycles` in `loop.toml` is reached.
