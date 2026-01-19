# RX72N Firmware Build Environment
# Build a modern RX GNU toolchain with C23 support.

FROM ubuntu:22.04 AS toolchain

ARG BINUTILS_VERSION=2.41
ARG GCC_VERSION=13.2.0
ARG NEWLIB_VERSION=4.3.0.20230120
ARG MAKE_JOBS=1

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y \
    build-essential \
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

# Download sources
RUN wget -q "https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.xz" && \
    wget -q "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz" && \
    wget -q "https://sourceware.org/pub/newlib/newlib-${NEWLIB_VERSION}.tar.gz" && \
    tar -xf "binutils-${BINUTILS_VERSION}.tar.xz" && \
    tar -xf "gcc-${GCC_VERSION}.tar.xz" && \
    tar -xf "newlib-${NEWLIB_VERSION}.tar.gz"

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

# Install build dependencies (cached in image)
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
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
RUN ARCH="$(uname -m)" && \
    case "$ARCH" in \
      x86_64) CMAKE_ARCH=linux-x86_64 ;; \
      aarch64|arm64) CMAKE_ARCH=linux-aarch64 ;; \
      *) echo "Unsupported arch: $ARCH" >&2; exit 1 ;; \
    esac && \
    curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-${CMAKE_ARCH}.tar.gz" \
      -o /tmp/cmake.tar.gz && \
    curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-SHA-256.txt" \
      -o /tmp/cmake.sha256 && \
    grep "cmake-${CMAKE_VERSION}-${CMAKE_ARCH}.tar.gz" /tmp/cmake.sha256 | awk '{print $1}' > /tmp/cmake.expected && \
    echo "$(cat /tmp/cmake.expected)  /tmp/cmake.tar.gz" | sha256sum -c - && \
    tar -xzf /tmp/cmake.tar.gz --strip-components=1 -C /usr/local && \
    rm -rf /tmp/cmake.tar.gz /tmp/cmake.sha256 /tmp/cmake.expected

# Copy toolchain from builder
COPY --from=toolchain /opt/rx-elf-gcc /opt/rx-elf-gcc

# Set environment
ENV GNURX_ROOT=/opt/rx-elf-gcc
WORKDIR /work

# Default command
CMD ["bash"]
