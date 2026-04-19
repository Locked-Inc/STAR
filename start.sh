#!/usr/bin/env bash
# STAR start -- auto-detect hardware and boot all components in order.
# Usage: ./start.sh [--no-lidar] [--no-ui] [--no-compliance] [--rviz] [--help]

set -euo pipefail

STAR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="/tmp/star-logs"

# -- colours ------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

say() { echo -e "${GREEN}[start]${NC} $*"; }
warn() { echo -e "${YELLOW}[start]${NC} $*"; }
die() {
  echo -e "${RED}[start] ERROR:${NC} $*" >&2
  exit 1
}

# Verify persistent Pi5 host setup (installed by scripts/setup-pi5-host.sh).
# Warn-only -- never blocks startup -- so a fresh Pi5 still boots the stack.
# Wifi power-save is intentionally NOT checked during development.
# TODO(pre-field): re-enable a power-save check once
#   /etc/NetworkManager/conf.d/10-wifi-powersave.conf is in place.
require_host_setup() {
  local issues=()
  local rule_file=/etc/udev/rules.d/70-star-usb-no-autosuspend.rules
  if [[ ! -f "$rule_file" ]]; then
    issues+=("USB autosuspend udev rule missing ($rule_file)")
  elif ! grep -q '1d6b.*0104.*power/control' "$rule_file" 2>/dev/null; then
    issues+=("USB autosuspend udev rule present but BBB entry looks wrong ($rule_file)")
  fi
  # Pi5 has a single cpufreq policy (policy0) covering all 4 cores, but loop
  # over every policy so this also works on multi-policy hosts.
  for _pol in /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; do
    [[ -e "$_pol" ]] || continue
    if [[ "$(cat "$_pol" 2>/dev/null)" != "schedutil" ]]; then
      issues+=("CPU governor not schedutil on $(basename "$(dirname "$_pol")")")
      break
    fi
  done
  if [[ ! -f /etc/logrotate.d/star-robot ]]; then
    issues+=("logrotate config missing (/etc/logrotate.d/star-robot)")
  fi
  if (( ${#issues[@]} > 0 )); then
    warn "Pi5 host setup incomplete -- robot may be unreliable on long runs:"
    for _msg in "${issues[@]}"; do warn "  - $_msg"; done
    warn "To fix: sudo bash $STAR_DIR/scripts/setup-pi5-host.sh"
  fi
  return 0
}

# -- argument parsing ----------------------------------------------------------
OPT_NO_LIDAR=false
OPT_NO_UI=false
OPT_NO_COMPLIANCE=false
OPT_RVIZ=false
OPT_NO_AP=false

LICHTBLICK_WEB_DIR="/opt/star-lichtblick-web"
LICHTBLICK_PORT=8081

WIFI_SSID="${STAR_WIFI_SSID:-STAR-Robot}"
WIFI_PASS="${STAR_WIFI_PASS:-STAR2026!}"
WIFI_IFACE="${STAR_WIFI_IFACE:-wlan0}"
AP_CON="STAR-Hotspot"
AP_IP="192.168.50.1"
AP_ACTIVE=false

usage() {
  cat <<EOF
Usage: ./start.sh [OPTIONS]

  (no flags)   Auto-detect hardware; start everything
  --no-lidar       Skip LiDAR/SLAM/EKF even if /dev/rplidar is present
  --no-ui          Skip npm dev server
  --no-compliance  Skip the compliance-engine launch (ADA checks)
  --no-ap          Skip WiFi AP setup (use when connected via Ethernet)
  --rviz       Launch RViz2 after startup
  --help       Print this help

Environment overrides (checked before auto-detection):
  STAR_SIMULATION_MODE=true   Force dev mode (virtual RX72N, no real SPI)
  STAR_SIMULATION_MODE=false  Force HW mode (real SPI, no virtual RX72N)
  STAR_WIFI_SSID=MyNet        Custom AP SSID       (default: STAR-Robot)
  STAR_WIFI_PASS=secret       Custom AP password   (default: STAR2026!)
  STAR_WIFI_IFACE=wlan1       Custom WiFi interface (default: wlan0)
EOF
  exit 0
}

for arg in "$@"; do
  case "$arg" in
  --no-lidar) OPT_NO_LIDAR=true ;;
  --no-ui) OPT_NO_UI=true ;;
  --no-compliance) OPT_NO_COMPLIANCE=true ;;
  --no-ap) OPT_NO_AP=true ;;
  --rviz) OPT_RVIZ=true ;;
  --help | -h) usage ;;
  *) die "Unknown option: $arg (try --help)" ;;
  esac
