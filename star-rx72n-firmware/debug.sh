#!/bin/bash
# Start GDB debugging session

set -e

# Check if firmware is built
if [ ! -f "build/star-rx72n-firmware.elf" ]; then
    echo "Error: Firmware not built. Run ./build.sh first."
    exit 1
fi

echo "Starting GDB debug session..."
echo "Make sure GDB server is running on localhost:2331"
echo ""
echo "Start GDB server in another terminal:"
echo "  JLinkGDBServer -device R5F572NN -if JTAG -speed 4000 -port 2331"
echo ""

docker run --rm -it -v "$(pwd):/work" -w /work/build --net=host rx72n-build make debug
