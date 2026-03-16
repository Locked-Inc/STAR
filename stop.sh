#!/usr/bin/env bash
# STAR stop -- kill all components. Safe to run when nothing is running.

STAR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
say()  { echo -e "${GREEN}[stop]${NC} $*"; }
warn() { echo -e "${YELLOW}[stop]${NC} $*"; }

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

# Go binaries (match full path to avoid false positives)
pkill -f "$STAR_DIR/star-gateway/star-gateway"  2>/dev/null && say "gateway stopped"        || true
pkill -f "$STAR_DIR/star-gateway/virtual_rx72n" 2>/dev/null && say "virtual_rx72n stopped"  || true

# UI
pkill -f "vite"   2>/dev/null && say "UI dev server stopped" || true

# RViz
pkill -f "rviz2"  2>/dev/null && say "rviz2 stopped"         || true

# Restore ModemManager (we stop it only to free /dev/ttyUSB0)
sudo systemctl start ModemManager 2>/dev/null \
    && say "ModemManager restored" \
    || warn "ModemManager restart skipped (not installed?)"

sleep 1
say "Done."
