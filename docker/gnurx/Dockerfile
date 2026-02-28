# =============================================================================
# Minimal GNURX Toolchain Image
#
# Purpose: Lightweight cross-compiler image for T2 CI (firmware build verify).
#   ~400 MB vs ~3 GB for the full devcontainer (osrf/ros:jazzy-desktop base).
#
# Build context: repo root (context: . in firmware-toolchain-image.yml).
#   The build context MUST be the repo root so that COPY can find the LFS-
#   tracked installer.  The .dockerignore at the repo root already allowlists
#   gcc-14.2.0.202511-GNURX-ELF.run and excludes everything else, keeping the
#   context small even though it is rooted at the top of the repo.
#
# LFS note: DO NOT replace COPY with curl/wget/ARG URL.
#   GitHub raw URLs for LFS-tracked files return a 134-byte pointer file, not
#   the real binary.  The firmware-toolchain-image.yml workflow uses
#   actions/checkout with lfs: true, which materializes the real binary on the
#   runner disk before Docker's build context is assembled.
# =============================================================================

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install cross-compile build dependencies.
# libncurses5, lib32z1, lib32stdc++6 are required by the Renesas .run installer
# (self-extracting SFX binary compiled for 32-bit).
RUN apt-get update && apt-get install -y \
    cmake \
    make \
    ninja-build \
    python3 \
    git \
    libncurses5 \
    lib32z1 \
    lib32stdc++6 \
    && rm -rf /var/lib/apt/lists/*

# Copy the LFS-tracked installer from the runner filesystem (materialized by
# actions/checkout with lfs: true in firmware-toolchain-image.yml).
# Flags -p /opt/gnurx -y copied verbatim from root Dockerfile line 77 --
# verified working with gcc-14.2.0.202511.  Do not invent or change flags:
# Renesas .run installers are version-specific and wrong flags cause a silent
# hang waiting for user input with no error output, timing out after 6 hours.
COPY gcc-14.2.0.202511-GNURX-ELF.run /tmp/gnurx.run
RUN chmod +x /tmp/gnurx.run && \
    /tmp/gnurx.run -p /opt/gnurx -y && \
    rm /tmp/gnurx.run

# Verify installation before declaring the layer complete.
# Fails the build immediately if the installer silently produced no toolchain.
RUN test -f /opt/gnurx/bin/rx-elf-gcc && \
    /opt/gnurx/bin/rx-elf-gcc --version

ENV PATH="/opt/gnurx/bin:${PATH}"
