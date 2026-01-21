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
ARG BINUTILS_SHA256=e2caa33c1a53708eb9bdf1d4c5a0fd9b3e0d47f3aa3a17e94fe2fad19f9dc6a0
ARG GCC_SHA256=dba20a7cf0d98bf4f39a1da1d3e8c12ff48d5b2b3003f36c7308fbab8b55f2c89
ARG NEWLIB_SHA256=cbe1e34d80c3dbafc3e7e2e93fd371fa7fed5135d1a0d4d5a9e3dff4f80c0d58

# Pinned Debian package versions from ubuntu:22.04 for reproducible builds
# These versions must match the runtime libraries installed in the final stage
ARG GCC_PKG_VERSION=11.4.0-1ubuntu1~22.04
ARG LIBGMP_DEV_VERSION=2:6.2.1+dfsg-3ubuntu1
ARG LIBMPFR_DEV_VERSION=4.1.0-3build3
ARG LIBMPC_DEV_VERSION=1.2.1-2build1
ARG LIBISL_DEV_VERSION=0.24-2build1
ARG LIBZSTD_DEV_VERSION=1.4.8+dfsg-3build1
ARG BISON_VERSION=2:3.8.2+dfsg-1
ARG FLEX_VERSION=2.6.4-8.2
ARG TEXINFO_VERSION=6.8-4build1
ARG PYTHON3_VERSION=3.10.12-1~22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    gcc=${GCC_PKG_VERSION} \
    g++=${GCC_PKG_VERSION} \
    bison=${BISON_VERSION} \
    flex=${FLEX_VERSION} \
    texinfo=${TEXINFO_VERSION} \
    wget \
    curl \
    ca-certificates \
    xz-utils \
    python3=${PYTHON3_VERSION} \
    libgmp-dev=${LIBGMP_DEV_VERSION} \
    libmpfr-dev=${LIBMPFR_DEV_VERSION} \
    libmpc-dev=${LIBMPC_DEV_VERSION} \
    libisl-dev=${LIBISL_DEV_VERSION} \
    libzstd-dev=${LIBZSTD_DEV_VERSION} \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp/src

# Download and verify toolchain sources with SHA256 checksums
RUN /bin/bash -euo pipefail -c '\
    set -x && \
    BINUTILS_FILE="binutils-${BINUTILS_VERSION}.tar.xz" && \
    GCC_FILE="gcc-${GCC_VERSION}.tar.xz" && \
    NEWLIB_FILE="newlib-${NEWLIB_VERSION}.tar.gz" && \
    \
    # Download all tarballs
    wget -q "https://ftp.gnu.org/gnu/binutils/${BINUTILS_FILE}" || exit 1 && \
    wget -q "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/${GCC_FILE}" || exit 1 && \
    wget -q "https://sourceware.org/pub/newlib/${NEWLIB_FILE}" || exit 1 && \
    \
    # Create SHA256 manifest
    cat > SHA256SUMS <<EOF
${BINUTILS_SHA256}  ${BINUTILS_FILE}
${GCC_SHA256}  ${GCC_FILE}
${NEWLIB_SHA256}  ${NEWLIB_FILE}
EOF && \
    \
    # Verify all checksums
    echo "Verifying SHA256 checksums..." && \
    if ! sha256sum -c SHA256SUMS; then \
      echo "ERROR: Checksum verification failed for toolchain sources!" >&2 && \
      rm -f ${BINUTILS_FILE} ${GCC_FILE} ${NEWLIB_FILE} SHA256SUMS && \
      exit 1; \
    fi && \
    echo "Checksums verified successfully" && \
    \
    # Extract only after successful verification
    tar -xf "${BINUTILS_FILE}" && \
    tar -xf "${GCC_FILE}" && \
    tar -xf "${NEWLIB_FILE}" && \
    \
    # Clean up verification manifest and archives (keep extracted sources)
    rm -f SHA256SUMS ${BINUTILS_FILE} ${GCC_FILE} ${NEWLIB_FILE} && \
    \
    echo "Toolchain sources extracted and verified"
  '

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

# Pinned runtime library and tool versions from ubuntu:22.04 for reproducible builds
# Library versions must match the -dev package versions in the builder stage
# Tool versions ensure consistent build environment across all builds
ARG LIBISL23_VERSION=0.24-2build1
ARG LIBGMP10_VERSION=2:6.2.1+dfsg-3ubuntu1
ARG LIBMPFR6_VERSION=4.1.0-3build3
ARG LIBMPC3_VERSION=1.2.1-2build1
ARG LIBZSTD1_VERSION=1.4.8+dfsg-3build1
ARG MAKE_VERSION=4.3-4.1
ARG GIT_VERSION=1:2.34.1-1ubuntu1

# Install runtime dependencies (cached in image)
RUN apt-get update && \
  DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    make=${MAKE_VERSION} \
    git=${GIT_VERSION} \
    libisl23=${LIBISL23_VERSION} \
    libgmp10=${LIBGMP10_VERSION} \
    libmpfr6=${LIBMPFR6_VERSION} \
    libmpc3=${LIBMPC3_VERSION} \
    libzstd1=${LIBZSTD1_VERSION} \
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
