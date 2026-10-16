---
name: mxp-alphaevolve-optimizer
description: Autonomous scientific mixed-precision optimization, RAPTOR + FPChecker profiling, AlphaEvolve package synthesis, and mathematical proof pipeline for stencil-based fluid dynamics and level-set mini-apps.
---

# Mixed-Precision Scientific Kernel Optimizer & AlphaEvolve Pipeline (`mxp-alphaevolve-optimizer`)

Use this skill when tasked with optimizing scientific computing kernels, PDE solvers, or stencil-based fluid dynamics applications for mixed precision while strictly guaranteeing bounded cumulative trajectory error ($\le 10^{-7}$) with zero secular drift.

## Core Capabilities & Workflow

1. **Phase 0: Environment Probe & Storage Cascade:**
   - Probes available hardware (CPU cores, GPUs, memory) and toolchains (LLVM-20, OpenMPI, MPFR).
   - Establishes `AGENTS.md`-compliant repository structure:
     - `dev/provenance/`: Epistemic uncertainty ledger, persona impact ledger, and debate transcripts.
     - `dev/evaluators/`: Local pre-evaluators testing decoupled atomic kernel contracts.
     - `dev/proofs/`: Formal algebraic and numerical error bound proofs.
     - `dev/studies/`: Grid convergence, precision conversion, and robustness studies.
     - `assets/alphaevolve/`: AlphaEvolve experiment configurations and cost-weighted evaluators.

2. **Stage 1: Hotspot Decomposition & Complementary Profiling:**
   - Pairs **RIKEN-RCCS RAPTOR** (exponent/mantissa dynamic range profiling) with **LLNL FPChecker** (catastrophic cancellation, subnormal flushes, and precision loss traps).
   - Decouples dominant hotspots (e.g. WENO5, Godunov Eikonal) into `// EVOLVE-BLOCK-START` and `// EVOLVE-BLOCK-END` regions.
   - Applies Error-Free Transformations (Knuth TwoSum, Dekker FastTwoSum, TwoProd) and algebraic cancellation elimination (center-shifted stencils).

3. **Stage 2: Cost-Weighted AlphaEvolve Evaluation:**
   - Produces `evaluator.py` enforcing:
     - Composite fitness scoring: $-\log_{10}(\text{MSE} + 10^{-16}) \ge 14.0$ (RMSE $\le 10^{-7}$).
     - Disassembly audit: Verifies absence of unwhitelisted 64-bit double operations (`*sd`/`*pd`) via `objdump -d`.
     - Rich telemetry: Emits structured `AlphaEvolveEvaluationInsight` messages.

4. **Stage 3: Turnkey Distillation & Epistemic Governance:**
   - Enforces the hard **Pause-and-Ask Protocol**: Never guess or loosen constants; escalate unresolved uncertainties to the human.
   - Synthesizes reusable Docker container specs and provenance runbooks.
