#!/bin/bash

# PYNQ-Z2 Robot SD Card Flash Script for macOS
# Usage: ./flash-pynq-z2-macos.sh /dev/diskX

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_FILE="$SCRIPT_DIR/deploy/images/pynq-z2-robot.img"
KERNEL_FILE="$SCRIPT_DIR/deploy/images/zImage"

if [ $# -eq 0 ]; then
    echo "PYNQ-Z2 Robot SD Card Flash Script for macOS"
    echo "Usage: $0 <device>"
    echo "Example: $0 /dev/disk4"
    echo ""
    echo "Available devices:"
    diskutil list | grep -E "(external|removable)"
    echo ""
    echo "To see all devices: diskutil list"
    exit 1
fi

DEVICE="$1"

if [[ ! "$DEVICE" =~ ^/dev/disk[0-9]+$ ]]; then
    echo "Error: $DEVICE is not a valid disk device"
    echo "Expected format: /dev/diskX (e.g., /dev/disk4)"
    exit 1
fi

if [ ! -f "$IMAGE_FILE" ]; then
    echo "Error: Robot image not found at $IMAGE_FILE"
    echo "Make sure you have run the build and downloaded the images."
    exit 1
fi

echo "🤖 PYNQ-Z2 Robot SD Card Flash (macOS)"
echo "===================================="
echo "Image: $IMAGE_FILE ($(du -h "$IMAGE_FILE" | cut -f1))"
echo "Kernel: $KERNEL_FILE ($(du -h "$KERNEL_FILE" | cut -f1))"
echo "Target: $DEVICE"
echo ""

read -p "⚠️  This will ERASE all data on $DEVICE. Continue? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Cancelled."
    exit 1
fi

echo "🔧 Unmounting disk..."
diskutil unmountDisk "$DEVICE" || true

echo "📀 Flashing robot image to SD card..."
# Use raw disk (rdisk) for faster writing
RAW_DEVICE="${DEVICE/disk/rdisk}"
sudo dd if="$IMAGE_FILE" of="$RAW_DEVICE" bs=4m status=progress

echo "✅ Ejecting SD card..."
diskutil eject "$DEVICE"

echo ""
echo "🎉 PYNQ-Z2 Robot SD Card Ready!"
echo "================================"
echo "✅ Robot filesystem flashed successfully"
echo "✅ Linux kernel: ARM Cortex-A9 compatible"
echo "✅ Ready for PYNQ-Z2 robot hardware"
echo ""
echo "Next steps:"
echo "1. Insert SD card into PYNQ-Z2"
echo "2. Connect power and Ethernet"
echo "3. SSH to robot: ssh root@<robot-ip>"
echo ""
echo "Your PYNQ-Z2 robot is ready for LiDAR SLAM and computer vision!"