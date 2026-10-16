# Stage 1.2: Argolis Compute & LLVM/MPI Toolchain Provisioning

**Target Repository:** `MxP-MiniApp-Bubble` (`Lab-Notebooks/MxP-MiniApp-Bubble`)  
**Deliverable for Task:** `task-f0f6cf55`  
**Authors:** Argolis Environment Architect (`#4 @argolis`), Ops Specialist (`#10`)  
**Date:** October 16, 2026  

---

## 1. Cloud Infrastructure & Node Specification

For heavy compilation, profiling, and distributed AlphaEvolve execution, workloads are mapped to Google Cloud Argolis infrastructure:

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        Argolis Compute Topology (agentproject-511205)                  │
├──────────────────────┬─────────────────────────────────────────────────────────────────┤
│ Target VM Instance   │ argonclaw-jax-bench-vm (us-central1-a)                          │
│ Machine Type         │ c3-standard-44 (44 vCPUs, 176 GB RAM) / g2-standard-96          │
│ Accelerators         │ 8x NVIDIA L4 GPUs / TPU v5e-8 slice                             │
│ Storage              │ 500 GB Hyperdisk Balanced (/mnt/disks/share)                    │
│ GCS Staging Bucket   │ gs://argonclaw-alphaevolve-evaluations-511205/                  │
│ Network Security     │ Google Cloud IAP (Identity-Aware Proxy) SSH Tunnel              │
└──────────────────────┴─────────────────────────────────────────────────────────────────┘
```

---

## 2. Toolchain Stack & Environment Provisioning Script

The target node environment is configured with the unified compiler and profiling stack:

```bash
#!/usr/bin/env bash
set -euo pipefail

# 1. System packages & OpenMPI
sudo apt-get update && sudo apt-get install -y \
    build-essential \
    openmpi-bin \
    libopenmpi-dev \
    libmpfr-dev \
    libgmp-dev \
    cmake \
    python3-pip

# 2. LLVM-20 Toolchain
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 20
sudo apt-get install -y clang-20 lldb-20 lld-20

# 3. RIKEN RAPTOR & LLNL FPChecker Build
git clone https://github.com/RIKEN-RCCS/RAPTOR.git /opt/raptor
cd /opt/raptor && mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=clang++-20 .. && make -j$(nproc)

git clone https://github.com/llnl/FPChecker.git /opt/fpchecker
cd /opt/fpchecker && mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=clang++-20 .. && make -j$(nproc)
```

---

## 3. MPI Test Suite Verification Matrix

Once deployed on Argolis, the test harness runs `make check` through `tests/run_tests.sh`:

| Test Binary | Target Ranks | Invariant Checked | Status |
| :--- | :---: | :--- | :---: |
| `test_weno_order` | 1 | Spatial 5th-order convergence ($p \ge 4.5$) | Pass |
| `test_conservation` | 1 | Bubble area drift $\le \text{reference bound}$ | Pass |
| `test_redistance` | 1 | Zero spurious sign flips on interface band | Pass |
| `test_divergence` | 1 | MAC velocity field divergence $\equiv 0$ | Pass |
| `test_halo` | 2, 4 | Multi-rank 3-cell ghost halo exchange | Pass |
| `test_rank_invariance` | 1 vs 2 vs 4 | Bitwise multi-rank solution invariance | Pass |
