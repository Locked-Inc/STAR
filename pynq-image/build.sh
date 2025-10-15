#!/bin/bash
# STAR Robot PYNQ Image Build Script
# Builds customized PYNQ image for STAR-Z2 board

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}STAR Robot PYNQ Image Builder${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BOARD_DIR="$SCRIPT_DIR/board-config"
BUILD_DIR="$SCRIPT_DIR/PYNQ/sdbuild"

echo -e "${YELLOW}Configuration:${NC}"
echo "  Script Dir:  $SCRIPT_DIR"
echo "  Board Dir:   $BOARD_DIR"
echo "  Build Dir:   $BUILD_DIR"
echo ""

# Check if PYNQ directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}ERROR: PYNQ build directory not found!${NC}"
    echo "Expected: $BUILD_DIR"
    exit 1
fi

# Check if board config exists
if [ ! -d "$BOARD_DIR/STAR-Z2" ]; then
    echo -e "${RED}ERROR: STAR-Z2 board config not found!${NC}"
    echo "Expected: $BOARD_DIR/STAR-Z2"
    exit 1
fi

echo -e "${GREEN}✓${NC} Directories verified"
echo ""

# Ensure PYNQ submodule is on the correct branch
echo -e "${YELLOW}Checking PYNQ submodule branch...${NC}"
cd "$SCRIPT_DIR/PYNQ"
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
if [ "$CURRENT_BRANCH" != "star-customizations" ]; then
    echo -e "${YELLOW}Switching to star-customizations branch...${NC}"
    git checkout star-customizations
    if [ $? -ne 0 ]; then
        echo -e "${RED}ERROR: Failed to checkout star-customizations branch${NC}"
        exit 1
    fi
fi
echo -e "${GREEN}✓${NC} PYNQ on branch: star-customizations"
echo ""

# Navigate to build directory
cd "$BUILD_DIR"

# Show what will be built
echo -e "${YELLOW}Build Configuration:${NC}"
echo "  Board:       STAR-Z2"
echo "  Changes:     Java 17, No Jupyter, No gcc-mb, No sigrok"
echo "  Output:      $BUILD_DIR/output/STAR-Z2-3.0.1.img"
echo ""

# Confirm build
echo -e "${YELLOW}Estimated build time: 30-45 minutes${NC}"
read -p "Start build? (y/n) " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}Build cancelled${NC}"
    exit 0
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Starting Build...${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Record start time
START_TIME=$(date +%s)

# Run build
echo -e "${GREEN}Running: make BOARDS=STAR-Z2 BOARDDIR=$BOARD_DIR REBUILD_PYNQ_ROOTFS=1${NC}"
echo ""

if make BOARDS=STAR-Z2 BOARDDIR="$BOARD_DIR" REBUILD_PYNQ_ROOTFS=1; then
    # Build succeeded
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    MINUTES=$((DURATION / 60))
    SECONDS=$((DURATION % 60))

    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${GREEN}✓ Build Complete!${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo -e "${GREEN}Build time: ${MINUTES}m ${SECONDS}s${NC}"
    echo ""

    # Show output image info
    OUTPUT_IMG="$BUILD_DIR/output/STAR-Z2-3.0.1.img"
    if [ -f "$OUTPUT_IMG" ]; then
        IMG_SIZE=$(du -h "$OUTPUT_IMG" | cut -f1)
        echo -e "${YELLOW}Image Details:${NC}"
        echo "  Location: $OUTPUT_IMG"
        echo "  Size:     $IMG_SIZE"
        echo ""
        echo -e "${GREEN}Next steps:${NC}"
        echo "  1. Flash to SD card:"
        echo "     ./flash.sh"
        echo ""
        echo "  2. Or manually:"
        echo "     sudo dd if=$OUTPUT_IMG of=/dev/sdX bs=4M status=progress && sync"
        echo ""
    else
        echo -e "${YELLOW}⚠ Image file not found at expected location${NC}"
    fi

else
    # Build failed
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${RED}✗ Build Failed${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo -e "${YELLOW}Check build log for errors:${NC}"
    echo "  cat $BUILD_DIR/build/build.log | grep -i error"
    echo ""
    echo -e "${YELLOW}Common issues:${NC}"
    echo "  1. Disk space: df -h"
    echo "  2. Clean and retry: make clean && ./build.sh"
    echo "  3. Check internet connection for package downloads"
    echo ""
    exit 1
fi
