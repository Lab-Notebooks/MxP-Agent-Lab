# Plan: raptorize advectWeno and redistance (8-bit emulation), run 2

Loop-back of 1-raptorize: there fp8 truncated all of redistance, so `sm - lsDT*sgn*agf`
rounded phi to fp8 every step (0.031 apart near |phi| = 0.3); advection's ~4e-4 per step was
rounded away and the bubble froze after ~20 steps (1-raptorize/results/anim_fp8). Fix: as in
advect (fp8 rhs, fp64 Euler), truncate only redistance's tendency sgn*agf; phi and
lsDT*tendency stay fp64.

## Objective

Add precision modes fp64, fp8 (E4M3, E5M2) via RAPTOR, and a native float seed, to
advectWeno (src/advect.cpp) and redistance (src/redistance.cpp), an FP64 shadow run,
and err_rms/err_max columns in history.csv, without changing fp64 output.

## Health check (app.md), 2026-10-28 (run 1; app/ carries run 1's diff)

- clean git status app: PASS
- make check: PASS (17/17)
- make RAPTOR=0 check: PASS
- RAPTOR=1 build: PASS
- kernel files compile alone: PASS
- small case under a minute: PASS (all modes < 5 s)
- two fp64 runs byte-identical: PASS

## Decisions

- function, source, object: advectWeno in src/advect.cpp -> build/advect.o, and
  redistance in src/redistance.cpp -> build/redistance.o, truncated at its tendency
  (redistanceTendency + godunovGradMag); its state update stays fp64.
- formats: fp8 E4M3 (4, 3) and fp8 E5M2 (5, 2).
- lowprec: float.
- errors: err_rms, err_max of dfun vs the shadow over owned cells, MPI-reduced, appended to
  history.csv (0 at step 0 and in fp64 mode).
- run, in app/: mpirun -n 1 ./bubble params/bubble.ini nx=64 ny=64 max_steps=50
  plot_interval=0 precision={mode} outdir={out}. Small and deterministic.

## Source change

- params.h: enum Precision {kFp64, kFp8E4M3, kFp8E5M2, kLowPrec}; plus `precision` key parsing.
- advect.h: advectWenoAt declaration.
- advect.cpp: EVOLVE-BLOCK float seed + RAPTOR truncation branches.
- redistance.h: redistanceAt declaration.
- redistance.cpp: EVOLVE-BLOCK float seed; fp8 branches call redistanceTruncated (fp64 loop,
  same Jacobi sweep, interface freeze and flips as redistanceIteration) with
  __raptor_truncate_op_func(redistanceTendency, ...), where redistanceTendency returns
  sgn * (godunovGradMag - 1). redistanceIteration is untouched (no zero guard needed there).
- driver.cpp: precision dispatch (advectWenoAt, redistanceAt) and fp64 shadow.
- io.h/.cpp: err_rms, err_max columns (already template) unchanged.
- Makefile: RAPTOR compile for advect.o and redistance.o.

## Steps

1. Health check in bmx.
2. Apply edits.
3. Self-check: git diff -U0 app | grep '^-[^-]'; each removed line must have a RAPTOR: was twin.
4. make RAPTOR=0 -j check, then bmx make clean check from the repo root (RAPTOR=1 comes only
   from the root Makefile; make in app/ is RAPTOR=0).
5. Eval run per mode with {out}=R/results/<mode>; pass: fp64 unchanged, other modes finite nonzero,
   and fp8 area still changing over the last 10 steps (not frozen).
6. Save patches.
7. Animation (after checkpoint 3): fp64 and fp8e4m3, nx=64 ny=64 to tmax=2, plot_interval=20
   hist_interval=10, {out}=R/results/anim_<mode>; R/overlay.py draws both phi=0 contours per
   frame to R/results/overlay/ and overlay.gif.

## Checkpoints

1. Scope: files, callees. (record)
   Outcome: redistance.cpp only; redistanceIteration restored, new redistanceTendency (truncated,
   with godunovGradMag) and redistanceTruncated (fp64 loop) under BUBBLE_RAPTOR.
2. Diff: git diff --stat, RAPTOR: was lines.
   Outcome: 9 files, +384 -7; the 7 removed lines all have RAPTOR: was twins (run 1's).
3. Results: pass/fail and final errors.
   Outcome: make RAPTOR=0 check 17/17, bmx make check (RAPTOR=1, -fpass-plugin) 17/17. Step 50:

   | mode | area | err_rms | err_max |
   |------|------|---------|---------|
   | fp64 | 0.1258795 (= run 1) | 0 | 0 |
   | fp8e4m3 | 0.1260213 | 4.70e-2 | 1.59e-1 |
   | fp8e5m2 | 0.1259367 | 9.27e-2 | 2.35e-1 |
   | lowprec | 0.1258795 | 1.06e-8 | 4.23e-8 |

   Not frozen: fp8e4m3 area changes every step. Animation to t = 2 (5112 steps): area fp64
   0.1772, fp8e4m3 0.2104, err_rms 3.3e-2, err_max 1.9e-1; fp8 deforms and splits like fp64
   with ragged contours and spurious phi = 0 islands (results/overlay/overlay.gif).

## Runner

Single agent, all steps in bmx.
