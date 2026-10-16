# Phase 0: Environment Probe & Hybrid Compute Topology

**Target Repository:** `https://github.com/Lab-Notebooks/MxP-MiniApp-Bubble`  
**Local Staging Path:** `/var/opt/argonclaw/shared_blackboard/repos/mxp-miniapp-bubble`  
**Timestamp:** `2026-10-16T03:02:00Z`  
**Probed by:** Swarm Commander (`argonclaw-inst-1`)  

---

## 1. Local Container Environment Hardware & OS

| Component | Probed Value | Notes |
| :--- | :--- | :--- |
| **Operating System** | Linux 7.1.6-1rodete1-amd64 (glibc 2.34) | Hardened Rocky Linux 9.8 DISA STIG container |
| **CPU Architecture** | x86_64 (Intel Xeon / AMD EPYC) | 24 vCPUs available |
| **Python Environment** | `/opt/argonclaw/.venv/bin/python3` (v3.11) | Standard agent harness virtual environment |
| **Binary Disassembler** | `/usr/bin/objdump` | Available for `*sd`/`*pd` assembly auditing |
| **Local Compilers** | `clang++-20`, `mpicxx`, `mpirun`: Uninstalled | Delegated to Argolis compute instance |

---

## 2. Hybrid Compute & Execution Target Topology

Following the master prompt's hybrid architecture selection:

1. **Stage 1 Profiling, AST Analysis & Pre-Evaluator Authoring:**
   - Executed across **Git Repo Hub (`#2`)**, **Tool Forge (`#7`)**, and **Mathematical Data Refactoring (`#8`)** inside the Antigravity container workspace.
   - Evaluator scaffolding and algorithmic transformations (Ozaki, TwoSum, WENO shifts) developed in `dev/evaluators/`.

2. **Stage 1.1 / 1.2 Binary Build, MPI Test Suite & RAPTOR / FPChecker Execution:**
   - Target Host: **Argolis GCP Instance `agentproject-511205`** (`argonclaw-jax-bench-vm` in `us-central1-a`).
   - Toolchain: Clang/LLVM-20, OpenMPI (`mpicxx`/`mpirun`), RIKEN RAPTOR (`dev/tools/RAPTOR`), and LLNL FPChecker (`dev/tools/FPChecker`).
   - Connectivity: IAP SSH tunnel managed by Argolis Specialist (`argonclaw-inst-4`).

3. **Stage 2 Multi-Instance AlphaEvolve Execution:**
   - Multi-branch search distributed across Argolis worker pools with GCS artifact caching (`_CLOUD_BUCKET_NAME` / `/mnt/disks/share`).
   - Coupled full-trajectory evaluation enforcing cumulative Trajectory $\text{RMSE} \le 10^{-7}$.

---

## 3. Storage Cascade & Governance

* **Local Working Copy:** `/var/opt/argonclaw/shared_blackboard/repos/mxp-miniapp-bubble/`
* **Agentic Orchestration Layer:** `dev/` (strictly adhering to repo `AGENTS.md`)
* **Google Drive Sync:** `/var/opt/argonclaw/workspace/gdrive/` via LOAS2 Stubby RPC (:18889)
* **High-Side / Air-Gap Diode Export:** Package ready for Sandia-style cross-domain staging.
