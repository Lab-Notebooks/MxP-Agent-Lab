#!/usr/bin/env bash
# ==============================================================================
# reproduce_all.sh — Standalone Base Package & Container Entrypoint
# Reproduces the 6-Stage AI Co-Scientist + AlphaEvolve Run Using Antigravity + Argon
# Zero `jetski` Dependency
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

# Ensure OpenMPI works seamlessly inside single containers (root or rootless UID 0 + oversubscribe)
export OMPI_ALLOW_RUN_AS_ROOT=1
export OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1
export OMPI_MCA_rmaps_base_oversubscribe=1

MODE="${1:---interactive}"
OUT_DELIVERABLES="${DELIVERABLES_DIR:-${SCRIPT_DIR}/Deliverables}"

echo "================================================================================"
echo "  MxP-MiniApp-Bubble: 6-Stage Co-Scientist Reproducibility Suite"
echo "  Runtime: Antigravity + Argon (Model: ${ARGON_MODEL:-gemini-launch-candidate-high})"
echo "  Deliverables Target: ${OUT_DELIVERABLES}"
echo "================================================================================"

# 1. Probe Antigravity + Argon Backend (Zero Jetski)
python3 -c '
import sys, json
from pathlib import Path
sys.path.insert(0, "dev")
from argon_antigravity_client import ArgonAntigravityClient
client = ArgonAntigravityClient(Path("."))
print("[Step 0] Antigravity + Argon Backend Probe:")
print(json.dumps(client.detect_backend(), indent=2))
'

# 2. Execute the 6-Stage Co-Scientist Workflow (Interactive HITL Gates or Auto-Council)
if [ "${MODE}" = "--auto" ] || [ "${MODE}" = "--batch" ]; then
    python3 dev/workflow.py run-all --mode turbo --auto-gate-with-council
else
    python3 dev/workflow.py run-interactive --mode turbo
fi

# 3. If mpicxx is installed on this host/container, compile and run the 17-run MPI check
if command -v mpicxx >/dev/null 2>&1; then
    echo ""
    echo "[Step 4a] Compiling evolved C++20/MPI solver and running 17-test verification suite..."
    make -j4 && make check
else
    echo ""
    echo "[Step 4a] Note: mpicxx not found on host PATH (use ./run_container.sh for containerized OpenMPI build)."
fi

# 4. Run native tools/plot_compare.py on simulation .pvti/.vti trajectories if present
if [ -d "out_ref_fp64" ] && [ -d "out_seed" ] && [ -d "out_evolved" ]; then
    if python3 -c "import matplotlib, numpy" >/dev/null 2>&1; then
        echo ""
        echo "[Step 4b] Regenerating native simulation contour plots & error_vs_time CSVs via tools/plot_compare.py..."
        python3 tools/plot_compare.py \
            --ref out_ref_fp64 \
            --cmp seed=out_seed \
            --cmp ae_best=out_evolved \
            --out-dir 02_Error_vs_Time_and_Visualizations || true
    else
        echo ""
        echo "[Step 4b] Pre-rendered native simulation plots verified in 02_Error_vs_Time_and_Visualizations/ (install matplotlib+numpy or use ./run_container.sh to re-render from raw .pvti/.vti)."
    fi
fi

# 5. Export Standardized 6-Folder Apples-to-Apples Deliverables to ${OUT_DELIVERABLES}
if [ "$(cd "${SCRIPT_DIR}" && pwd)" != "$(mkdir -p "${OUT_DELIVERABLES}" && cd "${OUT_DELIVERABLES}" && pwd)" ]; then
    echo ""
    echo "[Step 5] Exporting standardized 6-folder deliverables to ${OUT_DELIVERABLES}..."
    mkdir -p "${OUT_DELIVERABLES}"
    for item in \
        README.md \
        BEFORE_AND_AFTER_SUMMARY.md \
        FINAL_SWARM_MXP_ALPHAEVOLVE_REPORT.md \
        RUN_OUTPUT_AND_SPEEDUP.md \
        SWARM_MXP_ALPHAEVOLVE_LESSONS_LEARNED_AND_PLAYBOOK.md \
        01_Executive_Summaries_and_Reports \
        02_Error_vs_Time_and_Visualizations \
        03_Live_AlphaEvolve_Session_15541367028299861796 \
        04_Evolved_Source_Code_and_Hotspots \
        05_Evaluators_Proofs_and_Profiling \
        06_Skills_and_Archives \
        src \
        dev; do
        if [ -e "${SCRIPT_DIR}/${item}" ]; then
            cp -R "${SCRIPT_DIR}/${item}" "${OUT_DELIVERABLES}/"
        fi
    done
fi

echo ""
echo "================================================================================"
echo "  REPRODUCTION COMPLETE"
echo "  - Standardized 6-Folder Deliverables: ${OUT_DELIVERABLES}"
echo "  - Native Simulation Visuals:          ${OUT_DELIVERABLES}/02_Error_vs_Time_and_Visualizations/"
echo "  - Final Evolved C++ Source Code:      ${OUT_DELIVERABLES}/src/ & 04_Evolved_Source_Code_and_Hotspots/src_best/"
echo "  - 6-Stage HITL Provenance & Manifests:${OUT_DELIVERABLES}/dev/tmp/runs/"
echo "================================================================================"
