# Plan: raptorize advectWeno and redistance (8-bit emulation)

## Objective

Add precision modes fp64, fp8 (E4M3, E5M2) via RAPTOR, and a native float seed, to
advectWeno (src/advect.cpp) and redistance (src/redistance.cpp), an FP64 shadow run,
and err_rms/err_max columns in history.csv, without changing fp64 output.

## Health check (app.md), 2026-10-28

- clean git status app: PASS
- make check: PASS (17/17)
- make RAPTOR=0 check: PASS
- RAPTOR=1 build: PASS
- kernel files compile alone: PASS
- small case under a minute: PASS (all modes < 5 s)
- two fp64 runs byte-identical: PASS

## Decisions

- function, source, object: advectWeno in src/advect.cpp -> build/advect.o, and
  redistance in src/redistance.cpp -> build/redistance.o. FP callees in the same files
  (wenoFaceValue, sqr; signOf, maxd, mind, lsPseudoTimestep) stay FP64 in non-lowprec paths.
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
- redistance.cpp: EVOLVE-BLOCK float seed + RAPTOR truncation branches.
- driver.cpp: precision dispatch (advectWenoAt, redistanceAt) and fp64 shadow.
- io.h/.cpp: err_rms, err_max columns (already template) unchanged.
- Makefile: RAPTOR compile for advect.o and redistance.o.

## Steps

1. Health check in bmx.
2. Apply edits.
3. Self-check: git diff -U0 app | grep '^-[^-]'; each removed line must have a RAPTOR: was twin.
4. make -j check, make RAPTOR=0 -j check, bmx make check.
5. Eval run per mode with {out}=R/results/<mode>; pass: fp64 unchanged, other modes finite nonzero.
6. Save patches.

## Checkpoints

1. Scope: files, callees. (record)
2. Diff: git diff --stat, RAPTOR: was lines.
3. Results: pass/fail and final errors.

## Runner

Single agent, all steps in bmx.
