# AlphaEvolve Mixed-Precision Optimization

This directory contains the initial reduced-precision seed, the best evolved (after 6 generations) mixed-precision kernels discovered by AlphaEvolve, the evaluation configuration, and the comparison results against the 64-bit (`double`) baseline in [`../src/`](../src/).

## Directory Layout

```text
alphaevolve/
├── src_seed/                  # Initial reduced-precision (_Float16 / float) seed kernels
├── src_best/                  # Best AlphaEvolve-discovered kernels (FP32 reformulations)
├── params/
│   └── bubble_eval.ini        # Fast 128x128, 1,024-step config used during evolution
├── experiment/
│   ├── experiment.yaml        # AlphaEvolve experiment specification & problem prompt
│   └── evaluator.py           # Strict 2-stage precision audit (objdump) + trajectory MSE scorer
└── demo/
    ├── compare_seed_vs_ae_best.gif        # 256x256, 20,000-step deformation (FP64 vs Seed vs Best)
    ├── compare_ae_best.gif                # 256x256, 20,000-step deformation (FP64 vs Best)
    ├── error_vs_time_seed_vs_ae_best.png  # 4-panel error & area drift over t = [0, 2.0]
    ├── error_vs_time_seed_vs_ae_best.csv  # Per-snapshot Linf and RMS errors (Seed & Best)
    ├── error_vs_time_ae_best.png          # 4-panel error & area drift (Best only)
    └── error_vs_time_ae_best.csv          # Per-snapshot Linf and RMS errors (Best only)
```

---

## Summary of Evolved Algorithmic Improvements (`src_best/`)

Both `src_seed/` and `src_best/` execute strictly in reduced precision (`_Float16` / `float`, verified via source scan and `objdump -d` disassembly to contain **zero** 64-bit `*sd`/`*pd` instructions inside `EVOLVE-BLOCK`s):

1. **`advect.cpp` (Stencil-Centered WENO5):** Shifts each 5-point stencil by the center cell ($v_k = s_k - s_3$) before evaluating smoothness indicators $IS_k$ and substencil fluxes in `float`, eliminating catastrophic cancellation.
2. **`init.cpp` (Product-to-Sum MAC Velocity):** Replaces finite-difference cosine/sine subtractions with analytic product-to-sum identities ($-2\sin(2\pi y_c)\sin(\pi\Delta y)$), reducing discrete velocity divergence $\max|\nabla\cdot\mathbf{u}|$ by **3,062×**.
3. **`redistance.cpp` (FP32 Eikonal Redistancing):** Evaluates Godunov upwind differences in `float` with $\epsilon = 10^{-6}$, eliminating all **85** spurious level-set sign flips caused by `_Float16` quantization.
4. **`driver.cpp` (Interface-Selective Compensated Time Stepping):** Uses Knuth `TwoSum` error compensation on $\phi \mathrel{+}= \Delta t \cdot \text{rhs}$, preserving sub-ULP residuals on interface-pinned cells while resetting them on redistanced cells.

| Benchmark / Metric | 64-Bit Baseline (`src/`) | Initial Seed (`src_seed/`) | AlphaEvolve Best (`src_best/`) |
| :--- | :---: | :---: | :---: |
| **Eval Score (`128×128`, 1,024 steps, 4 ranks)** | `16.0000` (ref) | `4.6268` | **`11.0896`** |
| **Eval Trajectory MSE / RMSE** | `0.0` | `2.36e-05` / `4.86e-03` | **`8.14e-12` / `2.85e-06`** (**2.9M× lower MSE**) |
| **Full Run (`256×256`, 20k steps): RMS $|\Delta\phi|$ @ Step 1,024** | `0.0` | `1.46e-02` | **`6.76e-07`** (**21,614× lower**) |
| **Full Run (`256×256`, 20k steps): Final Area Drift ($t=2.0$)** | `2.8368%` | `3.5875%` ($\Delta=+0.75\text{ pp}$) | **`3.0169%` ($\Delta=+0.18\text{ pp}$)** |
| **Full Run (`256×256`, 20k steps): Spurious Sign Flips** | `0` | `85` | **`0`** |

---
