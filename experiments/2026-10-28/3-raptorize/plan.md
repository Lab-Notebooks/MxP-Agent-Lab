# Plan: raptorize advectWeno only (8-bit emulation), run 3

Isolates advection: in 2-raptorize both advectWeno and redistance's tendency ran in fp8, so the
error mixed the two. Here only advectWeno is truncated; redistance always runs in fp64, so
err_rms/err_max measure fp8 advection alone. 2-raptorize's app/ state is in its 3-results.patch.

## Objective

Precision modes fp64, fp8 (E4M3, E5M2) via RAPTOR, and a native float seed, for advectWeno
(src/advect.cpp) only, with an FP64 shadow run and err_rms/err_max columns in history.csv,
without changing fp64 output.

## Health check (app.md), 2026-10-28

app/ carries run 2's diff minus redistance (not clean, by design); gate below re-tests it.

## Decisions

- function, source, object: advectWeno in src/advect.cpp -> build/advect.o. Its FP callees
  in the same file (wenoFaceValue, sqr) are truncated with it; redistance stays fp64 in all modes.
- formats: fp8 E4M3 (4, 3) and fp8 E5M2 (5, 2).
- lowprec: float.
- errors: err_rms, err_max of dfun vs the shadow over owned cells, MPI-reduced, appended to
  history.csv (0 at step 0 and in fp64 mode).
- run, in app/: mpirun -n 1 ./bubble params/bubble.ini nx=64 ny=64 max_steps=50
  plot_interval=0 precision={mode} outdir={out}. Small and deterministic.

## Source change

- redistance.cpp, redistance.h: back to HEAD (no RAPTOR code).
- driver.cpp: advectWenoAt dispatch and fp64 shadow; redistance call as in HEAD.
- Makefile: RAPTOR compile for advect.o only.
- advect.cpp/.h, params.h, io.cpp/.h: as in 2-raptorize.

## Steps

1. Self-check: git diff -U0 app | grep '^-[^-]'; each removed line has a RAPTOR: was twin.
2. In bmx: make RAPTOR=0 -j check, then make clean check from the repo root (RAPTOR=1; compile
   lines show -fpass-plugin).
3. Eval run per mode with {out}=R/results/<mode>; pass: fp64 equals 2-raptorize's fp64, other
   modes finite nonzero error, fp8 area still changing at step 50.
4. Animation: fp64 and fp8e4m3, nx=64 ny=64 tmax=2 plot_interval=20 hist_interval=10,
   {out}=R/results/anim_<mode>; python3 R/overlay.py draws both phi = 0 contours per frame to
   R/results/overlay/ and overlay.gif.
5. Save patches.

## Checkpoints

1. Scope: files, callees.
   Outcome: advect.cpp (advectWeno, wenoFaceValue, sqr truncated); redistance.* at HEAD.
2. Diff: git diff --stat, RAPTOR: was lines.
   Outcome: 7 files, +206 -6; all 6 removed lines have RAPTOR: was twins.
3. Results: pass/fail and final errors.
   Outcome: make RAPTOR=0 check 17/17, bmx make check (RAPTOR=1, -fpass-plugin) 17/17. Step 50
   (fp64 identical to 2-raptorize):

   | mode | area | err_rms | err_max | 2-raptorize err_rms |
   |------|------|---------|---------|---------------------|
   | fp64 | 0.1258795 | 0 | 0 | 0 |
   | fp8e4m3 | 0.1260227 | 9.15e-4 | 4.37e-3 | 4.70e-2 |
   | fp8e5m2 | 0.1258520 | 1.07e-3 | 6.62e-3 | 9.27e-2 |
   | lowprec | 0.1258795 | 2.94e-10 | 1.47e-9 | 1.06e-8 |

   Animation to t = 2: area fp64 0.1772, fp8e4m3 0.2229; err_rms 9.1e-3 at t = 0.5, peak 8.9e-2
   at t = 1, 3.2e-2 at t = 2 (run 2: 3.3e-2). fp8 splits like fp64; spurious phi = 0 islands
   remain along the thin filament (results/overlay/overlay.gif).

## Runner

Single agent, all steps in one bmx container, nothing else using app/.
