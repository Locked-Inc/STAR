# =============================================================================
# Minimal ARM GCC Toolchain Image for STM32 Firmware Build CI
#
# Purpose: Lightweight cross-compiler image for STM32F767 firmware build
#   verification in CI.  ~300 MB vs the full devcontainer.
#
# Toolchain: gcc-arm-none-eabi 10.3-2021.07 from Ubuntu 22.04 apt.
#   No LFS installer required -- arm-none-eabi-gcc is a standard apt package.
#   Unlike docker/gnurx/Dockerfile, the build context is just docker/stm32/
#   (not the repo root) since no files need to be COPYed from the workspace.
#
# Provides:
#   arm-none-eabi-gcc, arm-none-eabi-g++, arm-none-eabi-as, arm-none-eabi-ld
#   arm-none-eabi-objcopy, arm-none-eabi-size (via binutils-arm-none-eabi)
#   cmake 3.22, ninja, python3
# =============================================================================

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install ARM cross-compile toolchain and build dependencies.
# binutils-arm-none-eabi: provides arm-none-eabi-size used for binary size
#   budget checks in CI.  gcc-arm-none-eabi depends on it so this is redundant
#   but explicit -- intent is clear and apt deduplicates the install.
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    cmake \
    ninja-build \
    make \
    python3 \
    git \
    && rm -rf /var/lib/apt/lists/*

# Sanity-check: fail fast if toolchain is not on PATH.
RUN arm-none-eabi-gcc --version && arm-none-eabi-size --version

WORKDIR /firmware
