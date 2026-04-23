#!/usr/bin/env bash
# Fetch Renesas FIT modules into vendor/FITModules/.
#
# Mechanism: clone https://github.com/renesas/rx-driver-package and run its
# top-level Makefile -- that target downloads the official FIT module zips
# from Renesas servers into a FITModules/ directory in the clone. We then
# unzip just the ones we need into our local vendor/FITModules/.
#
# vendor/ is gitignored so we never check in vendor source -- the script is
# the source of truth, and pinning the upstream tag keeps re-fetches
# reproducible.

set -euo pipefail

# -----------------------------------------------------------------------------
# Configuration
# -----------------------------------------------------------------------------
RX_DRIVER_REPO="https://github.com/renesas/rx-driver-package.git"
# Pin a known release tag here once we've smoke-tested one. Empty = HEAD of
# default branch (acceptable for first-fetch sandbox; tighten before promotion).
RX_DRIVER_TAG=""

# Modules we actually need for v0:
#   r_bsp     -- board support, clock, MSTP, vectors, startup
#   r_sci_rx  -- SCI9 UART to /dev/ttyACM0
#   r_riic_rx -- RIIC1 I2C to BNO055
#   r_gpt_rx  -- GPTW PWM to DRV8263H motor drivers
#   r_byteq   -- ring-buffer dependency of buffered r_sci_rx
MODULES=(r_bsp r_sci_rx r_riic_rx r_gpt_rx r_byteq)

# -----------------------------------------------------------------------------
# Paths
# -----------------------------------------------------------------------------
HERE="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$HERE/vendor/FITModules"
WORK="$(mktemp -d -t rx-driver-pkg-XXXXXX)"
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$DEST"

# -----------------------------------------------------------------------------
# Sanity checks
# -----------------------------------------------------------------------------
for tool in git make unzip; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: required tool '$tool' not on PATH" >&2
        exit 1
    fi
done

# -----------------------------------------------------------------------------
# Clone + build
# -----------------------------------------------------------------------------
echo "==> cloning $RX_DRIVER_REPO into $WORK"
git clone --depth 1 ${RX_DRIVER_TAG:+--branch "$RX_DRIVER_TAG"} \
    "$RX_DRIVER_REPO" "$WORK/rx-driver-package"

echo "==> running upstream Makefile to download FIT zips"
make -C "$WORK/rx-driver-package"

ZIP_DIR="$WORK/rx-driver-package/FITModules"
if [ ! -d "$ZIP_DIR" ]; then
    echo "ERROR: $ZIP_DIR not created -- upstream Makefile changed?" >&2
    exit 1
fi

# -----------------------------------------------------------------------------
# Unzip just the modules we want
# -----------------------------------------------------------------------------
for mod in "${MODULES[@]}"; do
    # Each FIT module zip is named like r_sci_rx_vX.YZ.zip; glob-match the
    # prefix so we don't have to know the exact version string.
    zip_path="$(ls "$ZIP_DIR"/${mod}_*.zip 2>/dev/null | head -n 1 || true)"
    if [ -z "$zip_path" ]; then
        echo "WARNING: no zip found for module '$mod' in $ZIP_DIR -- skipping" >&2
        continue
    fi
    echo "==> unpacking $(basename "$zip_path") -> $DEST/$mod/"
    rm -rf "$DEST/$mod"
    mkdir -p "$DEST/$mod"
    unzip -q -o "$zip_path" -d "$DEST/$mod"
done


# -----------------------------------------------------------------------------
# Post-extract: select the GENERIC_RX72N board in r_bsp's platform.h.
#
# platform.h ships with every #include "./board/generic_rxXX/r_bsp.h" line
# COMMENTED OUT -- the user is expected to uncomment exactly one. We do it
# here in code so the sandbox builds without any manual editing of vendor/.
# Idempotent: matches both the commented and already-uncommented form.
# -----------------------------------------------------------------------------
PLATFORM_H="$DEST/r_bsp/r_bsp/platform.h"
if [ -f "$PLATFORM_H" ]; then
    echo "==> selecting GENERIC_RX72N board in $PLATFORM_H"
    sed -i 's|^//#include "./board/generic_rx72n/r_bsp.h"|#include "./board/generic_rx72n/r_bsp.h"|' \
        "$PLATFORM_H"
else
    echo "WARNING: $PLATFORM_H not found; cannot select board" >&2
fi

echo
echo "Done. FIT modules installed at:"
echo "  $DEST"
echo
echo "Next:"
echo "  cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-rx72n.cmake"
echo "  cmake --build build -j"
