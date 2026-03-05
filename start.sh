#!/usr/bin/env bash
# STAR start — auto-detect hardware and boot all components in order.
# Usage: ./start.sh [--no-lidar] [--no-ui] [--rviz] [--help]

set -euo pipefail

STAR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="/tmp/star-logs"

# ── colours ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

say()  { echo -e "${GREEN}[start]${NC} $*"; }
warn() { echo -e "${YELLOW}[start]${NC} $*"; }
die()  { echo -e "${RED}[start] ERROR:${NC} $*" >&2; exit 1; }

# ── argument parsing ──────────────────────────────────────────────────────────
OPT_NO_LIDAR=false
OPT_NO_UI=false
OPT_RVIZ=false

usage() {
    cat <<EOF
Usage: ./start.sh [OPTIONS]

  (no flags)   Auto-detect hardware; start everything
  --no-lidar   Skip LiDAR/SLAM/EKF even if /dev/rplidar is present
  --no-ui      Skip npm dev server
  --rviz       Launch RViz2 after startup
  --help       Print this help

Environment overrides (checked before auto-detection):
  STAR_SIMULATION_MODE=true   Force dev mode (virtual RX72N, no real SPI)
  STAR_SIMULATION_MODE=false  Force HW mode (real SPI, no virtual RX72N)
EOF
    exit 0
}

for arg in "$@"; do
    case "$arg" in
        --no-lidar) OPT_NO_LIDAR=true ;;
        --no-ui)    OPT_NO_UI=true ;;
        --rviz)     OPT_RVIZ=true ;;
        --help|-h)  usage ;;
        *) die "Unknown option: $arg (try --help)" ;;
    esac
done

# ── trap Ctrl-C during startup ────────────────────────────────────────────────
trap 'echo -e "\n${YELLOW}[start] Interrupted — stopping all components...${NC}"; "$STAR_DIR/stop.sh"; exit 130' INT

# ── fresh log directory ───────────────────────────────────────────────────────
rm -rf "$LOG_DIR"
mkdir -p "$LOG_DIR"

# ── Phase 1: stop anything already running ───────────────────────────────────
say "Stopping any running STAR components..."
"$STAR_DIR/stop.sh"

# ── source ROS2 environments (inherited by all background &-jobs) ─────────────
ROS2_SETUP=/opt/ros/jazzy/setup.bash
ROS2_WS_SETUP="$STAR_DIR/star-ros2/install/local_setup.bash"

[[ -f "$ROS2_SETUP" ]]    || die "ROS2 not found at $ROS2_SETUP"
[[ -f "$ROS2_WS_SETUP" ]] || die "ROS2 workspace not built — run build-ros2.sh first"

# ROS2 setup scripts reference unset vars; disable -u temporarily
set +u
# shellcheck disable=SC1090
source "$ROS2_SETUP"
# shellcheck disable=SC1090
source "$ROS2_WS_SETUP"
set -u

# ── Phase 2: hardware auto-detection ─────────────────────────────────────────
DEV_MODE=false   # default; overridden below
HAS_LIDAR=false
PROBE_PID=""
SPI_BRIDGE_PID=""  # set if probe succeeds and we keep the process

if [[ "${STAR_SIMULATION_MODE:-}" == "true" ]]; then
    say "STAR_SIMULATION_MODE=true → forcing DEV mode"
    DEV_MODE=true
elif [[ "${STAR_SIMULATION_MODE:-}" == "false" ]]; then
    say "STAR_SIMULATION_MODE=false → forcing HW mode"
    DEV_MODE=false
