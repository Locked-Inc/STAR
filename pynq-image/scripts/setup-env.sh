#!/bin/bash
# STAR-Z2 Build Environment Setup Script
# Sets up the build environment for custom PYNQ image creation

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}STAR-Z2 Build Environment Setup${NC}"
echo -e "${GREEN}========================================${NC}"

# Check Ubuntu version
echo -e "\n${YELLOW}Checking Ubuntu version...${NC}"
UBUNTU_VERSION=$(lsb_release -rs)
if [[ "$UBUNTU_VERSION" != "18.04" && "$UBUNTU_VERSION" != "20.04" ]]; then
    echo -e "${RED}WARNING: Ubuntu $UBUNTU_VERSION detected. PYNQ builds are tested on 18.04 and 20.04.${NC}"
    echo -e "${RED}You may encounter compatibility issues.${NC}"
fi

# Check for Xilinx tools
echo -e "\n${YELLOW}Checking for Xilinx tools...${NC}"
if [ -d "/tools/Xilinx" ]; then
    echo -e "${GREEN}Xilinx tools directory found at /tools/Xilinx${NC}"
    ls -d /tools/Xilinx/*/ 2>/dev/null || echo "No tool versions found"
elif [ -d "/opt/Xilinx" ]; then
    echo -e "${GREEN}Xilinx tools directory found at /opt/Xilinx${NC}"
    ls -d /opt/Xilinx/*/ 2>/dev/null || echo "No tool versions found"
else
    echo -e "${RED}Xilinx tools not found at /tools/Xilinx or /opt/Xilinx${NC}"
    echo -e "${YELLOW}Please install Vivado/Vitis and PetaLinux tools${NC}"
    echo -e "${YELLOW}Required versions: 2020.2, 2021.1, or 2022.1${NC}"
fi

# Check disk space
echo -e "\n${YELLOW}Checking disk space...${NC}"
AVAILABLE_SPACE=$(df -BG . | awk 'NR==2 {print $4}' | sed 's/G//')
if [ "$AVAILABLE_SPACE" -lt 200 ]; then
    echo -e "${RED}WARNING: Less than 200GB available ($AVAILABLE_SPACE GB)${NC}"
    echo -e "${YELLOW}Builds require 200GB+ free space${NC}"
else
    echo -e "${GREEN}Sufficient disk space: ${AVAILABLE_SPACE}GB available${NC}"
fi

# Check RAM
echo -e "\n${YELLOW}Checking RAM...${NC}"
TOTAL_RAM=$(free -g | awk 'NR==2 {print $2}')
if [ "$TOTAL_RAM" -lt 16 ]; then
    echo -e "${RED}WARNING: Less than 16GB RAM detected (${TOTAL_RAM}GB)${NC}"
else
    echo -e "${GREEN}Sufficient RAM: ${TOTAL_RAM}GB${NC}"
fi

# Setup PYNQ build environment
echo -e "\n${YELLOW}Setting up PYNQ build environment...${NC}"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PYNQ_DIR="$SCRIPT_DIR/../PYNQ"

if [ -f "$PYNQ_DIR/sdbuild/scripts/setup_host.sh" ]; then
    echo -e "${GREEN}PYNQ setup_host.sh found (skipping - already set up)${NC}"
    # Skip running setup_host.sh since we already have QEMU 5.2.0 and dependencies installed
else
    echo -e "${RED}ERROR: PYNQ repository not found or incomplete${NC}"
    echo -e "${YELLOW}Run: git submodule update --init --recursive${NC}"
    exit 1
fi

# Export environment variables
export PYNQ_SDIST="$PYNQ_DIR"
export BOARD_DIR="$SCRIPT_DIR/../board-config"

# Set build parallelism based on CPU cores
NPROC=$(nproc)
export BB_NUMBER_THREADS=$((NPROC - 1))
export PARALLEL_MAKE="-j$((NPROC - 1))"

echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}Environment Setup Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "\nBuild parallelism: $BB_NUMBER_THREADS threads"
echo -e "PYNQ source: $PYNQ_SDIST"
echo -e "Board config: $BOARD_DIR"
echo -e "\n${YELLOW}Next steps:${NC}"
echo -e "1. Install Xilinx tools (Vivado/Vitis + PetaLinux)"
echo -e "2. Source Xilinx tool settings: source /opt/Xilinx/Vivado/202X.X/settings64.sh"
echo -e "3. Run build script: ./scripts/build.sh"
