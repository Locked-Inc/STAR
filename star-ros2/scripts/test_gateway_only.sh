#!/bin/bash
# Quick test of Gateway and Virtual RX72N (no ROS2 required)
#
# This validates:
# - Virtual RX72N telemetry generation
# - Gateway HARQ processing
# - Frame sequence tracking
#
# Note: Does not test ROS2 diagnostics (requires devcontainer)

set -euo pipefail

DURATION_SECONDS=${1:-60}  # Default: 1 minute
OUTPUT_DIR="$(pwd)/../baselines/gateway_only_$(date +%Y%m%d_%H%M%S)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Process tracking
VIRTUAL_RX72N_PID=""
GATEWAY_PID=""

# Cleanup function
cleanup() {
    echo ""
    echo "=== Stopping processes ==="

    if [ -n "$GATEWAY_PID" ] && kill -0 "$GATEWAY_PID" 2>/dev/null; then
        echo "Stopping Gateway (PID: $GATEWAY_PID)..."
        kill "$GATEWAY_PID" 2>/dev/null || true
    fi

    if [ -n "$VIRTUAL_RX72N_PID" ] && kill -0 "$VIRTUAL_RX72N_PID" 2>/dev/null; then
        echo "Stopping Virtual RX72N (PID: $VIRTUAL_RX72N_PID)..."
        kill "$VIRTUAL_RX72N_PID" 2>/dev/null || true
    fi

    sleep 2

    # Force kill if still running
    for pid in "$GATEWAY_PID" "$VIRTUAL_RX72N_PID"; do
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            echo "Force killing PID $pid..."
            kill -9 "$pid" 2>/dev/null || true
        fi
    done

    echo "All processes stopped."
}

trap cleanup EXIT INT TERM

mkdir -p "$OUTPUT_DIR"

echo "=========================================="
echo "Gateway + Virtual RX72N Test"
echo "=========================================="
echo "Duration:       $DURATION_SECONDS seconds"
echo "Output:         $OUTPUT_DIR"
echo "=========================================="
echo ""

# Start Virtual RX72N
echo "[1/2] Starting Virtual RX72N..."
cd "$PROJECT_ROOT/star-gateway"
./virtual_rx72n > "$OUTPUT_DIR/virtual_rx72n.log" 2>&1 &
VIRTUAL_RX72N_PID=$!
echo "  Started (PID: $VIRTUAL_RX72N_PID)"
sleep 2

# Start Gateway
echo "[2/2] Starting Gateway..."
STAR_SIMULATION_MODE=true ./star-gateway > "$OUTPUT_DIR/gateway.log" 2>&1 &
GATEWAY_PID=$!
echo "  Started (PID: $GATEWAY_PID)"
sleep 3

echo ""
echo "Running for $DURATION_SECONDS seconds..."
echo ""

START_TIME=$(date +%s)
while true; do
    ELAPSED=$(($(date +%s) - START_TIME))

    if [ $ELAPSED -ge $DURATION_SECONDS ]; then
        break
    fi

    REMAINING=$((DURATION_SECONDS - ELAPSED))
    printf "\r  Time remaining: %02d seconds" $REMAINING
    sleep 1
done

echo ""
echo ""
echo "Test complete!"
echo ""
echo "Check logs in: $OUTPUT_DIR"
echo "  - virtual_rx72n.log"
echo "  - gateway.log"
echo ""
