# Deforming Bubble mini-app: one image for the containerized workflow stages
# (dev/workflows/alphaevolve, dev/workflows/verify).
#
#   docker build -t bubble-mxp .        # toolchain + RAPTOR, then make + make check
#   docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/work bubble-mxp make check
#
# The build runs the test suite against the src/ in the build context, so an
# image only exists for a tree that passes `make check`.  At run time the repo
# is mounted over /work, so the stages work on (and write into) the live tree.
#
# Stages:
#   toolchain  Ubuntu 24.04, OpenMPI, LLVM/clang 20 (apt.llvm.org), MPFR, Python
#   raptor     RAPTOR pass + runtime from the dev/tools/RAPTOR submodule
#   bubble     toolchain + RAPTOR install + the repo, built and tested

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

# The Makefile picks up RAPTOR from here (RAPTOR_INSTALL_DIR ?= ...).
# OpenMPI: allow root (docker build), oversubscription (tests run up to 4
# ranks on any core count), and no CMA single-copy (blocked in containers).
ENV RAPTOR_INSTALL_DIR=/opt/raptor-install \
    LLVM_VER=${LLVM_VER} \
    OMPI_ALLOW_RUN_AS_ROOT=1 \
    OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1 \
    OMPI_MCA_rmaps_base_oversubscribe=1 \
    OMPI_MCA_btl_vader_single_copy_mechanism=none \
    MPLBACKEND=Agg \
    HOME=/tmp

# The repo is mounted at run time with the host uid; let git read it.
RUN git config --system --add safe.directory '*'

WORKDIR /work
COPY . /work
RUN make -j"$(nproc)" && make check

CMD ["make", "check"]
