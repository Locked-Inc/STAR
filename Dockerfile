# syntax=docker/dockerfile:1.4
FROM osrf/ros:jazzy-desktop

# Set up environment
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV DEBIAN_FRONTEND=noninteractive

# Install essential tools and dependencies
# Using BuildKit cache mounts for faster rebuilds
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y \
    python3-pip \
    python3-colcon-common-extensions \
    python3-rosdep \
    libgpiod-dev \
    i2c-tools \
    v4l-utils \
    udev \
    sudo \
    libprotobuf-dev \
    protobuf-compiler \
    libgrpc++-dev \
    protobuf-compiler-grpc \
    libgrpc-dev \
    curl \
    wget \
    ca-certificates \
    lcov \
    vim \
    cmake

# Install clang-18 toolchain from LLVM apt repo (pinned to 18.1.8).
# Matches CI workflow (firmware-unit-tests.yml) which also uses clang-18.
# The unversioned Ubuntu packages (clang, clang-tidy, clang-format) resolve
# to whatever version Ubuntu ships, which can drift between base images.
RUN wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc >/dev/null && \
    echo "deb http://apt.llvm.org/noble/ llvm-toolchain-noble-18 main" | tee /etc/apt/sources.list.d/llvm-18.list >/dev/null && \
    apt-get update && apt-get install -y --no-install-recommends \
    clang-18 \
    clangd-18 \
    clang-format-18 \
    clang-tidy-18 \
    libclang-rt-18-dev && \
    update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 && \
    update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-18 100 && \
    update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-18 100 && \
    update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-18 100

# Install Doxygen documentation toolchain with minimal LaTeX
# - graphviz: For @dot, @callgraph, @callergraph diagrams
# - plantuml: For @startuml state machine diagrams
# - mscgen: For @msc message sequence chart diagrams
# - default-jre-headless: Java runtime for PlantUML (headless = no GUI)
# - texlive (minimal): Only packages needed for Doxygen PDF generation
#   Replaces texlive-full (~5-7 GB) with targeted packages (~500 MB)
# - latexmk: Build automation for LaTeX (handles multiple passes)
#
# NOTE: doxygen is NOT installed from apt. Ubuntu 24.04 apt only ships 1.9.8
# which generates LaTeX using the abandoned tabu/longtabu package (broken with
# TeX Live 2022+). We install from GitHub below to get a working version.
# NOTE: If LaTeX compilation fails for star_documentation.tex, add packages
# incrementally. Likely candidates:
#   texlive-latex-extra  (tikz extras, booktabs, enumitem, etc.)
#   texlive-science      (algorithm packages)
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y \
    graphviz \
    plantuml \
    mscgen \
    default-jre-headless \
    texlive-latex-base \
    texlive-latex-recommended \
    texlive-fonts-recommended \
    texlive-pictures \
    texlive-xetex \
    fonts-urw-base35 \
    latexmk

# Install GNURX toolchain for RX72N firmware development
# Download directly from GitHub to avoid Git LFS issues in Codespaces
# GitHub's raw URL automatically resolves LFS files to actual binaries
ARG GNURX_INSTALLER_URL="https://github.com/Locked-Inc/STAR/raw/main/gcc-14.2.0.202511-GNURX-ELF.run"
RUN curl -sSL "${GNURX_INSTALLER_URL}" -o /tmp/gnurx-installer.run && \
    if head -1 /tmp/gnurx-installer.run | grep -q "version https://git-lfs"; then \
      echo "ERROR: Downloaded file is a Git LFS pointer, not the actual binary." && \
      echo "Ensure the repository is public or use a GitHub Release asset URL." && \
      exit 1; \
    fi && \
    mkdir -p /opt/gnurx && \
    chmod +x /tmp/gnurx-installer.run && \
    /tmp/gnurx-installer.run -p /opt/gnurx -y && \
    rm /tmp/gnurx-installer.run && \
    echo "GNURX toolchain installed successfully"

# Verify GNURX installation
RUN if [ ! -f /opt/gnurx/bin/rx-elf-gcc ]; then \
    echo "ERROR: GNURX installation failed - rx-elf-gcc not found" && exit 1; \
    fi && \
    /opt/gnurx/bin/rx-elf-gcc --version && \
    echo "GNURX toolchain verification successful"

# Add GNURX toolchain to PATH
ENV PATH="/opt/gnurx/bin:${PATH}"

# Install buf and Doxygen 1.16.1 in one layer (both are small curl-based
# downloads from GitHub releases; batching them cuts one image layer and keeps
# cache invalidation scoped together).
# Doxygen note: Ubuntu 24.04 apt only ships 1.9.8 which uses the unmaintained
# tabu/longtabu LaTeX package that fails with TeX Live 2022+. Doxygen 1.10+
# dropped tabu, so we must install from GitHub releases.
ARG BUF_VERSION=1.28.1
ARG DOXYGEN_VERSION=1.16.1
RUN set -eux; \
    curl -sSL "https://github.com/bufbuild/buf/releases/download/v${BUF_VERSION}/buf-$(uname -s)-$(uname -m)" \
        -o /usr/local/bin/buf; \
    chmod +x /usr/local/bin/buf; \
    DOXYGEN_TAG=$(echo "$DOXYGEN_VERSION" | tr '.' '_'); \
    curl -fsSL \
      "https://github.com/doxygen/doxygen/releases/download/Release_${DOXYGEN_TAG}/doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz" \
      -o /tmp/doxygen.tar.gz; \
    tar -xzf /tmp/doxygen.tar.gz -C /usr/local --strip-components=1; \
    rm /tmp/doxygen.tar.gz; \
    buf --version; \
    doxygen --version

