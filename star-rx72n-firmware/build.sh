#!/bin/bash
# Quick build script for RX72N firmware
# NOTE: Protocol buffers should be generated before running this script
# (via 'make proto-gen' or 'make proto-gen-firmware')

set -e

# Build Docker image (cached after first run)
docker build -t rx72n-build .

# Run build in Docker container
docker run --rm -v "$(pwd):/work" -w /work rx72n-build bash -c "
    # Build firmware
    mkdir -p build
    cd build
    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-gnurx.cmake ..
    make -j\${MAKE_JOBS:-\$(nproc)}
"

echo ""
echo "Build complete! Output files:"
ls -lh build/star-rx72n-firmware.{elf,hex,bin}
