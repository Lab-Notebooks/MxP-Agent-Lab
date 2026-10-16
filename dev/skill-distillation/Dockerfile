# Dockerfile -- Reproducible Environment for MxP-MiniApp-Bubble & AlphaEvolve
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

# 1. Install system toolchains, MPI, and MPFR
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    curl \
    python3 \
    python3-pip \
    openmpi-bin \
    libopenmpi-dev \
    libmpfr-dev \
    libgmp-dev \
    binutils \
    && rm -rf /var/lib/apt/lists/*

# 2. Install LLVM-20 toolchain
RUN wget https://apt.llvm.org/llvm.sh && \
    chmod +x llvm.sh && \
    ./llvm.sh 20 && \
    apt-get install -y clang-20 lldb-20 lld-20 && \
    rm -f llvm.sh

# 3. Clone and build RIKEN-RCCS RAPTOR
RUN git clone https://github.com/RIKEN-RCCS/RAPTOR.git /opt/raptor && \
    mkdir -p /opt/raptor/build && \
    cd /opt/raptor/build && \
    cmake -DCMAKE_CXX_COMPILER=clang++-20 .. && \
    make -j$(nproc)

# 4. Clone and build LLNL FPChecker
RUN git clone https://github.com/llnl/FPChecker.git /opt/fpchecker && \
    mkdir -p /opt/fpchecker/build && \
    cd /opt/fpchecker/build && \
    cmake -DCMAKE_CXX_COMPILER=clang++-20 .. && \
    make -j$(nproc)

# 5. Set up working directory
WORKDIR /workspace/mxp-miniapp-bubble
CMD ["/bin/bash"]
