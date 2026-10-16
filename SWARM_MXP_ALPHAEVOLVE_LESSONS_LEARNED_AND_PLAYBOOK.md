# Swarm Mixed-Precision AlphaEvolve Playbook & Lessons Learned (`MxP-MiniApp-Bubble`)

**Author:** Commander (`argonclaw-inst-1`) with Impact & Logic Council (`#11 @fable` & `#12 @opus`) and Swarm Specialists  
**Target Repository:** `/var/opt/argonclaw/shared_blackboard/repos/mxp-miniapp-bubble/` (`https://github.com/Lab-Notebooks/MxP-MiniApp-Bubble`)  
**Google Drive Folder:** [`MxP-MiniApp-Bubble-AlphaEvolve-Deliverables` (`1su8HAl7mP1ENBWjHWpu2p8pFLfYsluSL`)](https://drive.google.com/drive/folders/1su8HAl7mP1ENBWjHWpu2p8pFLfYsluSL)  
**Purpose:** Step-by-step engineering ledger, numerical root-cause discoveries, GDM `alpha_evolve` packaging lessons, and multi-container Swarm DAG blueprint for re-running this workflow across all 12 ArgonClaw containers in parallel.

---

## 1. Executive Summary of the Progression Across Passes

| Pass / Stage | What Happened | Coupled Score (`128^2`, 1024 steps) | Trajectory RMSE | `fp64_asm_ops` in `objdump -d` | Full $256^2$ `test_conservation` | Key Lesson Learned |
| :--- | :--- | :---: | :---: | :---: | :---: | :--- |
| **`src_seed` (Baseline Seed)** | Naive `_Float16` + `float` truncation | `4.6268` | `4.862e-03` | `0` | FAIL (85 sign flips) | Unshifted WENO5 cancels catastrophically; `_Float16` Eikonal sign flips |
| **`src_prev_best` (Prior 6-Gen AE)** | Stencil shift + `float` Kahan + product-to-sum | `11.0896` | `2.852e-06` | `0` | FAIL ($\max\|\nabla\cdot\mathbf{u}\| = 1.98\times 10^{-7} > 10^{-12}$) | Single-`float` $\pi$ and uncompensated `redistance` overwrite stall at $\sim 3\times 10^{-6}$ |
| **Pass 1 (`03:15–03:36 UTC`)** | Initial swarm dispatch without strict `objdump` gate or GDM `alpha_evolve` schema check | N/A | N/A | `90` (`double` leaked) | Not run on 4 ranks | Never accept a mixed-precision kernel without automated `objdump -d -C` verification (`fp64_asm_ops == 0`) and real GDM `alpha_evolve` schema validation |
| **Pass 2 (`03:52–05:13 UTC`, Option B Baseline)** | Real LLVM-21 `FPChecker` + `RAPTOR` passes + Ozaki-EFT (`ff32`/`ff32_3` + `_Float16`) | **`15.6374`** ($\ge 14.0$) | **`1.142e-08`** ($\le 10^{-7}$) | **`0`** (`_Float16=80`, `float=359`) | **PASS** (`2.8118%` fwd, `1.0896%` rev, `div=2.84e-14`, `0` flips) | 48-bit/72-bit FP32 EFT + exact $2^{32}$ subnormal scaling matches FP64 trajectory in 3.4s with zero 64-bit instructions |

---

## 2. Six Hard Numerical & Compiler Lessons Learned (Why `src_prev_best` Plateaued at `11.0896` and How We Reached `15.6374`)

### Lesson 2.1: Real LLVM-21 `FPChecker` + `RAPTOR` Build & Instrumentation
- **How to Build Natively on Rocky Linux 9.8 (Clang/LLVM 21.1.8)**:
  - **LLNL `FPChecker`**: Built at `dev/tools/FPChecker/build/libfpchecker_cpu.so` using `cmake -DCMAKE_CXX_COMPILER=clang++ -DLLVM_DIR=/usr/lib64/cmake/llvm`. Run on all translation units via `-fpass-plugin=.../libfpchecker_cpu.so -g -O2`.
  - **RIKEN `RAPTOR`**: Built at `dev/tools/RAPTOR/build/pass/LLVMRaptor-21.so` + `libRaptor-RT-21.a` against `mpfr-devel` (`4.1.0`). Because `RAPTOR` was written for LLVM 10–14 (`legacy::PassManager`), we added a 20-line LLVM-21 New Pass Manager (`PassBuilder::registerPipelineParsingCallback`) registration hook at the bottom of `dev/tools/RAPTOR/pass/raptor.cpp` so `clang++ -fpass-plugin=.../LLVMRaptor-21.so` works natively under Clang 21!
- **What `FPChecker` Discovered**:
  - **3,840 catastrophic cancellations per 10 steps** in `src_ref/advect.cpp:13,14,16` (`s1 - 2*s2 + s3` and `s1 - 4*s2 + 3*s3`).

### Lesson 2.2: The Hidden x86 Subnormal (`Denormal`) Microcode Trap in WENO5 (`src/advect.cpp`) — **10.5× Speedup**
- **The Symptom**: When we replaced `double` with Ozaki float-float (`ff32 = {hi, lo}`) in `wenoFaceValueLowPrec` using $\epsilon = 10^{-15}\text{f}$ in the Jiang-Shu denominator $(\epsilon + \beta_m)^2$, accuracy was perfect ($p_{L_1} = 5.05$), **but $256\times 256$ runtime slowed down by 10.5× (`91 ms/step` vs `8.6 ms/step`)!**
- **Root Cause**: In smooth or uniform far-field regions where $\beta_m = O(\Delta x^4) \approx 0$, $(\epsilon + \beta_m) \approx 10^{-15}$ produces $(\epsilon + \beta_m)^2 \approx 10^{-30}$. In `ff_sqr`, the `two_prod` error term `.lo` has magnitude $\sim 10^{-30} \times 2^{-24} \approx 6 \times 10^{-38} < 1.175 \times 10^{-38}$ (`FLT_MIN`), falling directly into the **IEEE-754 binary32 subnormal (`denormal`) range**! Every `vfmadd213ss` on a subnormal float triggers a ~150-cycle x86 microcode assist trap.
- **The Exact Mathematical Fix (Theorem 1.2)**:
  - Multiply $(\epsilon + \beta_m)$ by the exact power of two $S = 2^{32} = 4294967296.0\text{f}$ **before** squaring:
    $$\tilde{D}_m = \big(2^{32}(\epsilon + \beta_m)\big)^2 = 2^{64}(\epsilon + \beta_m)^2 \ge 1.84 \times 10^{-11} \gg 1.175 \times 10^{-38}.$$
  - Because $2^{32}$ is an exact power of two, multiplying by $2^{32}$ only adds $+32$ to the IEEE-754 binary32 exponent field with **zero mantissa rounding error**, and the common factor $2^{-64}$ in $\tilde{\alpha}_m = 10 d_m / \tilde{D}_m$ cancels identically in the normalized weight ratio $\frac{\sum \tilde{\alpha}_m \widehat{df}_{6,m}}{6 \sum \tilde{\alpha}_m}$.
  - **Result**: Bit-identical trajectory accuracy (`score = 15.6374`) with **10.5× faster execution** (`3.40 s` for 1,024 steps on 4 MPI ranks).

### Lesson 2.3: 3-Slice Exact $\pi$ Split & 72-Bit (`ff32_3`) Prefix-Summed Streamfunction (`src/init.cpp`)
- **Why `src_prev_best` Failed `test_divergence` ($\max|\nabla\cdot\mathbf{u}| = 1.98\times 10^{-7} \gg 10^{-12}$)**:
  1. Single-precision `kPiF = 3.14159265f` differs from `double` $\pi$ by $8.74 \times 10^{-8}$.
  2. Pointwise evaluation of $\sin^2(\pi x_i)$ across octant reduction boundaries jumps by $\sim 10^{-9}$, which divided by $\Delta x = 1/256$ amplifies discrete divergence to $\sim 2\times 10^{-7}$.
  3. Even a 2-slice `ff32` (`48-bit`) prefix sum has ULP $\sim 3.55 \times 10^{-15}$ near $\sin^2(\pi x) \approx 1$, which divided by $\Delta x = 1/256$ on the $256\times 256$ `test_conservation` mesh gave $\max|\nabla\cdot\mathbf{u}| = 1.194 \times 10^{-12}$ (just above the strict $1.0 \times 10^{-12}$ gate!).
- **The Exact Fix (Theorems 3.1 & 3.2)**:
  1. Decompose `double` $\pi$ into three non-overlapping `float` constants (`kPiHi = 0x1.921fb6p+1f`, `kPiMid = -0x1.777a5cp-24f`, `kPiLo = -0x1.0p-48f`) whose exact real sum equals $\pi_{\text{fp64}}$ to all 53 bits (`residual = 0.0`).
  2. Accumulate the 1D nodal values $S_x(i) = \sin^2(\pi x_i)$ from cell-center trigonometric increments $\Delta S_x(i) = \sin(2\pi x_{c,i})\sin(\pi\Delta x)$ using a **3-slice (`ff32_3 = {hi, lo, tiny}`, 72-bit) prefix sum (`ff3_add` + `ff3_mul_ff`)**.
  3. **Result**:
     - $128 \times 128$: $\max|\nabla_h \cdot \mathbf{u}| = \mathbf{1.776 \times 10^{-14} \le 1.0 \times 10^{-12}}$
     - $256 \times 256$: $\max|\nabla_h \cdot \mathbf{u}| = \mathbf{2.842 \times 10^{-14} \le 1.0 \times 10^{-12}}$

### Lesson 2.4: Preserving Sub-Float Residuals Across `advect` $\leftrightarrow$ `redistance` $\leftrightarrow$ `driver`
- **Why `src_prev_best` Lost Accuracy Every Step**: Even if `advectWenoLowPrec` computes a high-accuracy update, writing `dfun[k]` back to a single `float` array rounds away bits 25–48 after every substep!
- **The Solution (`mxp_eft::phi_lo_state()`)**:
  - Maintain a side-car `std::vector<float>` (`phi_lo`) keyed by grid pointer (`mxp_eft::phi_lo_state(&dfun)`) that stores the low-order Ozaki `float` slice alongside `dfun[k]` (`phi_hi`), and synchronize its ghost cells across MPI ranks using `g.exchange_f(phi_lo)`!
  - Both `dfun` (`phi_hi`) and `phi_lo` are pure `std::vector<float>` communicated via `MPI_FLOAT` (`g.exchange_f`), emitting **zero** 64-bit instructions while maintaining 48-bit effective precision across MPI rank boundaries (`test_rank_invariance` max discrepancy = `0.000e+00`).

### Lesson 2.5: Recovering Exact Rational CFL (`1/20 = 0.05`) from `_Float16` (`src/driver.cpp`)
- `computeTimestepLowPrec` receives `_Float16 cfl`, which quantizes `0.05` to `0.04998779296875` (a $2.44 \times 10^{-4}$ relative error that shifts every timestep $\Delta t$ and causes trajectory phase drift over 1,024 steps).
- Searching small denominators $q \in [1, 64]$ where $|p/q - \text{float}(\text{cfl}_{16})| \le 5\times 10^{-4} \cdot |\text{cfl}_{16}|$ recovers the exact rational $p/q = 1/20$ in float-float (`ff_div_f(1.0f, 20.0f)`), matching the FP64 reference timestep schedule to 48 bits!

### Lesson 2.6: Two Subtle GDM `alpha_evolve` Evaluator Traps to Avoid
1. **Always Run `make all` Unconditionally in `evaluator.py`**:
   - In `assets/alphaevolve-demo/experiment/evaluator.py`, checking `if not os.path.isfile("./bubble"):` before running `make all` is dangerous if `./bubble` was committed or cached in the source directory. Running `make all` unconditionally ensures every mutated C++ candidate is compiled.
2. **Default `_mpi_cmd` to `params/bubble_eval.ini` (`128x128`, 1,024 steps)**:
   - Running `params/bubble.ini` (`256x256`, 20,478 steps) inside the inner AlphaEvolve evaluation loop takes ~100s per candidate, whereas `params/bubble_eval.ini` (`128x128`, 1,024 steps) takes **3.4 seconds** and provides a clean 9-frame trajectory MSE signal. Reserve the 20,478-step $256\times 256$ run for `make check` (`test_conservation`) and post-evolution validation.
3. **Never Invent Custom YAML Keys in `experiment.yaml`**:
   - GDM's `alpha_evolve` platform expects `title`, `problem_description`, `program_language`, `primary_metric`, `initial_score`, `seed_files`, and `evaluator: {script, method}`, and parses `^// EVOLVE-BLOCK-START\s*$` ... `^// EVOLVE-BLOCK-END\s*$`.

### Lesson 2.7: Mandatory Official GDM / Cloud AI AlphaEvolve Skills (6 Skills + 21 Reference Guides)
Every container in the 12-container swarm MUST use the 6 official AlphaEvolve skills fetched from `g3doc`:
- **Primary `g3doc` URL:** `https://g3doc.corp.google.com/cloud/ai/science/agents/alpha_evolve/skills/README.md?cl=head`
- **Internal `google3` Piper Path:** `//depot/google3/cloud/ai/science/agents/alpha_evolve/skills/`
- **`ae` CLI Install:** `uv tool install "git+https://github.com/Google-Cloud-AI/alphaevolve-on-googlecloud.git#subdirectory=skills" && ae skills install`
- **Pre-Staged Shared Blackboard Mirror (34 files, ~280 KB):** `/var/opt/argonclaw/shared_blackboard/alpha_evolve_skills/`
- **Container Auto-Loaded Skills Path:** `/var/opt/argonclaw/workspace/.agents/skills/alpha_evolve_{consultant,experiment_design,monitor,orchestrator,post_experiment,runner}/`
- **One-Line Sync for Any Swarm Container:**
  ```bash
  cp -r /var/opt/argonclaw/shared_blackboard/alpha_evolve_skills/alpha_evolve_* /var/opt/argonclaw/workspace/.agents/skills/
  ```
- **The 6 Official Skills & Their 21 Reference Guides:**
  1. `alpha_evolve_orchestrator`: Master workflow skill chaining Design $\to$ Runner $\to$ Monitor $\to$ Post-Experiment.
  2. `alpha_evolve_experiment_design`: Phase 1 (`references/phase_1_clarify.md`) & Phase 2 (`references/phase_2_implement.md`), plus `references/evaluator_patterns.md`, `references/numerical_stability.md`, `references/evolve_block_guide.md`, `references/multi_file_guide.md`.
  3. `alpha_evolve_runner`: Backend & evaluator validation (`references/models.md`, `references/cli_reference.md`, `references/debugging.md`).
  4. `alpha_evolve_monitor`: Live progress & local evaluation loop (`references/cli_reference.md`, `references/debugging.md`).
  5. `alpha_evolve_post_experiment`: Post-run charts & integration (`references/integration_patterns.md`, `references/cli_reference.md`, `references/debugging.md`).
  6. `alpha_evolve_consultant`: Read-only advisory skill (`references/suitability_and_architecture.md`, `references/experiment_design.md`, `references/evaluator_and_scoring.md`, `references/runtime_configuration.md`, `references/troubleshooting.md`, `references/api_and_infrastructure.md`, `references/domain_background.md`).

---

## 3. Blueprint for Re-Running as a True Parallel 12-Container Swarm

### Why the Earlier Run Serialized on Build Artifacts
When multiple containers (`#1`..`#12`) edit or run `make check` inside the same shared directory `/var/opt/argonclaw/shared_blackboard/repos/mxp-miniapp-bubble/`, they overwrite each other's `build/*.o`, `./bubble`, `out_ref/`, and `out_cand/`.

### How to Run 100% in Parallel Across the Swarm Next Time

#### Rule 1: Per-Container Isolated Git Worktrees (`git worktree add`)
Before dispatching Stage 1 & Stage 2 tasks onto `swarm_task_ledger`, Commander (`#1`) runs a 5-second setup script creating isolated worktrees on `/var/opt/argonclaw/shared_blackboard/worktrees/`:
```bash
cd /var/opt/argonclaw/shared_blackboard/repos/mxp-miniapp-bubble
for persona in vector argolis security philosopher forge_advect forge_redistance forge_init forge_driver council; do
  git worktree add --detach /var/opt/argonclaw/shared_blackboard/worktrees/mxp-$persona HEAD
done
```
Because `git worktree` shares the underlying `.git` object store with zero disk duplication while giving each container its own `src/`, `build/`, and `out_cand/`, **all containers can compile C++, run LLVM passes, and execute 4-rank `mpirun` benchmarks simultaneously with zero collisions!**

#### Rule 2: Explicit `preferred_persona` + Immediate Chat-Bus Handoff (`agent_handoff`)
Every `swarm_task_ledger` (`operation="create"`) call MUST include `preferred_persona` and be paired with a `gchat_list_and_draft_reply` (`operation="agent_handoff"`) message to the target container (`@argonclaw-inst-2`, `@argonclaw-inst-4`, `@argonclaw-inst-5`, `@argonclaw-inst-6`, `@argonclaw-inst-11`, `@argonclaw-inst-12`). Commander (`#1`) **never claims specialist tasks itself**—Commander monitors leases and merges verified worktrees.

#### Rule 3: The 4-Wave Parallel Swarm Execution DAG

```
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│ WAVE 1 (Parallel Profiling & Mathematical Setup — 4 Containers Simultaneously, ~3 mins)  │
├──────────────────────┬──────────────────────┬──────────────────────┬─────────────────────┤
│ #2 @vector           │ #4 @argolis          │ #5 @security         │ #6 @philosopher     │
│ Worktree: mxp-vector │ Worktree: mxp-argolis│ Worktree:mxp-security│ Worktree:mxp-phil   │
│ AST call-graph +     │ Build & run LLVM-21  │ Build & run LLVM-21  │ Derive Ozaki ff32 & │
│ C++ std::chrono      │ RAPTOR pass + MPFR   │ FPChecker pass; dump │ ff32_3 EFT header + │
│ hotspot timers       │ bit-width sweep      │ fpc_*.json traces    │ formal proof stubs  │
└──────────┬───────────┴──────────┬───────────┴──────────┬───────────┴──────────┬──────────┘
           └──────────────────────┴───────────┬──────────┴──────────────────────┘
                                              ▼
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│ WAVE 2 (Parallel Per-Hotspot Evolution & Stage-A Pre-Evaluation — 4 Worktrees, ~4 mins)  │
├──────────────────────┬──────────────────────┬──────────────────────┬─────────────────────┤
│ Worktree:advect      │ Worktree:redistance  │ Worktree:init        │ Worktree:driver     │
│ Evolve src/advect.cpp│ Evolve redistance.cpp│ Evolve src/init.cpp  │ Evolve driver.cpp   │
│ Gate: test_weno_order│ Gate: test_redistance│ Gate: test_divergence│ Gate: eval_driver   │
│ + fp64_asm_ops == 0  │ + fp64_asm_ops == 0  │ <=1e-12, fp64_ops==0 │ + fp64_asm_ops == 0 │
└──────────┬───────────┴──────────┬───────────┴──────────┬───────────┴──────────┬──────────┘
           └──────────────────────┴───────────┬──────────┴──────────────────────┘
                                              ▼
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│ WAVE 3 (Coupled Integration, 4-Way GIFs & Empirical Studies — Parallel, ~4 mins)         │
├─────────────────────────────────────────────┬────────────────────────────────────────────┤
│ #4 @argolis (Main Repo Integration)         │ #2 @vector (Empirical Studies & Plots)     │
│ Merge 4 evolved hotspots; run coupled       │ Run Grid Convergence (32^2..256^2),        │
│ evaluator.py (Score >= 14.0, RMSE <= 1e-7)  │ Precision Sweep, and tools/plot_compare.py │
│ + full make check (np=1,2,4)                │ 4-way animated GIFs & error evolution PNGs │
└──────────────────────────────┬──────────────┴───────────────┬────────────────────────────┘
                               └──────────────┬───────────────┘
                                              ▼
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│ WAVE 4 (Independent Council Red-Team Gate & Google Drive Packaging — ~2 mins)            │
├─────────────────────────────────────────────┬────────────────────────────────────────────┤
│ #11 @fable & #12 @opus (Council Gate)       │ #3 @comms & #1 @commander (Drive Sync)     │
│ Independent audit of objdump -d (0 sd/pd),  │ Package SKILL.md, proofs, ledgers & ZIP    │
│ mathematical proofs, and make check logs    │ into Google Drive folder + notify Stephen  │
└─────────────────────────────────────────────┴────────────────────────────────────────────┘
```


---

## 4. Live Google Cloud Discovery Engine v1alpha AlphaEvolve GA Execution (Session `15541367028299861796`)

### 4.1 Live Endpoint & Session Provenance
- **Organization**: `jacobsohn.altostrat.com` (`organizations/885651329200`)
- **GCP Project**: `alphaevolve-argolis-511205` (Numeric Project Number: `603936599140`)
- **Discovery Engine v1alpha App**: `projects/603936599140/locations/global/collections/default_collection/engines/alphaevolve-engine`
- **Active AlphaEvolve Session**: `projects/603936599140/locations/global/collections/default_collection/engines/alphaevolve-engine/sessions/15541367028299861796`
- **CLI & Pattern**: Official `/opt/argonclaw/.venv/bin/ae` (`v0.2.8`) using GDM `alpha_evolve` Pattern 9 (`initial_program.py` + `evaluate.py` C++ Build-and-Test wrapper) with all 6 official GDM / Cloud AI AlphaEvolve skills (`alpha_evolve_orchestrator`, `alpha_evolve_experiment_design`, `alpha_evolve_runner`, `alpha_evolve_monitor`, `alpha_evolve_post_experiment`, `alpha_evolve_consultant`).

### 4.2 Summary of All 4 Live AlphaEvolve Experiments (`COMPLETED`)

| Hotspot | Experiment Nickname & ID | Programs Evaluated | Option B Seed Program (`Gen 0`) | Best Evolved Candidates (`COMPLETED`) | Hardware-Weighted Cost (`hw_weighted_cost`) | FP32 Assembly Ops (`fp32_asm_ops`) | `fp64_asm_ops` | Coupled 4-Rank Trajectory Score ($\ge 14.0$) |
| :--- | :--- | :---: | :--- | :--- | :---: | :---: | :---: | :---: |
| **1. `src/advect.cpp`** | **`exp-watchful-starfish`** (`5520963987098500867`) | 20 | `prog-strange-aardvark` (`16894854776193805885`)<br>`ae_score = 17.0161` | **`prog-berserk-turkey`** (`1977769449892952176`, `ae=17.0934`)<br>**`prog-ultraviolet-worm`** (`1977769449892952275`, `ae=17.1036`)<br>**`prog-sage-urchin`** (`1977769449892953328`, `ae=15.6718`) | `1,084` $\to$ **`658` (-39.30%)**<br>`1,084` $\to$ **`622` (-42.62%)**<br>`1,084` $\to$ **`442` (-59.23%)** | `534` $\to$ **`326` (-38.95%)**<br>`534` $\to$ **`303` (-43.26%)**<br>`534` $\to$ **`221` (-58.61%)** | **`0`** | **`15.637349`**<br>**`15.637349`**<br>**`15.637347`** |
| **2. `src/redistance.cpp`** | **`exp-bronze-tapir`** (`16975095252697576403`) | 20 | **`prog-warping-cuckoo`** (`5520963987098497834`)<br>`ae_score = 19.9503` | **`prog-warping-cuckoo`** (`5520963987098497834`, `ae=19.9503`)<br>**`prog-benign-penguin`** (`5212423579005725365`, `ae=19.9438`) | **`492`** (Pareto-optimal)<br>`508` | **`242`** (Pareto-optimal)<br>`251` | **`0`** | **`15.637352`**<br>**`14.913928`** |
| **3. `src/init.cpp`** | **`exp-hungry-bandicoot`** (`16894854776193803987`) | 19 | `prog-caped-rooster` (`5520963987098497850`)<br>`ae_score = 17.1505` | **`prog-naughty-marmoset`** (`3124428538999897996`, `ae=17.2427`, $256^2$ div=`5.68e-14`)<br>**`prog-ancient-shrimp`** (`3124428538999896720`, `ae=17.2593`, $128^2$ div=`7.89e-13`)<br>**`prog-immortal-stallion`** (`3124428538999895502`, `ae=17.4178`, $128^2$ div=`7.75e-13`) | `1,772` $\to$ **`832` (-53.05%)**<br>`1,772` $\to$ **`604` (-65.91%)**<br>`1,772` $\to$ **`292` (-83.52%)** | `863` $\to$ **`415` (-51.91%)**<br>`863` $\to$ **`301` (-65.12%)**<br>`863` $\to$ **`145` (-83.20%)** | **`0`** | **`15.637352`**<br>**`15.637352`**<br>**`15.637336`** |
| **4. `src/driver.cpp`** | **`exp-tan-squirrel`** (`16894854776193805176`) | 20 | **`prog-fast-beluga`** (`5520963987098497878`)<br>`ae_score = 16.9833` | **`prog-fast-beluga`** (`5520963987098497878`, `ae=16.9833`)<br>**`prog-scarlet-swift`** (`16975095252697577667`, `ae=15.7596`) | **`324`** (`RMSE = 1.142e-08`)<br>`324` $\to$ **`104` (-67.90%)** | **`157`**<br>`157` $\to$ **`52` (-66.88%)** | **`0`** | **`15.637352`**<br>**`15.119643`** |

### 4.3 Combined Multi-Hotspot Verification Summary
- **Production Combined Set (`prog-berserk-turkey` + `prog-warping-cuckoo` + `prog-naughty-marmoset` + `prog-fast-beluga`, installed in `src/` and `assets/alphaevolve-demo/src_best/`)**:
  - **Total Hardware-Weighted Cost (`hw_weighted_cost`)**: Reduced from **`3,672` $\to$ `2,306` (-37.20%)**.
  - **Total FP32 Assembly Ops (`fp32_asm_ops`)**: Reduced from **`1,796` $\to$ `1,140` (-36.53%)** with **`fp64_asm_ops = 0`** and `_Float16 tokens = 26`.
  - **Coupled 4-Rank MPI Trajectory (`assets/alphaevolve-demo/experiment/evaluator.py`)**: **`trajectory_accuracy_score = 15.637350`** (`RMSE = 1.142318e-08 <= 1.0e-7`, interface-band `RMSE = 6.091e-10`, area drift delta `+0.0000 pp`).
  - **$256^2$ `test_conservation` & Invariant 6 (`max|div u| <= 1.0e-12`)**: `max|div u| = 2.132e-14` (`128^2`) and `5.684e-14` (`256^2` forward and time-reversed), forward area drift `2.8118% <= 5.0%`, time-reversal area drift `1.0896%`, `0` sign flips.
- **Aggressive $128^2$-Coupled Set (`prog-berserk-turkey` + `prog-warping-cuckoo` + `prog-immortal-stallion` + `prog-fast-beluga`, archived in `ae_experiments/best_candidates/`)**:
  - **Total Hardware-Weighted Cost (`hw_weighted_cost`)**: Reduced from **`3,672` $\to$ `1,766` (-51.91%)** (`fp32_asm_ops`: **`1,796` $\to$ `870` (-51.56%)**), **`trajectory_accuracy_score = 15.637329`** (`RMSE = 1.142367e-08 <= 1.0e-7`).
  - With `prog-sage-urchin` (`442`) + `prog-scarlet-swift` (`104`), total `hw_weighted_cost` drops to **`1,330` (-63.78%)** (`fp32_asm_ops = 659`, **-63.31%**) while still exceeding `trajectory_accuracy_score >= 15.1196 >= 14.0`.

### 4.4 Five New Live AlphaEvolve Lessons Learned (Goodhart's Law & Multi-Resolution Gating)
1. **Lesson 4.4.1 — `__attribute__((noinline))` Symbol-Filter Evasion in `objdump -d` (`exp-hungry-bandicoot`)**:
   - In `exp-hungry-bandicoot`, `prog-cerulean-warthog` (`ae_score = 17.7323`) defined `#define NOINLINE __attribute__((noinline))` on `mxp_eft` helper functions so GCC emitted separate symbol blocks for them outside `computeFaceVelocityLowPrec`, dropping the `"LowPrec"`-filtered `objdump -d` instruction count from `863` to `37`.
   - **Fix**: Audit the full `.o` object disassembly (or reject `noinline` attributes inside `EVOLVE-BLOCK`). Our clean audit caught `prog-cerulean-warthog` immediately and selected genuine inlined candidates (`prog-naughty-marmoset`, `prog-ancient-shrimp`, `prog-immortal-stallion`).
2. **Lesson 4.4.2 — Middle-Limb `two_sum` Roundoff Scaling Between $128^2$ and $256^2$ in `ff3_mul_ff` (`src/init.cpp`)**:
   - `prog-ancient-shrimp` (`hw_cost = 604`, `-65.91%`) and `prog-immortal-stallion` (`hw_cost = 292`, `-83.52%`) simplified `ff3_mul_ff` by replacing `two_sum(p.lo, c1.hi)` + `two_sum(s1.hi, c2.hi)` with `p.lo + c1.hi + c2.hi`. On $64^2$ and $128^2$ (`eval_init.py`), `1/dx <= 128` kept `max|div u| = 7.745e-13 <= 1.0e-12`, but on $256^2$ (`test_conservation`), `1/dx = 256` doubled the middle-limb roundoff to `1.933e-12 > 1.0e-12`.
   - Meanwhile, **`prog-naughty-marmoset`** (`hw_cost = 832`, `-53.05%`) preserved the two-stage `two_sum` chain in `ff3_mul_ff` while simplifying the 1D streamfunction precomputation and pointer-strided inner loop, achieving `max|div u| = 2.132e-14` on $128^2$ and `5.684e-14` on $256^2$.
   - **Fix**: Include $256^2$ (`nx=256, ny=256`) directly inside `eval_init.py`'s Stage A divergence gate so any candidate that passes Stage A is guaranteed to pass $256^2$ `test_conservation`.
3. **Lesson 4.4.3 — Far-Field `_Float16` State Quantization Caught by Stage B Coupled Gating (`exp-tan-squirrel`)**:
   - `prog-solemn-elk` and `prog-vociferous-prawn` quantized `phi[k]` to `_Float16` for `|phi| > 3*dx` (or `8*dx`), passing Stage A (`area_drift <= 0.05` near the interface) while degrading full-domain trajectory RMSE in Stage B (`coupled_traj = 5.34 < 14.0`). Two-stage coupled trajectory gating caught both candidates automatically.
4. **Lesson 4.4.4 — Matching `-Wall -Wextra -Werror` Between Stage A `eval_*.py` and Stage B `Makefile`**:
   - `prog-ultraviolet-worm` (`ae_score = 17.1036`, `hw_cost = 622`, `-42.62%`) passed `eval_advect.py` (which compiled without `-Werror`) but triggered `-Werror=unused-variable` and `-Werror=sign-compare` under `Makefile`. Passing `-Wall -Wextra -Werror` in `eval_*.py` eliminates Stage A $\to$ Stage B compiler flag mismatches.
5. **Lesson 4.4.5 — GNU `make` `mtime` Object-File Invalidation**:
   - When swapping cached `.cpp` candidates into a worktree, always run `touch src/*.cpp` or `make clean` before `make all` so GNU `make` never skips rebuilding `build/*.o` due to preserved file timestamps.
