# Plan: raptorize advectWeno (splitting bubble)

## Objective

Add precision modes fp64, fp32, bf16 (RAPTOR) and lowprec (native float seed) to advectWeno,
an FP64 shadow run, and its error columns in history.csv, without changing fp64 output.

## Health check (app.md), 2026-10-26

- clean git status app: FAIL (problem-change + raptor edits uncommitted; carried as the base)
- make check: PASS (17/17)
- make RAPTOR=0 check: PASS
- RAPTOR=1 build: PASS
- kernel file compiles alone: PASS
- small case under a minute: PASS
- two fp64 runs byte-identical: to verify

## Decisions

- function, source, object: advectWeno in src/advect.cpp -> build/advect.o. FP callees in the
  same file: wenoFaceValue, sqr; stay FP64 in non-lowprec paths.
- formats: fp32 (8, 23), bf16 (8, 7).
- lowprec: float.
- errors: err_rms, err_max of dfun vs the shadow over owned cells, MPI-reduced, appended to
  history.csv (0 at step 0 and in fp64 mode).
- run, in app/: mpirun -n 1 ./bubble params/bubble.ini nx=128 ny=128 max_steps=100
  plot_interval=0 precision={mode} outdir={out}. 128^2 resolves the bubble; 100 steps leave
  headroom for MPFR-emulated modes.

## Source change

Already applied from the raptorize template: params.h enum Precision and key; advect.h
advectWenoAt; advect.cpp EVOLVE-BLOCK lowprec seed + RAPTOR truncation; driver.cpp shadow;
io.h/.cpp error columns; Makefile RAPTOR block.

## Steps

1. Health check in bmx.
2. Self-check: git diff -U0 app | grep '^-[^-]'; each removed line must have a RAPTOR: was twin.
3. make -j check, make RAPTOR=0 -j check, bmx make check.
4. Eval run per mode with {out}=R/results/<mode>; pass: fp64 unchanged, other modes finite nonzero.
5. Save patches.

## Checkpoints

1. Scope: files, callees. (already applied; record)
2. Diff: git diff --stat, RAPTOR: was lines.
3. Results: pass/fail and final errors.

## Runner

Single agent, all steps in bmx.
