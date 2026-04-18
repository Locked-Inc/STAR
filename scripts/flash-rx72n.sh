#!/usr/bin/env bash
#
# flash-rx72n.sh -- Flash an ELF or S-record image to an RX72N on the
#                   STAR custom PCB, using either the E2 Lite (FINE) or
#                   the on-board USB-UART in SCI Boot Mode.
#
# Usage:
#   scripts/flash-rx72n.sh <file> [e2lite|sci] [port]
#
#   <file>  : path to a .elf or .mot image (required). ELF is auto-converted
#             to Motorola S-record via rx-elf-objcopy.
#   method  : flashing method (default: e2lite)
#   port    : override serial port for sci method (e.g. /dev/tty.usbserial-B0046G3K)
#
# Prerequisites:
#   /opt/gnurx/bin/rx-elf-objcopy   (GNURX 14.2 toolchain)
#   /opt/rfp/linux-x64/rfp-cli      (Renesas Flash Programmer CLI v3.22)
#   /etc/udev/rules.d/60-renesas-e2lite.rules  (for unprivileged E2 Lite)
#
# e2lite method (default):
#   - Board powered externally (USB-UART cable provides 5V rail)
#   - SW4 Pin1=OFF, Pin2=OFF  (Single Chip Mode -- JTAG/FINE bypasses this)
#   - E2 Lite plugged into debug header
#   - Uses FINE interface @ 250 kbps (1 Mbps gives framing errors on our PCB)
#   - ID code is the factory default (16 bytes of 0xFF)
#
# sci method:
#   - Board powered externally (USB-UART cable also handles boot communication)
#   - SW4 Pin1=ON,  Pin2=OFF  (SCI Boot Mode)
#   - Power-cycle the board AFTER setting the switch so it enters boot mode
#   - After flashing, set SW4 Pin1=OFF and power-cycle to run the code

set -euo pipefail

if [[ "$(uname)" == "Darwin" ]]; then
    RFP=/Users/bsikar/opt/rfp/rfp-cli
    OBJCOPY=/opt/gnurx/bin/rx-elf-objcopy
else
    RFP=/opt/rfp/linux-x64/rfp-cli
    OBJCOPY=/opt/gnurx/bin/rx-elf-objcopy
fi

FILE="${1:-}"
METHOD="${2:-e2lite}"
PORT_OVERRIDE="${3:-}"

if [ -z "$FILE" ]; then
    echo "Usage: $0 <elf-or-mot-file> [e2lite|sci]" >&2
    exit 1
fi
if [ ! -f "$FILE" ]; then
    echo "Error: file not found: $FILE" >&2
    exit 1
fi
if [ ! -x "$RFP" ]; then
    echo "Error: rfp-cli not found at $RFP" >&2
    exit 1
fi

# If an ELF was passed in, convert to a Motorola S-record next to it.
EXT="${FILE##*.}"
if [ "$EXT" = "elf" ]; then
    if [ ! -x "$OBJCOPY" ]; then
        echo "Error: rx-elf-objcopy not found at $OBJCOPY" >&2
        exit 1
    fi
    MOT="${FILE%.elf}.mot"
    echo ">> Converting $FILE -> $MOT"
    "$OBJCOPY" -O srec "$FILE" "$MOT"
    FILE="$MOT"
fi

case "$METHOD" in
  e2lite|e2l)
    # Make the E2 Lite USB device writable to the current user.
    # (Udev rule also sets MODE=0666 on plug-in, but covers late plug-in here.)
    E2L_DEV=$(lsusb | awk '/045b:82a0/ {
        bus=$2; dev=$4; sub(":","",dev); print "/dev/bus/usb/"bus"/"dev
    }')
    if [ -z "$E2L_DEV" ]; then
        echo "Error: E2 Lite (USB 045b:82a0) not found. Plug it in." >&2
        exit 1
    fi
    if [ ! -w "$E2L_DEV" ]; then
        sudo chmod 666 "$E2L_DEV"
    fi

    echo ">> Flashing $FILE via E2 Lite (FINE @ 250 kbps)"
    exec "$RFP" \
        -device RX72x \
        -tool e2l \
        -if fine \
        -s 250000 \
        -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \
        -a \
        -file "$FILE" \
        -run
    ;;

  sci|uart)
    if [ -n "$PORT_OVERRIDE" ]; then
        PORT="$PORT_OVERRIDE"
    elif [[ "$(uname)" == "Darwin" ]]; then
        PORT=$(ls /dev/tty.usbmodem* /dev/tty.usbserial-* 2>/dev/null | head -1)
    else
        PORT=/dev/ttyACM0
    fi
    if [ -z "$PORT" ] || [ ! -c "$PORT" ]; then
        echo "Error: serial port not found." >&2
        echo "Pass the port as the 3rd argument: $0 <file> sci <port>" >&2
        exit 1
    fi
    echo ">> Flashing $FILE via SCI Boot Mode on $PORT"
    echo "   (requires SW4 Pin1=ON, Pin2=OFF, and a power-cycle AFTER setting it)"
    exec "$RFP" \
        -device RX72x \
        -port "$PORT" \
        -if uart \
        -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \
        -a \
        -file "$FILE" \
        -run
    ;;

  *)
    echo "Unknown method: $METHOD (use 'e2lite' or 'sci')" >&2
    exit 1
    ;;
esac