else
    # Probe: start spi_bridge for ~4 s and check its log
    say "Probing for RX72N hardware (~4 s)..."
    PROBE_LOG="$LOG_DIR/probe.log"
    SPI_BRIDGE_LAUNCH="$STAR_DIR/star-ros2/src/star_spi_bridge/launch/star_spi_bridge.launch.py"
    ros2 launch "$SPI_BRIDGE_LAUNCH" \
        >"$PROBE_LOG" 2>&1 &
    PROBE_PID=$!

    sleep 4

    if grep -qE "SPI Frame Decode Failed|SPI Transfer Failed" "$PROBE_LOG" 2>/dev/null; then
        say "RX72N probe: no valid STAR frame received → DEV mode (virtual RX72N)"
        DEV_MODE=true
        kill "$PROBE_PID" 2>/dev/null || true
        sleep 1
    else
        say "RX72N probe: SPI transfers OK → HW mode (real RX72N)"
        DEV_MODE=false
        # Keep the spi_bridge running; record its PID
        SPI_BRIDGE_PID=$PROBE_PID
        cp "$PROBE_LOG" "$LOG_DIR/spi_bridge.log"
    fi
fi

# LiDAR detection
if [[ "$OPT_NO_LIDAR" == "true" ]]; then
    HAS_LIDAR=false
    warn "--no-lidar flag set: skipping LiDAR/SLAM"
elif [[ -L /dev/rplidar || -c /dev/rplidar ]]; then
    HAS_LIDAR=true
    say "LiDAR detected: /dev/rplidar"
else
    HAS_LIDAR=false
    warn "No LiDAR found at /dev/rplidar — SLAM will be skipped"
fi

# ── Detection banner ──────────────────────────────────────────────────────────
if [[ "$DEV_MODE" == "true" ]]; then
    MODE_LABEL="DEV MODE  (virtual RX72N)"
    RX72N_LABEL="virtual (no hardware detected)"
else
    MODE_LABEL="HW MODE   (real RX72N)"
    RX72N_LABEL="real RX72N (SPI)"
fi

if [[ "$HAS_LIDAR" == "true" ]]; then
    LIDAR_LABEL="/dev/rplidar ✓"
else
    LIDAR_LABEL="not found (SLAM disabled)"
fi

echo -e ""
echo -e "${BOLD}${CYAN}╔══════════════════════════════════════════╗${NC}"
printf "${BOLD}${CYAN}║${NC}  STAR  —  %-32s${BOLD}${CYAN}║${NC}\n" "$MODE_LABEL"
echo -e "${BOLD}${CYAN}╚══════════════════════════════════════════╝${NC}"
echo -e "  RX72N  : $RX72N_LABEL"
echo -e "  LiDAR  : $LIDAR_LABEL"
echo -e ""

# ── helper: wait for a TCP port to open ──────────────────────────────────────
wait_port() {
    local host="$1" port="$2" label="$3" timeout="${4:-10}"
    local i=0
    while ! nc -z "$host" "$port" 2>/dev/null; do
        sleep 1
        i=$((i + 1))
        if [[ $i -ge $timeout ]]; then
            warn "$label: port $port not open after ${timeout}s — continuing anyway"
            return 1
        fi
    done
    say "$label: port $port open"
}

# ── PIDs for summary ──────────────────────────────────────────────────────────
PID_VRXN=""
PID_GW=""
PID_FAKEODOM=""
PID_SLAM=""
PID_GWBRIDGE=""
PID_UI=""
PID_RVIZ=""

# ── Step 1: virtual_rx72n (DEV mode only) ────────────────────────────────────
if [[ "$DEV_MODE" == "true" ]]; then
    say "Starting virtual_rx72n..."
    STAR_SIMULATION_MODE=true \
        "$STAR_DIR/star-gateway/virtual_rx72n" \
        >"$LOG_DIR/virtual_rx72n.log" 2>&1 &
    PID_VRXN=$!
    sleep 1
    if ! kill -0 "$PID_VRXN" 2>/dev/null; then
        die "virtual_rx72n failed to start — check $LOG_DIR/virtual_rx72n.log"
    fi
    say "virtual_rx72n running (PID $PID_VRXN)"
fi

# ── Step 2: star-gateway ─────────────────────────────────────────────────────
say "Starting star-gateway..."
GW_ENV=""
if [[ "$DEV_MODE" == "true" ]]; then
    GW_ENV="STAR_SIMULATION_MODE=true"
fi
env WS_STRICT_ORIGIN=false ${GW_ENV} \
    "$STAR_DIR/star-gateway/star-gateway" \
    >"$LOG_DIR/gateway.log" 2>&1 &
