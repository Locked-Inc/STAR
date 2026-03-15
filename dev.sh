#!/usr/bin/env bash
# STAR dev -- mapping session with virtual RX72N.
# Wraps start.sh with STAR_SIMULATION_MODE=true and --rviz.
# Usage: ./dev.sh [--no-ui] [--no-lidar] [--help]
set -euo pipefail
STAR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec env STAR_SIMULATION_MODE=true "$STAR_DIR/start.sh" --rviz "$@"
