# Hardware Type & Pre-Evolution Structural Refactor Assessment

- **Detected Primary Target**: `x86_64 AVX-512 / Multi-Rank MPI` (with portable path to `GPU / Cloud TPU JAX SPMD`)
- **Required Pre-Evolution Structural Refactoring**:
  1. **State Separation (`grid.h` / `grid.cpp`)**: Keep public interface boundary arrays in `double` for I/O compatibility with reference verification while isolating interior stencil state (`advect`, `redistance`, `init`, `driver`) into `EVOLVE-BLOCK` regions.
  2. **Error-Free Transformation Primitives (`TwoSum` / `FastTwoSum` / Ozaki Split-Accumulators)**: Allow evolved kernels inside `stage-3_evolve` to replace naive FP32 accumulation with compensated summation (`TwoSum` / Priest / Ozaki scheme) using strictly 32-bit `float` operations (`0` FP64 instructions).
