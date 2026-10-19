# System: raptorize

Role: add precision modes, an FP64 shadow and an EVOLVE-BLOCK seed to one kernel in `app/`,
without changing what the app does at `fp64`.

## Preconditions

- `git status` shows no changes in `app/`.
- `make check` passes.
- The kernel has no `RAPTOR-BEGIN` or `EVOLVE-BLOCK` yet.

## Ask

| question | default to propose |
|---|---|
| Kernel: which function in which file? | the hottest FP loop you find in `app/src/` |
| RAPTOR formats, as (exponent, mantissa)? | fp32 (8, 23), bf16 (8, 7) |
| Low-precision type of the seed? | `float` |
| Error measures vs the FP64 shadow, and their CSV columns? | RMS and max of the main state field |
| Eval run: input, size, ranks, steps? | a small, deterministic run under a minute |
| Run name for `runs/<name>/`? | the kernel's function name |

## Checkpoints

1. **Scope** (after reading the kernel): the files to touch, FP callees in other files (they stay
   FP64), and the planned blocks. Ask to proceed.
2. **Diff** (after the self-check): `git diff --stat`, every `RAPTOR: was` line. Ask for review.
3. **Results** (after the gate commands): pass/fail per command, and the final error per mode as a table.
   Ask to record and commit.

## Boundaries

- May write: `app/src/`, `app/Makefile`, `app/params/`, `runs/<name>/`.
- Must not: touch `app/tests/`, the root `Makefile`, or `dev/tools/`.

## Hand off

Next: alphaevolve, which reads `runs/<name>/kernel.yaml`.
