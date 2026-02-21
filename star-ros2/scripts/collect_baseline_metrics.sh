#!/bin/bash
# Baseline metrics collection for frame drop analysis
#
# Usage:
#   ./collect_baseline_metrics.sh [DURATION_MINUTES] [SCENARIO_NAME]
#
# Example:
#   ./collect_baseline_metrics.sh 30 idle
#   ./collect_baseline_metrics.sh 15 active_control
#   ./collect_baseline_metrics.sh 10 stress_test

set -euo pipefail

# Configuration
DURATION_MINUTES=${1:-60}  # Default: 1 hour
SCENARIO_NAME=${2:-"baseline"}
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
OUTPUT_DIR="$SCRIPT_DIR/../baselines/${SCENARIO_NAME}_$(date +%Y%m%d_%H%M%S)"

# Process tracking
VIRTUAL_RX72N_PID=""
GATEWAY_PID=""
ROS2_BRIDGE_PID=""
DIAG_RECORDER_PID=""

# Cleanup function
cleanup() {
    echo ""
    echo "=== Stopping all processes ==="

    if [ -n "$DIAG_RECORDER_PID" ] && kill -0 "$DIAG_RECORDER_PID" 2>/dev/null; then
        echo "Stopping diagnostics recorder (PID: $DIAG_RECORDER_PID)..."
        kill "$DIAG_RECORDER_PID" 2>/dev/null || true
    fi

    if [ -n "$ROS2_BRIDGE_PID" ] && kill -0 "$ROS2_BRIDGE_PID" 2>/dev/null; then
        echo "Stopping ROS2 bridge (PID: $ROS2_BRIDGE_PID)..."
        kill "$ROS2_BRIDGE_PID" 2>/dev/null || true
    fi

    if [ -n "$GATEWAY_PID" ] && kill -0 "$GATEWAY_PID" 2>/dev/null; then
        echo "Stopping Gateway (PID: $GATEWAY_PID)..."
        kill "$GATEWAY_PID" 2>/dev/null || true
    fi

    if [ -n "$VIRTUAL_RX72N_PID" ] && kill -0 "$VIRTUAL_RX72N_PID" 2>/dev/null; then
        echo "Stopping Virtual RX72N (PID: $VIRTUAL_RX72N_PID)..."
        kill "$VIRTUAL_RX72N_PID" 2>/dev/null || true
    fi

    # Wait for processes to terminate
    sleep 2

    # Force kill if still running
    for pid in "$DIAG_RECORDER_PID" "$ROS2_BRIDGE_PID" "$GATEWAY_PID" "$VIRTUAL_RX72N_PID"; do
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            echo "Force killing PID $pid..."
            kill -9 "$pid" 2>/dev/null || true
        fi
    done

    echo "All processes stopped."
}

# Set trap for cleanup on exit
trap cleanup EXIT INT TERM

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "=========================================="
echo "STAR Frame Drop Baseline Collection"
echo "=========================================="
echo "Scenario:       $SCENARIO_NAME"
echo "Duration:       $DURATION_MINUTES minutes"
echo "Output:         $OUTPUT_DIR"
echo "=========================================="
echo ""

# Step 1: Start Virtual RX72N
echo "[1/5] Starting Virtual RX72N..."
if [ ! -f "$PROJECT_ROOT/star-gateway/virtual_rx72n" ]; then
    echo "ERROR: virtual_rx72n binary not found!"
    echo "Build it with: cd star-gateway && go build ./cmd/virtual_rx72n"
    exit 1
fi

cd "$PROJECT_ROOT/star-gateway"
./virtual_rx72n > "$OUTPUT_DIR/virtual_rx72n.log" 2>&1 &
VIRTUAL_RX72N_PID=$!
echo "  Started (PID: $VIRTUAL_RX72N_PID)"
sleep 2

# Verify Virtual RX72N is running
if ! kill -0 "$VIRTUAL_RX72N_PID" 2>/dev/null; then
    echo "ERROR: Virtual RX72N failed to start!"
    cat "$OUTPUT_DIR/virtual_rx72n.log"
    exit 1
fi

