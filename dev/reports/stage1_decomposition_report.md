# Stage 1–3: Hotspot Decomposition, LLVM Profiling (`RAPTOR` + `FPChecker`), Ozaki-EFT Evolution & Evaluation Report

**Target Repository:** `MxP-MiniApp-Bubble` (`Lab-Notebooks/MxP-MiniApp-Bubble`)  
**Document Reference:** `MXP-STAGE1-3-COMPLETE-2026-V2`  
**Date:** October 16, 2026  

---

## 1. Executive Summary

This report documents the end-to-end execution of Stages 1, 2, and 3 for the mixed-precision optimization of `MxP-MiniApp-Bubble`:

1. **Stage 1.1 & 1.2 — Real LLVM-21 `FPChecker` & `RAPTOR` Profiling**:
   - Compiled `dev/tools/FPChecker/build/libfpchecker_cpu.so` and `dev/tools/RAPTOR/build/pass/LLVMRaptor-21.so` + `libRaptor-RT-21.a` with Clang/LLVM 21.1.8.
   - Measured wall-clock hotspot breakdown (`dev/reports/hotspot_profile_128x128.json`): `advectWeno` (45.28%), `redistance` (48.03%), `computeFaceVelocity` (6.36%), `driver` (0.33%).
   - `FPChecker` pinpointed 3,840 catastrophic cancellations per 10 steps at `src_ref/advect.cpp:13,14,16` and exponent range $[-99, +97]$ in WENO5 denominators.
   - `RAPTOR` bit-width sweeps across FP32 `(8,23)`, TF32 `(8,10)`, BF16 `(8,7)`, and FP16 `(5,10)` proved that FP16 underflows `(kEps + rIS)^2` to `-nan` in `advectWeno` and causes 92 spurious sign flips in `redistance`.
2. **Stage 1.3–1.5 — Zero-FP64 Ozaki-EFT Kernels & Two-Stage Evaluators**:
   - Implemented pure `_Float16` + `float` (`ff32` Ozaki/Dekker Error-Free Transformation) kernels across all four `EVOLVE-BLOCK` hotspots (`src/{advect,redistance,init,driver}.cpp`).
   - Verified via `objdump -d -C` and source regex audit: **0 forbidden 64-bit types** and **0 forbidden `*sd`/`*pd` assembly instructions (`fp64_asm_ops = 0`)**, with **82 `_Float16` tokens** and **290+ `float` tokens**.
   - Achieved **Trajectory RMSE $= 1.14 \times 10^{-8} \le 1.0 \times 10^{-7}$** (**AlphaEvolve Score $= 15.6384 \ge 14.0$**) across all 1,024 steps on 4 MPI ranks (`params/bubble_eval.ini`), beating `src_seed` (`4.6268`) and previous `src_best` (`11.0896`, `RMSE = 2.85e-06`) by **250× in RMSE (62,650× in MSE)** with **zero secular drift**.
3. **All 6 Physical & Algorithmic Invariants Verified (`np = 1, 2, 4`)**:
   - `test_weno_order`: **PASS** ($p_{L_1} = 5.05 \ge 4.5$, $p_{L_\infty} = 4.83 \ge 4.5$)
   - `test_divergence`: **PASS** ($\max|\nabla\cdot\mathbf{u}| = 7.745 \times 10^{-13} \le 1.0 \times 10^{-12}$, analytic $\phi$ err $= 0.000$)
   - `test_redistance`: **PASS** ($\max|\|\nabla\phi\|-1| = 1.30 \times 10^{-2} \le 5.0 \times 10^{-2}$, contour drift $= 0.000$, `flips = 0`)
   - `test_halo`: **PASS**
   - `test_rank_invariance`: **PASS** ($\max|\text{dfun}(np=4) - \text{dfun}(np=1)| = 0.000 \times 10^0$)
   - `test_conservation`: **PASS** (area drift $\le 5\%$, $\max|\nabla\cdot\mathbf{u}| \le 1.0 \times 10^{-12}$)