done

# -- trap Ctrl-C during startup ------------------------------------------------
trap 'echo -e "\n${YELLOW}[start] Interrupted -- stopping all components...${NC}"; "$STAR_DIR/stop.sh"; exit 130' INT

# -- fresh log directory -------------------------------------------------------
rm -rf "$LOG_DIR"
mkdir -p "$LOG_DIR"

# -- verify persistent host setup ---------------------------------------------
require_host_setup

# -- Phase 0: WiFi Access Point -----------------------------------------------
if [[ "$OPT_NO_AP" == "false" ]]; then
  if ! ip link show "$WIFI_IFACE" &>/dev/null; then
    warn "WiFi interface $WIFI_IFACE not found -- skipping AP (use --no-ap to suppress)"
  else
    say "Setting up WiFi AP: SSID=$WIFI_SSID on $WIFI_IFACE..."
    warn "If you are connected via WiFi SSH, your session will drop momentarily."
    warn "Reconnect after joining '$WIFI_SSID': ssh star@$AP_IP"

    # Open STAR service ports in UFW so AP clients can reach them.
    # UFW defaults to blocking all inbound; without these rules the Mac
    # will connect to the AP but WebSocket/UI connections will be refused.
    if sudo ufw status 2>/dev/null | grep -q "Status: active"; then
      say "UFW is active -- allowing STAR ports from AP subnet (192.168.50.0/24)..."
      sudo ufw allow from 192.168.50.0/24 to any port 8080 proto tcp >/dev/null 2>&1 || true
      sudo ufw allow from 192.168.50.0/24 to any port 8765 proto tcp >/dev/null 2>&1 || true
      sudo ufw allow from 192.168.50.0/24 to any port 5173 proto tcp >/dev/null 2>&1 || true
      sudo ufw allow from 192.168.50.0/24 to any port "$LICHTBLICK_PORT" proto tcp >/dev/null 2>&1 || true
      say "UFW: ports 8080 8765 5173 $LICHTBLICK_PORT open for AP clients"
    fi

    # If already active, reuse it (idempotent across start.sh re-runs)
    if sudo nmcli -t -f NAME,DEVICE connection show --active 2>/dev/null |
      grep -q "^${AP_CON}:"; then
      say "WiFi AP '$AP_CON' already active -- reusing"
      AP_ACTIVE=true
    else
      # Remove any stale profile from a previous run
      sudo nmcli connection delete "$AP_CON" 2>/dev/null || true

      # Drop current wlan0 station link so the chip can enter AP mode
      sudo nmcli device disconnect "$WIFI_IFACE" 2>/dev/null || true
      sleep 1

      # Create the AP connection profile
      sudo nmcli connection add \
        type wifi \
        ifname "$WIFI_IFACE" \
        con-name "$AP_CON" \
        autoconnect no \
        wifi.mode ap \
        wifi.ssid "$WIFI_SSID" \
        wifi.band bg \
        wifi.channel 6 \
        wifi-sec.key-mgmt wpa-psk \
        wifi-sec.psk "$WIFI_PASS" \
        ipv4.method shared \
        ipv4.addresses "${AP_IP}/24" \
        ipv6.method disabled \
        >/dev/null 2>&1

      # Activate
      if sudo nmcli connection up "$AP_CON" >/dev/null 2>&1; then
        sleep 2
        AP_ACTIVE=true
        say "WiFi AP active -- SSID: $WIFI_SSID  IP: $AP_IP"
      else
        warn "WiFi AP failed to start -- check: sudo nmcli connection up $AP_CON"
        warn "Continuing without AP (use --no-ap to suppress this warning)"
      fi
    fi
  fi
fi

# Resolve display IP now -- used in step log messages and summary banner.
# AP_ACTIVE is final at this point; hostname -I is the fallback.
if [[ "$AP_ACTIVE" == "true" ]]; then
  DISPLAY_IP="$AP_IP"
else
  DISPLAY_IP="$(hostname -I 2>/dev/null | awk '{print $1}')"
fi

# -- Phase 1: stop anything already running -----------------------------------
say "Stopping any running STAR components..."
"$STAR_DIR/stop.sh"

# -- source ROS2 environments (inherited by all background &-jobs) -------------
ROS2_SETUP=/opt/ros/jazzy/setup.bash
ROS2_WS_SETUP="$STAR_DIR/star-ros2/install/local_setup.bash"

