# PYNQ-Z2 Yocto Build Environment Dockerfile
# Based on Ubuntu 22.04 with Xilinx tools integration

FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Set up build user with same UID/GID as host user (will be overridden)
ARG USERNAME=builder
ARG USER_UID=1000
ARG USER_GID=1000

# Install essential packages for Yocto and PetaLinux
RUN apt-get update && apt-get install -y \
    # Essential build tools
    build-essential \
    git \
    python3 \
    python3-pip \
    python3-setuptools \
    python3-venv \
    chrpath \
    diffstat \
    gawk \
    wget \
    curl \
    cpio \
    unzip \
    file \
    # Additional Yocto dependencies
    texinfo \
    libssl-dev \
    libelf-dev \
    zlib1g-dev \
    libncurses5-dev \
    libncursesw5-dev \
    # PetaLinux specific dependencies
    iproute2 \
    make \
    net-tools \
    tftp \
    flex \
    bison \
    libselinux1 \
    gnupg \
    socat \
    xterm \
    autoconf \
    libtool \
    tar \
    unzip \
    automake \
    screen \
    pax \
    gzip \
    # Additional utilities
    sudo \
    vim \
    nano \
    rsync \
    xz-utils \
    debianutils \
    iputils-ping \
    libegl1-mesa \
    libsdl1.2-dev \
    pylint \
    # Development tools
    cmake \
    pkg-config \
    libjson-c-dev \
    locales \
    # Additional compression tools required by Yocto
    lz4 \
    zstd \
    # Cleanup
    && apt-get autoremove -y \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install multilib packages only on x86_64 platforms
RUN if [ "$(dpkg --print-architecture)" = "amd64" ]; then \
        apt-get update && \
        apt-get install -y gcc-multilib g++-multilib libc6-dev-i386 && \
        apt-get clean && rm -rf /var/lib/apt/lists/*; \
    fi

# Configure Git (required for Yocto)
RUN git config --global user.name "PYNQ Builder" && \
    git config --global user.email "builder@pynq.local" && \
    git config --global color.ui false && \
    git config --global init.defaultBranch main

# Create build user
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo "$USERNAME:$USERNAME" | chpasswd \
    && adduser $USERNAME sudo \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# Set up Python environment
RUN python3 -m pip install --upgrade pip && \
    python3 -m pip install \
        pexpect \
        jinja2 \
        GitPython \
        subunit \
        testtools

# Configure shell environment for Yocto
RUN echo 'export LC_ALL=en_US.UTF-8' >> /etc/bash.bashrc && \
    echo 'export LANG=en_US.UTF-8' >> /etc/bash.bashrc && \
    echo 'export LANGUAGE=en_US.UTF-8' >> /etc/bash.bashrc

# Set locale
RUN apt-get update && apt-get install -y locales && \
    locale-gen en_US.UTF-8 && \
    update-locale LANG=en_US.UTF-8 && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en
ENV LC_ALL=en_US.UTF-8

# Create workspace directories
RUN mkdir -p /workspace/yocto-build && \
    chown -R $USERNAME:$USERNAME /workspace

# Switch to build user
USER $USERNAME
WORKDIR /workspace

# Set up build environment script
COPY --chown=$USERNAME:$USERNAME docker/build-env-setup.sh /workspace/
RUN chmod +x /workspace/build-env-setup.sh

# Default command
CMD ["/bin/bash"]