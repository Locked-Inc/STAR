#!/usr/bin/env bash
# Flash a .mot file to the RX72N via rfp-cli.
#
# rfp-cli on this Pi5 is an x86_64 binary that segfaults under qemu-user but
# runs cleanly under box64. See ~/.claude memory project_rfp_cli_box64.md.
#
# Usage:
#   ./scripts/flash.sh                           # flashes build/fit-module-untested.mot
#   ./scripts/flash.sh build/foo.mot             # flashes named file

set -euo pipefail

HERE="$(cd "$(dirname "$0")/.." && pwd)"
MOT="${1:-$HERE/build/fit-module-untested.mot}"

if [ ! -f "$MOT" ]; then
    echo "ERROR: file not found: $MOT" >&2
    echo "Hint: run 'cmake --build build' first." >&2
    exit 1
fi

if ! command -v box64 >/dev/null 2>&1; then
    echo "ERROR: box64 not on PATH (needed to launch x86_64 rfp-cli on aarch64)" >&2
    exit 1
fi

# Adjust RFP_CLI_BIN to wherever rfp-cli is installed on this host.
RFP_CLI_BIN="${RFP_CLI_BIN:-/opt/renesas/rfp-cli/rfp-cli}"
if [ ! -x "$RFP_CLI_BIN" ]; then
    echo "ERROR: rfp-cli not found at $RFP_CLI_BIN (override with RFP_CLI_BIN=...)" >&2
    exit 1
fi

echo "==> flashing $MOT"
exec box64 "$RFP_CLI_BIN" \
    -device RX72N \
    -tool e2lite \
    -if fine \
    -auto \
    -p "$MOT"