[[ -f "$ROS2_SETUP" ]] || die "ROS2 not found at $ROS2_SETUP"
[[ -f "$ROS2_WS_SETUP" ]] || die "ROS2 workspace not built -- run build-ros2.sh first"

# ROS2 setup scripts reference unset vars; disable -u temporarily
set +u
# shellcheck disable=SC1090
source "$ROS2_SETUP"
# shellcheck disable=SC1090
source "$ROS2_WS_SETUP"
set -u

# Explicit ROS2 environment overrides -- safe under systemd/cron/ssh
# (non-login shells that don't source ~/.bashrc).
#
# RMW is FORCED (no :- default): we never want FastDDS in this project,
# even if a stray caller env var says so. ROS_DOMAIN_ID respects caller
# override so multi-robot/multi-operator setups can pick a unique id.
#
# CYCLONEDDS_URI points at our config/cyclonedds.xml, which raises the
# participant index limit and disables multicast. Without it, stale
# participants from crashed nodes exhaust the default 10-slot pool and
# new nodes fail with "Failed to find a free participant index".
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-42}"
export CYCLONEDDS_URI="file://$STAR_DIR/config/cyclonedds.xml"
export RCUTILS_COLORIZED_OUTPUT=1

# Flush the ROS2 daemon's stale participant cache before launching nodes.
# The daemon caches DDS discovery state; restarting it clears zombie entries
# left by previous crashes without waiting for lease expiry.
say "Restarting ROS2 daemon to flush stale DDS participants..."
pkill -9 -f _ros2_daemon 2>/dev/null || true
sleep 1
ros2 daemon start 2>/dev/null || true

# -- Phase 2: hardware auto-detection -----------------------------------------
DEV_MODE=false  # default; overridden below
BBB_MODE=false  # BeagleBone Blue over USB CDC
BBB_DEV=""      # /dev/ttyACMN path discovered by VID:PID scan
HAS_LIDAR=false
PROBE_PID=""
SPI_BRIDGE_PID="" # set if probe succeeds and we keep the process

# BBB connection parameters (override with env vars if needed)
BBB_HOST="${BBB_HOST:-192.168.7.2}"
BBB_USER="${BBB_USER:-debian}"
BBB_PASS="${BBB_PASS:-StarBBB2026!}"
BBB_FW_PATH="${BBB_FW_PATH:-/home/debian/star-beaglebone-blue}"

# ------------------------------------------------------------------
# Detect BeagleBone Blue by USB VID:PID (1d6b:0104) across all
# ttyACM* devices.  After a disconnect/reconnect the kernel may
# assign a different minor number (ttyACM0 -> ttyACM1), so we scan
# every ttyACM* entry in sysfs rather than assuming a fixed name.
#
# sysfs layout:
#   /sys/class/tty/ttyACMN/device  -> symlink to USB interface dir
#   readlink -f resolves it to: .../2-1:1.0  (USB interface)
#   dirname gives:               .../2-1      (USB device, has idVendor)
# ------------------------------------------------------------------
for _tty_link in /sys/class/tty/ttyACM*/device; do
  [[ -e "$_tty_link" ]] || continue
  _iface_dir=$(readlink -f "$_tty_link" 2>/dev/null) || continue
  _usb_dir=$(dirname "$_iface_dir")
  _vid=$(cat "$_usb_dir/idVendor" 2>/dev/null | tr -d '[:space:]')
  _pid=$(cat "$_usb_dir/idProduct" 2>/dev/null | tr -d '[:space:]')
  if [[ "$_vid" == "1d6b" && "$_pid" == "0104" ]]; then
    BBB_DEV="/dev/$(basename "$(dirname "$_tty_link")")"
    BBB_MODE=true
    say "BeagleBone Blue detected at $BBB_DEV (VID:PID 1d6b:0104)"
    break
  fi
done

if [[ "$BBB_MODE" == "true" ]]; then
  say "BBB mode: USB CDC motor controller (skipping RX72N/SPI)"
elif [[ "${STAR_SIMULATION_MODE:-}" == "true" ]]; then
  say "STAR_SIMULATION_MODE=true -> forcing DEV mode"
  DEV_MODE=true
elif [[ "${STAR_SIMULATION_MODE:-}" == "false" ]]; then
  say "STAR_SIMULATION_MODE=false -> forcing HW mode"
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
    say "RX72N probe: no valid STAR frame received -> DEV mode (virtual RX72N)"
    DEV_MODE=true
    kill "$PROBE_PID" 2>/dev/null || true
    sleep 1
  else
    say "RX72N probe: SPI transfers OK -> HW mode (real RX72N)"
    DEV_MODE=false
    # Keep the spi_bridge running; record its PID
    SPI_BRIDGE_PID=$PROBE_PID
    cp "$PROBE_LOG" "$LOG_DIR/spi_bridge.log"
  fi
