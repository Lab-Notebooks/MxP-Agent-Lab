# Epistemic Uncertainty & Assumption Ledger (Live Swarm Governance)

**Target Repository:** `MxP-MiniApp-Bubble` (`Lab-Notebooks/MxP-MiniApp-Bubble`)  
**Lead Epistemic Auditors:** Opus Logical Correctness Counsel (`#12 @opus` / Claude Opus 5.5 Max) & Philosophy/Impact Arbiter (`#11 @fable` / Claude Fable 5.1 High)  
**Commander & Swarm Authority:** Swarm Commander (`#1 @commander` / Gemini 4 Argon)  
**Status:** **ACTIVE EPISTEMIC TRACKING & INVARIANT OVERSIGHT**  
**Governing Rule:** *A tolerance that looks wrong is a blocked unit and a question for the human, never a loosened constant.* Any unresolved high-severity uncertainty triggers the hard **Pause-and-Ask Protocol**.

---

## 1. Epistemic Confidence & Uncertainty Register

| ID | Domain / Kernel | Epistemic Claim or Assumption | Evidence & Theoretical Risk Analysis | Severity | Epistemic Confidence | Status & Formal Resolution |
| :---: | :--- | :--- | :--- | :---: | :---: | :--- |
| **U-01** | `src/advect.cpp`<br/>(WENO5) | Stencil center shift ($v_k = s_k - s_3$) preserves exact high-order smoothness indicators $IS_k$. | Since $IS_k$ computes finite differences $\Delta s$ and second derivatives, subtracting $s_3$ is translationally invariant algebraically: $(s_1 - s_3) - 2(s_2 - s_3) + (s_3 - s_3) \equiv s_1 - 2s_2 + s_3$. However, finite-precision subtraction introduces $\mathcal{O}(\epsilon)$ perturbations before squaring. | **MEDIUM** | **0.95** (High) | Verified algebraically; pre-evaluator confirms condition number reduction when $|s_3| \gg |s_{k+1}-s_k|$. |
| **U-02** | `src/redistance.cpp`<br/>(Godunov) | Interface cell freezing (`sm * sxl < 0`) prevents zero-contour drift under low precision. | Freezing preserves original level-set values near the interface ($|\phi| \le \Delta x$). However, non-interface cells updated via Godunov Hamiltonian can develop gradient slope steepening if $|\nabla \phi| \ne 1$. In lower precision, gradient errors can creep into frozen neighbor boundaries. | **HIGH** | **0.88** (Medium-High) | Requires `test_redistance` check and Ozaki TwoSum residual accumulation. |
| **U-03** | `src/driver.cpp`<br/>(Trajectory Error) | Can mixed-precision (FP16/TF32/FP8) maintain cumulative $\text{RMSE} \le 1.0\times 10^{-7}$ over 20,000 steps without secular drift? | Under naive truncation, rounding errors compound as $\mathcal{O}(\sqrt{N})$ (random walk) or $\mathcal{O}(N)$ (biased rounding). Over $N=20,000$ steps, standard FP16 ($\epsilon \approx 10^{-3}$) or FP32 ($\epsilon \approx 10^{-7}$) will secularly drift far beyond $10^{-7}$. | **CRITICAL** | **0.98** (High with EFT) | **Mandate Error-Free Transformations (Knuth TwoSum)** or Ozaki mantissa slicing with high-precision residual accumulation on state update: $\phi^{n+1} = \text{TwoSum}(\phi^n, \Delta t \cdot \text{rhs})$. |
| **U-04** | `src/grid.cpp`<br/>(MPI Multi-Rank) | Mixed-precision local reductions and halo exchanges are bitwise invariant across rank counts (1, 2, 4). | Halo boundary padding in `src/grid.cpp` uses 3 ghost cells for WENO5. Non-associative floating-point addition in global reductions ($L_1, L_\infty$) causes divergence across MPI decomposition boundaries if order of summation varies. | **HIGH** | **0.92** (High) | Enforce fixed-order compensated reductions (e.g. reproBLAS or two-pass tree reductions) across halo exchanges (`test_rank_invariance`). |
| **U-05** | `src/redistance.cpp`<br/>(Hamiltonian Cancellation) | Computing $\text{agf} = \sqrt{|\nabla \phi|^2} - 1.0$ suffers catastrophic cancellation near converged distance fields $|\nabla \phi| \approx 1.0$. | When $|\nabla \phi| \approx 1.0$, subtracting $1.0$ in FP16 or TF32 cancels leading significant bits, generating pseudo-zero residuals and arresting redistancing convergence. | **HIGH** | **0.96** (High) | **Rationalized Reformulation**: Compute $\text{agf} = \frac{|\nabla \phi|^2 - 1.0}{\sqrt{|\nabla \phi|^2} + 1.0}$. This algebraically eliminates cancellation while maintaining IEEE equivalence. |
| **U-06** | `src/init.cpp` & `driver.cpp`<br/>(MAC Divergence) | Low-precision velocity field representation does not induce spurious mass generation through $\nabla \cdot \mathbf{u} \ne 0$. | Staggered MAC grid enforces $\nabla \cdot \mathbf{u} = 0$ on cell centers. Truncating face velocities to FP16 creates $\mathcal{O}(\Delta x^{-1} \epsilon)$ divergence perturbations, which act as unphysical sources/sinks on the level set. | **HIGH** | **0.90** (Medium-High) | Face velocities $u, v$ must maintain high-precision accumulator or exact integer-scaled fixed-point representation. |

