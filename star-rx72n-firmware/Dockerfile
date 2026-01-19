# RX72N Firmware Build Environment
FROM msalau/rx-elf-gcc:latest

# Install build dependencies (cached in image)
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
    ca-certificates \
    curl \
    make \
    git \
    && rm -rf /var/lib/apt/lists/*

# Install modern CMake (>= 3.20)
ARG CMAKE_VERSION=3.27.9
RUN curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
    -o /tmp/cmake.tar.gz && \
    curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-SHA-256.txt" \
    -o /tmp/cmake.sha256 && \
    grep "cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" /tmp/cmake.sha256 | awk '{print $1}' > /tmp/cmake.expected && \
    echo "$(cat /tmp/cmake.expected)  /tmp/cmake.tar.gz" | sha256sum -c - && \
    tar -xzf /tmp/cmake.tar.gz --strip-components=1 -C /usr/local && \
    rm -rf /tmp/cmake.tar.gz /tmp/cmake.sha256 /tmp/cmake.expected

# Set environment
ENV GNURX_ROOT=/opt/rx-elf-gcc
WORKDIR /work

# Default command
CMD ["bash"]
