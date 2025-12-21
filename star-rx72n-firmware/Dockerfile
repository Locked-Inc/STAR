# RX72N Firmware Build Environment
FROM msalau/rx-elf-gcc:latest

# Install build dependencies (cached in image)
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
    cmake \
    make \
    git \
    && rm -rf /var/lib/apt/lists/*

# Set environment
ENV GNURX_ROOT=/opt/rx-elf-gcc
WORKDIR /work

# Default command
CMD ["bash"]