# Install ARM bare-metal toolchain for STM32 development
# - gcc-arm-none-eabi / binutils-arm-none-eabi: Cross-compiler for Cortex-M targets
# - gdb-multiarch: Multi-architecture debugger (replaces arm-none-eabi-gdb)
# - ninja-build: Fast build system used by STM32CubeMX-generated CMake projects
# - openocd: On-chip debugger for flashing and debugging via ST-Link
# - stlink-tools: ST-specific CLI tools (st-flash, st-info, st-util)
# - usbutils: lsusb for diagnosing ST-Link passthrough inside container
# - libusb-1.0-0-dev: USB library required by OpenOCD and stlink-tools
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y \
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    gdb-multiarch \
    ninja-build \
    openocd \
    stlink-tools \
    usbutils \
    libusb-1.0-0-dev

# Install Gazebo Harmonic simulation stack for star_simulation package
# - ros-jazzy-ros-gz: Meta-package bridging ROS2 Jazzy and Gazebo Harmonic
# - ros-jazzy-ros-gz-sim: Launch Gazebo from ROS2 launch files
# - ros-jazzy-ros-gz-bridge: Bidirectional topic bridge (Gazebo <-> ROS2)
# Install SLAM, sensor fusion, and navigation (used by sim and real robot)
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y \
    ros-jazzy-ros-gz \
    ros-jazzy-ros-gz-sim \
    ros-jazzy-ros-gz-bridge \
    ros-jazzy-slam-toolbox \
    ros-jazzy-robot-localization \
    ros-jazzy-navigation2 \
    ros-jazzy-nav2-bringup \
    ros-jazzy-bondcpp \
    ros-jazzy-ros2run

# Install Renesas Flash Programmer CLI for RX72N flashing via E2 Lite
ARG RFP_CLI_URL="https://github.com/Locked-Inc/STAR/raw/main/RFP_CLI_Linux_V32200_x64.tgz"
RUN curl -sSL "${RFP_CLI_URL}" -o /tmp/rfp-cli.tgz && \
    tar -xzf /tmp/rfp-cli.tgz -C /opt && \
    mv /opt/linux-x64 /opt/rfp && \
    rm /tmp/rfp-cli.tgz && \
    ln -s /opt/rfp/rfp-cli /usr/local/bin/rfp-cli && \
    chmod +x /opt/rfp/rfp-cli

# Install CodeRabbit CLI for code review
RUN curl -fsSL https://cli.coderabbit.ai/install.sh | sh

# Install nanopb for Protocol Buffer C code generation
# Required for star-rx72n-firmware embedded target
RUN rm -f /usr/lib/python*/EXTERNALLY-MANAGED && \
    python3 -m pip install --no-cache-dir nanopb gcovr

# Verify nanopb installation (fails fast if installation broken)
RUN python3 -m pip show nanopb > /dev/null || (echo "ERROR: nanopb installation failed" && exit 1)

# Set up user LAST for UID/GID flexibility (Codespaces compatibility)
# Changing USER_UID won't invalidate cached layers above
ARG USERNAME=star
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN if getent passwd $USER_UID >/dev/null; then \
    EXISTING_USER=$(getent passwd $USER_UID | cut -d: -f1); \
    if [ "$EXISTING_USER" != "$USERNAME" ]; then \
    usermod -l $USERNAME $EXISTING_USER; \
    groupmod -n $USERNAME $(getent group $USER_GID | cut -d: -f1); \
    usermod -d /home/$USERNAME -m $USERNAME; \
    fi; \
    else \
    groupadd --gid $USER_GID $USERNAME; \
    useradd --uid $USER_UID --gid $USER_GID -m $USERNAME; \
    fi

# NOTE: Granting NOPASSWD sudo access to all commands is a security risk.
# This is intended only for development inside this container. Do NOT use
# this pattern in production; instead, restrict sudo to specific commands
# (e.g., usermod, rosdep) as needed.
RUN echo "$USERNAME ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# Add user to groups for hardware access
# Create spi and i2c groups if they don't exist, then add user
RUN groupadd -f spi && groupadd -f i2c \
    && usermod -aG video,dialout,plugdev,spi,i2c $USERNAME

# Ensure HOME is set for tool configurations
ENV HOME=/home/$USERNAME
# Do NOT set TMPDIR - let it default to /tmp for VS Code server,
# Docker-in-Docker, and Codespaces compatibility

# Ensure /tmp has correct permissions (standard across platforms)
RUN chmod 1777 /tmp

# Fix ownership of entire home directory.
# DevContainer features (Go, Node, Docker-in-Docker) run as root and may
# create subdirectories (.cache, .npm, .config, etc.) owned by root:root.
# This single chown fixes all current and future permission issues.
RUN chown -R $USERNAME:$USERNAME /home/$USERNAME

# Set up workspace
WORKDIR /workspaces/STAR

# Source ROS2 setup and add GNURX to PATH in bashrc
RUN echo "source /opt/ros/jazzy/setup.bash" >> /home/$USERNAME/.bashrc && \
    echo 'export PATH="/opt/gnurx/bin:$PATH"' >> /home/$USERNAME/.bashrc && \
    echo "alias claudee='claude --allow-dangerously-skip-permissions'" >> /home/$USERNAME/.bashrc

USER $USERNAME