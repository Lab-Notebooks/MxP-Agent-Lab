# Complementary Numerical Profiling: RIKEN-RCCS RAPTOR & LLNL FPChecker Analysis

**Target Repository:** `MxP-MiniApp-Bubble` (`Lab-Notebooks/MxP-MiniApp-Bubble`)  
**Toolchain:** Clang/LLVM 21.1.8 (`LLVMRaptor-21.so` + `libRaptor-RT-21.a` + `libfpchecker_cpu.so`) & GCC 15.2.1 (`-mavx2 -mf16c -mfma`)  
**Raw Trace Artifacts:** `dev/reports/fpchecker_traces/` (`exponent_usage_*.json`, `fpc_*.json`, `raptor_precision_sweep.json`)  

---

## 1. Executive Summary & Real LLVM Pass Compilation

Both profiling tools were compiled from source and executed as real LLVM-21 instrumentation passes on `MxP-MiniApp-Bubble`:

1. **LLNL `FPChecker` (`dev/tools/FPChecker/build/libfpchecker_cpu.so`)**:
   - Built with Clang 21.1.8 (`-fPIC`) and executed via `-fpass-plugin=dev/tools/FPChecker/build/libfpchecker_cpu.so` with `FPC_EXPONENT_USAGE=1`.
   - Captured exact per-file, per-line exponent histograms (`dev/reports/fpchecker_traces/exponent_usage_*.json`) and floating-point exception/cancellation events (`dev/reports/fpchecker_traces/fpc_0.json`).
2. **RIKEN-RCCS `RAPTOR` (`dev/tools/RAPTOR/build/pass/LLVMRaptor-21.so` & `libRaptor-RT-21.a`)**:
   - Built against LLVM 21.1.8 and MPFR, instrumenting every floating-point operation with `__raptor_truncate_op_func` across four reduced-precision formats:
     - **IEEE FP32** $(e=8, m=23)$
     - **NVIDIA TF32** $(e=8, m=10)$
     - **Google BF16** $(e=8, m=7)$
     - **IEEE FP16** $(e=5, m=10)$
   - Results recorded in `dev/reports/fpchecker_traces/raptor_precision_sweep.json`.

---

## 2. Empirical Findings from `FPChecker` & `RAPTOR`

### A. WENO5 Reconstruction (`src_ref/advect.cpp`)
- **LLNL `FPChecker` Cancellation Traps (`fpc_0.json`)**:
  - Trapped **3,840 catastrophic cancellations** in just 10 timesteps (`1,280` events each at `src_ref/advect.cpp:13`, `14`, and `16`) in the unshifted smoothness indicator polynomials:
    - Line 13: `(s1 - 2.0*s2 + s3)^2` and `(s1 - 4.0*s2 + 3.0*s3)^2`
    - Line 14: `(s2 - 2.0*s3 + s4)^2` and `(s2 - s4)^2`
    - Line 16: `(s3 - 2.0*s4 + s5)^2` and `(3.0*s3 - 4.0*s4 + s5)^2`
- **LLNL `FPChecker` Exponent Histogram (`exponent_usage_0.json`)**:
  - At `src_ref/advect.cpp:25` (`denom = sqr(kEps + rIS)`), exponents span **$-99$ to $-12$** (with $10^{-30} \approx 2^{-100}$).
  - At `src_ref/advect.cpp:26` (`aT = d / sqr(kEps + rIS)`), exponents spike to **$+12 \dots +97$**.
- **RIKEN `RAPTOR` Bit-Width Truncation Sweep (`raptor_precision_sweep.json`)**:
  - **IEEE FP16 $(e=5, m=10)$**: **CATASTROPHIC FAILURE (`L1 = -nan`, `Linf = -nan`)**. Because FP16 has a minimum normal exponent of $-14$ (and subnormal floor $2^{-24} \approx 5.96 \times 10^{-8}$), squaring $(\epsilon + \beta_k)$ underflows to `0.0`, producing `0.0 / 0.0 = -nan` in weight normalization!
  - **BF16 $(e=8, m=7)$**: Survives without NaN because the 8-bit exponent spans $[-126, +127]$, but 7 mantissa bits degrade spatial order on $128 \times 128$ to $p_{L_1} = 0.09$, $p_{L_\infty} = -0.10$ ($L_1 = 6.90 \times 10^{-3}$).
  - **TF32 $(e=8, m=10)$**: Achieves $p_{L_1} = 4.90$ on $32 \to 64$, but stalls at $p_{L_1} = 2.05$, $p_{L_\infty} = 0.72$ on $64 \to 128$ ($L_1 = 3.16 \times 10^{-5}$).
  - **Ozaki-EFT (`ff32` + `_Float16` constants + $S=2^{32}$ scaling)**: Restores full 5th-order convergence ($p_{L_1} = 5.05$, $p_{L_\infty} = 4.83$, $L_1 = 3.94 \times 10^{-6}$) with `fp64_asm_ops = 0`.

### B. Godunov Eikonal Redistancing (`src_ref/redistance.cpp`)
- **LLNL `FPChecker` Exponent Histogram**:
  - `src_ref/redistance.cpp:69` (`sgn = orig / (fabs(orig) + kEps)`): exponents concentrate in $[-13, 0]$.
  - `src_ref/redistance.cpp:88,90` (`agf = sqrt(...) - 1.0`): near the converged Eikonal surface $\|\nabla \phi\| \approx 1$, subtracting $1.0$ shifts exponents down to $[-24, -7]$.
- **RIKEN `RAPTOR` Bit-Width Truncation Sweep**:
  - **IEEE FP16 $(5,10)$**: Produces **72 spurious sign flips** on $64 \times 64$ and **92 spurious sign flips** on $128 \times 128$, with contour drift $= 1.031$ cells (10.3× above the $0.10\,\Delta x$ tolerance).
  - **BF16 $(8,7)$**: Zero sign flips, but $\max|\|\nabla\phi\|-1| = 2.1875 \times 10^{-1} > 5.0 \times 10^{-2}$ (FAIL).
  - **TF32 $(8,10)$**: $\max|\|\nabla\phi\|-1| = 2.832 \times 10^{-2} \le 5.0 \times 10^{-2}$ (PASS), contour drift $= 1.199 \times 10^{-2}$ cells.
  - **Ozaki-EFT (`ff32` + `_Float16`)**: $\max|\|\nabla\phi\|-1| = 1.3016 \times 10^{-2}$, contour drift $= 0.000$ cells, `sign flips = 0`.

### C. Streamfunction MAC Face Velocity (`src_ref/init.cpp`)
- **RIKEN `RAPTOR` Bit-Width Truncation Sweep**:
  - **FP16 $(5,10)$**: $\max|\nabla\cdot\mathbf{u}| = 2.411 \times 10^{-1}$ (11 orders of magnitude above $10^{-12}$!).
  - **BF16 $(8,7)$**: $\max|\nabla\cdot\mathbf{u}| = 1.622 \times 10^0$.
  - **TF32 $(8,10)$**: $\max|\nabla\cdot\mathbf{u}| = 2.078 \times 10^{-1}$.
  - **FP32 $(8,23)$**: $\max|\nabla\cdot\mathbf{u}| = 6.774 \times 10^{-5}$ (still 7 orders of magnitude above $10^{-12}$!).
  - **Ozaki-EFT 3-Slice $\pi$ + Prefix-Summed Streamfunction (`src/init.cpp`)**: $\max|\nabla\cdot\mathbf{u}| = \mathbf{7.745 \times 10^{-13} \le 1.0 \times 10^{-12}}$ with **zero** FP64 instructions!
