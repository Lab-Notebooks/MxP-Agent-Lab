# Before & After Summary: Live Google Cloud AlphaEvolve Mixed-Precision Evolution (`MxP-MiniApp-Bubble`)

**Repository & Branch:** [`https://github.com/Lab-Notebooks/MxP-MiniApp-Bubble/tree/dev`](https://github.com/Lab-Notebooks/MxP-MiniApp-Bubble/tree/dev)  
**Google Drive Folder:** [`MxP-MiniApp-Bubble-AlphaEvolve-Deliverables` (`1su8HAl7mP1ENBWjHWpu2p8pFLfYsluSL`)](https://drive.google.com/drive/folders/1su8HAl7mP1ENBWjHWpu2p8pFLfYsluSL)  
**Live Discovery Engine v1alpha Session:** `projects/603936599140/locations/global/collections/default_collection/engines/alphaevolve-engine/sessions/15541367028299861796` (`alphaevolve-argolis-511205`)

---

## 1. End-to-End Coupled Trajectory: Before vs. After (`128x128`, 1,024 Steps, 4 MPI Ranks)

| Metric | 1. FP64 Reference (`src_ref/`, 64-Bit `double`) | 2. Initial Reduced-Precision Seed (`src_seed/`) | 3. Prior 6-Gen AE Best (`src_prev_best/`) | 4. Option-B Warm-Start Seed (`Gen 0`) | 5. **Live AlphaEvolve GA Best (`src/` & `src_best/`)** |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **AlphaEvolve Trajectory Score** | `16.0000` (ceiling) | `4.6268` | `11.0896` | `15.637352` | **`15.637350`** (Target >= 14.0) |
| **Trajectory MSE (9 frames)** | `0.0` | `2.3614e-05` | `8.1367e-12` | `1.3049e-16` | **`1.3049e-16`** (**62,350x lower vs Prior AE**) |
| **Trajectory RMSE** | `0.0` | `4.8594e-03` | `2.8525e-06` | `1.1423e-08` | **`1.1423e-08`** (<= 1.0e-7) |
| **Interface-Band RMSE (|phi| <= 5*dx)** | `0.0` | `3.120e-03` | `1.940e-06` | `6.091e-10` | **`6.091e-10`** |
| **Step 1,024 Final Linf / RMS Error** | `0.0` / `0.0` | `2.579e-02` / `5.957e-03` | `1.165e-04` / `5.537e-06` | `2.983e-08` / `1.103e-08` | **`2.985e-08` / `1.103e-08`** |
| **Area Drift (t = 0.2001, Step 1,024)** | `0.2978%` | `0.3226%` (delta = +0.0248 pp) | `0.3075%` (delta = +0.0097 pp) | `0.2978%` (delta = +0.0000 pp) | **`0.2978%` (delta = +0.0000 pp)** |
| **Discrete Velocity Divergence max|div u|** | `7.752e-13` | `1.391e+00` (FAIL) | `1.981e-07` (FAIL) | `2.132e-14` (PASS) | **`2.132e-14` (`128^2`) / `5.684e-14` (`256^2`)** |
| **Spurious Sign Flips** | `0` | `85` (FAIL) | `0` | `0` | **`0`** |
| **64-Bit Assembly Ops (`fp64_asm_ops`)** | N/A (FP64 baseline) | `0` | `0` | `0` | **`0`** (100% 16/32-bit inside `EVOLVE-BLOCK`) |
| **Total `fp32_asm_ops` Across 4 Hotspots** | N/A |  --  |  --  | `1,796` | **`1,140` (-36.53%)** *(down to `659` / **-63.31%** on 128x128)* |
| **Total `hw_weighted_cost` Across 4 Hotspots** | N/A |  --  |  --  | `3,672` | **`2,306` (-37.20%)** *(down to `1,330` / **-63.78%** on 128x128)* |
| **Wall Time (1,024 steps, 4 MPI ranks)** | `2.18 s` | `1.95 s` | `2.10 s` | `3.40 s` | **`2.34 s` (-31.18% faster than Option-B Seed)** |
| **Full 17-Run `make check` Suite (256x256)** | `17 / 17 PASS` | `FAIL` | `FAIL` | `17 / 17 PASS` | **`17 / 17 PASS`** |

---

## 2. Error vs. Time Trajectory Table (`128x128`, 4 MPI Ranks, Steps `0..1024`)

Generated directly by `tools/plot_compare.py` (`assets/alphaevolve-demo/demo/error_vs_time_seed_vs_ae_best.csv` & `.png`) against the 64-bit `double` reference (`out_ref_eval`):

| Step | Time t | `src_seed` (Linf / RMS) | `src_prev_best` (Linf / RMS) | Option-B `Gen 0` Seed (Linf / RMS) | **Live AlphaEvolve GA Best (Linf / RMS)** |
| :---: | :---: | :---: | :---: | :---: | :---: |
| **0** | `0.00000` | `2.979e-08` / `1.162e-08` | `2.979e-08` / `1.162e-08` | `2.979e-08` / `1.162e-08` | **`2.979e-08` / `1.162e-08`** |
| **128** | `0.02501` | `1.124e-02` / `3.482e-03` | `3.064e-06` / `1.817e-07` | `3.005e-08` / `1.163e-08` | **`3.005e-08` / `1.163e-08`** |
| **256** | `0.05002` | `1.835e-02` / `4.381e-03` | `2.359e-05` / `6.540e-07` | `2.982e-08` / `1.164e-08` | **`2.982e-08` / `1.164e-08`** |
| **384** | `0.07503` | `2.025e-02` / `4.724e-03` | `2.841e-05` / `9.941e-07` | `2.982e-08` / `1.151e-08` | **`2.986e-08` / `1.151e-08`** |
| **512** | `0.10004` | `2.180e-02` / `5.160e-03` | `7.666e-05` / `1.878e-06` | `2.983e-08` / `1.147e-08` | **`2.986e-08` / `1.147e-08`** |
| **640** | `0.12505` | `2.320e-02` / `5.501e-03` | `6.206e-05` / `2.247e-06` | `2.982e-08` / `1.135e-08` | **`2.990e-08` / `1.135e-08`** |
| **768** | `0.15006` | `2.483e-02` / `5.728e-03` | `1.259e-04` / `3.623e-06` | `2.984e-08` / `1.127e-08` | **`2.992e-08` / `1.127e-08`** |
| **896** | `0.17507` | `2.608e-02` / `5.807e-03` | `1.452e-04` / `4.408e-06` | `2.984e-08` / `1.127e-08` | **`2.985e-08` / `1.127e-08`** |
| **1024** | `0.20008` | `2.579e-02` / `5.957e-03` | `1.165e-04` / `5.537e-06` | `2.983e-08` / `1.103e-08` | **`2.985e-08` / `1.103e-08`** |

---

## 3. Per-Hotspot Before vs. After (Live AlphaEvolve Session `15541367028299861796`)

| Hotspot File | Live Experiment ID & Nickname | Before (`Option-B Gen 0` Seed) | After (**Live AlphaEvolve GA Winner**) | Instruction & Cost Reduction | Key Algorithmic Transformation Discovered by AlphaEvolve |
| :--- | :--- | :--- | :--- | :---: | :--- |
| **`src/advect.cpp`** | `exp-watchful-starfish` (`5520963987098500867`) | `prog-strange-aardvark` ; `hw_cost = 1,084` ; `fp32_ops = 534` ; `ae_score = 17.0161` | **`prog-berserk-turkey`** (`1977769449892952176`, `ae=17.0934`) ; *(Alt: `prog-ultraviolet-worm` `hw=622`, `prog-sage-urchin` `hw=442`)* | **`hw_cost`: -39.30% (`1,084 -> 658`)** ; **`fp32_ops`: -38.95% (`534 -> 326`)** ; *(Alt: **-59.23%** in `sage-urchin`)* | Replaced nested `ff_mul`/`ff_sub` flux calls with fused FMA `ff_flux_x_y` and scalar smoothness accumulation `compute_rIS_sum` prior to 2^32 subnormal-free scaling. |
| **`src/redistance.cpp`** | `exp-bronze-tapir` (`16975095252697576403`) | `prog-warping-cuckoo` ; `hw_cost = 492` ; `fp32_ops = 242` ; `ae_score = 19.9503` | **`prog-warping-cuckoo`** (`5520963987098497834`, `ae=19.9503`) ; *(Alt: `prog-benign-penguin` `5212423579005725365`, `ae=19.9438`)* | **Pareto-Optimal (`492` cost, `242` ops, `0` sign flips)** | Confirmed 1-step Newton `ff_sqrt` + lexicographic `opposite_sign` interface freeze as Pareto-optimal (`drift = 0.000` cells). |
| **`src/init.cpp`** | `exp-hungry-bandicoot` (`16894854776193803987`) | `prog-caped-rooster` ; `hw_cost = 1,772` ; `fp32_ops = 863` ; `ae_score = 17.1505` | **`prog-naughty-marmoset`** (`3124428538999897996`, `ae=17.2427`) ; *(Alt 128x128: `prog-immortal-stallion` `hw=292`, `fp32_ops=145`)* | **`hw_cost`: -53.05% (`1,772 -> 832`)** ; **`fp32_ops`: -51.91% (`863 -> 415`)** ; *(Alt 128x128: **-83.52%** in `stallion`)* | Hoisted `inv_denom_x`/`inv_denom_y` reciprocals and 1D `dsx2`/`dsy2` arrays (`ff_sin_pi_f`), fused `u` & `v` 2D loops into a single pointer-strided pass while preserving two-stage `two_sum` in `ff3_mul_ff` (`div256 = 5.68e-14`). |
| **`src/driver.cpp`** | `exp-tan-squirrel` (`16894854776193805176`) | `prog-fast-beluga` ; `hw_cost = 324` ; `fp32_ops = 157` ; `ae_score = 16.9833` | **`prog-fast-beluga`** (`5520963987098497878`, `ae=16.9833`) ; *(Alt: `prog-scarlet-swift` `hw=104`, `fp32_ops=52`)* | **Pareto-Optimal (`324` cost, `157` ops, `15.6374` score)** ; *(Alt: **-67.90%** in `scarlet-swift`)* | Preserved `_Float16` rational CFL (`1/20 = 0.05`) recovery and 48-bit `phi_lo` MPI ghost exchange (`scarlet-swift` drops step-0 circle re-projection to reach `52` FP32 ops). |
