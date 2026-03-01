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
# xz-utils: the Renesas installer uses `tar --xz` (via an external xz binary)
# to decompress the embedded toolchain payload.  ubuntu:22.04 minimal does NOT
# include xz-utils, causing tar to silently fail and leave /opt/gnurx empty
# (the installer does not check tar exit code -- all errors go to /dev/null).
RUN apt-get update && apt-get install -y \
    cmake \
    make \
    ninja-build \
    python3 \
    git \
    xz-utils \
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

# Verify installation before declaring the layer complete.
# Fails the build immediately if the installer silently produced no toolchain.
RUN test -f /opt/gnurx/bin/rx-elf-gcc || \
    (echo "ERROR: rx-elf-gcc not found. Check that xz-utils is installed (the" && \
     echo "Renesas installer uses 'tar --xz' which requires the external xz" && \
     echo "program; ubuntu:22.04 minimal does NOT include it by default)." && \
     exit 1)
RUN /opt/gnurx/bin/rx-elf-gcc --version

ENV PATH="/opt/gnurx/bin:${PATH}"
