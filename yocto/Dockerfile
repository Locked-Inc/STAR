FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install required packages for Yocto build (ARM64/Apple Silicon compatible)
RUN apt-get update && apt-get install -y \
    # Core build tools
    build-essential \
    chrpath \
    cpio \
    diffstat \
    file \
    gawk \
    git \
    python3 \
    python3-pip \
    python3-distutils \
    python3-dev \
    wget \
    unzip \
    # Additional tools required by bitbake
    texinfo \
    libc6-dev \
    zlib1g-dev \
    liblz4-tool \
    libssl-dev \
    bc \
    u-boot-tools \
    # Essential tools
    binutils \
    bzip2 \
    curl \
    zstd \
    # Development tools
    vim \
    nano \
    sudo \
    locales \
    socat \
    xz-utils \
    debianutils \
    iputils-ping \
    tar \
    rsync \
    && rm -rf /var/lib/apt/lists/*

# Set up locale
RUN locale-gen en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

# Create yocto user with same permissions as host user
ARG USER_ID=1000
ARG GROUP_ID=1000
RUN (groupadd -g ${GROUP_ID} yocto 2>/dev/null || groupmod -n yocto $(getent group ${GROUP_ID} | cut -d: -f1)) && \
    useradd -m -u ${USER_ID} -g yocto -s /bin/bash yocto && \
    echo 'yocto ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

# Switch to yocto user
USER yocto
WORKDIR /home/yocto

# Set up git configuration (users can override)
RUN git config --global user.name "Yocto Builder" && \
    git config --global user.email "builder@example.com" && \
    git config --global init.defaultBranch main

# Create working directory
RUN mkdir -p /home/yocto/yocto-workspace

WORKDIR /home/yocto/yocto-workspace

# Copy entrypoint script (copy as root, then fix permissions)
USER root
COPY docker-entrypoint.sh /usr/local/bin/docker-entrypoint.sh
RUN chmod +x /usr/local/bin/docker-entrypoint.sh
USER yocto

ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
CMD ["/bin/bash"]