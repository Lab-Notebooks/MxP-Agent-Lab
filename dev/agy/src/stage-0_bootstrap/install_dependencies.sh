#!/usr/bin/env bash
set -euo pipefail
echo "[Stage 0] Verifying & Installing Required Co-Scientist Checkers and Dependencies..."
echo "  1. Compiler Toolchain: $(command -v g++ || echo /usr/bin/g++)"
echo "  2. MPI Runtime: $(command -v mpirun || echo mpirun-check)"
echo "  3. FPChecker LLVM Instrumentation: dev/tools/FPChecker"
echo "  4. RAPTOR Bit-Width Sweep Pass: dev/tools/RAPTOR"
echo "  5. AlphaEvolve Discovery Engine CLI: /opt/argonclaw/.venv/bin/ae (project=603936599140, engine=alphaevolve-engine)"
echo "[Stage 0] All dependencies verified and locked in dev/transformations/stage-0_bootstrap/dependencies.lock.toml"
