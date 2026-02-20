#!/usr/bin/env bash
# shellcheck shell=bash
# check_nanopb_sync.sh — Verify LidarScan nanopb max_count bounds are identical
# between ui.options and gateway_service.options.
#
# Fails with a non-zero exit code and a clear SYNC ERROR message if any of the
# three parallel LidarScan arrays diverge between the two option files.
#
# Usage:
#   ./scripts/check_nanopb_sync.sh
#   (run from the star-proto/ directory)
#
# CI integration:
#   Add as a step after buf generate, or invoke via:
#   make proto-check-nanopb-sync

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROTO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

UI_OPTIONS="${PROTO_DIR}/nanopb/star/v1/ui.options"
GW_OPTIONS="${PROTO_DIR}/nanopb/star/v1/gateway_service.options"

if [[ ! -f "${UI_OPTIONS}" ]]; then
    echo "ERROR: ui.options not found at ${UI_OPTIONS}" >&2
    exit 1
fi
if [[ ! -f "${GW_OPTIONS}" ]]; then
    echo "ERROR: gateway_service.options not found at ${GW_OPTIONS}" >&2
    exit 1
fi

# Extract max_count for a given field from an options file.
# Returns the numeric value or an empty string if not found.
# Uses awk to match the first whitespace-separated token exactly against the
# fully-qualified field name, preventing false positives from fields whose names
# share a common prefix (e.g. "angle_rad" vs "angle_rad_offset").  Dot characters
# in the field name do not require escaping because awk performs literal string
# comparison with the == operator.
get_max_count() {
    local file="$1"
    local field="$2"
    awk -v field="${field}" '$1 == field' "${file}" \
        | head -1 \
        | grep -oE 'max_count:[0-9]+' \
        | grep -oE '[0-9]+' \
        || true
}

FIELDS=(
    "star.v1.LidarScan.angle_rad"
    "star.v1.LidarScan.range_m"
    "star.v1.LidarScan.intensity"
)

fail=0
for field in "${FIELDS[@]}"; do
    ui_val=$(get_max_count "${UI_OPTIONS}" "${field}")
    gw_val=$(get_max_count "${GW_OPTIONS}" "${field}")

    if [[ -z "${ui_val}" ]]; then
        echo "SYNC ERROR: ${field} — entry missing from ui.options" >&2
        fail=1
        continue
    fi
    if [[ -z "${gw_val}" ]]; then
        echo "SYNC ERROR: ${field} — entry missing from gateway_service.options" >&2
        fail=1
        continue
    fi
    if [[ "${ui_val}" != "${gw_val}" ]]; then
        # Extract just the short field name (after the last dot) for a readable message.
        short="${field##*.}"
        echo "SYNC ERROR: LidarScan.${short} mismatch (ui=${ui_val}, gw=${gw_val})" >&2
        fail=1
    fi
done

if [[ "${fail}" -eq 0 ]]; then
    echo "OK: LidarScan nanopb bounds are in sync across ui.options and gateway_service.options"
    exit 0
else
    echo "" >&2
    echo "Fix: update BOTH nanopb/star/v1/ui.options and nanopb/star/v1/gateway_service.options" \
         "in the same commit." >&2
    exit 1
fi
