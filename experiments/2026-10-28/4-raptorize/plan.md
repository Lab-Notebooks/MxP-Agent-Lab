# Plan: raptorize advectWeno only, interface error columns, run 4

Loop-back of 3-raptorize for the optimize score: err_rms is dominated by far-field cells
(|phi| > 0.1) after t ~ 0.6 and falls at t = 2 while the interface worsens (3-raptorize frames:
band error 1.3e-2 -> 2.0e-2, wrong-sign cells 7% -> 16% from t = 1 to 2). Adds two columns that
measure the interface. Kernel, modes and shadow as in run 3.

## Objective

Precision modes fp64, fp8 (E4M3, E5M2) via RAPTOR, and a native float seed, for advectWeno
(src/advect.cpp) only, with an FP64 shadow run and err_rms/err_max columns in history.csv,
without changing fp64 output.

## Health check (app.md), 2026-10-28

app/ carries run 3's diff (not clean, by design); the gate below re-tests it.

## Decisions

- function, source, object: advectWeno in src/advect.cpp -> build/advect.o. Its FP callees
  in the same file (wenoFaceValue, sqr) are truncated with it; redistance stays fp64 in all modes.
- formats: fp8 E4M3 (4, 3) and fp8 E5M2 (5, 2).
- lowprec: float.
- errors: err_rms, err_max of dfun vs the shadow over owned cells; err_band, the rms over cells
  with |shadow| < 3 min(dx, dy); sign_mismatch, the fraction of cells where dfun and shadow
  differ in sign. MPI-reduced, appended to history.csv (0 at step 0 and in fp64 mode).
- run, in app/: mpirun -n 1 ./bubble params/bubble.ini nx=64 ny=64 max_steps=50
  plot_interval=0 precision={mode} outdir={out}. Small and deterministic.

## Source change

- io.h: Diagnostics.err_band, sign_mismatch.
- io.cpp: errorsVsShadow computes them; two history.csv columns.
- Everything else as in 3-raptorize.

## Steps

1. Self-check: git diff -U0 app | grep '^-[^-]'; each removed line has a RAPTOR: was twin.
2. In bmx: make RAPTOR=0 -j check, then make clean check from the repo root (RAPTOR=1; compile
   lines show -fpass-plugin).
3. Eval run per mode with {out}=R/results/<mode>; pass: fp64 columns 1-9 equal 3-raptorize's fp64, other
   modes finite nonzero error, fp8 area still changing at step 50.
4. Animation: fp64 and fp8e4m3, nx=64 ny=64 tmax=2 plot_interval=20 hist_interval=10,
   {out}=R/results/anim_<mode>; python3 R/overlay.py draws both phi = 0 contours per frame to
   R/results/overlay/ and overlay.gif, with err_band and sign_mismatch vs time beside it.
5. Save patches.

## Checkpoints

1. Scope: files, callees.
   Outcome: advect.cpp (advectWeno, wenoFaceValue, sqr truncated); redistance stays fp64 in all modes;
   io.cpp/.h add err_rms, err_max, err_band, sign_mismatch.
2. Diff: git diff --stat, RAPTOR: was lines.
   Outcome: 7 files, +225 -6; all 6 removed lines have RAPTOR: was twins (run 3's).
3. Results: pass/fail and final errors.
   Outcome: make RAPTOR=0 check 17/17, bmx make check 17/17. Step 50:

   | mode | area | err_rms | err_max | err_band | sign_mismatch |
   |------|------|---------|---------|----------|---------------|
   | fp64 | 0.1258795 | 0 | 0 | 0 | 0 |
   | fp8e4m3 | 0.1260227 | 9.15e-4 | 4.37e-3 | 1.32e-3 | 0 |
   | fp8e5m2 | 0.1258520 | 1.07e-3 | 6.62e-3 | 1.05e-3 | 9.77e-4 |
   | lowprec | 0.1258795 | 2.94e-10 | 1.47e-9 | 2.09e-10 | 0 |

   Animation to t = 2: area fp64 0.1772, fp8e4m3 0.2229; err_rms 3.2e-2, err_band 2.0e-2,
   sign_mismatch 16%. Overlay frames + err_band/sign_mismatch curves saved.

## Runner

Single agent, all steps in one bmx container, nothing else using app/.
