# Stage 2, evaluator: plan

**Goal:** a native low-precision seed of the kernel, run as a new `precision`
mode against the stage-1 FP64 shadow, with the evolvable functions between
`// EVOLVE-BLOCK-START` and `// EVOLVE-BLOCK-END`. Needs stage 1.

**Scientist decides:**
- which functions to evolve (contiguous, in one file)
- the low-precision type (e.g. `float`, `_Float16` storage)
- what stays fixed outside the block

## Steps

1. Read `example.patch`. It applies the pattern to the stage-1 toy:
   - a `lowprec` mode
   - `*LowPrec` seed functions, a plain transcription of the FP64 kernel, in
     the block. Every FP helper's name contains `LowPrec`, so the stage-3
     audit sees it.
   - the fp64 <-> low conversion outside the block
   - a fixed `params/<name>_eval.ini`
2. Write the same change for the scientist's kernel. Put the marker comments
   by hand on their own lines, directly around the seed functions:
   ```cpp
   // EVOLVE-BLOCK-START
   float myKernelLowPrec(...) { ... }
   // EVOLVE-BLOCK-END
   ```
3. Save it, then gate:
   ```sh
   mkdir -p dev/workflows/evaluator/patches && git diff > dev/workflows/evaluator/patches/<kernel>.patch
   make && make check                                                        # green
   mpirun -np 4 ./bubble params/<name>_eval.ini precision=lowprec outdir=out_eval   # finite, small error
   grep -c "EVOLVE-BLOCK-" src/<file>.cpp                                    # 2
   ```
   - There's no `double` inside the block.
   - The FP64 path and `tests/` are untouched.
   - The scientist reviews the patch and commits.
