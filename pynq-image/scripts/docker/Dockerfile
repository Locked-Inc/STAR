FROM ubuntu:20.04

# Prevent interactive prompts during build
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    git \
    build-essential \
    libncurses5-dev \
    zlib1g-dev \
    gawk \
    flex \
    bison \
    libssl-dev \
    bc \
    python3 \
    python3-pip \
    chrpath \
    socat \
    cpio \
    python3-pexpect \
    xz-utils \
    debianutils \
    iputils-ping \
    python3-git \
    python3-jinja2 \
    libegl1-mesa \
    libsdl1.2-dev \
    pylint3 \
    xterm \
    git-lfs \
    sudo \
    && rm -rf /var/lib/apt/lists/*

# Create build user
RUN useradd -m -s /bin/bash builder && \
    echo "builder ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# Set working directory
WORKDIR /workspace

# Switch to builder user
USER builder

# Set environment variables
ENV BB_NUMBER_THREADS=8
ENV PARALLEL_MAKE="-j8"

# Entry point
CMD ["/bin/bash"]