fi

# LiDAR detection -- stop ModemManager FIRST (it grabs /dev/ttyUSB0 on boot
# and resets the CP210x USB-UART bridge, disconnecting the RPLiDAR C1).
say "Stopping ModemManager (interferes with LiDAR)..."
sudo systemctl stop ModemManager 2>/dev/null || true
sleep 2 # give ttyUSB0 time to re-enumerate after ModemManager releases it

if [[ "$OPT_NO_LIDAR" == "true" ]]; then
  HAS_LIDAR=false
  warn "--no-lidar flag set: skipping LiDAR/SLAM"
elif [[ -L /dev/rplidar || -c /dev/rplidar || -c /dev/ttyUSB0 ]]; then
  HAS_LIDAR=true
  LIDAR_DEV=$(ls /dev/rplidar /dev/ttyUSB0 2>/dev/null | head -1)
  say "LiDAR detected: $LIDAR_DEV"
else
  HAS_LIDAR=false
  warn "No LiDAR found at /dev/rplidar or /dev/ttyUSB0 -- SLAM will be skipped"
fi

# -- Detection banner ----------------------------------------------------------
if [[ "$BBB_MODE" == "true" ]]; then
  MODE_LABEL="BBB MODE  (BeagleBone Blue USB)"
  MC_LABEL="BeagleBone Blue ($BBB_DEV -- USB CDC only, no SPI)"
elif [[ "$DEV_MODE" == "true" ]]; then
  MODE_LABEL="DEV MODE  (virtual RX72N)"
  MC_LABEL="virtual (no hardware detected)"
else
  MODE_LABEL="HW MODE   (real RX72N)"
  MC_LABEL="real RX72N (SPI)"
fi

if [[ "$HAS_LIDAR" == "true" ]]; then
  LIDAR_LABEL="/dev/rplidar [PASS]"
else
  LIDAR_LABEL="not found (SLAM disabled)"
fi

echo -e ""
echo -e "${BOLD}${CYAN}+==========================================+${NC}"
printf "${BOLD}${CYAN}|${NC}  STAR  --  %-32s${BOLD}${CYAN}|${NC}\n" "$MODE_LABEL"
echo -e "${BOLD}${CYAN}+==========================================+${NC}"
echo -e "  Motor  : $MC_LABEL"
echo -e "  LiDAR  : $LIDAR_LABEL"
echo -e ""

# -- helper: wait for a TCP port to open --------------------------------------
wait_port() {
  local host="$1" port="$2" label="$3" timeout="${4:-10}"
  local i=0
  while ! nc -z "$host" "$port" 2>/dev/null; do
    sleep 1
    i=$((i + 1))
    if [[ $i -ge $timeout ]]; then
      warn "$label: port $port not open after ${timeout}s -- continuing anyway"
      return 1
    fi
  done
  say "$label: port $port open"
}

# -- PIDs for summary ----------------------------------------------------------
PID_VRXN=""
PID_GW=""
PID_FAKEODOM=""
PID_SLAM=""
PID_GWBRIDGE=""
PID_UI=""
PID_RVIZ=""
PID_FOXGLOVE=""