# Step 2: Start Gateway
echo "[2/5] Starting Gateway..."
if [ ! -f "$PROJECT_ROOT/star-gateway/star-gateway" ]; then
    echo "ERROR: star-gateway binary not found!"
    echo "Build it with: cd star-gateway && go build ./cmd/star-gateway"
    exit 1
fi

STAR_SIMULATION_MODE=true ./star-gateway > "$OUTPUT_DIR/gateway.log" 2>&1 &
GATEWAY_PID=$!
echo "  Started (PID: $GATEWAY_PID)"
sleep 3

# Verify Gateway is running
if ! kill -0 "$GATEWAY_PID" 2>/dev/null; then
    echo "ERROR: Gateway failed to start!"
    cat "$OUTPUT_DIR/gateway.log"
    exit 1
fi

# Step 3: Start ROS2 bridge
echo "[3/5] Starting ROS2 bridge..."
cd "$PROJECT_ROOT/star-ros2"

# Check if ROS2 workspace is built
if [ ! -d "install/star_gateway_bridge" ]; then
    echo "ERROR: ROS2 workspace not built!"
    echo "Build it with: cd star-ros2 && colcon build --packages-select star_gateway_bridge"
    exit 1
fi

# Source ROS2 setup (temporarily disable strict mode for setup.bash)
set +u
source install/setup.bash
set -u

# Start the bridge
ros2 run star_gateway_bridge star_gateway_bridge_main > "$OUTPUT_DIR/ros2_bridge.log" 2>&1 &
ROS2_BRIDGE_PID=$!
echo "  Started (PID: $ROS2_BRIDGE_PID)"
sleep 5

# Verify ROS2 bridge is running
if ! kill -0 "$ROS2_BRIDGE_PID" 2>/dev/null; then
    echo "ERROR: ROS2 bridge failed to start!"
    cat "$OUTPUT_DIR/ros2_bridge.log"
    exit 1
fi

# Step 4: Start diagnostics recording
echo "[4/5] Starting diagnostics recording..."
ros2 topic echo /diagnostics > "$OUTPUT_DIR/diagnostics.log" 2>&1 &
DIAG_RECORDER_PID=$!
echo "  Started (PID: $DIAG_RECORDER_PID)"
sleep 2

# Step 5: Run for specified duration
echo "[5/5] Collecting metrics..."
echo ""
echo "All processes started successfully:"
echo "  Virtual RX72N:    PID $VIRTUAL_RX72N_PID"
echo "  Gateway:          PID $GATEWAY_PID"
echo "  ROS2 Bridge:      PID $ROS2_BRIDGE_PID"
echo "  Diagnostics:      PID $DIAG_RECORDER_PID"
echo ""
echo "Collecting for $DURATION_MINUTES minutes..."
echo "Press Ctrl+C to stop early."
echo ""

# Progress indicator
TOTAL_SECONDS=$((DURATION_MINUTES * 60))
START_TIME=$(date +%s)

while true; do
    ELAPSED=$(($(date +%s) - START_TIME))

    if [ $ELAPSED -ge $TOTAL_SECONDS ]; then
        break
    fi

    REMAINING=$((TOTAL_SECONDS - ELAPSED))
    MINUTES=$((REMAINING / 60))
    SECONDS=$((REMAINING % 60))

    printf "\r  Time remaining: %02d:%02d" $MINUTES $SECONDS
    sleep 1
done

echo ""
echo ""
echo "Collection complete!"

# Analyze results
echo ""
echo "=========================================="
echo "Baseline Analysis"
echo "=========================================="

# Extract final statistics from diagnostics log
TELEOP_STATS=$(grep -A 20 "teleop_command_drops" "$OUTPUT_DIR/diagnostics.log" | tail -20)
TELEMETRY_STATS=$(grep -A 20 "telemetry_frame_drops" "$OUTPUT_DIR/diagnostics.log" | tail -20)

# Extract key metrics (last occurrence)
TELEOP_TOTAL=$(echo "$TELEOP_STATS" | grep "total_frames" | tail -1 | awk -F"'" '{print $2}')
TELEOP_DROPPED=$(echo "$TELEOP_STATS" | grep "dropped_frames" | tail -1 | awk -F"'" '{print $2}')
TELEOP_DROP_RATE=$(echo "$TELEOP_STATS" | grep "drop_rate_percent" | tail -1 | awk -F"'" '{print $2}')

