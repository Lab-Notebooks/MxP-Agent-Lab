# Stage 1, raptorize: plan

**Goal:** the scientist's kernel can run at RAPTOR-truncated precision next to
an FP64 shadow of the state, selected by `precision=` in `params/*.ini` or on
the command line. Everything else stays FP64.

**Scientist decides:**
- the kernel: which function(s) in which `src/` file
- the formats, as RAPTOR (exponent, mantissa) bits, e.g. fp32 (8, 23), tf32 (8, 10), bf16 (8, 7), fp16 (5, 10)
- which error measures to record against the FP64 shadow, so stage 3 can choose its score from them:
  - global field errors: `err_l2` (RMS), `err_linf`, band-restricted norms
  - scalar quantity-of-interest errors: e.g. `area - area_ref`, mass, or a probe value

## Steps

1. Read `example.patch`. It applies the pattern to a toy kernel
   (`kernelPoint` / `applyKernel`):
   - `params.h`: a `Precision` enum and the `precision` key
   - the kernel's file:
     - `#ifdef BUBBLE_RAPTOR` with one `__raptor_truncate_op_func(fn, 64, 1, E, M)` per format
     - `precisionAvailable()`
     - an `<kernel>At(m, ...)` dispatch
   - `driver.cpp`: the call goes through the dispatch; an FP64 shadow is
     advanced with the same dt, and its error is computed
   - `Makefile`: only the kernel's file is compiled with clang plus the RAPTOR
     plugin when `RAPTOR_INSTALL_DIR` has it; `make RAPTOR=0` still builds
2. Write the same change for the scientist's kernel in this repo. Also add:
   - one history column per recorded error measure
   - the summary lines in `main.cpp`
   - the `precision` key in `params/bubble.ini`
3. Save it, then gate:
   ```sh
   mkdir -p dev/workflows/raptorize/patches && git diff > dev/workflows/raptorize/patches/<kernel>.patch
   make && make check                                    # green
   make clean && make RAPTOR=0 && make check RAPTOR=0    # green without RAPTOR
   mpirun -np 2 ./bubble params/bubble.ini precision=bf16 max_steps=40 outdir=out_bf16   # error > 0
   ```
   - The fp64 output (history, fields) is unchanged in the original columns.
   - `tests/` are untouched.
   - The scientist reviews the patch and commits.
