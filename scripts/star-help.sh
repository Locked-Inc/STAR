#!/usr/bin/env bash
# STAR: star-help -- one-stop cheat sheet for everything bolted onto this Pi5.
# Run this any time you forget how to flip between modes.

set -u

if [ -t 1 ] && command -v tput >/dev/null 2>&1 && [ "$(tput colors 2>/dev/null || echo 0)" -ge 8 ]; then
    C=1
else
    C=0
fi
c()   { [ "$C" = 1 ] && printf '\e[%sm' "$1"; }
c256(){ [ "$C" = 1 ] && printf '\e[38;5;%sm' "$1"; }
R=$(c 0); B=$(c 1); D=$(c 2)
GOLD=$(c256 220); CYAN=$(c256 51); GREEN=$(c256 46); MAG=$(c256 201)
BLUE=$(c256 39); GREY=$(c256 244); WHITE=$(c256 255); LIME=$(c256 118)

hdr() {
    printf "\n${B}${GOLD}> %s${R}\n" "$1"
    printf "${D}  %s${R}\n" "$(printf '-%.0s' $(seq 1 56))"
}
cmd() {
    local c="$1" desc="$2"
    if [ ${#c} -le 32 ]; then
        printf "  ${CYAN}%-32s${R} ${D}%s${R}\n" "$c" "$desc"
    else
        printf "  ${CYAN}%s${R}\n      ${D}%s${R}\n" "$c" "$desc"
    fi
}
note(){ printf "    ${D}%s${R}\n" "$1"; }

printf "\n${B}${MAG}  STAR cheat sheet${R}   ${D}(run ${R}${GOLD}star-help${R}${D} anytime)${R}\n"

hdr "MODE SWITCHING"
cmd "star-ap up"         "broadcast STAR-Robot AP (disconnects wlan0!)"
cmd "star-ap down"       "AP off, rejoin home Wi-Fi"
cmd "star-ap status"     "show current Wi-Fi mode"
note "SSID: STAR-Robot * gateway IP: 192.168.50.1"
printf "\n"
cmd "star-eth up"        "force activate direct-ethernet shared mode"
cmd "star-eth down"      "deactivate eth0 shared mode"
cmd "star-eth status"    "show eth carrier, IP, peer lease"
cmd "star-eth auto on|off" "toggle NM autoconnect on carrier-detect"
note "plug cable in -> Pi auto-serves DHCP; ssh star@10.42.0.1 (or star-desktop.local)"
printf "\n"
cmd "give-head"          "start GNOME desktop on top of headless runtime"
cmd "take-head"          "stop GNOME, return to pure console (~1 GB RAM back)"
note "robot services keep running either way"

hdr "BUILD & RUN"
cmd "./build-ros2.sh"                           "full build: proto gen + colcon (~6 min)"
cmd "ros2 launch star_bringup slam.launch.py"   "SLAM + Nav2 + EKF stack"
cmd "ros2 run star_spi_bridge star_spi_bridge_main"       "IMU publisher @ 100 Hz"
cmd "ros2 run star_gateway_bridge star_gateway_bridge_main" "telemetry bridge @ 10 Hz"

hdr "SUPPORT PROCESSES"
cmd "virtual_rx72n"                             "motor-controller sim (when RX72N not plugged)"
cmd "WS_STRICT_ORIGIN=false star-gateway"       "gateway on :8080 (CORS open for cockpit)"
cmd "rviz2 -d .../slam_lidar.rviz"              "visualization"

hdr "HARDWARE QUICK-CHECKS"
cmd "sudo systemctl stop ModemManager"          "free /dev/ttyUSB0 for RPLiDAR"
cmd "sudo chmod a+rw /dev/ttyUSB0"              "serial perms without relogin"
cmd "nmcli -t -f NAME,TYPE con show"            "list saved Wi-Fi networks"
cmd "systemctl status star-robot.service"       "robot stack status"

hdr "LOCATIONS"
note "repo:        /workspaces/STAR"
note "ROS2 ws:     /workspaces/STAR/star-ros2"
note "scripts:     /workspaces/STAR/scripts"

printf "\n${D}  banner:    ~/.star-banner.sh${R}\n"
printf "${D}  this help: %s${R}\n\n" "$0"
