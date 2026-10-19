# Plan: raptorize

**Goal:** the kernel runs at `precision=fp64` (original), at RAPTOR formats (`BUBBLE_RAPTOR`
builds), and at `lowprec` (a native seed in `// EVOLVE-BLOCK-START/END`), each next to an FP64 shadow.

**Outputs:** the change in `app/`; `E/raptorize.patch` and `E/kernel.yaml`.

## Code rules

- Only add code; never rename, move, reorder, reformat or refactor existing code.
- New code goes in `// RAPTOR-BEGIN: <purpose>` … `// RAPTOR-END` blocks (`#` in Makefiles).
- Added line in existing code: `// RAPTOR`. Changed line: `// RAPTOR: was <old line>`.
- `__raptor_truncate_op_func(fn, 64, 1, E, M)` truncates `fn` and its callees in the
  same file; one cached call per format (`dev/tools/RAPTOR/Readme.md`).
- RAPTOR code sits under `#ifdef BUBBLE_RAPTOR`; `make RAPTOR=0` still runs `fp64` and `lowprec`.
- The shadow calls the existing step unchanged, at `fp64`, same dt.
- EVOLVE-BLOCK: no 64-bit FP, no `#include`, FP helpers named `*LowPrec*`. The fp64 <-> low
  conversion sits outside, without `LowPrec` in its name.
- Build changes go in `app/Makefile`, using `RAPTOR*` variables from the root `Makefile`.
- Each error measure is a column appended to a CSV the app writes.

## Steps

1. Read `example.patch` (toy kernel `applyKernel`) and the kernel's file. **Checkpoint 1.**
2. Write the change for the kernel, following `example.patch`.
3. Self-check: `git diff -U0 app | grep '^-[^-]'`; each line has a `RAPTOR: was` twin. **Checkpoint 2.**
4. Gate commands: `make check`, `make RAPTOR=0 check`, `bmx make check`, then the eval run per mode
   with `{out}` = `E/results/<mode>`.
   Pass: fp64 output unchanged; every other mode has a finite, nonzero error. **Checkpoint 3.**
5. Record `E/raptorize.patch` (`git diff`) and `E/kernel.yaml`:
   ```yaml
   source: src/<file>                # relative to app/
   objfile: build/<file>.o
   audit_tag: LowPrec
   modes: [fp64, <formats>, lowprec]
   run: "<eval command>"             # run in app/; {mode}, {out} substituted
   errors: {csv: "<file in {out}>", columns: [<columns>]}
   ```
