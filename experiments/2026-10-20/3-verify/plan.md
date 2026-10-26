# Plan: verify humongous-macaw

## Objective

Decide from data whether the applied candidate (humongous-macaw, 2-optimize Gate 2) merges: does
its gain over the seed hold beyond the 100-step eval run, and why does it behave as it does.

## Health check (app.md), 2026-10-22

- app.md: pass (1-raptorize/plan.md); `bmx make check` 17/17 with the candidate applied.
- Candidate in app/: pass, uncommitted on HEAD 05e8cfa (= 2-optimize/patches/5-gate-2.patch).

## Decisions

Defaults proposed by the agent (scientist: "run verification on these results"); limits are
"no worse than the seed", since system.md leaves them to the scientist.

- runs, in app/, mode lowprec, `params/bubble.ini nx=128 ny=128 plot_interval=... precision=lowprec`:
  eval (100 steps, 1 rank); np2, np4 (eval at 2, 4 ranks); nx256 (256², 200 steps, 1 rank:
  2x resolution, same t as dt halves); full (to t = 2, 2 ranks, `--bind-to none`, plot_interval
  1024: raptorize step 6).
- seed references: 1-raptorize/results/lowprec (eval), full_lowprec, full_fp64 (t = 2); seed at
  nx256 run once by the agent in /tmp/canary into results/seed_nx256 (the seed is not in app/).
- limits:
  - eval, nx256: RMS over steps of err_rms <= the seed's.
  - full: RMS over steps and final err_rms <= the seed's.
  - conservation at t = 2: |area - area_fp64| / area_fp64 and the same for mass <= 1e-5
    (fp32 level: 1.6e-6, 8.2e-6; bf16: 2.6e-3, 6.0e-3).
- run vs run: np2, np4 vs eval: dfun at step 100 bit-identical; err_rms relative <= 1e-12 (MPI
  sum order; seed: 1.7e-14).
- properties: the candidate's face value equals the seed's in exact arithmetic (sympy, both
  transcribed from src/advect.cpp); WENO order is covered by make check (test_weno_order).
- questions: does the gain hold at t = 2, at 2x resolution and across ranks; why does the
  rewrite lower the error; where is the error (near the interface or away from it).

## Source change

None in app/. New in R = experiments/2026-10-20/3-verify: verification.py (template), analysis.md,
results/plots.py (figures for analysis.md, copied to E/figures as verify_*.png).

## Steps

1. Pin: `git log -1`, `git status app`, `make patch OUT=$R/patches/0-pin.patch`.
2. Seed reference: in /tmp/canary (seed), the nx256 run with {out} = $R/results/seed_nx256.
3. Fill verification.py as above; black, pylint.
4. `bmx make verify EXP=$R`; pass: every check PASS in results/verify/summary.md.
5. `python3 results/plots.py`: error history (seed, candidate, fp32), interface-band error.
6. analysis.md: answers to the questions, every number cited to its file.

## Checkpoints

1. Script: the filled verification.py. Run?
   Outcome: filled, black + pylint 10/10; sympy check tested against a perturbed formula; run
   (scientist: run verification and keep working).
2. Results: summary.md and any failing step with its log line.
   Outcome: 19/19 PASS (results/verify/summary.md). Not a check: an error jump at t = 1.45
   (seed) / 1.51 (candidate) from a sign flip in the FP64 redistance; see analysis.md.
3. Gate: analysis.md. Merge, or back to optimize with which new constraints?
   Outcome: pending; agent recommends merge, and a score that avoids the sign-flip regime.

## Runner

Single agent, all steps: sequential, local, in bmx.
