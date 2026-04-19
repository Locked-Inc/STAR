#!/usr/bin/env bash
# Install the Hailo-8L runtime stack on a Raspberry Pi 5.
#
# Run this once after mounting the Raspberry Pi AI HAT+ 13 TOPS on the
# Pi 5. Requires Raspberry Pi OS Bookworm 64-bit with kernel >= 6.6.31.
#
# Reboots the Pi at the end. Re-run if you swap the HAT or upgrade
# Raspberry Pi OS.

set -euo pipefail

if [[ $EUID -ne 0 ]]; then
  echo "Re-running with sudo..."
  exec sudo "$0" "$@"
fi

echo "[1/5] apt update + full-upgrade"
apt update
apt full-upgrade -y

echo "[2/5] Installing hailo-all (kernel driver, firmware, HailoRT, TAPPAS)"
apt install -y hailo-all

echo "[3/5] Confirming PCIe overlay is enabled in /boot/firmware/config.txt"
CONFIG=/boot/firmware/config.txt
if ! grep -q "^dtparam=pciex1" "$CONFIG"; then
  echo "dtparam=pciex1" >> "$CONFIG"
  echo "  added dtparam=pciex1"
fi
if ! grep -q "^dtoverlay=pciex1-compat-pi5" "$CONFIG"; then
  echo "dtoverlay=pciex1-compat-pi5,no-mip" >> "$CONFIG"
  echo "  added dtoverlay=pciex1-compat-pi5,no-mip"
fi

echo "[4/5] Verifying device file"
if [[ -e /dev/hailo0 ]]; then
  echo "  /dev/hailo0 present"
else
  echo "  /dev/hailo0 not present yet -- will appear after reboot"
fi

echo "[5/5] Reboot required to load the kernel driver."
echo "After reboot, run:"
echo "  hailortcli fw-control identify"
echo "Expected output: Device: Hailo-8L"
echo
read -r -p "Reboot now? [y/N] " ans
if [[ "$ans" =~ ^[Yy]$ ]]; then
  reboot
fi