TELEMETRY_TOTAL=$(echo "$TELEMETRY_STATS" | grep "total_frames" | tail -1 | awk -F"'" '{print $2}')
TELEMETRY_DROPPED=$(echo "$TELEMETRY_STATS" | grep "dropped_frames" | tail -1 | awk -F"'" '{print $2}')
TELEMETRY_DROP_RATE=$(echo "$TELEMETRY_STATS" | grep "drop_rate_percent" | tail -1 | awk -F"'" '{print $2}')

echo ""
echo "Teleop Command Frames:"
echo "  Total:         ${TELEOP_TOTAL:-N/A}"
echo "  Dropped:       ${TELEOP_DROPPED:-N/A}"
echo "  Drop Rate:     ${TELEOP_DROP_RATE:-N/A}%"
echo ""
echo "Telemetry Frames:"
echo "  Total:         ${TELEMETRY_TOTAL:-N/A}"
echo "  Dropped:       ${TELEMETRY_DROPPED:-N/A}"
echo "  Drop Rate:     ${TELEMETRY_DROP_RATE:-N/A}%"
echo ""
echo "=========================================="
echo ""
echo "Results saved to: $OUTPUT_DIR"
echo ""

# Create summary file
cat > "$OUTPUT_DIR/SUMMARY.txt" <<EOF
STAR Frame Drop Baseline Collection - Summary
==============================================

Scenario:       $SCENARIO_NAME
Duration:       $DURATION_MINUTES minutes
Date:           $(date)

Teleop Command Frames
---------------------------------------------
Total Frames:           ${TELEOP_TOTAL:-N/A}
Dropped Frames:         ${TELEOP_DROPPED:-N/A}
Drop Rate:              ${TELEOP_DROP_RATE:-N/A}%

Telemetry Frames
---------------------------------------------
Total Frames:           ${TELEMETRY_TOTAL:-N/A}
Dropped Frames:         ${TELEMETRY_DROPPED:-N/A}
Drop Rate:              ${TELEMETRY_DROP_RATE:-N/A}%

Files
---------------------------------------------
Virtual RX72N Log:      virtual_rx72n.log
Gateway Log:            gateway.log
ROS2 Bridge Log:        ros2_bridge.log
Diagnostics Log:        diagnostics.log
Summary:                SUMMARY.txt

Acceptance Criteria
---------------------------------------------
✓ Teleop drop rate < 1.0% under normal load
✓ Telemetry drop rate < 5.0% under normal load
✓ Telemetry drop rate < 10.0% under stress test

Analysis
---------------------------------------------
$(if [ -n "$TELEOP_DROP_RATE" ]; then
    if (( $(echo "$TELEOP_DROP_RATE < 1.0" | bc -l) )); then
        echo "✅ Teleop performance: EXCELLENT (< 1% drop rate)"
    elif (( $(echo "$TELEOP_DROP_RATE < 5.0" | bc -l) )); then
        echo "⚠️  Teleop performance: ACCEPTABLE (< 5% drop rate)"
    else
        echo "❌ Teleop performance: POOR (>= 5% drop rate)"
    fi
else
    echo "❓ Teleop performance: NO DATA"
fi)

$(if [ -n "$TELEMETRY_DROP_RATE" ]; then
    if (( $(echo "$TELEMETRY_DROP_RATE < 5.0" | bc -l) )); then
        echo "✅ Telemetry performance: EXCELLENT (< 5% drop rate)"
    elif (( $(echo "$TELEMETRY_DROP_RATE < 10.0" | bc -l) )); then
        echo "⚠️  Telemetry performance: ACCEPTABLE (< 10% drop rate)"
    else
        echo "❌ Telemetry performance: POOR (>= 10% drop rate)"
    fi
else
    echo "❓ Telemetry performance: NO DATA"
fi)
EOF

echo "Summary written to: $OUTPUT_DIR/SUMMARY.txt"
echo ""
