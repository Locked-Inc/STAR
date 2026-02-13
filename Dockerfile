FROM osrf/ros:jazzy-desktop

# Set up environment
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV DEBIAN_FRONTEND=noninteractive

# Install essential tools and dependencies
RUN apt-get update && apt-get install -y \
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
    clangd \
    clang-format \
    clang \
    vim \
    cmake \
    && rm -rf /var/lib/apt/lists/*

# Install Doxygen documentation toolchain
# - doxygen: Core documentation generator
# - graphviz: For @dot, @callgraph, @callergraph diagrams
# - plantuml: For @startuml state machine diagrams
# - default-jre-headless: Java runtime for PlantUML (headless = no GUI)
# - texlive-full: Complete LaTeX distribution for PDF generation
# - latexmk: Build automation for LaTeX (handles multiple passes)
RUN apt-get update && apt-get install -y \
    doxygen \
    graphviz \
    plantuml \
    default-jre-headless \
    texlive-full \
    latexmk \
    && rm -rf /var/lib/apt/lists/*

# Install nanopb for Protocol Buffer C code generation
# Required for star-rx72n-firmware embedded target
RUN rm -f /usr/lib/python*/EXTERNALLY-MANAGED && \
    python3 -m pip install --no-cache-dir nanopb

# Verify nanopb installation (fails fast if installation broken)
RUN python3 -m pip show nanopb > /dev/null || (echo "ERROR: nanopb installation failed" && exit 1)

# Install buf
ARG BUF_VERSION=1.28.1
RUN curl -sSL "https://github.com/bufbuild/buf/releases/download/v${BUF_VERSION}/buf-$(uname -s)-$(uname -m)" -o /usr/local/bin/buf \
    && chmod +x /usr/local/bin/buf

# Install GNURX toolchain for RX72N firmware development
# Copy local installer and install the toolchain to /opt/gnurx
COPY gcc-14.2.0.202511-GNURX-ELF.run /tmp/gnurx-installer.run
RUN mkdir -p /opt/gnurx && \
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

# Set up user
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

# Set up workspace
WORKDIR /workspaces/STAR

# Source ROS2 setup and add GNURX to PATH in bashrc
RUN echo "source /opt/ros/jazzy/setup.bash" >> /home/$USERNAME/.bashrc && \
    echo 'export PATH="/opt/gnurx/bin:$PATH"' >> /home/$USERNAME/.bashrc

USER $USERNAME