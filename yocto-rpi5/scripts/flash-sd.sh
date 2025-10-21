#!/bin/bash
#
# Flash STAR minimal image to SD card
# Enhanced with safety checks and helpful prompts
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
YOCTO_ROOT="$(dirname "${SCRIPT_DIR}")"
BUILD_DIR="${YOCTO_ROOT}/build"
IMAGE_DIR="${BUILD_DIR}/tmp/deploy/images/raspberrypi5"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check for device argument
if [ -z "$1" ]; then
    echo "Usage: sudo $0 /dev/sdX"
    echo ""
    echo "Available block devices:"
    echo "------------------------"
    lsblk -d -o NAME,SIZE,TYPE,HOTPLUG,MODEL | grep -E "disk|NAME"
    echo ""
    echo -e "${YELLOW}TIP: Look for devices with HOTPLUG=1 (removable devices)${NC}"
    echo -e "${RED}WARNING: This will ERASE all data on the target device!${NC}"
    echo ""
    echo "Example: sudo $0 /dev/sdb"
    exit 1
fi

DEVICE="$1"

# Validate device
if [ ! -b "${DEVICE}" ]; then
    echo -e "${RED}ERROR: ${DEVICE} is not a block device!${NC}"
    exit 1
fi

# Safety check: prevent flashing to system disk
if [[ "${DEVICE}" == "/dev/sda" ]] || [[ "${DEVICE}" == "/dev/nvme0n1" ]]; then
    echo -e "${RED}ERROR: Refusing to flash to ${DEVICE} - this looks like a system disk!${NC}"
    echo "If you're ABSOLUTELY sure, edit this script to remove this safety check."
    exit 1
fi

# Check if device is mounted and auto-unmount
if mount | grep -q "${DEVICE}"; then
    echo -e "${YELLOW}Device ${DEVICE} is mounted. Attempting to unmount...${NC}"
    if [ "$EUID" -eq 0 ]; then
        umount ${DEVICE}* 2>/dev/null || true
        echo "✓ Unmounted"
    else
        echo -e "${RED}ERROR: ${DEVICE} is currently mounted!${NC}"
        echo "Please unmount it first:"
        echo "  sudo umount ${DEVICE}*"
        echo "Or run this script with sudo"
        exit 1
    fi
fi

# Find the image
IMAGE_FILE=$(ls -t "${IMAGE_DIR}/star-minimal-image"*".wic.bz2" 2>/dev/null | head -1)

if [ -z "${IMAGE_FILE}" ]; then
    # Try uncompressed
    IMAGE_FILE=$(ls -t "${IMAGE_DIR}/star-minimal-image"*".wic" 2>/dev/null | head -1)
fi

if [ -z "${IMAGE_FILE}" ]; then
    echo "ERROR: No image found in ${IMAGE_DIR}"
    echo "Please build the image first:"
    echo "  ./scripts/build-image.sh"
    exit 1
fi

echo "========================================="
echo "Flash SD Card"
echo "========================================="
echo ""
echo "Image: $(basename ${IMAGE_FILE})"
echo "Size:  $(du -h ${IMAGE_FILE} | cut -f1)"
echo "Device: ${DEVICE}"
echo ""

# Show device info
echo "Device info:"
lsblk -o NAME,SIZE,TYPE,VENDOR,MODEL ${DEVICE} 2>/dev/null || true
echo ""

echo -e "${RED}WARNING: This will DESTROY ALL DATA on ${DEVICE}!${NC}"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}ERROR: This script must be run as root${NC}"
    echo "Usage: sudo $0 ${DEVICE}"
    exit 1
fi

echo "Type 'yes' to confirm you want to erase ${DEVICE}"
read -p "Confirm: " confirm

if [ "$confirm" != "yes" ]; then
    echo "Aborted."
    exit 0
fi

echo ""
echo -e "${GREEN}Flashing image to ${DEVICE}...${NC}"
echo ""

# Decompress and write
if [[ "${IMAGE_FILE}" == *.bz2 ]]; then
    echo "Decompressing and writing (this may take several minutes)..."
    bunzip2 -dc "${IMAGE_FILE}" | dd of="${DEVICE}" bs=4M status=progress conv=fsync oflag=direct
else
    echo "Writing (this may take several minutes)..."
    dd if="${IMAGE_FILE}" of="${DEVICE}" bs=4M status=progress conv=fsync oflag=direct
fi

echo ""
echo "Syncing to disk (please wait)..."
sync
sleep 2  # Give the OS time to finish writes

echo ""
echo "========================================="
echo -e "${GREEN}✓ Flashing Complete!${NC}"
echo "========================================="
echo ""
echo "Next steps:"
echo -e "  1. ${GREEN}Remove SD card safely${NC} (it's safe to remove now)"
echo "  2. Insert into Raspberry Pi 5"
echo "  3. Connect Ethernet cable (recommended for first boot)"
echo "  4. Power on"
echo ""
echo "First boot:"
echo -e "  • Static IP: ${GREEN}192.168.2.100${NC}"
echo -e "  • SSH in: ${GREEN}ssh root@192.168.2.100${NC} or ${GREEN}ssh star@192.168.2.100${NC}"
echo ""
echo "Credentials:"
echo -e "  • root user: ${YELLOW}root:star${NC}"
echo -e "  • normal user: ${YELLOW}star:star${NC}"
echo ""
echo "System info:"
echo "  • Hostname: star-robot"
echo "  • Init: systemd"
echo "  • SSH: dropbear (port 22)"
echo "  • Network: Static IP 192.168.2.100 on eth0"
echo ""
echo "========================================="