# -- Step 0: BBB firmware (BBB mode only) --------------------------------------
PID_BBB_FW=""
if [[ "$BBB_MODE" == "true" ]]; then
  say "Starting BBB firmware via SSH ($BBB_USER@$BBB_HOST)..."
  # Kill any existing firmware, then start fresh
  sshpass -p "$BBB_PASS" ssh -o StrictHostKeyChecking=no "$BBB_USER@$BBB_HOST" \
    "echo '$BBB_PASS' | sudo -S killall -9 star-beaglebone-blue 2>/dev/null; \
         sleep 1; \
         echo '$BBB_PASS' | sudo -S nohup $BBB_FW_PATH > /tmp/firmware.log 2>&1 &" \
    >"$LOG_DIR/bbb_ssh.log" 2>&1
  sleep 3
  # Verify firmware is running
  if sshpass -p "$BBB_PASS" ssh -o StrictHostKeyChecking=no "$BBB_USER@$BBB_HOST" \
    "pgrep -f star-beaglebone-blue >/dev/null 2>&1"; then
    say "BBB firmware running on $BBB_HOST"
    # Wait for the USB CDC port to settle after firmware opens /dev/ttyGS0.
    # Re-scan for the BBB device in case the minor number changed after
    # firmware init (the kernel may re-enumerate the CDC ACM interface).
    say "Waiting for USB CDC to stabilize..."
    for _i in $(seq 1 10); do
      # Re-detect BBB_DEV in case minor changed
      for _tl in /sys/class/tty/ttyACM*/device; do
        [[ -e "$_tl" ]] || continue
        _id=$(readlink -f "$_tl" 2>/dev/null) || continue
        _v=$(cat "$(dirname "$_id")/idVendor" 2>/dev/null | tr -d '[:space:]')
        _p=$(cat "$(dirname "$_id")/idProduct" 2>/dev/null | tr -d '[:space:]')
        if [[ "$_v" == "1d6b" && "$_p" == "0104" ]]; then
          BBB_DEV="/dev/$(basename "$(dirname "$_tl")")"
          break
        fi
      done
      if python3 -c "import serial; s=serial.Serial('${BBB_DEV}',timeout=0.1); s.close()" 2>/dev/null; then
        say "USB CDC ready at $BBB_DEV"
        break
      fi
      sleep 1
    done
  else
    warn "BBB firmware may have failed -- check $BBB_HOST:/tmp/firmware.log"
  fi
fi

# -- Step 1: virtual_rx72n (DEV mode only) ------------------------------------
if [[ "$DEV_MODE" == "true" ]]; then
  say "Starting virtual_rx72n..."
  STAR_SIMULATION_MODE=true \
    "$STAR_DIR/star-gateway/bin/virtual_rx72n" \
    >"$LOG_DIR/virtual_rx72n.log" 2>&1 &
  PID_VRXN=$!
  sleep 1
  if ! kill -0 "$PID_VRXN" 2>/dev/null; then
    die "virtual_rx72n failed to start -- check $LOG_DIR/virtual_rx72n.log"
  fi
  say "virtual_rx72n running (PID $PID_VRXN)"
fi

# -- Step 2: star-gateway -----------------------------------------------------
say "Starting star-gateway..."
GW_ENV=""
if [[ "$DEV_MODE" == "true" ]]; then
  GW_ENV="STAR_SIMULATION_MODE=true"
fi

# Retry gateway start up to 3 times -- the serial port may still be releasing
# from the previous instance after stop.sh.
GW_STARTED=false
for gw_attempt in 1 2 3; do
  env WS_STRICT_ORIGIN=false ${GW_ENV} \
    "$STAR_DIR/star-gateway/bin/star-gateway" \
    >"$LOG_DIR/gateway.log" 2>&1 &
  PID_GW=$!
  sleep 2
  if kill -0 "$PID_GW" 2>/dev/null; then
    GW_STARTED=true
    break
  fi
  warn "star-gateway attempt $gw_attempt failed -- retrying in 3s..."
  sleep 3
done

if [[ "$GW_STARTED" == "false" ]]; then
  die "star-gateway failed to start after 3 attempts -- check $LOG_DIR/gateway.log"
fi

say "Waiting for gRPC :50051..."
wait_port localhost 50051 "star-gateway" 10
say "star-gateway running (PID $PID_GW)"

# -- Step 3: LiDAR permissions (ModemManager already stopped above) -----------
if [[ "$HAS_LIDAR" == "true" ]]; then
  sudo chmod a+rw "$LIDAR_DEV" 2>/dev/null || true
fi

# -- Step 4: star_spi_bridge (skip in BBB mode) --------------------------------
SPI_BRIDGE_LAUNCH="$STAR_DIR/star-ros2/src/star_spi_bridge/launch/star_spi_bridge.launch.py"

if [[ "$BBB_MODE" == "true" ]]; then
  say "Skipping spi_bridge (BBB uses USB CDC via gateway)"
elif [[ "$DEV_MODE" == "false" && -n "$SPI_BRIDGE_PID" ]]; then
  say "spi_bridge already running from probe (PID $SPI_BRIDGE_PID)"
  SPI_BRIDGE_PID="$SPI_BRIDGE_PID"
else
  say "Starting star_spi_bridge..."
  ros2 launch "$SPI_BRIDGE_LAUNCH" \
    >"$LOG_DIR/spi_bridge.log" 2>&1 &
  SPI_BRIDGE_PID=$!
  sleep 4
  if ! kill -0 "$SPI_BRIDGE_PID" 2>/dev/null; then
    warn "spi_bridge may have exited -- check $LOG_DIR/spi_bridge.log"
  else
    say "spi_bridge running (PID $SPI_BRIDGE_PID)"
  fi
