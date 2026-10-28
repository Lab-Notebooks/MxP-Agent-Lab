# Plan: optimize advectWenoLowPrec (splitting bubble)

## Objective

Evolve the float seed advectWenoLowPrec (app/src/advect.cpp EVOLVE-BLOCK) with AlphaEvolve for
lower error vs the FP64 shadow, score each candidate locally, and apply the one the scientist picks.

## Health check (app.md + preconditions), 2026-10-26

- app.md: pass (raptorize completed); bmx make check 17/17.
- One EVOLVE-BLOCK in src/advect.cpp: pass.
- Seed has no FP64 instruction in *LowPrec* functions: to verify.
- ae, objdump, PyYAML in bmx: to verify.
- bmx gcloud auth: FAIL (scientist must run gcloud auth application-default login).
- Raptorize committed: not applicable (uncommitted base).

## Decisions

- score: -log10 of the RMS over rows of err_rms, mode lowprec, 128^2, 100 steps, 1 rank.
- speed: not counted.
- extra rules: none beyond template.
- hints: none.
- GCP: project/engine to be supplied by scientist; location global; model gemini-3-flash-preview.
- job: max_programs 20, concurrency 4, timeout 600 s.

## Source change

None before Gate 2. New in R = experiments/2026-10-26/2-optimize: experiment.yaml, evaluator.py.

## Steps

1. Write experiment.yaml and evaluator.py.
2. Calibrate: bmx make evaluate EXP=$R twice; save results/baseline.json; pass: identical scores.
3. Red-team in /tmp/canary: double, *1.1, #include, untagged helper double via auto, skip work,
   edit outside block. Pass: each scores null or below seed.
4. Gate 1: show files, baseline, red-team, job. Submit only on approval (needs GCP auth).
5. Launch: bmx make evolve EXP=$R.
6. Monitor and Gate 2 as in system.md.

## Checkpoints

1. Calibration: both baseline scores.
2. Red-team: cheat -> result.
3. Gate 1: submit?
4. Progress.
5. Gate 2: apply candidate.

## Runner

Single agent local steps 1-4; AlphaEvolve swarm step 5.
