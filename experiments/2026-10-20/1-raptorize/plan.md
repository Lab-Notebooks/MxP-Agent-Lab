# Plan: raptorize advectWeno

## Objective

Add precision modes fp64, fp32, bf16 (RAPTOR) and lowprec (native float seed) to advectWeno, an
FP64 shadow run, and its error columns in history.csv, without changing fp64 output.

## Health check (app.md), 2026-10-20

All pass: clean `git status app`; `make -j check` 17/17 natively and in bmx; RAPTOR=1 natively
and in bmx; advect.cpp -> build/advect.o alone; 128² x 200 steps runs in 0.9 s; two fp64 runs give
byte-identical history.csv. Items 4 (RAPTOR build) and 5 (`precision` key) are added by this stage.

## Decisions

- function, source, object: `advectWeno` in src/advect.cpp -> build/advect.o. Hottest FP loop:
  52% of run time (wenoFaceValue 40%, advectWeno 12%) vs redistance 24% (gprof, 128² x 400
  steps). FP callees in the same file: wenoFaceValue, sqr.
  Callees elsewhere: Grid::dx/dy, Grid::index (inline accessors, src/grid.h:65-69), stay FP64.
- formats: fp32 (8, 23), bf16 (8, 7).
- lowprec: float.
- errors: `err_rms`, `err_max` of dfun vs the shadow over owned cells, MPI-reduced, appended to
  history.csv (0 at step 0 and in fp64 mode).
- run, in app/: `mpirun -n 1 ./bubble params/bubble.ini nx=128 ny=128 max_steps=100
  plot_interval=0 precision={mode} outdir={out}`. 128² resolves the bubble (12.8 cells per
  radius); 100 steps leave headroom for the MPFR-emulated modes (est. ~50x slower in advect).
  Ranks: 1; verify reruns at other counts.

## Source change

- src/params.h: `enum class Precision { kFp64, kFp32, kBf16, kLowPrec };` (here, not advect.h:
  advect.h -> grid.h -> params.h); `Params::precision`; key `precision=fp64|fp32|bf16|lowprec`.
- src/advect.h: declare `advectWenoAt(Precision, g, phi, u, v, rhs)`.
- src/advect.cpp: EVOLVE-BLOCK with `wenoFaceValueLowPrec`, `sqrLowPrec`,
  `advectWenoLowPrec(const Grid& g, float dxi, float dyi, const float* phi, const float* u,
  const float* v, float* rhs)` (dxi, dyi passed in: Grid::dx() is double); then `advectWenoAt`
  with the double <-> float copies and `__raptor_truncate_op_func(advectWeno, 64, 1, E, M)`
  under `#ifdef BUBBLE_RAPTOR`.
- src/driver.cpp: call site in advanceOneStep -> advectWenoAt(p.precision, ...); in
  runSimulation the shadow (`dfun` copy, own rhs/phiOrig, `Params fp64` with kFp64) advanced by
  advanceOneStep with the same dt, errors set on `diag` when a history row is written.
- src/io.h/.cpp: `Diagnostics::err_rms, err_max`; `errorsVsShadow(g, a, b, d)`; header and row
  gain the two columns (header line changed, `RAPTOR: was`).
- app/Makefile: with RAPTOR=1, build/advect.o uses RAPTOR_CXX, RAPTOR_CXXFLAGS and the MPI
  compile flags (`mpicxx --showme:compile`); LDLIBS += RAPTOR_LDLIBS.

## Steps

1. Edit as above, following dev/workflows/raptorize/kernel.cpp and its code rules.
2. Self-check: `git diff -U0 app | grep '^-[^-]'`; pass: each line has a `RAPTOR: was` twin.
All commands run in bmx (scientist, checkpoint 1).

3. `make -j check`, `make RAPTOR=0 -j check`; pass: all tests pass.
4. Eval run per mode with `{out}` = $PWD/experiments/2026-10-20/1-raptorize/results/<mode>;
   plus a pre-change fp64 baseline at results/baseline (bmx, same toolchain). Pass: fp64 history.csv equals the
   baseline in its original 9 columns and has zero errors; fp32, bf16, lowprec have finite,
   nonzero err_rms and err_max; each run < 60 s.
5. `git diff app > experiments/2026-10-20/1-raptorize/raptorize.patch`.
6. Added at checkpoint 3 (scientist): full run to tmax = 2 per mode, as step 4 without
   max_steps, with plot_interval=1024 and `mpirun --bind-to none -n 2` (MPI exercised; 128 is not divisible by 3),
   plus fp64 at 1 rank (results/full_fp64_np1) whose history.csv must match fp64 at 2 ranks; {out} = results/full_<mode>; compare phi = 0 per mode
   (results/plots.py). Informational, no pass criterion.

## Checkpoints

1. Scope: files, callees left FP64, planned blocks. Ask to proceed.
   Outcome: approved; run everything in bmx.
2. Diff: `git diff --stat`, every `RAPTOR: was` line. Ask for review.
   Outcome: 7 files, +206/-3; 3 `RAPTOR: was` lines; shown with checkpoint 3; no objection.
3. Results: pass/fail per command, final err_rms/err_max per mode. Ask to record and commit.
   Outcome: all pass. Step 100 err_rms/err_max: fp32 6.0e-10/4.4e-9, bf16 8.0e-5/5.3e-4,
   lowprec = fp32 (byte-identical). Step 6, t = 2 on 2 ranks: fp32 = lowprec 6.3e-7/6.2e-6,
   bf16 2.6e-4/1.3e-3; 1-rank fp64 field bit-identical. Not committed: the scientist commits (AGENTS.md).

## Runner

Single agent, all steps: the change is small and sequential.