PID_GW=$!

say "Waiting for gRPC :50051..."
wait_port localhost 50051 "star-gateway" 10
say "star-gateway running (PID $PID_GW)"

# ── Step 3: LiDAR setup (ModemManager + permissions) ─────────────────────────
if [[ "$HAS_LIDAR" == "true" ]]; then
    say "Stopping ModemManager (safety, udev rule already blocks probe)..."
    sudo systemctl stop ModemManager 2>/dev/null || true
    # chmod not needed: udev rule sets GROUP=dialout MODE=0660
fi

# ── Step 4: star_spi_bridge ───────────────────────────────────────────────────
SPI_BRIDGE_LAUNCH="$STAR_DIR/star-ros2/src/star_spi_bridge/launch/star_spi_bridge.launch.py"

if [[ "$DEV_MODE" == "false" && -n "$SPI_BRIDGE_PID" ]]; then
    say "spi_bridge already running from probe (PID $SPI_BRIDGE_PID)"
    SPI_BRIDGE_PID="$SPI_BRIDGE_PID"
else
    say "Starting star_spi_bridge..."
    ros2 launch "$SPI_BRIDGE_LAUNCH" \
        >"$LOG_DIR/spi_bridge.log" 2>&1 &
    SPI_BRIDGE_PID=$!
    sleep 4
    if ! kill -0 "$SPI_BRIDGE_PID" 2>/dev/null; then
        warn "spi_bridge may have exited — check $LOG_DIR/spi_bridge.log"
    else
        say "spi_bridge running (PID $SPI_BRIDGE_PID)"
    fi
fi

# ── Step 4b: fake odom TF (DEV mode only) ────────────────────────────────────
# In dev mode the virtual RX72N never emits autonomous telemetry, so the EKF
# has no input and never publishes the odom→base_link TF. Publishing a static
# identity transform keeps the TF chain intact so SLAM can process laser scans.
if [[ "$DEV_MODE" == "true" ]]; then
    say "DEV mode: publishing static odom→base_link (robot stationary at origin)..."
    ros2 run tf2_ros static_transform_publisher \
        --frame-id odom --child-frame-id base_link \
        >"$LOG_DIR/fake_odom.log" 2>&1 &
    PID_FAKEODOM=$!
    sleep 1
    if ! kill -0 "$PID_FAKEODOM" 2>/dev/null; then
        die "static_transform_publisher failed — check $LOG_DIR/fake_odom.log"
    fi
    say "fake odom TF running (PID $PID_FAKEODOM)"
fi

# ── Step 5: SLAM stack (only if LiDAR present) ───────────────────────────────
SLAM_LAUNCH="$STAR_DIR/star-ros2/src/star_bringup/launch/slam.launch.py"

if [[ "$HAS_LIDAR" == "true" ]]; then
    say "Starting SLAM stack (slam.launch.py)..."
    if [[ "$DEV_MODE" == "true" ]]; then
        ros2 launch "$SLAM_LAUNCH" use_nav2:=false use_ekf:=false \
            >"$LOG_DIR/slam.log" 2>&1 &
    else
        ros2 launch "$SLAM_LAUNCH" use_nav2:=false \
            >"$LOG_DIR/slam.log" 2>&1 &
    fi
    PID_SLAM=$!
    say "Waiting ~8 s for LiDAR init..."
    sleep 8
    if ! kill -0 "$PID_SLAM" 2>/dev/null; then
        warn "SLAM launch may have exited — check $LOG_DIR/slam.log"
    else
        say "SLAM stack running (PID $PID_SLAM)"
    fi
else
    say "Skipping SLAM stack (no LiDAR)"
fi

# ── Step 6: star_gateway_bridge_main ─────────────────────────────────────────
say "Starting star_gateway_bridge_main..."
ros2 run star_gateway_bridge star_gateway_bridge_main \
    >"$LOG_DIR/gw_bridge.log" 2>&1 &
PID_GWBRIDGE=$!

