#!/bin/bash
# Flash firmware to RX72N hardware

set -e

# Add rfp-cli to PATH
export PATH="$HOME/bin:$PATH"

# Check if firmware is built
if [ ! -f "build/star-rx72n-firmware.hex" ]; then
    echo "Error: Firmware not built. Run ./build.sh first."
    exit 1
fi

# Check if rfp-cli is installed
if ! command -v rfp-cli &> /dev/null; then
    echo "Error: rfp-cli not found. Install Renesas Flash Programmer CLI."
    echo "Download: https://www.renesas.com/us/en/software-tool/renesas-flash-programmer-programming-gui"
    exit 1
fi

echo "Flashing firmware to RX72N via E2 Lite..."
echo "Make sure E2 Lite is connected to your Mac and RX72N board."
echo ""

# Flash using rfp-cli (runs on Mac, not in Docker, for USB access)
rfp-cli \
    --device RX72N \
    --tool E2Lite \
    --interface FINE \
    --file build/star-rx72n-firmware.hex \
    --erase \
    --program \
    --verify \
    --reset

echo ""
echo "Flashing complete!"
