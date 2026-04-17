#!/usr/bin/env bash
# STAR stop -- kill all components. Safe to run when nothing is running.
#
# Shutdown order:
#   1. ROS2 clients (bridge) before the gRPC server
#   2. ros2 launch groups (SIGTERM to launch proc, waits for process group)
#   3. Individual ROS2 nodes that may outlive their launch parent
#   4. Go binaries (gateway, virtual_rx72n)
#   5. Support processes (foxglove, UI, RViz)

set -uo pipefail

STAR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
say()  { echo -e "${GREEN}[stop]${NC} $*"; }
warn() { echo -e "${YELLOW}[stop]${NC} $*"; }

OPT_STOP_AP=false
AP_CON="STAR-Hotspot"
for arg in "$@"; do
    case "$arg" in
        --stop-ap) OPT_STOP_AP=true ;;
    esac
done

# ---------------------------------------------------------------------------
# stop_proc LABEL PATTERN [GRACE_SECONDS]
#
# Send SIGTERM to all processes matching PATTERN. Wait up to GRACE_SECONDS
# (default 3) for them to exit, then SIGKILL any survivors. Kills both the
# matched process AND its entire process group so that launch-file children
# don't linger after their parent exits.
#
# Design note: this function already extracts pgid via `ps -o pgid=` and
# kills the whole process group, which is functionally equivalent to the
# setsid-wrap + PID-file approach (writing .pgid files in start.sh and
# calling `kill -- -$pgid`). We deliberately keep pattern-based lookup
# because start.sh already uses `&` backgrounding and doesn't install
# pidfiles, so pgrep+pgid is the minimum-footprint path to the same
# behaviour.
# ---------------------------------------------------------------------------
stop_proc() {
    local label="$1"
    local pattern="$2"
    local grace="${3:-3}"

    local pids
    # pgrep -f returns exit 1 when no match -- suppress that
    pids=$(pgrep -f "$pattern" 2>/dev/null) || true
    [[ -z "$pids" ]] && return 0

    say "Stopping $label..."

    # SIGTERM to process AND its process group
    for pid in $pids; do
        kill -TERM "$pid" 2>/dev/null || true
        # Kill the entire process group (negative PID = process group)
        local pgid
        pgid=$(ps -o pgid= -p "$pid" 2>/dev/null | tr -d ' ') || true
        if [[ -n "$pgid" && "$pgid" != "0" ]]; then
            kill -TERM -"$pgid" 2>/dev/null || true
        fi
    done

    # Wait for graceful shutdown
    local waited=0
    while [[ $waited -lt $grace ]]; do
        local survivors
        survivors=$(pgrep -f "$pattern" 2>/dev/null) || true
        [[ -z "$survivors" ]] && return 0
        sleep 1
        (( waited++ ))
    done

    # SIGKILL any survivors
    local survivors
    survivors=$(pgrep -f "$pattern" 2>/dev/null) || true
    if [[ -n "$survivors" ]]; then
        warn "$label did not exit in ${grace}s -- force-killing"
        for pid in $survivors; do
            kill -KILL "$pid" 2>/dev/null || true
            local pgid
            pgid=$(ps -o pgid= -p "$pid" 2>/dev/null | tr -d ' ') || true
            if [[ -n "$pgid" && "$pgid" != "0" ]]; then
                kill -KILL -"$pgid" 2>/dev/null || true
            fi
        done
    fi
}

# ---------------------------------------------------------------------------
# BBB firmware (remote, best-effort)
# ---------------------------------------------------------------------------
BBB_HOST="${BBB_HOST:-192.168.7.2}"
BBB_USER="${BBB_USER:-debian}"
BBB_PASS="${BBB_PASS:-StarBBB2026!}"
if ping -c 1 -W 1 "$BBB_HOST" >/dev/null 2>&1; then
    sshpass -p "$BBB_PASS" ssh -o StrictHostKeyChecking=no -o ConnectTimeout=3 \
        "$BBB_USER@$BBB_HOST" \
        "echo '$BBB_PASS' | sudo -S killall -9 star-beaglebone-blue 2>/dev/null" \
        2>/dev/null && say "BBB firmware stopped" || true
fi

# ---------------------------------------------------------------------------
# ROS2 bridge first (gRPC client -- shut before the server)
# ---------------------------------------------------------------------------
stop_proc "gateway_bridge" "star_gateway_bridge_main" 3

