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

# Fail fast if the LFS pointer was used instead of the real binary.
# The pointer file is ~134 bytes; the real installer is ~215 MB.
# A successful install in <5 seconds is a reliable symptom of the wrong file.
RUN INSTALLER_SIZE=$(stat -c '%s' /tmp/gnurx.run) && \
    echo "Installer size: ${INSTALLER_SIZE} bytes" && \
    test "${INSTALLER_SIZE}" -gt 100000000 || \
    (echo "ERROR: gnurx.run is ${INSTALLER_SIZE} bytes -- looks like an LFS pointer, not the real installer." && exit 1)

RUN chmod +x /tmp/gnurx.run && \
    /tmp/gnurx.run -p /opt/gnurx -y && \
    rm /tmp/gnurx.run

# Diagnostic: show what the installer placed on disk and confirm architecture.
# Helps pinpoint "installed successfully" but binary-not-found failures.
RUN echo "=== Container arch ===" && uname -m && \
    echo "=== /opt/gnurx top-level ===" && (ls -la /opt/gnurx/ 2>&1 || echo "MISSING /opt/gnurx") && \
    echo "=== /opt/gnurx/bin (first 10) ===" && (ls -la /opt/gnurx/bin/ 2>&1 | head -10 || echo "MISSING /opt/gnurx/bin") && \
    echo "=== find rx-elf-gcc anywhere ===" && (find / -maxdepth 6 -name "rx-elf-gcc" 2>/dev/null || echo "NOT FOUND")

# Verify installation before declaring the layer complete.
# Fails the build immediately if the installer silently produced no toolchain.
RUN test -f /opt/gnurx/bin/rx-elf-gcc || \
    (echo "ERROR: rx-elf-gcc not found -- check diagnostic output above" && exit 1)
RUN /opt/gnurx/bin/rx-elf-gcc --version || \
    (echo "ERROR: rx-elf-gcc not executable (arch mismatch?)" && uname -m && exit 1)
RUN echo "GNURX toolchain verification successful"

ENV PATH="/opt/gnurx/bin:${PATH}"
