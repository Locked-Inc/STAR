#!/bin/bash
# Quick build script for RX72N firmware

set -e

# Build Docker image (cached after first run)
docker build -t rx72n-build .

# Run build (includes proto generation inside container)
docker run --rm -v "$(pwd):/work" -w /work rx72n-build bash -c "
    # Regenerate protocol buffers before building (nanopb for firmware)
    echo 'Ensuring protocol buffers are up to date...'
    cd /work/.. && make proto-gen-firmware && cd /work

    # Build firmware
    mkdir -p build
    cd build
    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-gnurx.cmake ..
    make -j\${MAKE_JOBS:-\$(nproc)}
"

echo ""
echo "Build complete! Output files:"
ls -lh build/star-rx72n-firmware.{elf,hex,bin}
