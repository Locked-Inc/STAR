#!/usr/bin/env bash
# flash-emmc.sh -- Flash a new OS image to BeagleBone Blue eMMC over USB SSH
#
# Usage:
#   ./scripts/flash-emmc.sh <image.img>
#   ./scripts/flash-emmc.sh <image.img.xz>
#
# This writes the image directly to /dev/mmcblk1 (eMMC) while the BBB is
# booted from the SD card. After flashing, remove the SD card and reboot
# to boot from the new eMMC image.
#
# WARNING: This ERASES the eMMC. All existing data on eMMC will be lost.
#
# Prerequisites:
#   - BBB booted from SD card (not eMMC)
#   - SSH access via USB (192.168.6.2 or 192.168.7.2)
#   - sshpass installed on host

set -euo pipefail

if [ $# -lt 1 ]; then
    echo "Usage: $0 <image.img or image.img.xz>"
    exit 1
fi

IMAGE="$1"
BBB_HOST="${BBB_HOST:-192.168.6.2}"
BBB_USER="${BBB_USER:-debian}"
BBB_PASS="${BBB_PASS:-temppwd}"

SSH_CMD="sshpass -p ${BBB_PASS} ssh -o StrictHostKeyChecking=no ${BBB_USER}@${BBB_HOST}"

if [ ! -f "${IMAGE}" ]; then
    echo "ERROR: Image file not found: ${IMAGE}"
    exit 1
fi

echo "=== BeagleBone Blue eMMC Flash ==="
echo "Image: ${IMAGE}"
echo "Target: ${BBB_USER}@${BBB_HOST}:/dev/mmcblk1"
echo ""
echo "WARNING: This will ERASE the eMMC!"
echo "Make sure BBB is booted from SD card, not eMMC."
echo ""
read -rp "Continue? [y/N] " confirm
if [[ "${confirm}" != "y" && "${confirm}" != "Y" ]]; then
    echo "Aborted."
    exit 0
fi

# Check if image is compressed
if [[ "${IMAGE}" == *.xz ]]; then
    echo "[1/3] Streaming compressed image to eMMC..."
    xzcat "${IMAGE}" | sshpass -p "${BBB_PASS}" ssh -o StrictHostKeyChecking=no \
        "${BBB_USER}@${BBB_HOST}" "echo ${BBB_PASS} | sudo -S dd of=/dev/mmcblk1 bs=4M"
else
    echo "[1/3] Streaming image to eMMC..."
    cat "${IMAGE}" | sshpass -p "${BBB_PASS}" ssh -o StrictHostKeyChecking=no \
        "${BBB_USER}@${BBB_HOST}" "echo ${BBB_PASS} | sudo -S dd of=/dev/mmcblk1 bs=4M"
fi

echo "[2/3] Syncing..."
${SSH_CMD} "echo ${BBB_PASS} | sudo -S sync"

echo "[3/3] Flash complete."
echo ""
echo "Next steps:"
echo "  1. Power off the BBB"
echo "  2. Remove the SD card"
echo "  3. Power on -- BBB will boot from new eMMC image"
echo "  4. Default login: debian / temppwd"
echo "  5. Install librobotcontrol: sudo apt update && sudo apt install librobotcontrol"