---

## 2. Proof vs. Empirical Validation Hierarchy

To eliminate epistemic ambiguity between heuristic curve-fitting and formal verification, every optimization must be categorized according to the **ArgonClaw Epistemic Verification Taxonomy**:

```
                       ┌────────────────────────────────────────────────────────┐
                       │          EPISTEMIC VERIFICATION TAXONOMY               │
                       └───────────────────────────┬────────────────────────────┘
                                                   │
         ┌─────────────────────────────────────────┴─────────────────────────────────────────┐
         ▼                                                                                   ▼
┌─────────────────────────────────┐                                                 ┌─────────────────────────────────┐
│   LEVEL 1: FORMAL PROOF         │                                                 │ LEVEL 2: EMPIRICAL VALIDATION   │
│   • Exact algebraic identity    │                                                 │ • Multi-grid convergence sweep  │
│   • Error-Free Transform (EFT)  │                                                 │ • WENO5 order p >= 4.5          │
│   • Analytical error bound      │                                                 │ • Multi-physics perturbation    │
│   • Machine-precision guarantee │                                                 │ • Multi-rank invariance (1,2,4) │
└─────────────────────────────────┘                                                 └─────────────────────────────────┘
```

### Level 1: Formal Mathematical Proof (Target Standard)
A kernel modification is certified as **Mathematically Proven** if and only if:
1. **Algebraic Invariance**: The modified kernel $\mathcal{K}'(x)$ is algebraically identical to reference $\mathcal{K}(x)$ over $\mathbb{R}$, up to machine-precision roundoff $\epsilon_{\text{mach}}$.
2. **Error-Free Transformation (EFT) Bound**: When using Knuth TwoSum or TwoProd, the residual error $e$ satisfies:
   $$a + b = s + e \quad \text{where } s = \text{fl}(a + b), \quad |e| \le \epsilon_{\text{mach}} |s|$$
   Guaranteeing zero lost mantissa information.
3. **Analytical Trajectory Bound**: A Lyapunov or contraction mapping proof demonstrates that cumulative error $\mathcal{E}(T)$ at simulation time $T = N \Delta t$ satisfies:
   $$\mathcal{E}(T) \le C_1 \Delta t^p + C_2 N \epsilon_{\text{EFT}} \le 1.0 \times 10^{-7}$$
   holding for all initial conditions $\phi_0 \in \mathcal{H}^s(\Omega)$.

### Level 2: Multi-Dataset Empirical Proof (Fallback Standard)
If a non-linear kernel (e.g. non-smooth WENO weights $a_k = d_k / (\epsilon + IS_k)^2$) cannot be bounded analytically across all possible gradient discontinuities, it must satisfy **Three-Tier Empirical Validation**:
1. **Grid Refinement Sweep**: Grid sizes $N \in \{32^2, 64^2, 128^2, 256^2, 512^2\}$ must demonstrate spatial convergence order $p \ge 4.5$ in both $L_1$ and $L_\infty$ norms.
2. **Precision Bit-Width Conversion Sweep**: Profiling across FP64 $\to$ FP32 $\to$ TF32 $\to$ FP16 $\to$ FP8 must pinpoint the exact inflection point where cumulative RMSE crosses $1.0 \times 10^{-7}$.
3. **Multi-Physics & Geometry Robustness**: Successful execution across standard benchmark regimes:
   - Zalesak's slotted disk solid-body rotation (slits preserved without erosion).
   - Single-vortex stretching deformation (thin filament resolution).
   - Coalescing / breaking bubble topologies (zero spurious sign flips).

---

## 3. Non-Negotiable Pause-and-Ask Escalation Triggers

In accordance with the project constitution, the Swarm MUST immediately pause execution on the affected workflow branch and present a decision-ready question to the user under any of the following conditions:

```
[PAUSE TRIGGER HIT] ──► Record Uncertainty in Ledger ──► Halt Swarm DAG Branch ──► Dispatch Decision-Ready Question to Human
```

1. **Trajectory Accuracy Violation**: Cumulative Trajectory RMSE exceeds $1.0 \times 10^{-7}$ ($\text{score} < 14.0$) at any timestep during a full-duration test.
2. **Convergence Order Degradation**: Spatial WENO order $p < 4.5$ on `test_weno_order` under candidate mixed-precision configuration.
3. **Interface Sign Contamination**: Unphysical sign flips detected on the frozen interface band (`flips > 0` in `redistanceIteration`).
4. **Physical Conservation Breach**: Bubble area/volume conservation drift exceeds the baseline FP64 reference envelope on `test_conservation`.
5. **Multi-Rank Divergence**: Non-bitwise invariance observed across 1, 2, and 4 MPI ranks (`test_rank_invariance`).
6. **Decoupling Semantic Ambiguity**: Inability to decouple a computational kernel without mutating external Flash-X API contracts or physics specifications.
7. **Tolerance Temptation**: Any situation where passing a test requires modifying, loosening, or weakening an existing test assertion threshold. *(Strictly prohibited: ask the human; never loosen constants).*
