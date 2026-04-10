#!/usr/bin/env bash
# STAR stop -- kill all components. Safe to run when nothing is running.

STAR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
say()  { echo -e "${GREEN}[stop]${NC} $*"; }
warn() { echo -e "${YELLOW}[stop]${NC} $*"; }

# BBB firmware (remote, best-effort)
BBB_HOST="${BBB_HOST:-192.168.7.2}"
BBB_USER="${BBB_USER:-debian}"
BBB_PASS="${BBB_PASS:-StarBBB2026!}"
if ping -c 1 -W 1 "$BBB_HOST" >/dev/null 2>&1; then
    sshpass -p "$BBB_PASS" ssh -o StrictHostKeyChecking=no -o ConnectTimeout=3 \
        "$BBB_USER@$BBB_HOST" \
        "echo '$BBB_PASS' | sudo -S killall -9 star-beaglebone-blue 2>/dev/null" \
        2>/dev/null && say "BBB firmware stopped" || true
fi

# ROS2 bridge first (gRPC client; shut before server)
pkill -f "star_gateway_bridge_main"           2>/dev/null && say "gateway_bridge stopped"    || true

# ros2 launch groups -- killing the launch process cascades to children
pkill -f "star_spi_bridge.launch.py"          2>/dev/null && say "spi_bridge launch stopped" || true
pkill -f "slam.launch.py"                     2>/dev/null && say "slam launch stopped"        || true

# ROS2 node processes (in case launch already exited)
pkill -f "sllidar_node"                       2>/dev/null || true
pkill -f "async_slam_toolbox_node"            2>/dev/null || true
pkill -f "ekf_node"                           2>/dev/null || true
pkill -f "star_spi_bridge_node"               2>/dev/null || true
pkill -f "robot_state_publisher"              2>/dev/null || true
pkill -f "static_transform_publisher"         2>/dev/null && say "fake odom TF stopped" || true

# Go binaries -- match binary name (not full path, which varies by how it was started)
pkill -f "[s]tar-gateway$"                        2>/dev/null && say "gateway stopped"        || true
pkill -f "$STAR_DIR/star-gateway/star-gateway"    2>/dev/null || true  # fallback: full path
pkill -f "[v]irtual_rx72n"                        2>/dev/null && say "virtual_rx72n stopped"  || true
# Wait for serial port to be fully released (prevents "Serial port busy" on restart)
sleep 2

# Foxglove bridge
pkill -f "foxglove_bridge" 2>/dev/null && say "foxglove_bridge stopped" || true

# UI
pkill -f "vite"   2>/dev/null && say "UI dev server stopped" || true

# RViz
pkill -f "rviz2"  2>/dev/null && say "rviz2 stopped"         || true

# Do NOT restore ModemManager -- it grabs /dev/ttyUSB0 and disconnects the
# RPLiDAR C1. ModemManager is stopped by start.sh and stays stopped.

sleep 1
say "Done."
