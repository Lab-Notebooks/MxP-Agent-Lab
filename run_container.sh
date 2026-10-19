#!/usr/bin/env bash
# ==============================================================================
# run_container.sh — Build & Launch the Self-Contained Co-Scientist Container
# Mounts local ./Deliverables so all 6 folders & native plots land on host machine
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

CONTAINER_ENGINE="${CONTAINER_ENGINE:-docker}"
if ! command -v "${CONTAINER_ENGINE}" >/dev/null 2>&1; then
    if command -v podman >/dev/null 2>&1; then
        CONTAINER_ENGINE="podman"
    else
        echo "Error: Neither docker nor podman found on PATH." >&2
        exit 1
    fi
fi

IMAGE_TAG="mxp-coscientist-argon:3.0"
HOST_DELIVERABLES="${HOST_DELIVERABLES:-${SCRIPT_DIR}/Deliverables}"
mkdir -p "${HOST_DELIVERABLES}"

echo "[1/2] Building ${IMAGE_TAG} using ${CONTAINER_ENGINE}..."
"${CONTAINER_ENGINE}" build -t "${IMAGE_TAG}" -f dev/skill-distillation/container_rollup/Containerfile .

echo "[2/2] Running ${IMAGE_TAG} with host deliverables mounted at ${HOST_DELIVERABLES}..."
"${CONTAINER_ENGINE}" run --rm -it \
    -e ARGON_MODEL="${ARGON_MODEL:-gemini-launch-candidate-high}" \
    -e ARGON_API_KEY="${ARGON_API_KEY:-}" \
    -e GEMINI_API_KEY="${GEMINI_API_KEY:-}" \
    -e GOOGLE_CLOUD_PROJECT="${GOOGLE_CLOUD_PROJECT:-}" \
    -e DELIVERABLES_DIR="/deliverables" \
    -v "${HOST_DELIVERABLES}:/deliverables" \
    "${IMAGE_TAG}" "${@:---interactive}"
