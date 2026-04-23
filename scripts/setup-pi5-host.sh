#!/usr/bin/env bash
# STAR Pi5 host setup -- idempotent installer for OS-level robot settings.
#
# Installs udev rules, a systemd one-shot to pin the CPU governor, a
# logrotate config, and an optional (disabled) boot-time robot service.
# Safe to re-run; every write is a simple overwrite of a known path.
#
# Usage: sudo bash scripts/setup-pi5-host.sh
#
# See CLAUDE.local.md "Host setup" section for the pending TODOs
# (notably wifi power-save, deferred until pre-field deployment).

set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    echo "ERROR: must run as root (sudo bash $0)" >&2
    exit 1
fi

STAR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
say() { echo "[setup-pi5-host] $*"; }

# -----------------------------------------------------------------------------
# 1. USB autosuspend: force ON (power_save OFF) for BBB and RPLiDAR.
# -----------------------------------------------------------------------------
say "Installing /etc/udev/rules.d/70-star-usb-no-autosuspend.rules"
cat > /etc/udev/rules.d/70-star-usb-no-autosuspend.rules <<'RULE'
# STAR robot: force USB autosuspend off for critical serial links.
# Installed by scripts/setup-pi5-host.sh -- do not edit by hand.
#
# ENV{DEVTYPE}=="usb_device" restricts to device-level events (not
# usb_interface), so idVendor/idProduct/power/control are all direct
# attributes of the current node and ATTR{} works without parent walk.
# Reference: kernel.org/doc/Documentation/usb/power-management.txt
#
# RX72N Cypress CY7C65213 USB-UART bridge (on STAR PCB)
ACTION=="add", SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ATTR{idVendor}=="04b4", ATTR{idProduct}=="0003", ATTR{power/control}="on"
# RPLiDAR C1 (Silicon Labs CP2102N USB-UART bridge)
ACTION=="add", SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ATTR{idVendor}=="10c4", ATTR{idProduct}=="ea60", ATTR{power/control}="on"
RULE

udevadm control --reload-rules
# --action=change avoids re-firing add handlers (e.g. ModemManager) on every
# existing USB device; we only need to re-apply power/control attributes.
udevadm trigger --action=change --subsystem-match=usb || true

# -----------------------------------------------------------------------------
# 2. CPU governor: pin to schedutil via one-shot systemd unit.
# -----------------------------------------------------------------------------
say "Installing /etc/systemd/system/star-cpu-governor.service"
cat > /etc/systemd/system/star-cpu-governor.service <<'UNIT'
[Unit]
Description=STAR: set CPU governor to schedutil
# Run very early, before normal services: skip the default dependency on
# sysinit.target and friends, and order before base.target so this is set
# before the rest of user-space comes up. Reference:
# https://www.freedesktop.org/software/systemd/man/systemd.special.html
DefaultDependencies=no
After=sysinit.target
Before=base.target shutdown.target
Conflicts=shutdown.target

[Service]
Type=oneshot
ExecStart=/bin/sh -c 'for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do echo schedutil > $g; done'
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
UNIT

# -----------------------------------------------------------------------------
# 3. Log rotation for /tmp/star-logs/*.log
# -----------------------------------------------------------------------------
say "Installing /etc/logrotate.d/star-robot"
cat > /etc/logrotate.d/star-robot <<'LOGROT'
/tmp/star-logs/*.log {
    daily
    rotate 3
    size 20M
    compress
    missingok
    notifempty
    copytruncate
}
LOGROT

# -----------------------------------------------------------------------------
# 4. Optional boot-time robot service (installed, NOT enabled).
# -----------------------------------------------------------------------------
say "Installing /etc/systemd/system/star-robot.service (disabled by default)"
cat > /etc/systemd/system/star-robot.service <<UNIT
[Unit]
Description=STAR robot stack
After=network-online.target NetworkManager.service star-cpu-governor.service
Wants=network-online.target

[Service]
# start.sh backgrounds every component and calls 'disown -a' before exiting,
# so the ExecStart PID exits cleanly. RemainAfterExit=yes keeps systemd's
# view of the unit 'active' so ExecStop runs on host shutdown and
# 'systemctl status' reflects reality.
Type=oneshot
RemainAfterExit=yes
User=star
Group=star
WorkingDirectory=${STAR_DIR}
Environment=HOME=/home/star
Environment=PATH=/usr/local/go/bin:/usr/local/bin:/usr/bin:/bin
ExecStart=${STAR_DIR}/start.sh --no-ui
ExecStop=${STAR_DIR}/stop.sh
TimeoutStartSec=120
TimeoutStopSec=30
StandardOutput=append:/tmp/star-logs/systemd.log
StandardError=append:/tmp/star-logs/systemd.log

[Install]
WantedBy=multi-user.target
UNIT

# -----------------------------------------------------------------------------
# 5. Reload systemd and enable the CPU governor unit (but not star-robot).
# -----------------------------------------------------------------------------
systemctl daemon-reload
systemctl enable --now star-cpu-governor.service

say "Done. Verify with:"
cat <<EOF
  cat /sys/devices/system/cpu/cpufreq/policy0/scaling_governor   # schedutil
  ls /etc/udev/rules.d/70-star-usb-no-autosuspend.rules
  systemctl status star-cpu-governor.service
  systemctl list-unit-files star-robot.service                   # disabled
  sudo logrotate -d /etc/logrotate.d/star-robot
EOF