# Wait up to 10 s for "connected to Gateway" in log
say "Waiting for gateway_bridge to connect..."
BRIDGE_TIMEOUT=10
for i in $(seq 1 "$BRIDGE_TIMEOUT"); do
    if grep -q "connected to Gateway\|Connected to gateway\|gRPC" "$LOG_DIR/gw_bridge.log" 2>/dev/null; then
        say "gateway_bridge connected (PID $PID_GWBRIDGE)"
        break
    fi
    if ! kill -0 "$PID_GWBRIDGE" 2>/dev/null; then
        warn "gateway_bridge exited — check $LOG_DIR/gw_bridge.log"
        break
    fi
    sleep 1
    if [[ $i -eq $BRIDGE_TIMEOUT ]]; then
        warn "gateway_bridge: no 'connected' message after ${BRIDGE_TIMEOUT}s — continuing"
    fi
done

# ── Step 7: UI dev server ─────────────────────────────────────────────────────
if [[ "$OPT_NO_UI" == "false" ]]; then
    say "Starting UI dev server..."
    (cd "$STAR_DIR/star-ui" && npm run dev \
        >"$LOG_DIR/ui.log" 2>&1) &
    PID_UI=$!
    sleep 3
    if ! kill -0 "$PID_UI" 2>/dev/null; then
        warn "UI dev server may have exited — check $LOG_DIR/ui.log"
    else
        say "UI dev server running (PID $PID_UI)"
    fi
else
    say "Skipping UI (--no-ui)"
fi

# ── Step 8: RViz2 ─────────────────────────────────────────────────────────────
RVIZ_CFG="$STAR_DIR/star-ros2/src/star_bringup/rviz/slam_lidar.rviz"

if [[ "$OPT_RVIZ" == "true" ]]; then
    say "Starting RViz2..."
    rviz2 -d "$RVIZ_CFG" \
        >"$LOG_DIR/rviz.log" 2>&1 &
    PID_RVIZ=$!
    say "RViz2 running (PID $PID_RVIZ)"
fi

# ── Phase 4: summary banner ───────────────────────────────────────────────────
TIMESTAMP="$(date '+%Y-%m-%d %H:%M:%S')"
LOCAL_IP="$(hostname -I 2>/dev/null | awk '{print $1}')"

echo -e ""
echo -e "${BOLD}${CYAN}══════════════════════════════════════════════════════${NC}"
printf "${BOLD}${CYAN}  STAR Robot — %s   [%s]${NC}\n" "$TIMESTAMP" "$MODE_LABEL"
echo -e "${BOLD}${CYAN}══════════════════════════════════════════════════════${NC}"

if [[ "$DEV_MODE" == "true" && -n "$PID_VRXN" ]]; then
    printf "  %-18s PID %s\n" "virtual_rx72n" "$PID_VRXN"
fi
printf "  %-18s PID %s   gRPC :50051   WS :8080\n" "star-gateway" "$PID_GW"
if [[ "$DEV_MODE" == "true" && -n "$PID_FAKEODOM" ]]; then
    printf "  %-18s PID %s   odom→base_link (static)\n" "fake_odom" "$PID_FAKEODOM"
fi
printf "  %-18s PID %s\n" "spi_bridge" "${SPI_BRIDGE_PID:-unknown}"
if [[ -n "$PID_SLAM" ]]; then
    printf "  %-18s PID %s   /scan @ 10 Hz\n" "slam" "$PID_SLAM"
fi
printf "  %-18s PID %s\n" "gw_bridge" "${PID_GWBRIDGE:-unknown}"
if [[ -n "$PID_UI" ]]; then
    printf "  %-18s PID %s   http://%s:5173\n" "UI" "$PID_UI" "$LOCAL_IP"
fi
if [[ -n "$PID_RVIZ" ]]; then
    printf "  %-18s PID %s\n" "rviz2" "$PID_RVIZ"
fi

echo -e ""
echo -e "  Logs : $LOG_DIR/"
echo -e "  Stop : ./stop.sh"
echo -e "${BOLD}${CYAN}══════════════════════════════════════════════════════${NC}"
echo -e ""

# Remove INT trap — startup is complete, daemons run in background
trap - INT
