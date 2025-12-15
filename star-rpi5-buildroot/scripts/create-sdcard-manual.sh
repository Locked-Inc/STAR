#!/bin/bash
# Manual SD card image creation script (without genimage)

set -e

IMAGES_DIR="/home/bsikar/git/rpi-minimal-os/buildroot-2024.02.1/output/images"
OUTPUT_IMG="${IMAGES_DIR}/sdcard.img"

echo "Creating SD card image manually..."

# Create a 2.5GB image file
dd if=/dev/zero of="${OUTPUT_IMG}" bs=1M count=2560 status=progress

# Create partition table
echo "Creating partition table..."
parted -s "${OUTPUT_IMG}" mklabel msdos
parted -s "${OUTPUT_IMG}" mkpart primary fat32 1MiB 129MiB
parted -s "${OUTPUT_IMG}" mkpart primary ext4 129MiB 100%
parted -s "${OUTPUT_IMG}" set 1 boot on

# Setup loop device
echo "Setting up loop device..."
LOOP_DEV=$(sudo losetup -f --show -P "${OUTPUT_IMG}")

# Format boot partition (FAT32)
echo "Formatting boot partition..."
sudo mkfs.vfat -F 32 -n boot "${LOOP_DEV}p1"

# Copy boot files
echo "Copying boot files..."
mkdir -p /tmp/boot_mount
sudo mount "${LOOP_DEV}p1" /tmp/boot_mount

sudo cp "${IMAGES_DIR}/Image" /tmp/boot_mount/
sudo cp "${IMAGES_DIR}/bcm2711-rpi-4-b.dtb" /tmp/boot_mount/
sudo cp "${IMAGES_DIR}/rpi-firmware"/* /tmp/boot_mount/
sudo cp /home/bsikar/git/rpi-minimal-os/configs/config.txt /tmp/boot_mount/

sudo umount /tmp/boot_mount
rmdir /tmp/boot_mount

# Write root filesystem to second partition
echo "Writing root filesystem..."
sudo dd if="${IMAGES_DIR}/rootfs.ext4" of="${LOOP_DEV}p2" bs=4M status=progress

# Cleanup
echo "Cleaning up..."
sudo losetup -d "${LOOP_DEV}"

echo ""
echo "=== SD Card Image Created Successfully! ==="
echo "Image location: ${OUTPUT_IMG}"
echo ""
echo "To flash to SD card:"
echo "  sudo dd if=${OUTPUT_IMG} of=/dev/sdX bs=4M status=progress conv=fsync"
echo ""
