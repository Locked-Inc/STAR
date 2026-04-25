#!/usr/bin/env bash
# STAR bootstrap -- one-shot setup of a fresh Raspberry Pi 5 for the
# compliance engine.
#
# This script is idempotent. Safe to re-run after a pull. It:
#   1. Installs ROS 2 Jazzy Python helpers that the compliance engine
#      imports at runtime (cv_bridge, sensor_msgs_py, etc).
#   2. Installs the pip dependencies for the compliance detectors
#      (open3d, scikit-image, scikit-learn, onnxruntime-compatible
#      loader via opencv-dnn).
#   3. Adds the compliance-engine package to the colcon workspace via
#      a symlink so colcon discovers it alongside star-ros2 packages.
#   4. Runs `colcon build` for the two new packages (star_compliance_msgs,
#      star_compliance).
#   5. Smoke-tests that the message types and node executables resolve.
#
# After this script completes, `./start.sh` launches the full stack
# including compliance. If any step fails the script exits non-zero
# with a clear message identifying which phase bailed.

set -euo pipefail

STAR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAR_ROS2_DIR="$STAR_DIR/star-ros2"
COMPLIANCE_SRC_DIR="$STAR_DIR/star-compliance"
COMPLIANCE_LINK_DIR="$STAR_ROS2_DIR/src/star_compliance"

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
say()  { echo -e "${GREEN}[bootstrap]${NC} $*"; }
warn() { echo -e "${YELLOW}[bootstrap]${NC} $*"; }
die()  { echo -e "${RED}[bootstrap]${NC} $*" >&2; exit 1; }

# ---------------------------------------------------------------------
# 0. Sanity checks
# ---------------------------------------------------------------------
[[ -d "$STAR_ROS2_DIR" ]] || die "cannot find star-ros2/ under $STAR_DIR"
[[ -d "$COMPLIANCE_SRC_DIR" ]] \
    || die "cannot find star-compliance/ under $STAR_DIR"

if ! command -v ros2 >/dev/null 2>&1; then
    die "ros2 CLI not found; install ROS 2 Jazzy first (see star-ros2/README.md)"
fi

ROS_DISTRO="${ROS_DISTRO:-jazzy}"
say "ROS 2 distro: $ROS_DISTRO"

# ---------------------------------------------------------------------
# 1. Apt packages that rosidl-built compliance nodes import at runtime
# ---------------------------------------------------------------------
say "Installing ROS 2 Python bridges via apt..."
APT_PKGS=(
    "ros-$ROS_DISTRO-cv-bridge"
    "ros-$ROS_DISTRO-sensor-msgs-py"
    "ros-$ROS_DISTRO-launch-testing-ament-cmake"
    "ros-$ROS_DISTRO-nav2-costmap-2d"
    "python3-pip"
    "python3-colcon-common-extensions"
)
if sudo -n true 2>/dev/null; then
    sudo apt-get update -q
    sudo apt-get install -y "${APT_PKGS[@]}" \
        || die "apt install failed; try re-running with sudo"
else
    warn "sudo unavailable - skipping apt step."
    warn "Install manually:  sudo apt-get install ${APT_PKGS[*]}"
fi

# ---------------------------------------------------------------------
# 2. Python dependencies for compliance detectors
# ---------------------------------------------------------------------
say "Installing Python deps for compliance-engine..."
python3 -m pip install --user --upgrade \
    "numpy>=1.26" \
    "scikit-image>=0.22" \
    "scikit-learn>=1.4" \
    "opencv-python>=4.9" \
    "onnxruntime>=1.17" \
    "reportlab>=4.0" \
    || die "pip install failed"

# open3d is slow on Pi5 arm64; install separately so the user sees
# progress and we do not abort the whole script on a timeout.
if ! python3 -c "import open3d" 2>/dev/null; then
    say "Installing open3d (can take 5-15 min on Pi5 arm64)..."
    python3 -m pip install --user "open3d>=0.18" \
        || warn "open3d install failed - ADA 307 + jamb-plane fit will be skipped."