fi

# -- Step 4b: fake odom TF (DEV mode only) ------------------------------------
# In dev mode the virtual RX72N never emits autonomous telemetry, so the EKF
# has no input and never publishes the odom->base_link TF. Publishing a static
# identity transform keeps the TF chain intact so SLAM can process laser scans.
if [[ "$DEV_MODE" == "true" ]]; then
  say "DEV mode: publishing static odom->base_link (robot stationary at origin)..."
  ros2 run tf2_ros static_transform_publisher \
    --frame-id odom --child-frame-id base_link \
    >"$LOG_DIR/fake_odom.log" 2>&1 &
  PID_FAKEODOM=$!
  sleep 1
  if ! kill -0 "$PID_FAKEODOM" 2>/dev/null; then
    die "static_transform_publisher failed -- check $LOG_DIR/fake_odom.log"
  fi
  say "fake odom TF running (PID $PID_FAKEODOM)"
fi

# -- Step 5: SLAM stack (only if LiDAR present) -------------------------------
SLAM_LAUNCH="$STAR_DIR/star-ros2/src/star_bringup/launch/slam.launch.py"

if [[ "$HAS_LIDAR" == "true" ]]; then
  say "Starting SLAM stack (slam.launch.py + foxglove + gateway_bridge + stereo)..."
  SLAM_ARGS="use_nav2:=false use_foxglove:=true"
  if [[ "$BBB_MODE" == "true" ]]; then
    SLAM_ARGS="$SLAM_ARGS use_bbb:=true"
    # EKF enabled: BBB publishes /odom/unfiltered (wheel encoders) and
    # /imu/data (gyro rate). robot_localization fuses both and publishes
    # the real odom->base_link TF. No fake static TF needed.
  fi
  if [[ "$DEV_MODE" == "true" ]]; then
    SLAM_ARGS="$SLAM_ARGS use_ekf:=false"
  fi
  # Stereo is launched inside slam.launch.py via use_stereo. Detect the
  # Waveshare IMX219-83 by its libcamera media nodes and enable only when
  # the hardware is present. libpisp 1.2.1/1.3.0 ABI fix for the child
  # gscam processes is applied via LD_LIBRARY_PATH.
  if [[ -e /dev/media2 && -e /dev/media3 ]]; then
    SLAM_ARGS="$SLAM_ARGS use_stereo:=true"
    say "Stereo camera detected (IMX219-83): use_stereo:=true"
  else
    SLAM_ARGS="$SLAM_ARGS use_stereo:=false"
    warn "Stereo camera not found (/dev/media2 or /dev/media3 missing) -- use_stereo:=false"
  fi
  LD_LIBRARY_PATH="/usr/lib/aarch64-linux-gnu:${LD_LIBRARY_PATH:-}" \
    ros2 launch "$SLAM_LAUNCH" $SLAM_ARGS \
    >"$LOG_DIR/slam.log" 2>&1 &
  PID_SLAM=$!
  say "Waiting ~8 s for LiDAR/stereo init..."
  sleep 8
  if ! kill -0 "$PID_SLAM" 2>/dev/null; then
    warn "SLAM launch may have exited -- check $LOG_DIR/slam.log"
  else
    say "SLAM stack running (PID $PID_SLAM)"
  fi
else
  say "Skipping SLAM stack (no LiDAR)"
  # Launch foxglove_bridge standalone so telemetry/IMU/odom are still visualizable
  say "Starting foxglove_bridge standalone (ws://0.0.0.0:8765)..."
  ros2 run foxglove_bridge foxglove_bridge --ros-args \
    -p port:=8765 -p address:="0.0.0.0" -p send_buffer_limit:=10000000 \
    >"$LOG_DIR/foxglove.log" 2>&1 &
  PID_FOXGLOVE=$!
  sleep 1
  say "foxglove_bridge running (PID $PID_FOXGLOVE)"
fi

# -- Step 5c: Compliance engine (ADA checks) ---------------------------------
# Brings up the seven compliance nodes (ramp slope, door clear width, door
# threshold, path blockage, protruding objects, dynamic obstacles, CPU
# safety-net monitor). Requires the compliance packages to have been built
# by bootstrap_pi.sh. Gracefully skipped if not installed.
COMPLIANCE_LAUNCH_AVAILABLE=false
if ros2 pkg list 2>/dev/null | grep -q '^star_compliance$'; then
  COMPLIANCE_LAUNCH_AVAILABLE=true
