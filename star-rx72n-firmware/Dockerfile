# RX72N Firmware Build Environment
# Build a modern RX GNU toolchain with C23 support.

FROM ubuntu:22.04 AS toolchain

ARG BINUTILS_VERSION=2.41
ARG GCC_VERSION=13.2.0
ARG NEWLIB_VERSION=4.3.0.20230120
ARG MAKE_JOBS=1

# Toolchain source tarball SHA256 checksums for supply-chain integrity verification
# Obtain official checksums from:
#   binutils: https://ftp.gnu.org/gnu/binutils/
#   gcc:      https://ftp.gnu.org/gnu/gcc/gcc-VERSION/
#   newlib:   https://sourceware.org/pub/newlib/
# Verify: sha256sum binutils-VERSION.tar.xz, etc.
ARG BINUTILS_SHA256=ae9a5789e23459e59606e6714723f2d3ffc31c03174191ef0d015bdf06007450
ARG GCC_SHA256=e275e76442a6067341a27f04c5c6b83d8613144004c0413528863dc6b5c743da
ARG NEWLIB_SHA256=83a62a99af59e38eb9b0c58ed092ee24d700fff43a22c03e433955113ef35150

# Build dependencies (compatible with both x86_64 and arm64)
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    gcc \
    g++ \
    bison \
    flex \
    texinfo \
    wget \
    curl \
    ca-certificates \
    xz-utils \
    python3 \
    libgmp-dev \
    libmpfr-dev \
    libmpc-dev \
    libisl-dev \
    libzstd-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp/src

# Download and verify toolchain sources with SHA256 checksums
RUN set -e && \
    BINUTILS_FILE="binutils-${BINUTILS_VERSION}.tar.xz" && \
    GCC_FILE="gcc-${GCC_VERSION}.tar.xz" && \
    NEWLIB_FILE="newlib-${NEWLIB_VERSION}.tar.gz" && \
    \
    wget -q "https://ftp.gnu.org/gnu/binutils/${BINUTILS_FILE}" && \
    wget -q "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/${GCC_FILE}" && \
    wget -q "https://sourceware.org/pub/newlib/${NEWLIB_FILE}" && \
    \
    echo "${BINUTILS_SHA256}  ${BINUTILS_FILE}" | sha256sum -c - && \
    echo "${GCC_SHA256}  ${GCC_FILE}" | sha256sum -c - && \
    echo "${NEWLIB_SHA256}  ${NEWLIB_FILE}" | sha256sum -c - && \
    \
    tar -xf "${BINUTILS_FILE}" && \
    tar -xf "${GCC_FILE}" && \
    tar -xf "${NEWLIB_FILE}" && \
    \
    rm "${BINUTILS_FILE}" "${GCC_FILE}" "${NEWLIB_FILE}"

# Build and install binutils
RUN mkdir -p /tmp/build/binutils && \
    cd /tmp/build/binutils && \
    /tmp/src/binutils-${BINUTILS_VERSION}/configure \
      --target=rx-elf \
      --prefix=/opt/rx-elf-gcc \
      --disable-nls \
      --disable-multilib \
      --disable-werror && \
    make -j"${MAKE_JOBS}" && \
    make install

# Ensure newly installed binutils/GCC are on PATH for subsequent stages.
ENV PATH="/opt/rx-elf-gcc/bin:${PATH}"

# Build and install GCC (stage 1, without libc)
RUN mkdir -p /tmp/build/gcc-stage1 && \
    cd /tmp/build/gcc-stage1 && \
    /tmp/src/gcc-${GCC_VERSION}/configure \
      --target=rx-elf \
      --prefix=/opt/rx-elf-gcc \
      --disable-nls \
      --enable-languages=c \
      --without-headers \
      --disable-shared \
      --disable-threads \
      --disable-multilib \
      --disable-lto \
      --disable-bootstrap \
      --disable-libssp \
      --disable-analyzer \
      --disable-libquadmath \
      --disable-libgomp \
      --disable-libatomic \
      --disable-libsanitizer \
      --disable-libstdcxx && \
    make -j"${MAKE_JOBS}" all-gcc && \
    make install-gcc

# Build and install newlib
RUN mkdir -p /tmp/build/newlib && \
    cd /tmp/build/newlib && \
    /tmp/src/newlib-${NEWLIB_VERSION}/configure \
      --target=rx-elf \
      --prefix=/opt/rx-elf-gcc \
      CC_FOR_TARGET=rx-elf-gcc \
      AR_FOR_TARGET=rx-elf-ar \
      AS_FOR_TARGET=rx-elf-as \
      RANLIB_FOR_TARGET=rx-elf-ranlib \
      LD_FOR_TARGET=rx-elf-ld \
      --disable-multilib \
      --disable-nls && \
    make -j"${MAKE_JOBS}" && \
    make install

# Build and install GCC (final)
RUN mkdir -p /tmp/build/gcc-final && \
    cd /tmp/build/gcc-final && \
    /tmp/src/gcc-${GCC_VERSION}/configure \
      --target=rx-elf \
      --prefix=/opt/rx-elf-gcc \
      --disable-nls \
      --enable-languages=c \
      --with-newlib \
      --disable-shared \
      --disable-threads \
      --disable-multilib \
      --disable-lto \
      --disable-bootstrap \
      --disable-libssp \
      --disable-libquadmath \
      --disable-libgomp \
      --disable-libatomic \
      --disable-libsanitizer \
      --disable-libstdcxx && \
    make -j"${MAKE_JOBS}" && \
    make install


FROM ubuntu:22.04

# Install runtime dependencies (compatible with both x86_64 and arm64)
RUN apt-get update && \
  DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    make \
    git \
    libisl23 \
    libgmp10 \
    libmpfr6 \
    libmpc3 \
    libzstd1 \
    && rm -rf /var/lib/apt/lists/*

# Install modern CMake (>= 3.20)
ARG CMAKE_VERSION=3.27.9
RUN /bin/bash -euo pipefail -c 'ARCH="$(uname -m)" && \
    case "$ARCH" in \
      x86_64) CMAKE_ARCH=linux-x86_64 ;; \
      aarch64|arm64) CMAKE_ARCH=linux-aarch64 ;; \
      *) echo "Unsupported arch: $ARCH" >&2; exit 1 ;; \
    esac && \
    curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-${CMAKE_ARCH}.tar.gz" \
      -o /tmp/cmake.tar.gz && \
    curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-SHA-256.txt" \
      -o /tmp/cmake.sha256 && \
    grep "cmake-${CMAKE_VERSION}-${CMAKE_ARCH}.tar.gz" /tmp/cmake.sha256 | awk "{print \$1}" > /tmp/cmake.expected && \
    echo "$(cat /tmp/cmake.expected)  /tmp/cmake.tar.gz" | sha256sum -c - && \
    tar -xzf /tmp/cmake.tar.gz --strip-components=1 -C /usr/local && \
    rm -rf /tmp/cmake.tar.gz /tmp/cmake.sha256 /tmp/cmake.expected'

# Copy toolchain from builder
COPY --from=toolchain /opt/rx-elf-gcc /opt/rx-elf-gcc

# Set environment - include cross-compiler tools on PATH
# GNURX_ROOT: Toolchain root directory
# PATH: Include rx-elf-gcc and other cross-compiler utilities for easy access
ENV GNURX_ROOT=/opt/rx-elf-gcc \
    PATH=/opt/rx-elf-gcc/bin:${PATH}

WORKDIR /work

# Default command
CMD ["bash"]