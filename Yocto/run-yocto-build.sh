#!/bin/bash

# Yocto Build Runner Script
# This script handles running the Yocto build as the correct user

set -e

# Signal handler for graceful shutdown
cleanup() {
    echo -e "\n${YELLOW}Received interrupt signal, cleaning up...${NC}"
    if [[ -n "$BITBAKE_PID" ]]; then
        echo -e "${YELLOW}Terminating bitbake process (PID: $BITBAKE_PID)...${NC}"
        kill -TERM "$BITBAKE_PID" 2>/dev/null || true
        sleep 3
        kill -KILL "$BITBAKE_PID" 2>/dev/null || true
    fi
    # Kill any remaining bitbake processes
    pkill -f "bitbake tisdk-adas-image" 2>/dev/null || true
    echo -e "${RED}Build cancelled by user${NC}"
    exit 130
}

# Trap SIGINT (Ctrl+C) and SIGTERM
trap cleanup SIGINT SIGTERM

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Yocto Build Runner ===${NC}"

# Check if running as root
if [[ $EUID -eq 0 ]]; then
    echo -e "${YELLOW}Running as root, switching to yocto user...${NC}"
    
    # Check if yocto user exists
    if ! id "yocto" &>/dev/null; then
        echo -e "${RED}Error: yocto user does not exist${NC}"
        echo "Please run the setup script first or create the user manually:"
        echo "sudo useradd -m -s /bin/bash yocto"
        exit 1
    fi
    
    # Check if build directory exists in yocto home
    if [[ ! -d "/home/yocto/yocto-j721e-build" ]]; then
        echo -e "${RED}Error: Build directory not found at /home/yocto/yocto-j721e-build${NC}"
        echo "Please ensure the build directory has been moved to the yocto user's home"
        exit 1
    fi
    
    # Copy script to yocto user's home if it doesn't exist there
    SCRIPT_PATH="/home/yocto/run-yocto-build.sh"
    if [[ ! -f "$SCRIPT_PATH" ]] || [[ "$0" -nt "$SCRIPT_PATH" ]]; then
        echo -e "${YELLOW}Copying script to yocto user's home directory...${NC}"
        cp "$0" "$SCRIPT_PATH"
        chown yocto:yocto "$SCRIPT_PATH"
        chmod +x "$SCRIPT_PATH"
    fi
    
    echo -e "${GREEN}Switching to yocto user and starting build...${NC}"
    exec su - yocto -c "$SCRIPT_PATH"
else
    echo -e "${GREEN}Running as user: $(whoami)${NC}"
fi

# Now running as yocto user
BUILD_DIR="$HOME/yocto-j721e-build/oe-layersetup/build"

# Check if build directory exists
if [[ ! -d "$BUILD_DIR" ]]; then
    echo -e "${RED}Error: Build directory not found at $BUILD_DIR${NC}"
    exit 1
fi

# Navigate to build directory
echo -e "${YELLOW}Navigating to build directory: $BUILD_DIR${NC}"
cd "$BUILD_DIR"

# Check if setenv exists
if [[ ! -f "conf/setenv" ]]; then
    echo -e "${RED}Error: conf/setenv not found${NC}"
    exit 1
fi

# Source the environment
echo -e "${YELLOW}Sourcing build environment...${NC}"
source conf/setenv

# Check if bitbake is available
if ! command -v bitbake &> /dev/null; then
    echo -e "${RED}Error: bitbake command not found after sourcing environment${NC}"
    exit 1
fi

# Check for and fix patch issues
echo -e "${YELLOW}Checking for patch issues...${NC}"

# Reset any existing devtool workspace that might interfere
if devtool status | grep -q u-boot-ti-staging; then
    echo -e "${YELLOW}Resetting devtool workspace for u-boot-ti-staging...${NC}"
    devtool reset u-boot-ti-staging || true
fi

# Restore missing patches if needed
PATCH_DIR="/home/yocto/yocto-j721e-build/oe-layersetup/sources/meta-edgeai/recipes-bsp/u-boot/u-boot-ti-staging"
WORK_DIR="/home/yocto/yocto-j721e-build/oe-layersetup/build/arago-tmp-default-glibc/work/j721e_evm-oe-linux/u-boot-ti-staging/2025.01+git"

if [[ ! -f "$PATCH_DIR/0001-Optimal-QoS-Settings.patch" ]] && [[ -f "$WORK_DIR/0001-Optimal-QoS-Settings.patch" ]]; then
    echo -e "${YELLOW}Restoring missing QoS patch...${NC}"
    cp "$WORK_DIR/0001-Optimal-QoS-Settings.patch" "$PATCH_DIR/" || true
fi

if [[ ! -f "$PATCH_DIR/0001-arch-arm-dts-k3-j721e-Update-memory-map-for-PSDK-RTO.patch" ]] && [[ -f "$WORK_DIR/0001-arch-arm-dts-k3-j721e-Update-memory-map-for-PSDK-RTO.patch" ]]; then
    echo -e "${YELLOW}Restoring missing memory-map patch...${NC}"
    cp "$WORK_DIR/0001-arch-arm-dts-k3-j721e-Update-memory-map-for-PSDK-RTO.patch" "$PATCH_DIR/" || true
fi

echo -e "${YELLOW}Patch check complete${NC}"

# Start the build
echo -e "${GREEN}Starting Yocto build for tisdk-adas-image...${NC}"
echo -e "${YELLOW}This will take 1-3 hours depending on system resources${NC}"
echo -e "${YELLOW}Press Ctrl+C to cancel${NC}"
echo

# Start bitbake in background and capture PID
bitbake tisdk-adas-image &
BITBAKE_PID=$!

# Wait for the background process to complete
wait $BITBAKE_PID

echo -e "${GREEN}=== Build Complete! ===${NC}"
echo -e "${GREEN}Images can be found at:${NC}"
echo -e "${YELLOW}$BUILD_DIR/arago-tmp-external-arm-glibc/deploy/images/j721e-evm/${NC}"