fi
PID_COMPLIANCE=""
if [[ "$OPT_NO_COMPLIANCE" != "true" && "$COMPLIANCE_LAUNCH_AVAILABLE" == "true" ]]; then
  say "Starting compliance engine (ros2 launch star_compliance compliance.launch.py)..."
  ros2 launch star_compliance compliance.launch.py \
    >"$LOG_DIR/compliance.log" 2>&1 &
  PID_COMPLIANCE=$!
  sleep 3
  if ! kill -0 "$PID_COMPLIANCE" 2>/dev/null; then
    warn "compliance engine may have exited -- check $LOG_DIR/compliance.log"
    PID_COMPLIANCE=""
  else
    say "compliance engine running (PID $PID_COMPLIANCE)"
  fi
else
  if [[ "$COMPLIANCE_LAUNCH_AVAILABLE" != "true" ]]; then
    warn "star_compliance package not found -- run ./bootstrap_pi.sh to build it."
  else
    say "Skipping compliance engine (--no-compliance)."
  fi
fi

# -- Step 6: star_gateway_bridge_main -----------------------------------------
# When SLAM is running, slam.launch.py already includes gateway_bridge and foxglove.
# Only launch standalone bridge when SLAM is NOT running.
if [[ "$HAS_LIDAR" == "true" ]]; then
  say "gateway_bridge + foxglove launched by slam.launch.py (skipping standalone)"
  PID_GWBRIDGE=""
else
  say "Starting star_gateway_bridge_main (standalone)..."
  if [[ "$BBB_MODE" == "true" ]]; then
    ros2 run star_gateway_bridge star_gateway_bridge_main \
      --ros-args -p use_bbb_telemetry:=true \
      >"$LOG_DIR/gw_bridge.log" 2>&1 &
  else
    ros2 run star_gateway_bridge star_gateway_bridge_main \
      >"$LOG_DIR/gw_bridge.log" 2>&1 &
  fi
  PID_GWBRIDGE=$!
  sleep 3
  say "gateway_bridge running (PID $PID_GWBRIDGE)"
fi

# -- Step 7: UI dev server -----------------------------------------------------
if [[ "$OPT_NO_UI" == "false" ]]; then
  say "Starting UI dev server..."
  (cd "$STAR_DIR/star-ui" && npm run dev \
    >"$LOG_DIR/ui.log" 2>&1) &
  PID_UI=$!
  sleep 3
  if ! kill -0 "$PID_UI" 2>/dev/null; then
    warn "UI dev server may have exited -- check $LOG_DIR/ui.log"
  else
    say "UI dev server running (PID $PID_UI)"
  fi
else
  say "Skipping UI (--no-ui)"
fi

# -- Step 8: RViz2 -------------------------------------------------------------
RVIZ_CFG="$STAR_DIR/star-ros2/src/star_bringup/rviz/slam_lidar.rviz"

if [[ "$OPT_RVIZ" == "true" ]]; then
  say "Starting RViz2..."
  rviz2 -d "$RVIZ_CFG" \
    >"$LOG_DIR/rviz.log" 2>&1 &
  PID_RVIZ=$!
  say "RViz2 running (PID $PID_RVIZ)"
fi

# -- Step 9: Lichtblick self-hosted web app ------------------------------------
# Lichtblick is the open-source Foxglove Studio fork. Served locally so the
# laptop can use it without internet while connected to the AP.
# One-time setup: ./scripts/setup-lichtblick-web.sh (needs internet).
PID_LICHTBLICK=""
if [[ -f "$LICHTBLICK_WEB_DIR/index.html" ]]; then
  say "Starting Lichtblick web server (:$LICHTBLICK_PORT)..."
  # npx serve --single: SPA mode -- all 404s return index.html for client routing
  npx --yes serve --single "$LICHTBLICK_WEB_DIR" \
    --listen "$LICHTBLICK_PORT" \
    >"$LOG_DIR/lichtblick.log" 2>&1 &
  PID_LICHTBLICK=$!
  sleep 2
  if ! kill -0 "$PID_LICHTBLICK" 2>/dev/null; then
    warn "Lichtblick server may have exited -- check $LOG_DIR/lichtblick.log"
    PID_LICHTBLICK=""
  else
    say "Lichtblick running (PID $PID_LICHTBLICK) -- http://$DISPLAY_IP:$LICHTBLICK_PORT"
  fi
