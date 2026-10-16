# AlphaEvolve Mixed-Precision Optimization (`MxP-MiniApp-Bubble`)

This directory contains the initial reduced-precision seed (`src_seed/`), the previous 6-generation FP32 candidate (`src_prev_best/`), the **Live Google Cloud AlphaEvolve GA evolved mixed-precision kernels (`src_best/` and `src_evolved/`)** discovered on Discovery Engine v1alpha Session `15541367028299861796` (`alphaevolve-argolis-511205`), the two-stage evaluation configuration (`experiment/`), and the regenerated side-by-side visual comparison artifacts (`demo/`).

## Directory Layout

```text
alphaevolve-demo/
├── src_seed/                  # Initial reduced-precision (_Float16 / float) seed kernels (Score: 4.6268)
├── src_prev_best/             # Previous 6-generation FP32 kernels (Score: 11.0896, RMSE: 2.85e-06)
├── src_evolved/               # Option-B Ozaki-EFT warm-start seed kernels (Score: 15.6374, 1,796 FP32 ops)
├── src_best/                  # Live AlphaEvolve GA Best kernels (Score: 15.6374, 1,140 FP32 ops, -36.5% ops, 2.34s)
├── params/
│   └── bubble_eval.ini        # 128x128, 1,024-step 4-rank evaluation config
├── experiment/
│   ├── experiment.yaml        # AlphaEvolve experiment specification & problem prompt
│   └── evaluator.py           # Strict zero-FP64 audit (objdump + source) + trajectory MSE scorer
└── demo/
    ├── compare_seed_vs_ae_best.gif                   # 4-way animated contour comparison (FP64 Ref vs Seed vs Prev Best vs Option-B vs Live AE Best)
    ├── compare_ae_best.gif                           # 2-way animated contour comparison (FP64 Ref vs Live AE GA Best)
    ├── error_vs_time_seed_vs_ae_best.png             # 4-way Linf and RMS error growth over 1,024 steps
    ├── error_vs_time_seed_vs_ae_best.csv             # Per-snapshot Linf and RMS errors across all 4 candidates
    ├── error_vs_time_ae_best.png                     # Linf and RMS error growth (Live AE GA Best vs FP64 Ref)
    ├── error_vs_time_ae_best.csv                     # Per-snapshot Linf and RMS errors (Live AE GA Best)
    ├── bubble_deformation_compare_seed_vs_ae_best.png # Final-frame (step 1024) contour overlay (4-way)
    └── bubble_deformation_compare_ae_best.png        # Final-frame (step 1024) contour overlay (Live AE GA Best)
```

---

## Quantitative Before & After Comparison (`128×128`, 1,024 Steps, 4 MPI Ranks)

| Benchmark / Metric | 64-Bit Baseline (`src_ref/`) | Initial Seed (`src_seed/`) | Prev AE Best (`src_prev_best/`) | Option-B Warm-Start Seed (`src_evolved/`) | **Live AlphaEvolve GA Best (`src_best/`)** |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **AlphaEvolve Trajectory Score** | `16.0000` (ceiling) | `4.6268` | `11.0896` | `15.637352` | **`15.637350`** (Target $\ge 14.0$) |
| **Trajectory MSE** | `0.0` | `2.3614e-05` | `8.1367e-12` | `1.3049e-16` | **`1.3049e-16`** (**62,350× lower MSE**) |
| **Trajectory RMSE** | `0.0` | `4.8594e-03` | `2.8525e-06` | `1.1423e-08` | **`1.1423e-08`** (**250× lower RMSE**, $\le 10^{-7}$) |
| **Step 1,024 Final RMSE / $L_\infty$** | `0.0` / `0.0` | `5.96e-03` / `2.58e-02` | `5.54e-06` / `1.16e-04` | `1.10e-08` / `2.98e-08` | **`1.10e-08` / `2.98e-08`** |
| **Discrete Velocity Divergence $\max\|\nabla\cdot\mathbf{u}\|$** | `7.752e-13` | `1.391e+00` | `1.981e-07` | `2.132e-14` | **`2.132e-14` (`128^2`) / `5.684e-14` (`256^2`)** |
| **Area Drift ($t=0.2001$, Step 1,024)** | `0.2978%` | `0.3226%` | `0.3075%` | `0.2978%` | **`0.2978%`** ($\Delta = 0.0000\text{ pp}$) |
| **Spurious Sign Flips** | `0` | `85` | `0` | `0` | **`0`** |
| **Total `fp32_asm_ops` / `hw_weighted_cost`** | N/A | — | — | `1,796` / `3,672` | **`1,140` (-36.53%) / `2,306` (-37.20%)** |
| **4-Rank Wall Time (1,024 steps)** | `2.18 s` | `1.95 s` | `2.10 s` | `3.40 s` | **`2.34 s` (-31.18% faster)** |
| **64-Bit Assembly Ops (`fp64_asm_ops`)** | N/A (FP64 ref) | `0` | `0` | `0` | **`0`** (`_Float16` tokens = 26) |
