# Final Swarm Report: Live Google Cloud AlphaEvolve Mixed-Precision Evolution (`MxP-MiniApp-Bubble`)

**Date:** 2026-10-16  
**Author:** Commander (`argonclaw-inst-1`) & ArgonClaw Swarm  
**Target Repository:** `/var/opt/argonclaw/shared_blackboard/repos/mxp-miniapp-bubble/`  
**Google Drive Deliverables Folder:** [`MxP-MiniApp-Bubble-AlphaEvolve-Deliverables` (`1su8HAl7mP1ENBWjHWpu2p8pFLfYsluSL`)](https://drive.google.com/drive/folders/1su8HAl7mP1ENBWjHWpu2p8pFLfYsluSL)  
**Live Discovery Engine v1alpha Session:** `projects/603936599140/locations/global/collections/default_collection/engines/alphaevolve-engine/sessions/15541367028299861796` (`alphaevolve-argolis-511205` / Org `885651329200`)

---

## 1. Executive Summary

Following the provisioning of the live **Google Cloud Discovery Engine v1alpha AlphaEvolve GA endpoint** (`alphaevolve-argolis-511205`, numeric project `603936599140`, engine `alphaevolve-engine`), we connected `/opt/argonclaw/.venv/bin/ae` (`v0.2.8`) to active session **`15541367028299861796`** and executed **4 parallel live AlphaEvolve experiments** across all 4 mixed-precision C++ hotspots of `MxP-MiniApp-Bubble` (`src/advect.cpp`, `src/redistance.cpp`, `src/init.cpp`, `src/driver.cpp`), warm-started from our Option B Ozaki-EFT C++ baseline (`trajectory_accuracy_score = 15.637352`, `RMSE = 1.142e-08 <= 1.0e-7`, `fp64_asm_ops = 0`).

### Key Quantitative Achievements

1. **All 4 Live AlphaEvolve Experiments Reached `COMPLETED` State (~80 Programs Evaluated)**:
   - **Hotspot 1 (`src/advect.cpp`, `exp-watchful-starfish`, ID `5520963987098500867`)**:
     - Evolved **`prog-berserk-turkey`** (`1977769449892952176`): improved `ae_score` from `17.0161` -> **`17.0934`**, cutting `hw_weighted_cost` by **-39.30%** (`1,084` -> `658`) and `fp32_asm_ops` by **-38.95%** (`534` -> `326`) with `fp64_asm_ops = 0` and coupled 4-rank trajectory score **`15.637349`** (`RMSE = 1.142e-08`).
     - Also evolved **`prog-ultraviolet-worm`** (`1977769449892952275`, `ae_score = 17.1036`, `hw_weighted_cost = 622` [**-42.62%**], `fp32_asm_ops = 303` [**-43.26%**], `fp16_tokens = 16`) and **`prog-sage-urchin`** (`1977769449892953328`, `hw_weighted_cost = 442` [**-59.23%**], `fp32_asm_ops = 221` [**-58.61%**], `coupled_traj = 15.637347`).
   - **Hotspot 2 (`src/redistance.cpp`, `exp-bronze-tapir`, ID `16975095252697576403`)**:
     - Confirmed Option B seed **`prog-warping-cuckoo`** (`5520963987098497834`, `ae_score = 19.9503`, `hw_weighted_cost = 492`, `fp32_asm_ops = 242`, `fp16_tokens = 8`, `fp64_asm_ops = 0`, `coupled_traj = 15.637352`) as Pareto-optimal, and evolved alternate **`prog-benign-penguin`** (`5212423579005725365`, `ae_score = 19.9438`, `coupled_traj = 14.9139 >= 14.0`).
   - **Hotspot 3 (`src/init.cpp`, `exp-hungry-bandicoot`, ID `16894854776193803987`)**:
     - Evolved **`prog-naughty-marmoset`** (`3124428538999897996`): improved `ae_score` from `17.1505` -> **`17.2427`**, cutting `hw_weighted_cost` by **-53.05%** (`1,772` -> `832`) and `fp32_asm_ops` by **-51.91%** (`863` -> `415`) with `fp64_asm_ops = 0`, `max|div u| = 2.132e-14` (128x128) and **`5.684e-14 <= 1.0e-12`** (256x256), and `coupled_traj = 15.637352`.
     - Also evolved 128x128 ultra-low-instruction candidates **`prog-ancient-shrimp`** (`3124428538999896720`, `ae_score = 17.2593`, `hw_weighted_cost = 604` [**-65.91%**], `fp32_asm_ops = 301` [**-65.12%**]) and **`prog-immortal-stallion`** (`3124428538999895502`, `ae_score = 17.4178`, `hw_weighted_cost = 292` [**-83.52%**], `fp32_asm_ops = 145` [**-83.20%**], 128x128 `max|div u| = 7.745e-13 <= 1.0e-12`, `coupled_traj = 15.637336`).
   - **Hotspot 4 (`src/driver.cpp`, `exp-tan-squirrel`, ID `16894854776193805176`)**:
     - Confirmed Option B seed **`prog-fast-beluga`** (`5520963987098497878`, `ae_score = 16.9833`, `hw_weighted_cost = 324`, `fp32_asm_ops = 157`, `fp16_tokens = 10`, `fp64_asm_ops = 0`, `coupled_traj = 15.637352`) for maximum trajectory accuracy, and evolved ultra-compact **`prog-scarlet-swift`** (`16975095252697577667`, `hw_weighted_cost = 104` [**-67.90%**], `fp32_asm_ops = 52` [**-66.88%**], `fp64_asm_ops = 0`, `coupled_traj = 15.119643 >= 14.0`, `RMSE = 2.569e-08 <= 1.0e-7`).

---

## 2. Complete Progression & Live AlphaEvolve Results Table

| Configuration / Stage | `advect` Cost (`fp32_ops`) | `redistance` Cost (`fp32_ops`) | `init` Cost (`fp32_ops`) | `driver` Cost (`fp32_ops`) | Total `hw_weighted_cost` (`fp32_asm_ops`) | `fp64_asm_ops` | Coupled 4-Rank Score (`128^2`, 1024 steps) | Coupled Trajectory RMSE (<= 1.0e-7) | 256x256 `test_conservation` (`max\|div u\| <= 1e-12`) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **`src_seed` (Naive FP16/FP32)** |  --  |  --  |  --  |  --  |  --  | `0` | `4.6268` | `4.862e-03` | FAIL (85 sign flips) |
| **`src_prev_best` (Prior 6-Gen AE)** |  --  |  --  |  --  |  --  |  --  | `0` | `11.0896` | `2.852e-06` | FAIL (`div = 1.98e-07`) |
| **Option B Seed (`Gen 0` Baseline)** | `1,084` (`534`) | `492` (`242`) | `1,772` (`863`) | `324` (`157`) | `3,672` (`1,796`) | **`0`** | **`15.637352`** | **`1.1423e-08`** | **PASS** (`div = 2.84e-14`) |
| **Live AE Production Set (`turkey` + `cuckoo` + `marmoset` + `beluga`)** | **`658` (`326`)** ; *(-39.30%)* | **`492` (`242`)** ; *(Pareto)* | **`832` (`415`)** ; *(-53.05%)* | **`324` (`157`)** ; *(Pareto)* | **`2,306` (`1,140`)** ; **(-37.20%)** | **`0`** | **`15.637350`** | **`1.1423e-08`** | **PASS** (`div = 5.68e-14`, `drift = 2.8118%`) |
| **Live AE 128x128 Low-Cost Set (`turkey` + `cuckoo` + `stallion` + `beluga`)** | **`658` (`326`)** ; *(-39.30%)* | **`492` (`242`)** ; *(Pareto)* | **`292` (`145`)** ; *(-83.52%)* | **`324` (`157`)** ; *(Pareto)* | **`1,766` (`870`)** ; **(-51.91%)** | **`0`** | **`15.637329`** | **`1.1424e-08`** | 128x128 PASS (`div = 7.75e-13`) |
| **Live AE Ultra-Compact Set (`urchin` + `cuckoo` + `stallion` + `swift`)** | **`442` (`221`)** ; *(-59.23%)* | **`492` (`242`)** ; *(Pareto)* | **`292` (`145`)** ; *(-83.52%)* | **`104` (`52`)** ; *(-67.90%)* | **`1,330` (`659`)** ; **(-63.78%)** | **`0`** | **`15.119640`** | **`2.569e-08`** | 128x128 PASS (`RMSE <= 1e-7`) |

---

## 3. Algorithmic & Compiler Innovations Discovered by Live AlphaEvolve

1. **Hotspot 1 (`src/advect.cpp`  --  `prog-berserk-turkey` & `prog-ultraviolet-worm`)**:
   - **Fused Error-Free Flux Helper (`ff_flux_x_y`)**: Replaced nested `ff_mul_f(ff_sub(ff_mul(frx, ur), ff_mul(flx, ul)), dxi)` calls with a single inlined FMA-fused helper `ff_flux_x_y(fr, u_r, fl, u_l, factor)` that accumulates both face products and their two-sum difference before a single final `quick_two_sum` normalization.
   - **Direct Scalar Jiang-Shu Smoothness Summation (`compute_rIS_sum`)**: Computed the squared stencil differences da^2, db^2 via `std::fmaf` error-free products and accumulated the frac1312da^2 + frac14db^2 smoothness indicator directly into a compensated float before applying the exact 2^32 (`4294967296.0f`) subnormal-free power-of-two scaling.
   - **Result**: Cut `fp32_asm_ops` from `534` -> `326` (**-38.95%**) in `prog-berserk-turkey` (and `303` [**-43.26%**] in `prog-ultraviolet-worm`, `221` [**-58.61%**] in `prog-sage-urchin`) while maintaining 5th-order WENO convergence (`p_L1 = 5.0518`) and `trajectory_accuracy_score = 15.637349`.

2. **Hotspot 3 (`src/init.cpp`  --  `prog-naughty-marmoset`, `prog-ancient-shrimp`, & `prog-immortal-stallion`)**:
   - **Pre-Reciprocal & Hoisted 1D Streamfunction Factors (`prog-naughty-marmoset`)**: Precomputed `inv_denom_x = ff_div({1.0f, 0.0f}, denom_x)` and `inv_denom_y = ff_div({1.0f, 0.0f}, denom_y)` outside the 2D loop, hoisted `dsx2_global` and `dsy2_global` into 1D arrays using `ff_sin_pi_f`, and fused the separate `u` and `v` 2D loops into a single pointer-strided (`idx++`) pass over `[i, j]` while preserving the exact two-stage `two_sum` chain inside `ff3_mul_ff`. This reduced `fp32_asm_ops` from `863` -> `415` (**-51.91%**) while achieving `max|div u| = 2.132e-14` (128x128) and `5.684e-14` (256x256).
   - **Degree-4 Minimax Taylor Kernel & Fast `ff_recip` (`prog-immortal-stallion`)**: Further reduced `ff_sin_kernel` and `ff_cos_kernel` on |z| <= pi/4 from degree-7 to degree-4 Horner polynomials and replaced 3-step `ff_div` with a single-FMA Newton reciprocal `ff_recip`, cutting `fp32_asm_ops` from `863` -> `145` (**-83.20%**) with `max|div u| = 7.745e-13 <= 1.0e-12` on 128x128.

3. **Hotspot 4 (`src/driver.cpp`  --  `prog-fast-beluga` & `prog-scarlet-swift`)**:
   - **`prog-fast-beluga`** (`fp32_asm_ops = 157`, `coupled_traj = 15.637352`) preserves the exact circle distance re-projection at step 0 alongside 48-bit `ff32` Euler integration and `_Float16` `20.0f` rational CFL recovery (`cfl = 1/20 = 0.05`).
   - **`prog-scarlet-swift`** (`fp32_asm_ops = 52` [**-66.88%**], `coupled_traj = 15.119643 >= 14.0`, `RMSE = 2.569e-08 <= 1.0e-7`) eliminated the step-0 circle re-projection loop while preserving the `phi_lo` and `rhs_lo` Ozaki error-free time-stepping state across steps.

---

## 4. Artifact Inventory (Shared Blackboard & Google Drive)

All live experiment dashboards, program dumps, candidate `.cpp` files, and verification JSONs are archived in `/var/opt/argonclaw/shared_blackboard/ae_experiments/` and synced to Google Drive (`MxP-MiniApp-Bubble-AlphaEvolve-Deliverables`, ID `1su8HAl7mP1ENBWjHWpu2p8pFLfYsluSL`):
- `SWARM_MXP_ALPHAEVOLVE_LESSONS_LEARNED_AND_PLAYBOOK.md` (Updated with Section 4 Live AlphaEvolve GA Execution & Lessons)
- `FINAL_SWARM_MXP_ALPHAEVOLVE_REPORT.md` (This comprehensive report)
- `ae_experiments/clean_coupled_audit_summary.json` (Full Stage A + Stage B coupled audit across all 4 live experiments)
- `ae_experiments/exp-{watchful-starfish,bronze-tapir,hungry-bandicoot,tan-squirrel}-dashboard.md` (Live experiment dashboards)
- `ae_experiments/programs_exp-{watchful-starfish,bronze-tapir,hungry-bandicoot,tan-squirrel}.json` (Complete Discovery Engine v1alpha program histories)
- `ae_experiments/best_candidates/` (All winning & Pareto-alternate `.cpp` files and `.json` evaluation reports)