else
  warn "Lichtblick not installed -- run ./scripts/setup-lichtblick-web.sh (needs internet)"
fi

# -- Phase 4: summary banner ---------------------------------------------------
TIMESTAMP="$(date '+%Y-%m-%d %H:%M:%S')"

echo -e ""
echo -e "${BOLD}${CYAN}======================================================${NC}"
printf "${BOLD}${CYAN}  STAR Robot -- %s   [%s]${NC}\n" "$TIMESTAMP" "$MODE_LABEL"
echo -e "${BOLD}${CYAN}======================================================${NC}"

if [[ "$BBB_MODE" == "true" ]]; then
  printf "  %-18s %s  CDC: %s\n" "bbb_firmware" "$BBB_HOST" "$BBB_DEV"
fi
if [[ "$DEV_MODE" == "true" && -n "$PID_VRXN" ]]; then
  printf "  %-18s PID %s\n" "virtual_rx72n" "$PID_VRXN"
fi
printf "  %-18s PID %s   gRPC :50051   WS :8080\n" "star-gateway" "$PID_GW"
if [[ "$DEV_MODE" == "true" && -n "$PID_FAKEODOM" ]]; then
  printf "  %-18s PID %s   odom->base_link (static)\n" "fake_odom" "$PID_FAKEODOM"
fi
if [[ "$BBB_MODE" != "true" ]]; then
  printf "  %-18s PID %s\n" "spi_bridge" "${SPI_BRIDGE_PID:-unknown}"
fi
if [[ -n "$PID_SLAM" ]]; then
  printf "  %-18s PID %s   /scan @ 10 Hz + stereo (when connected)\n" "slam" "$PID_SLAM"
fi
if [[ -n "$PID_COMPLIANCE" ]]; then
  printf "  %-18s PID %s   ADA checks on /compliance/*\n" "compliance" "$PID_COMPLIANCE"
fi
printf "  %-18s PID %s\n" "gw_bridge" "${PID_GWBRIDGE:-unknown}"
if [[ -n "$PID_UI" ]]; then
  printf "  %-18s PID %s   http://%s:5173\n" "UI" "$PID_UI" "$DISPLAY_IP"
fi
if [[ -n "$PID_RVIZ" ]]; then
  printf "  %-18s PID %s\n" "rviz2" "$PID_RVIZ"
fi
printf "  %-18s ws://%s:8765\n" "foxglove_bridge" "$DISPLAY_IP"
if [[ -n "$PID_LICHTBLICK" ]]; then
  printf "  %-18s PID %s   http://%s:%s\n" "lichtblick" "$PID_LICHTBLICK" "$DISPLAY_IP" "$LICHTBLICK_PORT"
else
  printf "  %-18s not installed (run ./scripts/setup-lichtblick-web.sh)\n" "lichtblick"
fi

if [[ "$AP_ACTIVE" == "true" ]]; then
  echo -e ""
  echo -e "  ${BOLD}WiFi AP -- connect your laptop to '$WIFI_SSID'${NC}"
  printf "    %-12s %s\n" "SSID" "$WIFI_SSID"
  printf "    %-12s %s\n" "Password" "$WIFI_PASS"
  printf "    %-12s %s\n" "Pi5 IP" "$AP_IP"
  echo -e ""
  printf "    %-12s ssh star@%s\n" "SSH" "$AP_IP"
  printf "    %-12s http://%s:5173\n" "UI" "$AP_IP"
  printf "    %-12s ws://%s:8080/ws\n" "Gateway" "$AP_IP"
  if [[ -n "$PID_LICHTBLICK" ]]; then
    printf "    %-12s http://%s:%s\n" "Lichtblick" "$AP_IP" "$LICHTBLICK_PORT"
    printf "    %-12s ws://%s:8765  (paste into Lichtblick open dialog)\n" "Bridge WS" "$AP_IP"
  else
    printf "    %-12s ws://%s:8765  (use Foxglove/Lichtblick desktop app)\n" "Bridge WS" "$AP_IP"
  fi
fi

echo -e ""
echo -e "  Logs : $LOG_DIR/"
echo -e "  Stop : ./stop.sh  (AP stays up)  |  ./stop.sh --stop-ap  (tear down AP too)"
echo -e "${BOLD}${CYAN}======================================================${NC}"
echo -e ""

# Remove INT trap -- startup is complete, daemons run in background
trap - INT

# Disown all background processes so they survive shell exit (prevents SIGHUP)
disown -a
