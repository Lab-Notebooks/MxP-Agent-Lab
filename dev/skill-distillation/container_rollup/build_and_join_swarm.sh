#!/usr/bin/env bash
# Build the Co-Scientist Container Rollup and optionally attach it as a worker node in an ArgonClaw Swarm
set -euo pipefail
RUNTIME="${CONTAINER_RUNTIME:-$(command -v podman || command -v docker)}"
IMAGE_TAG="${IMAGE_TAG:-argonclaw/co-scientist-mxp:2.0}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

echo "[Stage 5 Rollup] Building ${IMAGE_TAG} using ${RUNTIME} from ${REPO_ROOT}..."
"${RUNTIME}" build -t "${IMAGE_TAG}" -f "${REPO_ROOT}/dev/skill-distillation/container_rollup/Containerfile" "${REPO_ROOT}"

if [[ "${1:-}" == "--join-swarm" ]]; then
  INST_ID="${2:-13}"
  echo "[Stage 5 Rollup] Spawning argonclaw-inst-${INST_ID} on ArgonClaw Swarm..."
  "${RUNTIME}" run -d --name "argonclaw-inst-${INST_ID}" \
    -v "${HOME}/.argonclaw/shared_blackboard:/var/opt/argonclaw/shared_blackboard:z" \
    "${IMAGE_TAG}" status
fi