# ---------------------------------------------------------------------------
# ros2 launch groups -- SIGTERM to the launch process propagates to all
# children via the process group.
# ---------------------------------------------------------------------------
stop_proc "slam launch" "slam.launch.py" 5
stop_proc "compliance launch" "compliance.launch.py" 5
stop_proc "spi_bridge launch" "star_spi_bridge.launch.py" 3
stop_proc "stereo camera launch" "stereo_camera.launch.py" 5

# ---------------------------------------------------------------------------
# Individual ROS2 nodes -- catch any that outlived their launch parent.
# ---------------------------------------------------------------------------
stop_proc "sllidar" "sllidar_node" 3
stop_proc "slam_toolbox" "async_slam_toolbox_node" 5
stop_proc "ekf_node" "ekf_node" 3
stop_proc "spi_bridge" "star_spi_bridge_node" 3
stop_proc "robot_state_publisher" "robot_state_publisher" 3
stop_proc "static_transform_publisher" "static_transform_publisher" 3
stop_proc "gscam (cameras)" "gscam_node" 5
stop_proc "foxglove_bridge" "foxglove_bridge" 3

# ---------------------------------------------------------------------------
# Go binaries
# ---------------------------------------------------------------------------
stop_proc "star-gateway" "[s]tar-gateway" 3
# Belt-and-suspenders: also match by full path in case the binary name matched
# something else
stop_proc "star-gateway (bin)" "$STAR_DIR/star-gateway/bin/star-gateway" 3
stop_proc "virtual_rx72n" "[v]irtual_rx72n" 3

# Wait for the serial port to be fully released before returning.
# Prevents "Serial port busy" when start.sh is run immediately after stop.sh.
sleep 2

# ---------------------------------------------------------------------------
# UI and visualization
# ---------------------------------------------------------------------------
stop_proc "UI (vite)" "vite" 3
stop_proc "rviz2" "rviz2" 3
# Use the literal install path as the match, not "serve.*star-lichtblick-web".
# The regex form false-matches any bash -c wrapper whose argv happens to
# contain that substring (e.g. in stop.sh testing harnesses); the literal
# path /opt/star-lichtblick-web does not appear outside the actual serve
# process.
stop_proc "lichtblick (serve)" "/opt/star-lichtblick-web" 3

# ---------------------------------------------------------------------------
# Final sweep -- catch anything that slipped through (e.g. orphaned children
# of launch files that didn't propagate SIGTERM to their process group).
# ---------------------------------------------------------------------------
SWEEP_PATTERNS=(
    "gscam_node"
    "sllidar_node"
    "async_slam_toolbox_node"
    "foxglove_bridge"
    "star_gateway_bridge"
    "robot_state_publisher"
    "static_transform_publisher"
    "star-gateway"
    "virtual_rx72n"
    "/opt/star-lichtblick-web"
    "rtabmap"
    "star_ramp_slope_node"
    "star_door_clear_width_node"
    "star_door_threshold_node"
    "star_protruding_objects_node"
    "star_path_blockage_node"
    "star_dynamic_obstacle_node"
    "star_compliance_monitor_node"
)
for pat in "${SWEEP_PATTERNS[@]}"; do
    survivors=$(pgrep -f "$pat" 2>/dev/null) || true
    if [[ -n "$survivors" ]]; then
        warn "Final sweep: force-killing leftover '$pat' (PIDs: $survivors)"
        kill -KILL $survivors 2>/dev/null || true
    fi
done

sleep 1
say "Done."

# ---------------------------------------------------------------------------
# WiFi AP -- default: leave running so the user stays connected.
# Pass --stop-ap to tear it down (will disconnect any WiFi clients).
# ---------------------------------------------------------------------------
if [[ "$OPT_STOP_AP" == "true" ]]; then
    if sudo nmcli connection show --active 2>/dev/null | grep -q "$AP_CON"; then
        sudo nmcli connection down "$AP_CON" 2>/dev/null \
            && say "WiFi AP '$AP_CON' stopped" || true
    fi
else
    if sudo nmcli connection show --active 2>/dev/null | grep -q "$AP_CON"; then
        warn "WiFi AP '$AP_CON' still running -- pass --stop-ap to tear it down"
    fi
fi
