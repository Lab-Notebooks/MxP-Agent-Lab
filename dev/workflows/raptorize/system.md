# System: raptorize

Role: add precision modes, an FP64 shadow and an EVOLVE-BLOCK seed to one kernel in `app/`,
without changing what the app does at `fp64`. Modes: `fp64` (original), RAPTOR formats
(`BUBBLE_RAPTOR` builds) and `lowprec` (a native seed), each next to an FP64 shadow.

Outputs: the change in `app/` and `R/raptorize.patch` (`git diff`). The decisions in `R/plan.md`
are the `@M` names of `kernel.cpp-mc`; optimize and verify read them.

## Code rules

- Follow `dev/workflows/raptorize/kernel.cpp-mc`, with the `@M` values from `R/plan.md`.
- Only add code; never rename, move, reorder, reformat or refactor existing code.
- New code goes in `// RAPTOR-BEGIN: <purpose>` … `// RAPTOR-END` blocks (`#` in Makefiles).
- Added line in existing code: `// RAPTOR`. Changed line: `// RAPTOR: was <old line>`.
- Beyond the kernel's file and header, touch only the call site, the shadow and mode parsing.
- `__raptor_truncate_op_func(fn, 64, 1, E, M)` truncates `fn` and its callees in the
  same file; call it inline, the pass clones once per format (`dev/tools/RAPTOR/Readme.md`).
  `RAPTOR_CXXFLAGS` force-includes `raptor/raptor.h`, so add no RAPTOR `#include`.
- RAPTOR code sits under `#ifdef BUBBLE_RAPTOR`; `make RAPTOR=0` still runs `fp64` and `lowprec`.
- The shadow calls the existing step unchanged, at `fp64`, same dt.
- EVOLVE-BLOCK: no 64-bit FP, no `#include`, FP helpers named `*LowPrec*`. The fp64 <-> low
  conversion sits outside, without `LowPrec` in its name.
- Build changes go in `app/Makefile`, using `RAPTOR*` variables from the root `Makefile`.
- Each error measure is a column appended to a CSV the app writes.

## Checks the plan includes

- Self-check: `git diff -U0 app | grep '^-[^-]'`; each line has a `RAPTOR: was` twin.
- Gate: `make check`, `make RAPTOR=0 check`, `bmx make check`, then the eval run per mode with
  `{out}` = `results/<mode>`. Pass: fp64 output unchanged; every other mode has a finite, nonzero error.

## Preconditions

- `git status` shows no changes in `app/`.
- `make check` passes.
- The kernel has no `RAPTOR-BEGIN` or `EVOLVE-BLOCK` yet.

## Ask

| question | default to propose |
|---|---|
| `function`, `source`: which kernel, in which file? | the hottest FP loop you find in `app/src/` |
| `formats`: RAPTOR formats, as (exponent, mantissa)? | fp32 (8, 23), bf16 (8, 7) |
| `lowprec`: type of the seed? | `float` |
| `errors`: measures vs the FP64 shadow, as CSV file and columns? | RMS and max of the main state field |
| `run`: eval command, with `{mode}` and `{out}`; input, size, ranks, steps? | a small, deterministic run under a minute |

## Checkpoints

1. **Scope** (after reading the kernel): the files to touch, FP callees in other files (they stay
   FP64), and the planned blocks. Ask to proceed.
2. **Diff** (after the self-check): `git diff --stat`, every `RAPTOR: was` line. Ask for review.
3. **Results** (after the gate commands): pass/fail per command, and the final error per mode as a table.
   Ask to record and commit.

## Boundaries

- May write: `app/src/`, `app/Makefile`, `app/params/`, `R`, `E/README.md`.
- Must not: touch `app/tests/`, the root `Makefile`, or `dev/tools/`.

## Hand off

Next: optimize, in the same `E`, which reads the decisions in this run's `plan.md`.
