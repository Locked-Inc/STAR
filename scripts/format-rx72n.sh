#!/usr/bin/env bash
# Format or check STAR RX72N firmware C/H files with clang-format.
#
# Usage:
#   ./scripts/format-rx72n.sh              -- auto-format (modifies files)
#   ./scripts/format-rx72n.sh --check      -- check only, exit 1 if any file needs formatting
#   ./scripts/format-rx72n.sh --verbose    -- print each file being processed
#
# Excluded directories (vendor / generated code):
#   libs/threadx/             - Azure RTOS ThreadX (third-party)
#   libs/rx_nanopb/nanopb/    - nanopb library (third-party)
#   libs/rx_nanopb/inc/gen/   - auto-generated proto headers/sources
#   src/boot/smc_generated/   - Renesas SMC-generated BSP sources
#   src/boot/r_bsp/           - Renesas BSP library
#   src/boot/inc/smc_generated/ - Renesas SMC-generated BSP headers

set -uo pipefail

FIRMWARE_DIR="e2-studio-star-rx72n-firmware"
CHECK_MODE=false
VERBOSE=false

for arg in "$@"; do
  case "$arg" in
    --check)   CHECK_MODE=true ;;
    --verbose) VERBOSE=true ;;
    *)
      echo "Unknown argument: $arg"
      echo "Usage: $0 [--check] [--verbose]"
      exit 1
      ;;
  esac
done

# Verify clang-format is available
if ! command -v clang-format &>/dev/null; then
  echo "[FAIL] clang-format not found. Install with: sudo apt install clang-format"
  exit 1
fi

# Verify firmware directory exists
if [ ! -d "$FIRMWARE_DIR" ]; then
  echo "[FAIL] Firmware directory not found: $FIRMWARE_DIR"
  echo "       Run this script from the repository root."
  exit 1
fi

# Collect C/H files, pruning vendor and generated directories
mapfile -d '' files < <(find "$FIRMWARE_DIR" \
  \( \
    -path "$FIRMWARE_DIR/libs/threadx"               -o \
    -path "$FIRMWARE_DIR/libs/rx_nanopb/nanopb"      -o \
    -path "$FIRMWARE_DIR/libs/rx_nanopb/inc/gen"     -o \
    -path "$FIRMWARE_DIR/src/boot/smc_generated"     -o \
    -path "$FIRMWARE_DIR/src/boot/r_bsp"             -o \
    -path "$FIRMWARE_DIR/src/boot/inc/smc_generated" \
  \) -prune \
  -o \( -name "*.c" -o -name "*.h" \) -print0)

if [ ${#files[@]} -eq 0 ]; then
  echo "No C/H files found under $FIRMWARE_DIR (after exclusions)."
  exit 0
fi

error_found=0

if [ "$CHECK_MODE" = true ]; then
  echo "Checking RX72N firmware formatting (${#files[@]} files)..."
  for f in "${files[@]}"; do
    result=$(clang-format --dry-run --Werror "$f" 2>&1)
    if [ $? -ne 0 ]; then
      echo "[FAIL] $f"
      if [ "$VERBOSE" = true ]; then
        echo "$result"
      fi
      error_found=1
    elif [ "$VERBOSE" = true ]; then
      echo "  [OK] $f"
    fi
  done

  if [ $error_found -ne 0 ]; then
    echo ""
    echo "[FAIL] Formatting errors found. Run ./scripts/format-rx72n.sh to auto-fix."
    exit 1
  fi
  echo "[PASS] All RX72N firmware files are correctly formatted."
else
  echo "Formatting RX72N firmware (${#files[@]} files)..."
  for f in "${files[@]}"; do
    [ "$VERBOSE" = true ] && echo "  formatting: $f"
    clang-format -i "$f"
  done
  echo "[PASS] Formatting complete."
fi
