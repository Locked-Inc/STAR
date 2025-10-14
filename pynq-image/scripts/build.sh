#!/bin/bash
# STAR-Z2 Custom Image Build Script
# Builds custom PYNQ image for STAR robot

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}STAR-Z2 Custom Image Build${NC}"
echo -e "${GREEN}========================================${NC}"

# Configuration
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PYNQ_DIR="$SCRIPT_DIR/../PYNQ"
BOARD_NAME="STAR-Z2"
BUILD_MODE="${1:-prebuilt}"  # prebuilt or full

# Check if PYNQ exists
if [ ! -d "$PYNQ_DIR" ]; then
    echo -e "${RED}ERROR: PYNQ directory not found${NC}"
    exit 1
fi

# Source PYNQ setup
echo -e "\n${YELLOW}Setting up build environment...${NC}"
if [ -f "$SCRIPT_DIR/setup-env.sh" ]; then
    source "$SCRIPT_DIR/setup-env.sh"
else
    echo -e "${RED}ERROR: setup-env.sh not found${NC}"
    exit 1
fi

# Source Xilinx tools if not already sourced
echo -e "\n${YELLOW}Checking for Xilinx tools...${NC}"
if [ -z "$XILINX_VIVADO" ]; then
    echo -e "${YELLOW}Xilinx Vivado not sourced, attempting to source automatically...${NC}"
    if [ -f "/tools/Xilinx/Vivado/2022.1/settings64.sh" ]; then
        source /tools/Xilinx/Vivado/2022.1/settings64.sh
        echo -e "${GREEN}Sourced Vivado from /tools/Xilinx/Vivado/2022.1/${NC}"
    elif [ -f "/opt/Xilinx/Vivado/2022.1/settings64.sh" ]; then
        source /opt/Xilinx/Vivado/2022.1/settings64.sh
        echo -e "${GREEN}Sourced Vivado from /opt/Xilinx/Vivado/2022.1/${NC}"
    else
        echo -e "${RED}ERROR: Xilinx Vivado not found${NC}"
        echo -e "${YELLOW}Please install Vivado 2022.1 or source manually${NC}"
        exit 1
    fi
fi

# Source PetaLinux if not already sourced
if [ -z "$PETALINUX" ]; then
    echo -e "${YELLOW}PetaLinux not sourced, attempting to source automatically...${NC}"
    if [ -f "/tools/Xilinx/PetaLinux/2022.1/settings.sh" ]; then
        # Clear $1 to avoid PetaLinux using it as a directory
        set --
        source /tools/Xilinx/PetaLinux/2022.1/settings.sh
        # Restore build mode from earlier
        set -- "$BUILD_MODE"
        echo -e "${GREEN}Sourced PetaLinux from /tools/Xilinx/PetaLinux/2022.1/${NC}"
    elif [ -f "/opt/petalinux/2022.1/settings.sh" ]; then
        # Clear $1 to avoid PetaLinux using it as a directory
        set --
        source /opt/petalinux/2022.1/settings.sh
        # Restore build mode from earlier
        set -- "$BUILD_MODE"
        echo -e "${GREEN}Sourced PetaLinux from /opt/petalinux/2022.1/${NC}"
    else
        echo -e "${RED}ERROR: PetaLinux not found${NC}"
        echo -e "${YELLOW}Please install PetaLinux 2022.1 or source manually${NC}"
        exit 1
    fi
fi

echo -e "${GREEN}Xilinx tools configured: Vivado and PetaLinux ready${NC}"

# Copy board configuration to PYNQ boards directory
echo -e "\n${YELLOW}Copying board configuration...${NC}"
BOARD_CONFIG="$SCRIPT_DIR/../board-config/STAR-Z2"
PYNQ_BOARDS="$PYNQ_DIR/boards"

if [ -d "$BOARD_CONFIG" ]; then
    rm -rf "$PYNQ_BOARDS/$BOARD_NAME"
    cp -r "$BOARD_CONFIG" "$PYNQ_BOARDS/"
    echo -e "${GREEN}Board configuration copied${NC}"
else
    echo -e "${RED}ERROR: Board configuration not found at $BOARD_CONFIG${NC}"
    exit 1
fi

# Build image
cd "$PYNQ_DIR/sdbuild"

echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}Starting Build Process${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "Build mode: $BUILD_MODE"
echo -e "Board: $BOARD_NAME"
echo -e "Threads: $BB_NUMBER_THREADS"
echo -e "${GREEN}========================================${NC}\n"

if [ "$BUILD_MODE" == "prebuilt" ]; then
    # Check for prebuilt image
    PREBUILT_IMAGE="${PREBUILT_IMAGE:-}"
    if [ -n "$PREBUILT_IMAGE" ] && [ -f "$PREBUILT_IMAGE" ]; then
        echo -e "${YELLOW}Using prebuilt image: $PREBUILT_IMAGE${NC}"
        make PREBUILT="$PREBUILT_IMAGE" BOARDS="$BOARD_NAME"
    else
        echo -e "${RED}ERROR: Prebuilt image not specified or not found${NC}"
        echo -e "${YELLOW}Set PREBUILT_IMAGE environment variable or run with 'full' mode${NC}"
        echo -e "${YELLOW}Example: PREBUILT_IMAGE=/path/to/image.img $0 prebuilt${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}Building from scratch (this will take 2-4 hours)...${NC}"
    make BOARDS="$BOARD_NAME" REBUILD_PYNQ_ROOTFS=1 REBUILD_PYNQ_SDIST=1
fi

# Check if build succeeded
if [ -d "$PYNQ_DIR/sdbuild/output" ]; then
    echo -e "\n${GREEN}========================================${NC}"
    echo -e "${GREEN}Build Complete!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo -e "\nImage location: $PYNQ_DIR/sdbuild/output/"
    ls -lh "$PYNQ_DIR/sdbuild/output/"
    echo -e "\n${YELLOW}Next steps:${NC}"
    echo -e "1. Flash to SD card: sudo dd if=output/$BOARD_NAME-*.img of=/dev/sdX bs=4M status=progress"
    echo -e "2. Boot PYNQ-Z2 board with SD card"
    echo -e "3. Connect to board: ssh xilinx@pynq (password: xilinx)"
else
    echo -e "\n${RED}Build failed or output not found${NC}"
    exit 1
fi
