#!/bin/bash
# SD Card Flashing Script for STAR-Z2
# Safely flashes the custom PYNQ image to SD card

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}STAR-Z2 SD Card Flasher${NC}"
echo -e "${GREEN}========================================${NC}"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}ERROR: This script must be run as root (use sudo)${NC}"
    exit 1
fi

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PYNQ_DIR="$SCRIPT_DIR/../PYNQ"
OUTPUT_DIR="$PYNQ_DIR/sdbuild/output"

# Find the latest image
if [ -d "$OUTPUT_DIR" ]; then
    IMAGE=$(ls -t "$OUTPUT_DIR"/*.img 2>/dev/null | head -1)
    if [ -z "$IMAGE" ]; then
        echo -e "${RED}ERROR: No image files found in $OUTPUT_DIR${NC}"
        exit 1
    fi
else
    echo -e "${RED}ERROR: Output directory not found: $OUTPUT_DIR${NC}"
    exit 1
fi

# Check if image file specified as argument
if [ -n "$1" ]; then
    if [ -f "$1" ]; then
        IMAGE="$1"
    else
        echo -e "${RED}ERROR: Specified image file not found: $1${NC}"
        exit 1
    fi
fi

echo -e "\n${GREEN}Image to flash:${NC} $IMAGE"
IMAGE_SIZE=$(du -h "$IMAGE" | cut -f1)
echo -e "${GREEN}Image size:${NC} $IMAGE_SIZE"

# List available block devices
echo -e "\n${YELLOW}Available block devices:${NC}"
lsblk -d -o NAME,SIZE,TYPE,MODEL | grep -E "disk|NAME"

# Get target device
echo -e "\n${YELLOW}Enter target device (e.g., /dev/sdb):${NC} "
read -r DEVICE

# Validate device
if [ ! -b "$DEVICE" ]; then
    echo -e "${RED}ERROR: $DEVICE is not a valid block device${NC}"
    exit 1
fi

# Show device info and confirm
echo -e "\n${RED}WARNING: This will DESTROY all data on $DEVICE${NC}"
echo -e "${YELLOW}Device information:${NC}"
lsblk "$DEVICE"
echo -e "\n${YELLOW}Are you sure you want to flash to $DEVICE? (yes/no):${NC} "
read -r CONFIRM

if [ "$CONFIRM" != "yes" ]; then
    echo -e "${YELLOW}Aborted by user${NC}"
    exit 0
fi

# Unmount any mounted partitions
echo -e "\n${YELLOW}Unmounting any mounted partitions on $DEVICE...${NC}"
umount "${DEVICE}"* 2>/dev/null || true

# Flash image
echo -e "\n${GREEN}Flashing image to $DEVICE...${NC}"
echo -e "${YELLOW}This may take several minutes...${NC}"

dd if="$IMAGE" of="$DEVICE" bs=4M status=progress conv=fsync

# Sync to ensure all data is written
echo -e "\n${YELLOW}Syncing...${NC}"
sync

echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}Flashing Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "\n${YELLOW}Next steps:${NC}"
echo -e "1. Safely remove SD card"
echo -e "2. Insert into PYNQ-Z2 board"
echo -e "3. Set boot mode to SD card (jumper JP4)"
echo -e "4. Power on board"
echo -e "5. Connect to board: ssh xilinx@pynq (default password: xilinx)"
echo -e "\n${GREEN}Board IP will be displayed on HDMI or accessible via hostname 'pynq'${NC}"
