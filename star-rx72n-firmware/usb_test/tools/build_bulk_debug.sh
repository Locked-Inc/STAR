#!/bin/bash
# Build standalone bulk_debug.c + isr_stubs.c as a minimal RX72N USB bulk IN test.
# Not part of the Makefile target (which builds ThreadX-based usb_test.mot).
#
# Output: /tmp/bulk_debug.mot -- flash via rfp-cli.
set -e
cd "$(dirname "$0")/.."
export PATH="/opt/gnurx/bin:$PATH"
rx-elf-gcc -mcpu=rx72t -misa=v3 -mlittle-endian-data -O0 -g3 \
    -nostartfiles -T linker.ld -Wl,--gc-sections \
    bulk_debug.c startup.S vectors.S isr_stubs.c \
    -o /tmp/bulk_debug.elf
rx-elf-objcopy -O srec /tmp/bulk_debug.elf /tmp/bulk_debug.mot
rx-elf-size /tmp/bulk_debug.elf
echo
echo "Flash with:"
echo "  DOTNET_EnableDiagnostics=0 sudo box64 /opt/rfp/linux-x64/rfp-cli \\"
echo "      -d RX72N -t e2l -if fine -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \\"
echo "      -run -p /tmp/bulk_debug.mot"