else
    say "open3d already installed: $(python3 -c 'import open3d;print(open3d.__version__)')"
fi

# ---------------------------------------------------------------------
# 3. Symlink compliance-engine into the colcon workspace
# ---------------------------------------------------------------------
if [[ ! -e "$COMPLIANCE_LINK_DIR" ]]; then
    say "Symlinking $COMPLIANCE_SRC_DIR -> $COMPLIANCE_LINK_DIR"
    ln -s "$COMPLIANCE_SRC_DIR" "$COMPLIANCE_LINK_DIR"
elif [[ ! -L "$COMPLIANCE_LINK_DIR" ]]; then
    warn "$COMPLIANCE_LINK_DIR exists and is not a symlink; skipping."
else
    say "Compliance symlink already present."
fi

# ---------------------------------------------------------------------
# 4. colcon build the two new packages
# ---------------------------------------------------------------------
say "Building star_compliance_msgs and star_compliance..."
cd "$STAR_ROS2_DIR"
# shellcheck disable=SC1090
source "/opt/ros/$ROS_DISTRO/setup.bash"
colcon build \
    --symlink-install \
    --packages-select star_compliance_msgs star_compliance \
    --event-handlers console_direct+ \
    || die "colcon build failed (see output above)"

# shellcheck disable=SC1091
source "$STAR_ROS2_DIR/install/setup.bash"

# ---------------------------------------------------------------------
# 5. Smoke tests
# ---------------------------------------------------------------------
say "Smoke-testing ROS 2 interfaces..."
ros2 interface show star_compliance_msgs/msg/DoorwayMeasurement >/dev/null \
    || die "star_compliance_msgs/DoorwayMeasurement not resolvable"
ros2 interface show star_compliance_msgs/msg/ProtrudingObjectArray >/dev/null \
    || die "ProtrudingObjectArray not resolvable"
say "  star_compliance_msgs  OK"

say "Smoke-testing node executables..."
NODE_EXES=(
    ramp_slope_node
    door_clear_width_node
    door_threshold_node
    protruding_objects_node
    path_blockage_node
    dynamic_obstacle_node
    compliance_monitor_node
)
for exe in "${NODE_EXES[@]}"; do
    which_path=$(ros2 pkg executables star_compliance \
        | awk -v e="$exe" '$2 == e {print $2; exit}')
    [[ -n "$which_path" ]] || die "node executable missing: $exe"
done
say "  7 star_compliance node executables  OK"

say "Smoke-testing Python package imports..."
python3 - <<'EOF'
import sys

failed = []
for mod in [
    "star_compliance.detectors.doorway_lidar_detector",
    "star_compliance.detectors.jamb_plane_fitter",
    "star_compliance.detectors.door_state_classifier",
    "star_compliance.detectors.imu_jolt_detector",
    "star_compliance.detectors.cane_zone_filter",
    "star_compliance.detectors.wall_plane_fitter",
    "star_compliance.detectors.corridor_medial_axis",
    "star_compliance.detectors.obstacle_clusterer",
    "star_compliance.engines.floor_frame",
    "star_compliance.engines.door_offset_calibration",
    "star_compliance.engines.plane_segmentation",
]:
    try:
        __import__(mod)
    except Exception as exc:
        failed.append((mod, exc))
if failed:
    for mod, exc in failed:
        print(f"  FAIL  {mod}  {exc}")
    sys.exit(1)
print("  all compliance modules import cleanly")
EOF

echo
say "========================================"
say "Bootstrap complete."
say ""
say "Next steps:"
say "  1. Re-source the workspace in any new shell:"
say "       source $STAR_ROS2_DIR/install/setup.bash"
say "  2. Run ./start.sh to bring up the full stack (SLAM + stereo +"
say "     RTAB-Map + compliance-engine)."
say "  3. Individual compliance nodes can be launched manually with"
say "       ros2 launch star_compliance compliance.launch.py"
say "========================================"
