# One image for the containerized workflow stages (dev/).
#
#   make image                          # docker build -t bubble-mxp .
#   docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/work bubble-mxp make check
#
# The build runs the test suite against the src/ in the build context, so an
# image only exists for a tree that passes `make check`.  At run time the repo
# is mounted over /work, so the stages work on (and write into) the live tree.
#
# AlphaEvolve jobs need Google Cloud credentials; mount them at run time (HOME is /tmp):
#   -v "$HOME/.config/gcloud":/tmp/.config/gcloud     (the bmx alias in AGENTS.md does this)
#
# Stages:
#   toolchain  Ubuntu 24.04, OpenMPI, LLVM/clang 20 (apt.llvm.org), MPFR, Python
#   raptor     RAPTOR pass + runtime from the dev/tools/RAPTOR submodule
#   bubble     toolchain + RAPTOR install + gcloud + ae CLI + the repo, built and tested
#              (root Makefile -> app/Makefile)

FROM ubuntu:24.04 AS toolchain

ARG LLVM_VER=20
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates wget gnupg lsb-release software-properties-common \
        build-essential make cmake ninja-build git binutils \
        openmpi-bin libopenmpi-dev libmpfr-dev \
        python3 python3-numpy python3-matplotlib python3-yaml \
    && wget -qO /tmp/llvm.sh https://apt.llvm.org/llvm.sh \
    && bash /tmp/llvm.sh ${LLVM_VER} \
    && apt-get install -y --no-install-recommends \
        llvm-${LLVM_VER}-dev libclang-${LLVM_VER}-dev \
    && rm -rf /var/lib/apt/lists/* /tmp/llvm.sh

# ---------------------------------------------------------------------------
FROM toolchain AS raptor

ARG LLVM_VER=20
COPY dev/tools/RAPTOR /opt/src/RAPTOR
# Only the clang plugin and runtime are needed (no MLIR / Flang).
RUN cmake -S /opt/src/RAPTOR -B /tmp/raptor-build -G Ninja \
        -DLLVM_DIR=/usr/lib/llvm-${LLVM_VER}/lib/cmake/llvm \
        -DCMAKE_INSTALL_PREFIX=/opt/raptor-install \
        -DCMAKE_BUILD_TYPE=Release \
        -DRAPTOR_MLIR=OFF \
    && ninja -C /tmp/raptor-build install \
    && rm -rf /tmp/raptor-build

# ---------------------------------------------------------------------------
FROM toolchain AS bubble

ARG LLVM_VER=20
COPY --from=raptor /opt/raptor-install /opt/raptor-install

# The root Makefile picks up RAPTOR from here (RAPTOR_INSTALL_DIR ?= ...).
# OpenMPI: allow root (docker build), oversubscription (tests run up to 4
# ranks on any core count), and no CMA single-copy (blocked in containers).
ENV RAPTOR_INSTALL_DIR=/opt/raptor-install \
    LLVM_VER=${LLVM_VER} \
    OMPI_ALLOW_RUN_AS_ROOT=1 \
    OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1 \
    OMPI_MCA_rmaps_base_oversubscribe=1 \
    OMPI_MCA_btl_vader_single_copy_mechanism=none \
    MPLBACKEND=Agg \
    LAB_ROOT=/work \
    HOME=/tmp

# AlphaEvolve: gcloud (auth) and the ae CLI (needs Python >= 3.13, fetched by uv).
# Installed system-wide so the host uid can run them.
ARG AE_REF=674dd5e558e917320c251c085e47fae33e1bbcff
COPY --from=ghcr.io/astral-sh/uv:latest /uv /usr/local/bin/uv
ENV UV_TOOL_DIR=/opt/uv/tools UV_TOOL_BIN_DIR=/usr/local/bin UV_PYTHON_INSTALL_DIR=/opt/uv/python
RUN wget -qO- https://packages.cloud.google.com/apt/doc/apt-key.gpg \
        | gpg --dearmor -o /usr/share/keyrings/cloud.google.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/cloud.google.gpg] https://packages.cloud.google.com/apt cloud-sdk main" \
        > /etc/apt/sources.list.d/google-cloud-sdk.list \
    && apt-get update && apt-get install -y --no-install-recommends google-cloud-cli \
    && rm -rf /var/lib/apt/lists/* \
    && uv tool install --python 3.13 \
        "git+https://github.com/Google-Cloud-AI/alphaevolve-on-googlecloud.git@${AE_REF}#subdirectory=skills" \
    && chmod -R a+rX /opt/uv && rm -rf /tmp/.cache

# The repo is mounted at run time with the host uid; let git read it.
RUN git config --system --add safe.directory '*'

WORKDIR /work
COPY . /work
RUN make -j"$(nproc)" && make check

CMD ["make", "check"]
