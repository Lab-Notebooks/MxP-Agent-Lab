# Plan: optimize advectWenoLowPrec (interface-aware, 8-bit pathway)

## Objective

Evolve the float seed advectWenoLowPrec (app/src/advect.cpp EVOLVE-BLOCK) with AlphaEvolve
for lower error vs the FP64 shadow, scoring the interface band (err_band), so the candidate
improves the level-set interface that matters for the 8-bit (fp8e4m3) run. Score each candidate
locally; apply the one the scientist picks.

## Health check (app.md), 2026-10-28

- app/ carries run 4's diff (not clean, by design); `make check` 17/17 and `make RAPTOR=0 check` 17/17 (run 4 logs).
- One EVOLVE-BLOCK in src/advect.cpp: pass.
- `ae`, objdump, PyYAML in bmx: to verify.

## Decisions

- score: -log10 of the RMS over rows of `err_band` (history.csv), mode lowprec, the eval run
  below. err_band is RMS |dfun - shadow| over cells with |shadow| < 3 min(dx, dy), so it weights
  the phi = 0 interface. Seed to be calibrated.
- speed: not counted (template default).
- extra rules: none beyond the template.
- hints: none.
- GCP: project <numeric>, engine <id>, models <model>, location global. Scientist supplies.
- job: max_programs 20, concurrency 4, timeout 600 s.

## Source change

None in app/ before Gate 2. New in R = experiments/2026-10-28/5-optimize:

- experiment.yaml from the template: seed `app/src/advect.cpp`, language cpp, job above;
  problem: evolve the block; EVALUATION (build, lowprec run, score); RULES 1-5.
- evaluator.py from the template: source src/advect.cpp, objfile build/advect.o, command
  `mpirun -n 1 ./bubble params/bubble.ini nx=64 ny=64 tmax=2 plot_interval=0
  precision={mode} outdir={out}` with PRECISION=lowprec, ERRORS = OUTDIR/history.csv,
  column err_band; metrics score, accuracy (equal).

## Steps

1. Write experiment.yaml and evaluator.py as above (fill GCP fields).
2. Calibrate: `PRECISION=lowprec ERROR_COLUMN=err_band bmx make evaluate EXP=$R` twice; pass:
   identical scores; save results/baseline.json.
3. Red-team in /tmp/canary (`rsync -a --exclude .git ./ /tmp/canary/`), one edit at a time.
4. Gate 1: show files, baseline, red-team, job. Submit only on approval.
5. Launch (needs GCP fields): `bmx make evolve EXP=$R`.
6. Monitor: `AE_CONFIG_DIR=$R/results/ae ae experiment describe <nick>`, `ae results best <nick>`.
7. Candidates: `ae program show <prog> --output-file results/<prog>.cpp`, diff vs seed, re-score in
   /tmp/canary; apply the chosen one over the seed, then `bmx make check`.

## Checkpoints

1. Calibration: both baseline scores side by side; does the score mean what you intend?
2. Red-team: cheat -> result; any that scores gets a tightened rule.
3. Gate 1: experiment.yaml, evaluator.py, baseline, red-team, job. Submit?
4. Progress: best score per generation; flag jumps. Continue or stop?
5. Gate 2: top candidates as diff vs seed, re-score, one line on the numerics each. Which to apply?

## Runner

Single agent: steps 1-4 and 6-7 (all local, in bmx). AlphaEvolve is the swarm in step 5.
