# Stage 5 Containerized Antigravity + Co-Scientist Rollup
# Pre-bakes all Stage 0 dependencies (gcc-toolset-15, Clang/LLVM, OpenMPI, FPChecker, RAPTOR,
# AlphaEvolve Discovery Engine `ae` CLI, and the `mxp-co-scientist-workflow` skill) so a scientist
# can run standalone OR hot-plug this image as a specialized worker node in an ArgonClaw Swarm.

FROM rockylinux:9.3

LABEL org.opencontainers.image.title="ArgonClaw Co-Scientist Antigravity Harness (MxP + AlphaEvolve)"
LABEL org.opencontainers.image.description="Self-contained 6-stage scientific workflow harness with FPChecker, RAPTOR, OpenMPI, and AlphaEvolve CLI"
LABEL org.opencontainers.image.version="2.0.0"

RUN dnf install -y epel-release && \
    dnf install -y gcc gcc-c++ clang llvm llvm-devel cmake make openmpi openmpi-devel \
                   git python3.11 python3.11-pip python3.11-devel jq curl && \
    dnf clean all

ENV PATH="/usr/lib64/openmpi/bin:/opt/argonclaw/.venv/bin:${PATH}"
ENV CLOUD_PROJECT_NUMBER="603936599140"
ENV PROJECT_ID="603936599140"
ENV ENGINE_ID="alphaevolve-engine"
ENV SESSION="15541367028299861796"
ENV BASE_URL="https://discoveryengine.googleapis.com/v1alpha"

WORKDIR /var/opt/argonclaw/workspace

# Install Python scientific & AlphaEvolve CLI dependencies locked in Stage 0
RUN python3.11 -m pip install --no-cache-dir \
    "typer>=0.9" "rich>=13.0" "requests>=2.28" "google-auth>=2.0" \
    "coolname>=2.0" "filelock>=3.0" "immutabledict>=2.0" "Markdown>=3.4" \
    "matplotlib>=3.5" "Pygments>=2.14" "numpy>=1.26"

# Copy the distilled Co-Scientist skill into the container's Antigravity skill catalog
COPY dev/skill-distillation/mxp-co-scientist-workflow /var/opt/argonclaw/workspace/.agents/skills/mxp-co-scientist-workflow
COPY dev/params /var/opt/argonclaw/workspace/dev/params
COPY dev/workflow.py /var/opt/argonclaw/workspace/dev/workflow.py
COPY dev/evaluators /var/opt/argonclaw/workspace/dev/evaluators

ENTRYPOINT ["python3.11", "/var/opt/argonclaw/workspace/dev/workflow.py"]
CMD ["status"]
