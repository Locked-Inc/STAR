#!/bin/bash
# STAR Robot Image Flash Script
# Safely flashes PYNQ image to SD card

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}STAR Robot Image Flasher${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
IMAGE_FILE="$SCRIPT_DIR/PYNQ/sdbuild/output/STAR-Z2-3.0.1.img"

# Check if image exists
if [ ! -f "$IMAGE_FILE" ]; then
    echo -e "${RED}ERROR: Image file not found!${NC}"
    echo "Expected: $IMAGE_FILE"
    echo ""
    echo "Build the image first:"
    echo "  ./build.sh"
    exit 1
fi

IMG_SIZE=$(du -h "$IMAGE_FILE" | cut -f1)
echo -e "${GREEN}✓${NC} Found image: $IMG_SIZE"
echo ""

# Detect SD card
echo -e "${YELLOW}Detecting SD cards...${NC}"
echo ""
lsblk -o NAME,SIZE,TYPE,MOUNTPOINT,MODEL | grep -E "NAME|disk"
echo ""

# Prompt for device
echo -e "${YELLOW}Enter SD card device (e.g., sdc):${NC}"
read -p "Device: /dev/" DEVICE

if [ -z "$DEVICE" ]; then
    echo -e "${RED}No device specified!${NC}"
    exit 1
fi

DEVICE_PATH="/dev/$DEVICE"

# Verify device exists
if [ ! -b "$DEVICE_PATH" ]; then
    echo -e "${RED}ERROR: $DEVICE_PATH is not a valid block device!${NC}"
    exit 1
fi

# Show device info
echo ""
echo -e "${YELLOW}Device Information:${NC}"
lsblk -o NAME,SIZE,TYPE,FSTYPE,LABEL,MOUNTPOINT "$DEVICE_PATH"
echo ""

# Safety confirmation
echo -e "${RED}WARNING: This will ERASE ALL DATA on $DEVICE_PATH!${NC}"
echo ""
read -p "Type 'YES' to confirm: " CONFIRM

if [ "$CONFIRM" != "YES" ]; then
    echo -e "${YELLOW}Flash cancelled${NC}"
    exit 0
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Flashing Image...${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Unmount if mounted
echo -e "${YELLOW}Unmounting $DEVICE_PATH...${NC}"
sudo umount ${DEVICE_PATH}* 2>/dev/null || true
echo -e "${GREEN}✓${NC} Unmounted"
echo ""

# Flash image
echo -e "${YELLOW}Flashing (this will take 5-10 minutes)...${NC}"
echo ""

START_TIME=$(date +%s)

if sudo dd if="$IMAGE_FILE" of="$DEVICE_PATH" bs=4M status=progress && sync; then
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    MINUTES=$((DURATION / 60))
    SECONDS=$((DURATION % 60))

    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${GREEN}✓ Flash Complete!${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo -e "${GREEN}Flash time: ${MINUTES}m ${SECONDS}s${NC}"
    echo ""

    # Verify
    echo -e "${YELLOW}Verifying partitions...${NC}"
    sleep 2
    lsblk -o NAME,SIZE,TYPE,FSTYPE,LABEL "$DEVICE_PATH"
    echo ""

    echo -e "${GREEN}Next steps:${NC}"
    echo "  1. Safely eject SD card:"
    echo "     sudo eject $DEVICE_PATH"
    echo ""
    echo "  2. Insert into PYNQ-Z2 board"
    echo "  3. Set boot jumper to SD"
    echo "  4. Power on and SSH:"
    echo "     ssh star@192.168.2.99"
    echo "     (password: star)"
    echo ""
    echo "  Alternative with alias:"
    echo "     ssh star"
    echo ""
    echo -e "${YELLOW}Test checklist:${NC}"
    echo "  • Java version: java -version (should show 17)"
    echo "  • No Jupyter: jupyter --version (should fail)"
    echo "  • I2C: ls /dev/i2c-*"
    echo "  • USB: lsusb"
    echo ""

else
    echo ""
    echo -e "${RED}✗ Flash failed!${NC}"
    echo ""
    echo "Check:"
    echo "  1. SD card is not write-protected"
    echo "  2. Sufficient permissions (running with sudo)"
    echo "  3. Device is correct: $DEVICE_PATH"
    exit 1
fi
